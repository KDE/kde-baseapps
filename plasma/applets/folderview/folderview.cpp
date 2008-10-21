/*
 *   Copyright © 2008 Fredrik Höglund <fredrik@kde.org>
 *   Copyright © 2008 Rafael Fernández López <ereslibre@kde.org>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public License
 *   along with this library; see the file COPYING.LIB.  If not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#include "folderview.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QDrag>
#include <QGraphicsView>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsProxyWidget>
#include <QItemSelectionModel>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>

#include <KAction>
#include <KAuthorized>
#include <KBookmarkManager>
#include <KConfigDialog>
#include <KDesktopFile>
#include <KDirModel>
#include <KFileItemDelegate>
#include <kfileplacesmodel.h>
#include <kfilepreviewgenerator.h>
#include <KGlobalSettings>
#include <KMenu>
#include <KStandardShortcut>
#include <KStringHandler>

#include <kio/fileundomanager.h>
#include <kio/paste.h>
#include <KParts/BrowserExtension>

#include <knewmenu.h>
#include <konqmimedata.h>
#include <konq_operations.h>
#include <konq_popupmenu.h>

#include "dirlister.h"
#include "proxymodel.h"
#include "previewpluginsmodel.h"
#include "plasma/theme.h"
#include "plasma/corona.h"
#include "plasma/widgets/scrollbar.h"
#include "plasma/paintutils.h"

#include "folderviewadapter.h"

#ifdef Q_WS_X11
#include <QX11Info>
#include <X11/Xlib.h>
#endif

#include <limits.h>


K_EXPORT_PLASMA_APPLET(folderview, FolderView)

MimeModel::MimeModel(QObject *parent)
    : QStringListModel(parent)
{
    m_mimetypes = KMimeType::allMimeTypes();
}

QVariant MimeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    KMimeType *mime = static_cast<KMimeType*>(index.internalPointer());
    switch (role) {
        case Qt::DisplayRole: {
            if (!mime->comment().isEmpty()) {
                QString description;
                if (mime->patterns().count()) {
                    description = mime->patterns().join(", ");
                } else {
                    description = mime->name();
                }
                return QString("%1 (%2)").arg(mime->comment()).arg(description);
            } else {
                return mime->name();
            }
        }
        case Qt::DecorationRole:
            return KIcon(mime->iconName());
        case Qt::CheckStateRole:
            return m_state[mime];
        default:
            return QStringListModel::data(index, role);
    }
}

Qt::ItemFlags MimeModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags itemFlags = QStringListModel::flags(index);
    itemFlags &= ~Qt::ItemIsEditable;
    if (!index.isValid()) {
        return itemFlags;
    }
    return itemFlags | Qt::ItemIsUserCheckable;
}

QModelIndex MimeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || row >= m_mimetypes.count()) {
        return QModelIndex();
    } 
    return createIndex(row, column, (void*) m_mimetypes[row].data());
}

int MimeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_mimetypes.count();
}

bool MimeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) {
        return false;
    }
    
    if (role == Qt::CheckStateRole) {
        KMimeType *mime = static_cast<KMimeType*>(index.internalPointer());
        m_state[mime] = (Qt::CheckState) value.toInt();
        emit dataChanged(index, index);
        return true;
    }
    
    return QStringListModel::setData(index, value, role);
}



// ---------------------------------------------------------------------------



ProxyMimeModel::ProxyMimeModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void ProxyMimeModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    QSortFilterProxyModel::setSourceModel(sourceModel);
    sort(0);
}

void ProxyMimeModel::setFilter(const QString &filter)
{
    m_filter = filter;
    invalidateFilter();
}

bool ProxyMimeModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    KMimeType *leftPtr = static_cast<KMimeType*>(left.internalPointer());
    KMimeType *rightPtr = static_cast<KMimeType*>(right.internalPointer());
    
    return KStringHandler::naturalCompare(leftPtr->comment(), rightPtr->comment()) < 0;
}

bool ProxyMimeModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex sourceIndex = sourceModel()->index(source_row, 0, source_parent);
    KMimeType *mime = static_cast<KMimeType*>(sourceIndex.internalPointer());
    if (m_filter.isEmpty()) {
        return true;
    }
    
    bool fastRet = mime->comment().contains(m_filter, Qt::CaseInsensitive) ||
                   ((!mime->patterns().count() || mime->comment().isEmpty()) && mime->name().contains(m_filter, Qt::CaseInsensitive));
                   
    if (fastRet) {
        return true;
    }
    
    foreach (const QString &pattern, mime->patterns()) {
        if (pattern.contains(m_filter, Qt::CaseInsensitive)) {
            return true;
        }
    }
    
    return false;
}



// ---------------------------------------------------------------------------



// Proxy model for KFilePlacesModel that filters out hidden items.
class PlacesFilterModel : public QSortFilterProxyModel
{
public:
    PlacesFilterModel(QObject *parent = 0) : QSortFilterProxyModel(parent) {}
    bool filterAcceptsRow(int row, const QModelIndex &parent) const {
        KFilePlacesModel *model = static_cast<KFilePlacesModel*>(sourceModel());
        const QModelIndex index = model->index(row, 0, parent);
        return !model->isHidden(index);
    }
};



// ---------------------------------------------------------------------------



FolderView::FolderView(QObject *parent, const QVariantList &args)
    : Plasma::Containment(parent, args),
      m_previewGenerator(0),
      m_titleHeight(0),
      m_lastScrollValue(0),
      m_viewScrolled(false),
      m_newMenu(0),
      m_actionCollection(this),
      m_columns(0),
      m_rows(0),
      m_validRows(0),
      m_layoutBroken(false),
      m_needPostLayoutPass(false),
      m_initialListing(true),
      m_positionsLoaded(false),
      m_doubleClick(false),
      m_dragInProgress(false),
      m_animFrame(0)
{
    setContainmentType(DesktopContainment);
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    setAcceptHoverEvents(true);
    setAcceptDrops(true);
    resize(600, 400);

    m_dirModel = new KDirModel(this);
    m_dirModel->setDropsAllowed(KDirModel::DropOnDirectory | KDirModel::DropOnLocalExecutable);

    m_model = new ProxyModel(this);
    m_model->setSourceModel(m_dirModel);
    m_model->setSortLocaleAware(true);
    m_model->setFilterCaseSensitivity(Qt::CaseInsensitive);

    connect(m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(rowsInserted(QModelIndex,int,int)));
    connect(m_model, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(rowsRemoved(QModelIndex,int,int)));
    connect(m_model, SIGNAL(modelReset()), SLOT(modelReset()));
    connect(m_model, SIGNAL(layoutChanged()), SLOT(layoutChanged()));
    connect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(dataChanged(QModelIndex,QModelIndex)));

    m_delegate = new KFileItemDelegate(this);
    connect(m_delegate, SIGNAL(closeEditor(QWidget*,QAbstractItemDelegate::EndEditHint)),
            SLOT(closeEditor(QWidget*,QAbstractItemDelegate::EndEditHint)));
    connect(m_delegate, SIGNAL(commitData(QWidget*)), SLOT(commitData(QWidget*)));

    m_selectionModel = new QItemSelectionModel(m_model, this);
    m_scrollBar = new Plasma::ScrollBar(Qt::Vertical, this);
    m_scrollBar->hide();
    connect(m_scrollBar->nativeWidget(), SIGNAL(valueChanged(int)), SLOT(scrollBarValueChanged(int)));

    if (args.count() > 0) {
        setUrl(KUrl(args.value(0).toString()));
    }

    // As we use some part of konqueror libkonq must be added to have translations
    KGlobal::locale()->insertCatalog("libkonq");

}

void FolderView::init()
{
    Containment::init();

    // We handle the caching ourselves
    setCacheMode(NoCache);

    // Find out about icon and font settings changes
    connect(KGlobalSettings::self(), SIGNAL(kdisplayFontChanged()), SLOT(fontSettingsChanged()));
    connect(KGlobalSettings::self(), SIGNAL(iconChanged(int)), SLOT(iconSettingsChanged(int)));

    // Find out about theme changes
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), SLOT(themeChanged()));

    m_font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DesktopFont);

    KConfigGroup cg = config();

    m_customLabel = cg.readEntry("customLabel", "");
    m_customIconSize = cg.readEntry("customIconSize", 0);
    m_showPreviews = cg.readEntry("showPreviews", true);
    m_drawShadows = cg.readEntry("drawShadows", true);
    m_numTextLines = cg.readEntry("numTextLines", 2);
    m_textColor = cg.readEntry("textColor", QColor(Qt::transparent));

    if (!m_url.isValid()) {
        setUrl(cg.readEntry("url", KUrl(isContainment() ? QString("desktop:/") : QDir::homePath())));
    } else {
        KConfigGroup cg = config();
        cg.writeEntry("url", m_url);
    }

    m_filterFiles = cg.readEntry("filterFiles", "*");
    m_filterType = cg.readEntry("filter", 0);
    m_filterFilesMimeList = cg.readEntry("mimeFilter", QStringList());
    m_model->setFilterMode(ProxyModel::filterModeFromInt(m_filterType));
    m_model->setMimeTypeFilterList(m_filterFilesMimeList);

    m_sortDirsFirst = cg.readEntry("sortDirsFirst", true);
    m_sortColumn = cg.readEntry("sortColumn", int(KDirModel::Name));
    m_model->setSortDirectoriesFirst(m_sortDirsFirst);
    m_model->sort(m_sortColumn != -1 ? m_sortColumn : KDirModel::Name, Qt::AscendingOrder);

    DirLister *lister = new DirLister(this);
    lister->setDelayedMimeTypes(true);
    lister->setAutoErrorHandlingEnabled(false, 0);
    connect(lister, SIGNAL(started(KUrl)), SLOT(listingStarted(KUrl)));
    connect(lister, SIGNAL(completed()), SLOT(listingCompleted()));
    connect(lister, SIGNAL(canceled()), SLOT(listingCanceled()));
    connect(lister, SIGNAL(showErrorMessage(QString)), SLOT(listingError(QString)));

    m_model->setFilterFixedString(m_filterFiles);
    m_dirModel->setDirLister(lister);

    m_previewPlugins = cg.readEntry("previewPlugins", QStringList() << "imagethumbnail");

    FolderViewAdapter *adapter = new FolderViewAdapter(this);
    m_previewGenerator = new KFilePreviewGenerator(adapter, m_model);
    m_previewGenerator->setPreviewShown(m_showPreviews);
    m_previewGenerator->setEnabledPlugins(m_previewPlugins);

    lister->openUrl(m_url);

    m_flow = isContainment() ? QListView::TopToBottom : QListView::LeftToRight;
    m_flow = static_cast<QListView::Flow>(cg.readEntry("flow", static_cast<int>(m_flow)));
    m_iconsLocked = cg.readEntry("iconsLocked", false);
    m_alignToGrid = cg.readEntry("alignToGrid", false);

    createActions();

    connect(QApplication::clipboard(), SIGNAL(dataChanged()), SLOT(clipboardDataChanged()));
}

FolderView::~FolderView()
{
    delete m_newMenu;
}

void FolderView::saveState(KConfigGroup &config) const
{
    Q_UNUSED(config)

    if (m_delayedSaveTimer.isActive()) {
        //m_delayedSaveTimer.stop();  // Can't stop the timer since this is a const method
        saveIconPositions();
    }
}

QScrollBar *FolderView::verticalScrollBar() const
{
    return m_scrollBar->nativeWidget();
}

QAbstractItemModel *FolderView::model() const
{
    return m_model;
}

QRect FolderView::visibleArea() const
{
    return mapToViewport(contentsRect()).toAlignedRect();
}

void FolderView::createAnimationFrames()
{
    m_animFrames = QPixmap(100, 100 * 20);
    m_animFrames.fill(Qt::transparent);

    QPainterPath path;
    path.addRect(-2, -40, 4, 16);

    QPainter p(&m_animFrames);
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(50, 50);

    for (int i = 0; i < 20; i++)
    {
        p.translate(1, 1);
        for (int j = 0; j < 20; j++)
        {
            p.fillPath(path, QColor(0, 0, 0, 128));
            p.rotate(360 / 20);
        }
        p.translate(-1, -1);
        for (int j = 0; j < 20; j++)
        {
            const QColor color = (i == j) ? Qt::white : QColor(200, 200, 200);
            p.fillPath(path, color);
            p.rotate(360 / 20);
        }
        p.translate(0, 100);
    }
}

void FolderView::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget *widgetFilter = new QWidget;
    QWidget *widgetDisplay = new QWidget;
    QWidget *widgetLocation = new QWidget;
    uiFilter.setupUi(widgetFilter);
    uiDisplay.setupUi(widgetDisplay);
    uiLocation.setupUi(widgetLocation);

    KFilePlacesModel *placesModel = new KFilePlacesModel(parent);
    PlacesFilterModel *placesFilter = new PlacesFilterModel(parent);
    placesFilter->setSourceModel(placesModel);
    uiLocation.placesCombo->setModel(placesFilter);

    if (m_url == KUrl("desktop:/")) {
        uiLocation.showDesktopFolder->setChecked(true);
        uiLocation.placesCombo->setEnabled(false);
        uiLocation.lineEdit->setEnabled(false);
    } else {
        QModelIndex index;
        for (int i = 0; i < placesFilter->rowCount(); i++) {
            KUrl url = placesModel->url(placesFilter->mapToSource(placesFilter->index(i, 0)));
            if (url.equals(m_url, KUrl::CompareWithoutTrailingSlash)) {
                index = placesFilter->index(i, 0);
                break;
            }
        }
        if (index.isValid()) {
            uiLocation.placesCombo->setCurrentIndex(index.row());
            uiLocation.showPlace->setChecked(true);
            uiLocation.lineEdit->setEnabled(false);
        } else {
            uiLocation.showCustomFolder->setChecked(true);
            uiLocation.lineEdit->setUrl(m_url);
            uiLocation.placesCombo->setEnabled(false);
        }
    }

    uiLocation.lineEdit->setMode(KFile::Directory);
    uiFilter.filterFilesPattern->setText(m_filterFiles);

    MimeModel *mimeModel = new MimeModel(uiFilter.filterFilesList);
    ProxyMimeModel *pMimeModel = new ProxyMimeModel(uiFilter.filterFilesList);
    pMimeModel->setSourceModel(mimeModel);
    uiFilter.filterFilesList->setModel(pMimeModel);

    // The label is not shown when the applet is acting as a containment,
    // so don't confuse the user by making it editable.
    if (isContainment()) {
        uiDisplay.labelEdit->hide();
    }

    uiDisplay.labelEdit->setText(m_titleText);

    const QList<int> iconSizes = QList<int>() << 16 << 22 << 32 << 48 << 64 << 128;
    uiDisplay.sizeSlider->setRange(0, iconSizes.size() - 1);
    uiDisplay.sizeSlider->setValue(iconSizes.indexOf(iconSize().width()));

    uiDisplay.sortCombo->addItem(i18nc("Sort Icons", "Unsorted"), -1);
    uiDisplay.sortCombo->addItem(m_actionCollection.action("sort_name")->text(), KDirModel::Name);
    uiDisplay.sortCombo->addItem(m_actionCollection.action("sort_size")->text(), KDirModel::Size);
    uiDisplay.sortCombo->addItem(m_actionCollection.action("sort_type")->text(), KDirModel::Type);
    uiDisplay.sortCombo->addItem(m_actionCollection.action("sort_date")->text(), KDirModel::ModifiedTime);

    uiDisplay.flowCombo->addItem(i18n("Top to Bottom"), QListView::TopToBottom);
    uiDisplay.flowCombo->addItem(i18n("Left to Right"), QListView::LeftToRight);

    uiDisplay.alignToGrid->setChecked(m_alignToGrid);
    uiDisplay.lockInPlace->setChecked(m_iconsLocked);
    uiDisplay.drawShadows->setChecked(m_drawShadows);
    uiDisplay.showPreviews->setChecked(m_showPreviews);
    uiDisplay.previewsAdvanced->setEnabled(m_showPreviews);
    uiDisplay.numLinesEdit->setValue(m_numTextLines);

    if (m_textColor != Qt::transparent) {
        uiDisplay.colorButton->setColor(m_textColor);
    } else {
        uiDisplay.colorButton->setColor(Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
    }

    for (int i = 0; i < uiDisplay.sortCombo->maxCount(); i++) {
       if (m_sortColumn == uiDisplay.sortCombo->itemData(i).toInt()) {
           uiDisplay.sortCombo->setCurrentIndex(i);
           break;
       }
    }

    for (int i = 0; i < uiDisplay.flowCombo->maxCount(); i++) {
       if (m_flow == uiDisplay.flowCombo->itemData(i).toInt()) {
           uiDisplay.flowCombo->setCurrentIndex(i);
           break;
       }
    }

    // Hide the icon arrangement controls when we're not acting as a containment,
    // since this option doesn't make much sense in the applet.
    if (!isContainment()) {
        uiDisplay.flowLabel->hide();
        uiDisplay.flowCombo->hide();
    }

    connect(uiFilter.searchMimetype, SIGNAL(textChanged(QString)), pMimeModel, SLOT(setFilter(QString)));

    parent->addPage(widgetLocation, i18n("Location"), "folder");
    parent->addPage(widgetDisplay, i18n("Display"), "preferences-desktop-display");
    parent->addPage(widgetFilter, i18n("Filter"), "view-filter");
    parent->setButtons(KDialog::Ok | KDialog::Cancel | KDialog::Apply);

    connect(parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(configAccepted()));
    connect(uiLocation.showPlace, SIGNAL(toggled(bool)), uiLocation.placesCombo, SLOT(setEnabled(bool)));
    connect(uiLocation.showCustomFolder, SIGNAL(toggled(bool)), uiLocation.lineEdit, SLOT(setEnabled(bool)));
    connect(uiFilter.filterType, SIGNAL(currentIndexChanged(int)), this, SLOT(filterChanged(int)));
    connect(uiFilter.selectAll, SIGNAL(clicked(bool)), this, SLOT(selectUnselectAll()));
    connect(uiFilter.deselectAll, SIGNAL(clicked(bool)), this, SLOT(selectUnselectAll()));
    connect(uiDisplay.previewsAdvanced, SIGNAL(clicked()), this, SLOT(showPreviewConfigDialog()));
    connect(uiDisplay.showPreviews, SIGNAL(toggled(bool)), uiDisplay.previewsAdvanced, SLOT(setEnabled(bool)));

    // We can't use KGlobalSettings::desktopPath() here, since it returns the home dir if
    // the desktop folder doesn't exist.
    QString desktopPath = QDesktopServices::storageLocation(QDesktopServices::DesktopLocation);
    if (desktopPath.isEmpty()) {
        desktopPath = QDir::homePath() + "/Desktop";
    }

    // Don't show the warning label if the desktop folder exists
    if (QFile::exists(desktopPath)) {
        uiLocation.warningSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        uiLocation.warningLabel->setVisible(false);
    }

    KConfigGroup cg = config();
    int filter = cg.readEntry("filter", 0);
    uiFilter.filterType->setCurrentIndex(filter);
    filterChanged(filter);

    QStringList selectedItems = cg.readEntry("mimeFilter", QStringList());

    if (selectedItems.count()) {
        for (int i = 0; i < pMimeModel->rowCount(); i++) {
            const QModelIndex index = pMimeModel->index(i, 0);
            const KMimeType *mime = static_cast<KMimeType*>(pMimeModel->mapToSource(index).internalPointer());
            if (selectedItems.contains(mime->name())) {
                selectedItems.removeAll(mime->name());
                uiFilter.filterFilesList->model()->setData(index, Qt::Checked, Qt::CheckStateRole);
            }
        }
    }
}

void FolderView::configAccepted()
{
    KUrl url;

    if (uiLocation.showDesktopFolder->isChecked()) {
        url = KUrl("desktop:/");
    } else if (uiLocation.showPlace->isChecked()) {
        PlacesFilterModel *filter = static_cast<PlacesFilterModel*>(uiLocation.placesCombo->model());
        KFilePlacesModel *model = static_cast<KFilePlacesModel*>(filter->sourceModel());
        url = model->url(filter->mapToSource(filter->index(uiLocation.placesCombo->currentIndex(), 0)));
    } else {
        url = uiLocation.lineEdit->url();
    }

    if (url.isEmpty() || (url.isLocalFile() && !QFile::exists(url.path()))) {
        url = KUrl(QDir::homePath());
    }

    // Now, we have to iterate over all items (not only the filtered ones). For that reason we have
    // to ask the source model, not the proxy model.
    QStringList selectedItems;
    ProxyMimeModel *proxyModel = static_cast<ProxyMimeModel*>(uiFilter.filterFilesList->model());
    for (int i = 0; i < proxyModel->sourceModel()->rowCount(); i++) {
        const QModelIndex index = proxyModel->sourceModel()->index(i, 0);
        if (index.model()->data(index, Qt::CheckStateRole).toInt() == Qt::Checked) {
            selectedItems << static_cast<KMimeType*>(index.internalPointer())->name();
        }
    }

    int filterType = uiFilter.filterType->currentIndex();
    KConfigGroup cg = config();
    bool needRelayout = false;
    bool needRepaint = false;
    bool needReload = false;

    if (m_drawShadows != uiDisplay.drawShadows->isChecked()) {
        m_drawShadows = uiDisplay.drawShadows->isChecked();
        cg.writeEntry("drawShadows", m_drawShadows);
        needRepaint = true;
    }

    if (m_showPreviews != uiDisplay.showPreviews->isChecked() ||
        m_previewPlugins != m_previewGenerator->enabledPlugins())
    {
        m_showPreviews = uiDisplay.showPreviews->isChecked();
        cg.writeEntry("showPreviews", m_showPreviews);
        cg.writeEntry("previewPlugins", m_previewPlugins);

        m_previewGenerator->setEnabledPlugins(m_previewPlugins);
        m_previewGenerator->setPreviewShown(m_showPreviews);
        needReload = true;
    }

    const QColor color = uiDisplay.colorButton->color();
    if ((m_textColor != Qt::transparent && color != m_textColor) ||
        (m_textColor == Qt::transparent && color != Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor)))
    {
        m_textColor = color;
        cg.writeEntry("textColor", m_textColor);
        needRepaint = true;
    }

    if (m_numTextLines != uiDisplay.numLinesEdit->value()) {
        m_numTextLines = uiDisplay.numLinesEdit->value();
        cg.writeEntry("numTextLines", m_numTextLines);
        needRelayout = true;
    }

    const QList<int> iconSizes = QList<int>() << 16 << 22 << 32 << 48 << 64 << 128;
    int size = iconSizes.at(uiDisplay.sizeSlider->value());
    if ((m_customIconSize == 0 && size != KIconLoader::global()->currentSize(KIconLoader::Desktop)) ||
        (m_customIconSize != 0 && size != m_customIconSize))
    {
        m_customIconSize = size;
        cg.writeEntry("customIconSize", m_customIconSize);
        needRelayout = true;

        // This is to force the preview images to be regenerated with the new size
        if (m_showPreviews) {
            needReload = true;
        }
    }

    int sortColumn = uiDisplay.sortCombo->itemData(uiDisplay.sortCombo->currentIndex()).toInt();
    if (m_sortColumn != sortColumn) {
        if (sortColumn != -1) {
            m_sortColumn = sortColumn;
            m_model->invalidate();
            m_model->sort(m_sortColumn, Qt::AscendingOrder);
            m_layoutBroken = false;
            needRelayout = true;
            cg.writeEntry("sortColumn", m_sortColumn);
        } else {
            m_layoutBroken = true;
            // Note: updateSortActionsState() will set m_sortColumn and write the config entry.
        }
        updateSortActionsState();
    }

    int flow = uiDisplay.flowCombo->itemData(uiDisplay.flowCombo->currentIndex()).toInt();
    if (m_flow != flow) {
        m_flow = static_cast<QListView::Flow>(flow);
        cg.writeEntry("flow", flow);
        needRelayout = true;
    }

    if (m_alignToGrid != uiDisplay.alignToGrid->isChecked()) {
        m_alignToGrid = uiDisplay.alignToGrid->isChecked();
        cg.writeEntry("alignToGrid", m_alignToGrid);
        m_actionCollection.action("grid_align")->setChecked(m_alignToGrid);
        if (m_alignToGrid && m_layoutBroken) {
            alignIconsToGrid();
        }
    }

    if (m_iconsLocked != uiDisplay.lockInPlace->isChecked()) {
        m_iconsLocked = uiDisplay.lockInPlace->isChecked();
        cg.writeEntry("iconsLocked", m_iconsLocked);
        m_actionCollection.action("lock_icons")->setChecked(m_alignToGrid);
    }

    const QString label = uiDisplay.labelEdit->text();
    if ((m_customLabel.isEmpty() && label != m_titleText) ||
        (!m_customLabel.isEmpty() && label != m_customLabel))
    {
        m_customLabel = label;
        setUrl(url);
        cg.writeEntry("customLabel", m_customLabel);
        needRepaint = true;
    }

    if (m_url != url || m_filterFiles != uiFilter.filterFilesPattern->text() ||
        m_filterFilesMimeList != selectedItems || m_filterType != filterType)
    {
        m_initialListing = true;
        m_validRows = 0;
        m_model->setFilterFixedString(uiFilter.filterFilesPattern->text());
        m_filterFiles = uiFilter.filterFilesPattern->text();
        m_filterFilesMimeList = selectedItems;
        m_filterType = filterType;
        setUrl(url);

        cg.writeEntry("url", m_url);
        cg.writeEntry("filterFiles", m_filterFiles);
        cg.writeEntry("filter", m_filterType);
        cg.writeEntry("mimeFilter", m_filterFilesMimeList);

        m_model->setMimeTypeFilterList(m_filterFilesMimeList);
        m_model->setFilterMode(ProxyModel::filterModeFromInt(m_filterType));
        needReload = true;
    }

    if (needReload) {
        m_dirModel->dirLister()->openUrl(m_url);
    }

    if (needRelayout) {
        m_validRows = 0;
        m_delayedLayoutTimer.start(10, this);
    }

    if (needRepaint) {
        markEverythingDirty();
    }

    emit configNeedsSaving();
}

void FolderView::showPreviewConfigDialog()
{
    QWidget *widget = new QWidget;
    uiPreviewConfig.setupUi(widget);

    PreviewPluginsModel *model = new PreviewPluginsModel(this);
    model->setCheckedPlugins(m_previewPlugins);

    uiPreviewConfig.listView->setModel(model);

    KDialog *dialog = new KDialog;
    dialog->setMainWidget(widget);

    if (dialog->exec() == KDialog::Accepted) {
        m_previewPlugins = model->checkedPlugins();
    }

    delete widget;
    delete dialog;
    delete model;
}

void FolderView::fontSettingsChanged()
{
    QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DesktopFont);

    if (m_font != font) {
        m_font = font;
        m_validRows = 0;
        m_delayedLayoutTimer.start(10, this);
    }
}

void FolderView::iconSettingsChanged(int group)
{
    if (group == KIconLoader::Desktop)
    {
        m_validRows = 0;
        m_delayedLayoutTimer.start(10, this);
    }
}

void FolderView::themeChanged()
{
    // We'll mark the layout as invalid here just in case the content margins
    // have changed.
    m_validRows = 0;
    layoutItems();

    updateScrollBarGeometry();
    markEverythingDirty();
}

void FolderView::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    m_regionCache.clear();

    if (!m_positionsLoaded) {
        loadIconPositions();
        m_positionsLoaded = true;
    }

    if (!m_layoutBroken || m_initialListing) {
        if (first < m_validRows) {
            m_validRows = 0;   
        }
        m_delayedLayoutTimer.start(10, this);
    } else {
        const QStyleOptionViewItemV4 option = viewOptions();
        const QRect cr = contentsRect().toRect();
        const QSize grid = gridSize();
        QPoint pos = QPoint();

        m_items.insert(first, last - first + 1, ViewItem());

        // If a single item was inserted and we have a saved position from a deleted file,
        // reuse that position.
        if (first == last && !m_lastDeletedPos.isNull()) {
            const QModelIndex index = m_model->index(first, 0);
            QSize size = m_delegate->sizeHint(option, index).boundedTo(grid);
            size.rwidth() = grid.width();
            m_items[first].rect = QRect(m_lastDeletedPos, size);
            m_items[first].layouted = true;
            markAreaDirty(m_items[first].rect);
            m_lastDeletedPos = QPoint();
            m_validRows = m_items.size();
            return;
        }

        // Lay out the newly inserted files 
        for (int i = first; i <= last; i++) {
            const QModelIndex index = m_model->index(i, 0);
            const QSize size = m_delegate->sizeHint(option, index).boundedTo(grid);
            pos = findNextEmptyPosition(pos, grid, cr);
            m_items[i].rect = QRect(pos.x() + (grid.width() - size.width()) / 2, pos.y(),
                                    size.width(), size.height());
            m_items[first].layouted = true;
            markAreaDirty(m_items[i].rect);
        }

        m_validRows = m_items.size();
        m_delayedSaveTimer.start(5000, this);
        updateScrollBar();
    }
}

void FolderView::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    m_regionCache.clear();

    if (!m_layoutBroken) {
        if (first < m_validRows) {
            m_validRows = 0;
        }
        m_delayedLayoutTimer.start(10, this);
    } else {
        for (int i = first; i <= last; i++) {
            markAreaDirty(m_items[i].rect);
        }
        // When a single item is removed, we'll save the position and use it for the next new item.
        // The reason for this is that when a file is renamed, it will first be removed from the view
        // and then reinserted.
        if (first == last) {
            const QSize size = gridSize();
            m_lastDeletedPos.rx() = m_items[first].rect.x() - (size.width() - m_items[first].rect.width()) / 2;
            m_lastDeletedPos.ry() = m_items[first].rect.y();
        }
        m_items.remove(first, last - first + 1);
        m_delayedSaveTimer.start(5000, this);
        m_validRows = m_items.size();
    }
}

void FolderView::modelReset()
{
    m_validRows = 0;
    layoutItems();
}

void FolderView::layoutChanged()
{
    m_validRows = 0;
    layoutItems();
}

void FolderView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    const QStyleOptionViewItemV4 option = viewOptions();
    const QSize grid = gridSize();
    m_regionCache.clear();

    // Update the size of the items and center them in the grid cell
    for (int i = topLeft.row(); i <= bottomRight.row(); i++) {
        if (!m_items[i].layouted) {
            continue;
        }
        const QModelIndex index = m_model->index(i, 0);
        QSize size = m_delegate->sizeHint(option, index).boundedTo(grid);
        size.rwidth() = grid.width();
        QRect dirty = m_items[i].rect;
        if (size != m_items[i].rect.size()) {
            m_items[i].rect.setHeight(size.height());
            dirty |= m_items[i].rect;
        }
        markAreaDirty(dirty);
    }
}

void FolderView::clipboardDataChanged()
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (KonqMimeData::decodeIsCutSelection(mimeData)) {
        KUrl::List urls = KUrl::List::fromMimeData(mimeData);

        // TODO Mark the cut icons as cut
    }

    // Update the paste action
    if (QAction *action = m_actionCollection.action("paste"))
    {
        const QString actionText = KIO::pasteActionText();
        if (!actionText.isEmpty()) {
            action->setText(actionText);
            action->setEnabled(true);
        } else {
            action->setText(i18n("&Paste"));
            action->setEnabled(false);
        }
    }
}

void FolderView::scrollBarValueChanged(int value)
{
    Q_UNUSED(value)

    // TODO We should save the scrollbar value in the config file
    //      so we can restore it with the session.

    m_viewScrolled = true;
    update();
}

int FolderView::columnsForWidth(qreal width) const
{
    int spacing = 10;
    int margin = 10;

    qreal available = width - 2 * margin;
    return qFloor(available / (gridSize().width() + spacing));
}

int FolderView::rowsForHeight(qreal height) const
{
    int spacing = 10;
    int margin = 10;

    qreal available = height - 2 * margin;
    return qFloor(available / (gridSize().height() + spacing));
}

QPointF FolderView::mapToViewport(const QPointF &point) const
{
    return point + QPointF(0, m_scrollBar->value());
}

QRectF FolderView::mapToViewport(const QRectF &rect) const
{
    return rect.translated(0, m_scrollBar->value());
}

QPointF FolderView::mapFromViewport(const QPointF &point) const
{
    return point - QPointF(0, m_scrollBar->value());
}

QRectF FolderView::mapFromViewport(const QRectF &rect) const
{
    return rect.translated(0, -m_scrollBar->value());
}

QPoint inline FolderView::nextGridPosition(const QPoint &lastPos, const QSize &grid, const QRect &contentRect) const
{
    int spacing = 10;
    int margin = 10;

    if (lastPos.isNull()) {
        return QPoint(contentRect.left() + margin, contentRect.top() + m_titleHeight + margin);
    }

    QPoint pos = lastPos;

    if (m_flow == QListView::LeftToRight) {
        pos.rx() += grid.width() + spacing;
        if ((pos.x() + grid.width() + 10) >= (contentRect.right() - m_scrollBar->geometry().width() - margin)) {
            pos.ry() += grid.height() + spacing;
            pos.rx() = contentRect.left() + margin;
        }
    } else {
        pos.ry() += grid.height() + spacing;
        if ((pos.y() + grid.height() + 10) >= (contentRect.bottom() - margin)) {
            pos.rx() += grid.width() + spacing;
            pos.ry() = contentRect.top() + m_titleHeight + margin;
        }
    }

    return pos;
}

QPoint FolderView::findNextEmptyPosition(const QPoint &prevPos, const QSize &gridSize, const QRect &contentRect) const
{
    QPoint pos = prevPos;
    bool done = false;

    while (!done)
    {
        done = true;
        pos = nextGridPosition(pos, gridSize, contentRect);
        const QRect r(pos, gridSize);
        for (int i = 0; i < m_items.count(); i++) {
            if (m_items.at(i).rect.intersects(r)) {
                done = false;
                break;
            }
        }
    }

    return pos;
}

void FolderView::layoutItems()
{
    QStyleOptionViewItemV4 option = viewOptions();
    m_items.resize(m_model->rowCount());
    m_regionCache.clear();

    const QRect visibleRect = mapToViewport(contentsRect()).toAlignedRect();
    const QRect rect = contentsRect().toRect();
    const QSize grid = gridSize();
    int maxWidth = rect.width() - m_scrollBar->geometry().width();
    int maxHeight = rect.height() - m_titleHeight;
    m_columns = columnsForWidth(maxWidth);
    m_rows = rowsForHeight(maxHeight);
    bool needUpdate = false;

    m_delegate->setMaximumSize(grid);

    // If we're starting with the first item
    if (m_validRows == 0) {
        m_needPostLayoutPass = false;
        m_currentLayoutPos = QPoint();
    }

    if (!m_savedPositions.isEmpty()) {
        m_layoutBroken = true;
        // Restart the delayed cache clear timer if it's running and we haven't
        // finished laying out the icons.
        if (m_delayedCacheClearTimer.isActive() && m_validRows < m_items.size()) {
             m_delayedCacheClearTimer.start(5000, this);
        }
    } else {
        m_layoutBroken = false;
    }

    // Do a 20 millisecond layout pass
    QTime time;
    time.start();
    do {
        const int count = qMin(m_validRows + 50, m_items.size());
        if (!m_savedPositions.isEmpty()) {

            // Layout with saved icon positions
            // ================================================================
            for (int i = m_validRows; i < count; i++) {
                const QModelIndex index = m_model->index(i, 0);
                KFileItem item = m_model->itemForIndex(index);
                QSize size = m_delegate->sizeHint(option, index).boundedTo(grid);
                size.rwidth() = grid.width();

                const QPoint pos = m_savedPositions.value(item.name(), QPoint(-1, -1));
                if (pos != QPoint(-1, -1)) {
                    m_items[i].rect = QRect(pos, size);
                    m_items[i].layouted = true;
                    if (m_items[i].rect.intersects(visibleRect)) {
                        needUpdate = true;
                    }
                } else {
                    // We don't have a saved position for this file, so we'll record the
                    // size and lay it out in a second layout pass.
                    m_items[i].rect = QRect(QPoint(), size);
                    m_items[i].layouted = false;
                    m_needPostLayoutPass = true;
                }
            }
            // If we've finished laying out all the icons
            if (!m_initialListing && !m_needPostLayoutPass && count == m_items.size()) {
                needUpdate |= doLayoutSanityCheck();
            }
        } else {

            // Automatic layout
            // ================================================================
            QPoint pos = m_currentLayoutPos;
            for (int i = m_validRows; i < count; i++) {
                const QModelIndex index = m_model->index(i, 0);
                QSize size = m_delegate->sizeHint(option, index).boundedTo(grid);
                size.rwidth() = grid.width();

                pos = nextGridPosition(pos, grid, rect);
                m_items[i].rect = QRect(pos, size);
                m_items[i].layouted = true;
                if (m_items[i].rect.intersects(visibleRect)) {
                    needUpdate = true;
                }
            }
            m_currentLayoutPos = pos;
        }
        m_validRows = count;
    } while (m_validRows < m_items.size() && time.elapsed() < 30);


    // Second layout pass for files that didn't have a saved position
    // ====================================================================
    if (m_validRows == m_items.size() && m_needPostLayoutPass) {
        QPoint pos = QPoint();
        for (int i = 0; i < m_items.size(); i++) {
            if (m_items[i].layouted) {
                continue;
            }
            pos = findNextEmptyPosition(pos, grid, rect);
            m_items[i].rect.moveTo(pos.x() + (grid.width() - m_items[i].rect.width()) / 2, pos.y());
            if (m_items[i].rect.intersects(visibleRect)) {
                needUpdate = true;
            }
        }
        needUpdate |= doLayoutSanityCheck();
        m_needPostLayoutPass = false;
        return;
    }

    if (m_validRows < m_items.size() || m_needPostLayoutPass) {
        m_delayedLayoutTimer.start(10, this);
    }

    if (needUpdate) {
        m_dirtyRegion = QRegion(visibleRect);
        update();
    }

    updateScrollBar();
}

void FolderView::alignIconsToGrid()
{
    int margin = 10;
    int spacing = 10;
    const QRect cr = contentsRect().toRect();
    const QSize size = gridSize() + QSize(spacing, spacing);
    int topMargin = margin + cr.top() + m_titleHeight;
    int leftMargin = margin + cr.left();
    int vOffset = topMargin + size.height() / 2;
    int hOffset = leftMargin + size.width() / 2;
    bool layoutChanged = false;

    for (int i = 0; i < m_items.size(); i++) {
        const QPoint center = m_items[i].rect.center();
        const int col = qRound((center.x() - hOffset) / qreal(size.width()));
        const int row = qRound((center.y() - vOffset) / qreal(size.height()));

        const QPoint pos(leftMargin + col * size.width() + (size.width() - m_items[i].rect.width() - spacing) / 2,
                         topMargin + row * size.height());

        if (pos != m_items[i].rect.topLeft()) {
            m_items[i].rect.moveTo(pos);
            layoutChanged = true;
        }
    }

    if (layoutChanged) {
        doLayoutSanityCheck();
        updateScrollBar();
        markEverythingDirty();
        m_layoutBroken = true;
        updateSortActionsState();
        m_delayedSaveTimer.start(5000, this);
        m_savedPositions.clear();
    }
}

bool FolderView::doLayoutSanityCheck()
{
    // Make sure that the distance from the top of the viewport to the
    // topmost item is 10 pixels.
    int minY = INT_MAX;
    for (int i = 0; i < m_validRows; i++) {
        if (!m_items[i].layouted) {
            continue;
        }
        minY = qMin(minY, m_items[i].rect.y());
    }

    int topMargin = contentsRect().top() + 10 + m_titleHeight;
    if (minY != topMargin) {
        int delta = topMargin - minY;
        for (int i = 0; i < m_validRows; i++) {
            if (!m_items[i].layouted) {
                continue;
            }
            m_items[i].rect.translate(0, delta);
        }
        return true;
    }

    return false;
}

void FolderView::saveIconPositions() const
{
    if (m_layoutBroken && !m_initialListing && m_validRows == m_items.size()) {
        int version = 1;
        QStringList data;
        data << QString::number(version);
        data << QString::number(m_items.size());

        const QPoint offset = (contentsRect().topLeft() + QPointF(0, m_titleHeight)).toPoint();
        const QSize size = gridSize();
        for (int i = 0; i < m_items.size(); i++) {
            QModelIndex index = m_model->index(i, 0);
            KFileItem item = m_model->itemForIndex(index);
            data << item.name();
            data << QString::number(m_items[i].rect.x() - offset.x());
            data << QString::number(m_items[i].rect.y() - offset.y());
        }

        config().writeEntry("savedPositions", data);
    } else {
        config().deleteEntry("savedPositions");
    }
}

void FolderView::loadIconPositions()
{
    QStringList data = config().readEntry("savedPositions", QStringList());
    if (data.isEmpty()) {
        return;
    }

    // Sanity checks
    if (data.size() < 5 || data.at(0).toInt() != 1 || ((data.size() - 2) % 3) ||
        data.at(1).toInt() != ((data.size() - 2) / 3)) {
        return;
    }

    const QPoint offset = (contentsRect().topLeft() + QPointF(0, m_titleHeight)).toPoint();
    for (int i = 2; i < data.size(); i += 3) {
        const QString &name = data.at(i);
        int x = data.at(i + 1).toInt();
        int y = data.at(i + 2).toInt();
        m_savedPositions.insert(name, QPoint(x, y) + offset);
    }
}

void FolderView::updateScrollBar()
{
    // Find the height of the viewport
    int maxY = 0;
    for (int i = 0; i < m_items.size(); i++) {
        maxY = qMax(maxY, m_items[i].rect.bottom());
    }

    m_viewportRect = contentsRect();
    m_viewportRect.setBottom(qMax<int>(m_viewportRect.bottom(), maxY + 10));
    m_viewportRect.setWidth(m_viewportRect.width() - m_scrollBar->geometry().width());

    int max = int(m_viewportRect.height() - contentsRect().height());

    // Keep the scrollbar handle at the bottom if it was at the bottom and the viewport
    // has grown vertically
    bool updateValue = (m_scrollBar->minimum() != m_scrollBar->maximum()) &&
            (max > m_scrollBar->maximum()) && (m_scrollBar->value() == m_scrollBar->maximum());

    m_scrollBar->setRange(0, max);   
    m_scrollBar->setPageStep(contentsRect().height());
    m_scrollBar->setSingleStep(10);

    if (updateValue) {
        m_scrollBar->setValue(max);
    }

    if (max > 0) {
        m_scrollBar->show();
    } else {
        m_scrollBar->hide();
    }
}

// Marks the supplied rect, in viewport coordinates, as dirty and schedules a repaint.
void FolderView::markAreaDirty(const QRect &rect)
{
    if (rect.isEmpty()) {
        return;
    }

    const QRect visibleRect = mapToViewport(contentsRect()).toAlignedRect();
    if (!rect.intersects(visibleRect)) {
        return;
    }

    m_dirtyRegion += rect;
    update(mapFromViewport(rect));
}

void FolderView::markEverythingDirty()
{
    m_dirtyRegion = QRegion(mapToViewport(contentsRect()).toAlignedRect());
    update();
}

// This function scrolls the contents of the backbuffer the distance the scrollbar
// has moved since the last time this function was called.
QRect FolderView::scrollBackbufferContents()
{
    int value =  m_scrollBar->value();
    int delta = m_lastScrollValue - value;
    m_lastScrollValue = value;

    if (qAbs(delta) >= m_pixmap.height()) {
        return mapToViewport(contentsRect()).toAlignedRect();
    }

    int sy, dy, h;
    QRect dirty;
    if (delta < 0) {
        dy = 0;
        sy = -delta;
        h = m_pixmap.height() - sy;
        dirty = QRect(0, m_pixmap.height() - sy, m_pixmap.width(), sy);
    } else {
        dy = delta;
        sy = 0;
        h = m_pixmap.height() - dy;
        dirty = QRect(0, 0, m_pixmap.width(), dy);
    }
#ifdef Q_WS_X11
    // Avoid the overhead of creating a QPainter to do the blit.
    Display *dpy = QX11Info::display();
    GC gc = XCreateGC(dpy, m_pixmap.handle(), 0, 0);
    XCopyArea(dpy, m_pixmap.handle(), m_pixmap.handle(), gc, 0, sy, m_pixmap.width(), h, 0, dy);
    XFreeGC(dpy, gc);
#else
    m_pixmap = m_pixmap.copy(0, sy, m_pixmap.width(), h);
#endif
    return mapToViewport(dirty.translated(contentsRect().topLeft().toPoint())).toAlignedRect();
}

void FolderView::updateTextShadows(const QColor &textColor)
{
    if (!m_drawShadows) {
        m_delegate->setShadowColor(Qt::transparent);
        return;
    }

    QColor shadowColor;

    // Use black shadows with bright text, and white shadows with dark text.
    if (qGray(textColor.rgb()) > 192) {
        shadowColor = Qt::black;
    } else {
        shadowColor = Qt::white;
    }

    if (m_delegate->shadowColor() != shadowColor)
    {
        m_delegate->setShadowColor(shadowColor);

        // Center white shadows to create a halo effect, and offset dark shadows slightly.
        if (shadowColor == Qt::white) {
            m_delegate->setShadowOffset(QPoint(0, 0));
        } else {
            m_delegate->setShadowOffset(QPoint(layoutDirection() == Qt::RightToLeft ? -1 : 1, 1));
        }
    }
}

void FolderView::paintErrorMessage(QPainter *painter, const QRect &rect, const QString &message) const
{
    const QColor themeTextColor = Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
    const QColor textColor = (m_textColor.alpha() > 0 ? m_textColor : themeTextColor);

    QIcon icon = KIconLoader::global()->loadIcon("dialog-error", KIconLoader::NoGroup, KIconLoader::SizeHuge,
                                                 KIconLoader::DefaultState, QStringList(), 0, true);
    const QSize iconSize = icon.isNull() ? QSize() :
                               icon.actualSize(QSize(KIconLoader::SizeHuge, KIconLoader::SizeHuge));
    const int flags = Qt::AlignCenter | Qt::TextWordWrap;
    const int blur = qCeil(m_delegate->shadowBlur());

    QFontMetrics fm = painter->fontMetrics();
    QRect r = fm.boundingRect(rect.adjusted(0, 0, -iconSize.width() - 4, 0), flags, message);
    QPixmap pm(r.size());
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setFont(painter->font());
    p.setPen(textColor);
    p.drawText(pm.rect(), flags, message);
    p.end();

    QImage shadow;
    if (m_delegate->shadowColor().alpha() > 0) {
        shadow = QImage(pm.size() + QSize(blur * 2, blur * 2), QImage::Format_ARGB32_Premultiplied);
        p.begin(&shadow);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(shadow.rect(), Qt::transparent);
        p.drawPixmap(blur, blur, pm);
        p.end();

        Plasma::PaintUtils::shadowBlur(shadow, blur, m_delegate->shadowColor());
    }

    const QSize size(pm.width() + iconSize.width() + 4, qMax(iconSize.height(), pm.height()));
    const QPoint iconPos = rect.topLeft() + QPoint((rect.width() - size.width()) / 2,
                                                   (rect.height() - size.height()) / 2);
    const QPoint textPos = iconPos + QPoint(iconSize.width() + 4, (iconSize.height() - pm.height()) / 2);

    if (!icon.isNull()) {
        icon.paint(painter, QRect(iconPos, iconSize));
    }

    if (!shadow.isNull()) {
        painter->drawImage(textPos - QPoint(blur, blur) + m_delegate->shadowOffset().toPoint(), shadow);
    }
    painter->drawPixmap(textPos, pm);
}

void FolderView::paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect &contentRect)
{
    // Make sure the backbuffer pixmap has the same size as the content rect
    if (m_pixmap.isNull() || m_pixmap.size() != contentRect.size()) {
        if (!contentRect.isValid()) {
            return;
        }
        m_pixmap = QPixmap(contentRect.size());
        m_pixmap.fill(Qt::transparent);
        m_dirtyRegion = QRegion(mapToViewport(contentRect).toAlignedRect());
    }

    QRect clipRect = contentRect & option->exposedRect.toAlignedRect();
    if (clipRect.isEmpty()) {
        return;
    }

    const QColor themeTextColor = Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
    const QColor textColor = (m_textColor.alpha() > 0 ? m_textColor : themeTextColor);

    painter->setClipRect(clipRect);

    // Paint the folder title
    m_titleHeight = isContainment() ? 0 : painter->fontMetrics().height() + 4;
    if (m_titleHeight > 0 && option->exposedRect.y() <= m_titleHeight) {
        QString titleText = m_titleText;
        titleText = painter->fontMetrics().elidedText(titleText, Qt::ElideMiddle, contentRect.width());
        QColor titleColor = textColor;
        QPixmap titlePixmap = Plasma::PaintUtils::shadowText(titleText,
                                                             titleColor,
                                                             m_delegate->shadowColor(),
                                                             m_delegate->shadowOffset().toPoint());
        painter->drawPixmap(contentRect.topLeft(), titlePixmap);

        //Draw underline
        int width = contentRect.width() - (m_scrollBar->isVisible() ? m_scrollBar->geometry().width() + 4 : 0);
        if (m_divider.width() != width) {
            qreal fw = 1.0 / width * 20.0;
            m_divider = QPixmap(width, 2);
            m_divider.fill(Qt::transparent);
            QLinearGradient g(0, 0, width, 0);
            g.setColorAt(0, Qt::transparent);
            g.setColorAt(fw, Qt::black);
            g.setColorAt(1 - fw, Qt::black);
            g.setColorAt(1, Qt::transparent);
            QPainter p(&m_divider);
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.fillRect(0, 0, width, 1, QColor(0, 0, 0, 64));
            p.fillRect(0, 1, width, 1, QColor(255, 255, 255, 64));
            p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            p.fillRect(m_divider.rect(), g);
        }
        painter->drawPixmap(contentRect.left(), contentRect.top() + m_titleHeight - 2, m_divider);
    }

    if (m_viewScrolled) {
        m_dirtyRegion += scrollBackbufferContents();
        m_viewScrolled = false;
    }

    int offset = m_scrollBar->value();

    // Update the dirty region in the backbuffer
    // =========================================
    if (!m_dirtyRegion.isEmpty()) {
        QStyleOptionViewItemV4 opt = viewOptions();
        opt.palette.setColor(QPalette::All, QPalette::Text, textColor);
        updateTextShadows(textColor);

        QPainter p(&m_pixmap);
        p.translate(-contentRect.topLeft() - QPoint(0, offset));
        p.setClipRegion(m_dirtyRegion);

        // Clear the dirty region
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(mapToViewport(contentRect).toAlignedRect(), Qt::transparent);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);

        for (int i = 0; i < m_validRows; i++) {
            opt.rect = m_items[i].rect;

            if (!m_items[i].layouted || !m_dirtyRegion.intersects(opt.rect)) {
                continue;
            }

            const QModelIndex index = m_model->index(i, 0);
            opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver | QStyle::State_Selected);

            if (index == m_hoveredIndex) {
                opt.state |= QStyle::State_MouseOver;
            }

            if (m_selectionModel->isSelected(index)) {
                if (m_dragInProgress) {
                    continue;
                }
                updateTextShadows(palette().color(QPalette::HighlightedText));
                opt.state |= QStyle::State_Selected;
            } else {
                updateTextShadows(textColor);
            }

            if (hasFocus() && index == m_selectionModel->currentIndex()) {
                opt.state |= QStyle::State_HasFocus;
            }

            m_delegate->paint(&p, opt, index);
        }

        if (m_rubberBand.isValid())
        {
            QStyleOptionRubberBand opt;
            initStyleOption(&opt);
            opt.rect   = m_rubberBand;
            opt.shape  = QRubberBand::Rectangle;
            opt.opaque = false;

            style()->drawControl(QStyle::CE_RubberBand, &opt, &p);
        }

        m_dirtyRegion = QRegion();
    }

    const QRect topFadeRect(contentRect.x(), contentRect.y() + m_titleHeight, contentRect.width(), 16);
    const QRect bottomFadeRect(contentRect.bottomLeft() - QPoint(0, 16), QSize(contentRect.width(), 16));
    QRect titleRect = contentRect;
    titleRect.setHeight(m_titleHeight);

    // Draw the backbuffer on the Applet
    // =================================
    if ((m_titleHeight > 0 && titleRect.intersects(clipRect)) ||
        (offset > 0 && topFadeRect.intersects(clipRect)) ||
        (m_viewportRect.height() > (offset + contentRect.height()) && bottomFadeRect.intersects(clipRect)))
    {
        QPixmap pixmap = m_pixmap;
        QPainter p(&pixmap);

        // Clear the area under the title
        if (m_titleHeight > 0 && titleRect.intersects(clipRect))
        {
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.fillRect(0, 0, pixmap.width(), m_titleHeight, Qt::transparent);
        }
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);

        // Fade out the top section of the pixmap if the scrollbar slider isn't at the top
        if (offset > 0 && topFadeRect.intersects(clipRect))
        {
            if (m_topFadeTile.isNull())
            {
                m_topFadeTile = QPixmap(256, 16);
                m_topFadeTile.fill(Qt::transparent);
                QLinearGradient g(0, 0, 0, 16);
                g.setColorAt(0, Qt::transparent);
                g.setColorAt(1, Qt::black);
                QPainter p(&m_topFadeTile);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.fillRect(0, 0, 256, 16, g);
                p.end();
            }
            p.drawTiledPixmap(0, m_titleHeight, m_pixmap.width(), 16, m_topFadeTile);
        }

        // Fade out the bottom part of the pixmap if the scrollbar slider isn't at the bottom
        if (m_viewportRect.height() > (offset + contentRect.height()) && bottomFadeRect.intersects(clipRect))
        {
            if (m_topFadeTile.isNull())
            {
                m_bottomFadeTile = QPixmap(256, 16);
                m_bottomFadeTile.fill(Qt::transparent);
                QLinearGradient g(0, 0, 0, 16);
                g.setColorAt(0, Qt::black);
                g.setColorAt(1, Qt::transparent);
                QPainter p(&m_bottomFadeTile);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.fillRect(0, 0, 256, 16, g);
                p.end();
            }
            p.drawTiledPixmap(0, m_pixmap.height() - 16, m_pixmap.width(), 16, m_bottomFadeTile);
        }
        p.end();

        painter->drawPixmap(contentRect.topLeft(), pixmap);
    }
    else
    {
        painter->drawPixmap(contentRect.topLeft(), m_pixmap);
    }

    if (!m_errorMessage.isEmpty()) {
        paintErrorMessage(painter, contentRect, m_errorMessage);
    }

    // Show the spinning animation
    if (m_validRows < m_items.size() || m_initialListing) {
        if (!m_animTimer.isActive()) {
            m_animFrame = 0;
            m_animTimer.start(150, this);
        }
        if (m_animFrames.isNull()) {
            createAnimationFrames();
        }
        const QSize size(m_animFrames.width(), m_animFrames.width());
        QPoint pos = contentRect.center() - QPoint(size.width() / 2, size.width() / 2);
        painter->drawPixmap(pos, m_animFrames, QRect(QPoint(0, size.height() * m_animFrame), size));
    } else if (m_animTimer.isActive()) {
        m_animTimer.stop();
        update();
    }
}

bool FolderView::indexIntersectsRect(const QModelIndex &index, const QRect &rect) const
{
    QRect r = m_items[index.row()].rect;
    if (!r.intersects(rect)) {
        return false;
    }

    // If the item is fully contained in the rect
    if (r.left() > rect.left() && r.right() < rect.right() &&
        r.top() > rect.top() && r.bottom() < rect.bottom())
    {
        return true;
    }

    // If the item is partially inside the rect
    return visualRegion(index).intersects(rect);
}

QModelIndex FolderView::indexAt(const QPointF &point) const
{
    if (!mapToViewport(contentsRect()).contains(point))
        return QModelIndex();

    const QPoint pt = point.toPoint();

    // If we have a hovered index, check it before walking the list
    if (m_hoveredIndex.isValid()) {
        if (m_items[m_hoveredIndex.row()].rect.contains(pt) &&
            visualRegion(m_hoveredIndex).contains(pt))
        {
            return m_hoveredIndex;
        }
    }

    for (int i = 0; i < m_validRows; i++) {
        if (!m_items[i].layouted || !m_items[i].rect.contains(pt)) {
            continue;
        }

        const QModelIndex index = m_model->index(i, 0);
        if (visualRegion(index).contains(pt)) {
            return index;
        }
        break;
    }

    return QModelIndex();
}

QRect FolderView::visualRect(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_validRows ||
        !m_items[index.row()].layouted) {
        return QRect();
    }

    return m_items[index.row()].rect;
}

QRegion FolderView::visualRegion(const QModelIndex &index) const
{
    QStyleOptionViewItemV4 option = viewOptions();
    option.rect = m_items[index.row()].rect;
    if (m_selectionModel->isSelected(index)) {
        option.state |= QStyle::State_Selected;
    }
    if (index == m_hoveredIndex) {
        option.state |= QStyle::State_MouseOver;
    }

    quint64 key = quint64(option.state) << 32 | index.row();
    if (QRegion *region = m_regionCache.object(key)) {
        return *region;
    }

    QRegion region;
    // Make this a virtual function in KDE 5
    QMetaObject::invokeMethod(m_delegate, "shape", Q_RETURN_ARG(QRegion, region),
                              Q_ARG(QStyleOptionViewItem, option),
                              Q_ARG(QModelIndex, index));

    m_regionCache.insert(key, new QRegion(region));
    return region;
}

void FolderView::constraintsEvent(Plasma::Constraints constraints)
{
    // We should probably only do this when acting as the desktop containment
    if (constraints & Plasma::FormFactorConstraint) {
        if (isContainment()) {
            setBackgroundHints(Applet::NoBackground);
        } else if (formFactor() == Plasma::Planar || formFactor() == Plasma::MediaCenter) {
            setBackgroundHints(Applet::TranslucentBackground);
        }
    }

    if (constraints & Plasma::SizeConstraint)
    {
        updateScrollBarGeometry();
        int maxWidth  = contentsRect().width() - m_scrollBar->geometry().width();
        int maxHeight = contentsRect().height() - m_titleHeight;
        if ((m_flow == QListView::LeftToRight && columnsForWidth(maxWidth) != m_columns) ||
            (m_flow == QListView::TopToBottom && rowsForHeight(maxHeight) != m_rows)) {
            // The scrollbar range will be updated after the re-layout
            m_validRows = 0;
            m_delayedLayoutTimer.start(10, this);
        } else {
            updateScrollBar();
            markEverythingDirty();
        }
    }
}

void FolderView::updateScrollBarGeometry()
{
    QRectF cr = contentsRect();

    QRectF r = QRectF(cr.right() - m_scrollBar->geometry().width(), cr.top() + m_titleHeight,
                      m_scrollBar->geometry().width(), cr.height() - m_titleHeight);
    if (m_scrollBar->geometry() != r) {
        m_scrollBar->setGeometry(r);
    }
}

void FolderView::setUrl(const KUrl &url)
{
    m_url = url;

    if (!m_customLabel.isEmpty()) {
        m_titleText = m_customLabel;
    } else if (m_url == KUrl("desktop:/")) {
        m_titleText = i18n("Desktop Folder");
    } else {
        m_titleText = m_url.pathOrUrl();

        KFilePlacesModel places;
        const QModelIndex index = places.closestItem(url);
        if (index.isValid()) {
            m_titleText = m_titleText.right(m_titleText.length() - places.url(index).pathOrUrl().length());

            if (!m_titleText.isEmpty()) {
                if (m_titleText.at(0) == '/') {
                    m_titleText.remove(0, 1);
                }

                if (layoutDirection() == Qt::RightToLeft) {
                    m_titleText.prepend(" < ");
                } else {
                    m_titleText.prepend(" > ");
                }
            }

            m_titleText.prepend(places.text(index));
        }
    }
}

void FolderView::createActions()
{
    KIO::FileUndoManager *manager = KIO::FileUndoManager::self();

    // Remove the Shift+Delete shortcut from the cut action, since it's used for deleting files
    KAction *cut = KStandardAction::cut(this, SLOT(cut()), this);
    KShortcut cutShortCut = cut->shortcut();
    cutShortCut.remove(Qt::SHIFT + Qt::Key_Delete);
    cut->setShortcut(cutShortCut);

    KAction *copy = KStandardAction::copy(this, SLOT(copy()), this);

    KAction *undo = KStandardAction::undo(manager, SLOT(undo()), this);
    connect(manager, SIGNAL(undoAvailable(bool)), undo, SLOT(setEnabled(bool)));
    connect(manager, SIGNAL(undoTextChanged(QString)), SLOT(undoTextChanged(QString)));
    undo->setEnabled(manager->undoAvailable());

    KAction *paste = KStandardAction::paste(this, SLOT(paste()), this);
    KAction *pasteTo = KStandardAction::paste(this, SLOT(pasteTo()), this);
    pasteTo->setEnabled(false); // Only enabled during popupMenu()

    QString actionText = KIO::pasteActionText();
    if (!actionText.isEmpty()) {
       paste->setText(actionText);
    } else {
       paste->setEnabled(false);
    }

    KAction *reload = new KAction(i18n("&Reload"), this);
    connect(reload, SIGNAL(triggered()), SLOT(refreshIcons()));

    KAction *refresh = new KAction(isContainment() ? i18n("&Refresh Desktop") : i18n("&Refresh View"), this);
    refresh->setShortcut(KStandardShortcut::reload());
    if (isContainment()) {
        refresh->setIcon(KIcon("user-desktop"));
    }
    connect(refresh, SIGNAL(triggered()), SLOT(refreshIcons()));

    KAction *rename = new KAction(KIcon("edit-rename"), i18n("&Rename"), this);
    rename->setShortcut(Qt::Key_F2);
    connect(rename, SIGNAL(triggered()), SLOT(renameSelectedIcon()));

    KAction *trash = new KAction(KIcon("user-trash"), i18n("&Move to Trash"), this);
    trash->setShortcut(Qt::Key_Delete);
    connect(trash, SIGNAL(triggered(Qt::MouseButtons, Qt::KeyboardModifiers)),
            SLOT(moveToTrash(Qt::MouseButtons, Qt::KeyboardModifiers)));

    KAction *del = new KAction(i18n("&Delete"), this);
    del->setIcon(KIcon("edit-delete"));
    del->setShortcut(Qt::SHIFT + Qt::Key_Delete);
    connect(del, SIGNAL(triggered()), SLOT(deleteSelectedIcons()));

    m_actionCollection.addAction("cut", cut);
    m_actionCollection.addAction("undo", undo);
    m_actionCollection.addAction("copy", copy);
    m_actionCollection.addAction("paste", paste);
    m_actionCollection.addAction("pasteto", pasteTo);
    m_actionCollection.addAction("reload", reload);
    m_actionCollection.addAction("refresh", refresh);
    m_actionCollection.addAction("rename", rename);
    m_actionCollection.addAction("trash", trash);
    m_actionCollection.addAction("del", del);

    if (KAuthorized::authorize("editable_desktop_icons")) {
        KAction *alignToGrid = new KAction(i18n("Align to Grid"), this);
        alignToGrid->setCheckable(true);
        alignToGrid->setChecked(m_alignToGrid);
        connect(alignToGrid, SIGNAL(toggled(bool)), SLOT(toggleAlignToGrid(bool)));

        KAction *lockIcons = new KAction(i18nc("Icons on the desktop", "Lock in Place"), this);
        lockIcons->setCheckable(true);
        lockIcons->setChecked(m_iconsLocked);
        connect(lockIcons, SIGNAL(toggled(bool)), SLOT(toggleIconsLocked(bool)));

        m_sortingGroup = new QActionGroup(this);
        connect(m_sortingGroup, SIGNAL(triggered(QAction*)), SLOT(sortingChanged(QAction*)));
        QAction *sortByName = m_sortingGroup->addAction(i18nc("Sort icons", "By Name"));
        QAction *sortBySize = m_sortingGroup->addAction(i18nc("Sort icons", "By Size"));
        QAction *sortByType = m_sortingGroup->addAction(i18nc("Sort icons", "By Type"));
        QAction *sortByDate = m_sortingGroup->addAction(i18nc("Sort icons", "By Date"));
        sortByName->setCheckable(true);
        sortBySize->setCheckable(true);
        sortByType->setCheckable(true);
        sortByDate->setCheckable(true);

        KAction *dirsFirst = new KAction(i18nc("Sort icons", "Directories First"), this);
        dirsFirst->setCheckable(true);
        dirsFirst->setChecked(m_sortDirsFirst);
        connect(dirsFirst, SIGNAL(toggled(bool)), SLOT(toggleDirectoriesFirst(bool)));

        QMenu *sortMenu = new QMenu(i18n("Sort Icons"));
        sortMenu->addAction(sortByName);
        sortMenu->addAction(sortBySize);
        sortMenu->addAction(sortByType);
        sortMenu->addAction(sortByDate);
        sortMenu->addSeparator();
        sortMenu->addAction(dirsFirst);

        QMenu *iconsMenu = new QMenu;
        iconsMenu->addMenu(sortMenu);
        iconsMenu->addSeparator();
        iconsMenu->addAction(alignToGrid);
        iconsMenu->addAction(lockIcons);

        QAction *iconsMenuAction = new KAction(i18n("Icons"), this);
        iconsMenuAction->setIcon(KIcon("preferences-desktop-icons"));
        iconsMenuAction->setMenu(iconsMenu);

        // Create the new menu
        m_newMenu = new KNewMenu(&m_actionCollection, view(), "new_menu");
        connect(m_newMenu->menu(), SIGNAL(aboutToShow()), this, SLOT(aboutToShowCreateNew()));

        m_actionCollection.addAction("lock_icons", lockIcons);
        m_actionCollection.addAction("auto_align", alignToGrid);
        m_actionCollection.addAction("icons_menu", iconsMenuAction);
        m_actionCollection.addAction("sort_name", sortByName);
        m_actionCollection.addAction("sort_size", sortBySize);
        m_actionCollection.addAction("sort_type", sortByType);
        m_actionCollection.addAction("sort_date", sortByDate);
        updateSortActionsState();
    }

    // Note: We have to create our own action collection, because the one Plasma::Applet
    //       provides can only be manipulated indirectly, and we need to be able to pass
    //       a pointer to the collection to KNewMenu and KonqPopupMenu.
    //       But we still have to add all the actions to the collection in Plasma::Applet
    //       in order for the shortcuts to work.
    addAction("cut", cut);
    addAction("undo", undo);
    addAction("copy", copy);
    addAction("paste", paste);
    addAction("reload", reload);
    addAction("rename", rename);
    addAction("trash", trash);
    addAction("del", del);
}

QList<QAction*> FolderView::contextualActions()
{
    QList<QAction*> actions;

    if (KAuthorized::authorize("action/kdesktop_rmb"))
    {
        if (QAction *action = m_actionCollection.action("new_menu")) {
            actions.append(action);
            QAction *separator = new QAction(this);
            separator->setSeparator(true);
            actions.append(separator);
        }

        actions.append(m_actionCollection.action("undo"));
        actions.append(m_actionCollection.action("paste"));
 
        QAction *separator = new QAction(this);
        separator->setSeparator(true);
        actions.append(separator);

        if (QAction *iconsMenu = m_actionCollection.action("icons_menu")) {
            actions.append(iconsMenu);
        }

        actions.append(m_actionCollection.action("refresh"));

        separator = new QAction(this);
        separator->setSeparator(true);
        actions.append(separator);
    }

    return actions;
}

void FolderView::aboutToShowCreateNew()
{
    if (m_newMenu) {
        m_newMenu->slotCheckUpToDate();
        m_newMenu->setPopupFiles(m_url);
    }
}

KUrl::List FolderView::selectedUrls() const
{
    KUrl::List urls;
    foreach (const QModelIndex &index, m_selectionModel->selectedIndexes())
    {
        KFileItem item = m_model->itemForIndex(index);
        // Prefer the local URL if there is one, since we can't trash remote URL's
        const QString path = item.localPath();
        if (!path.isEmpty()) {
            urls.append(path);
        } else {
            urls.append(item.url());
        }
    }
    return urls;
}

void FolderView::copy()
{
    QMimeData *mimeData = new QMimeData;
    KonqMimeData::populateMimeData(mimeData, selectedUrls(), KUrl::List(), false);
    QApplication::clipboard()->setMimeData(mimeData);
}

void FolderView::cut()
{
    QMimeData *mimeData = new QMimeData;
    KonqMimeData::populateMimeData(mimeData, selectedUrls(), KUrl::List(), true);
    QApplication::clipboard()->setMimeData(mimeData);
}

void FolderView::paste()
{
    KonqOperations::doPaste(view(), m_url);
}

void FolderView::pasteTo()
{
    KUrl::List urls = selectedUrls();
    Q_ASSERT(urls.count() == 1);
    KonqOperations::doPaste(view(), urls.first());
}

void FolderView::refreshIcons()
{
    m_dirModel->dirLister()->updateDirectory(m_url);
}

void FolderView::renameSelectedIcon()
{
    QModelIndex index = m_selectionModel->currentIndex();
    if (!index.isValid())
        return;

    // Don't allow renaming of files the aren't visible in the view
    const QRect rect = visualRect(index);
    if (!mapToViewport(contentsRect()).contains(rect)) {
        return;
    }

    QStyleOptionViewItemV4 option = viewOptions();
    option.rect = rect;

    QWidget *editor = m_delegate->createEditor(0, option, index);
    editor->setAttribute(Qt::WA_NoSystemBackground);
    editor->installEventFilter(m_delegate);

    QGraphicsProxyWidget *proxy = new QGraphicsProxyWidget(this);
    proxy->setWidget(editor);

    m_delegate->updateEditorGeometry(editor, option, index);
    m_delegate->setEditorData(editor, index);

    editor->show();
    editor->setFocus();

    m_editorIndex = index;
}

void FolderView::toggleIconsLocked(bool locked)
{
    m_iconsLocked = locked;
    config().writeEntry("iconsLocked", locked);
    emit configNeedsSaving();
}

void FolderView::toggleAlignToGrid(bool align)
{
    m_alignToGrid = align;

    if (align) {
        alignIconsToGrid();
    }

    config().writeEntry("alignToGrid", align);
    emit configNeedsSaving();
}

void FolderView::toggleDirectoriesFirst(bool enable)
{
    m_sortDirsFirst = enable;

    m_model->setSortDirectoriesFirst(m_sortDirsFirst);
    if (m_sortColumn != -1) {
        m_savedPositions.clear();
        m_model->invalidate();
    }

    config().writeEntry("sortDirsFirst", m_sortDirsFirst);
    emit configNeedsSaving();
}

void FolderView::sortingChanged(QAction *action)
{
    int column = KDirModel::Name;

    if (action == m_actionCollection.action("sort_name")) {
        column = KDirModel::Name;
    } else if (action == m_actionCollection.action("sort_size")) {
        column = KDirModel::Size;
    } else if (action == m_actionCollection.action("sort_type")) {
        column = KDirModel::Type;
    } else if (action == m_actionCollection.action("sort_date")) {
        column = KDirModel::ModifiedTime;
    }

    if (column != m_sortColumn) {
        m_savedPositions.clear();
        m_model->invalidate();
        m_model->sort(column, Qt::AscendingOrder);
        m_delayedSaveTimer.start(5000, this);
        m_sortColumn = column;
        m_layoutBroken = false;
        config().writeEntry("sortColumn", m_sortColumn);
        emit configNeedsSaving();
    }
}

void FolderView::updateSortActionsState()
{
    if (m_sortColumn != -1 && m_layoutBroken) {
        foreach (QAction *action, m_sortingGroup->actions()) {
            action->setChecked(false);
        }
        m_sortColumn = -1;
        config().writeEntry("sortColumn", m_sortColumn);
        emit configNeedsSaving();
        return;
    }

    switch (m_sortColumn)
    {
    case KDirModel::Name:
        m_actionCollection.action("sort_name")->setChecked(true);
        break;
    case KDirModel::Size:
        m_actionCollection.action("sort_size")->setChecked(true);
        break;
    case KDirModel::Type:
        m_actionCollection.action("sort_type")->setChecked(true);
        break;
    case KDirModel::ModifiedTime:
        m_actionCollection.action("sort_date")->setChecked(true);
        break;
    }
}

void FolderView::listingStarted(const KUrl &url)
{
    Q_UNUSED(url)

    // Reset any error message that may have resulted from an earlier listing
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        update();
    }
}

void FolderView::listingCompleted()
{
    m_delayedCacheClearTimer.start(5000, this);
    m_initialListing = false;
}

void FolderView::listingCanceled()
{
    m_delayedCacheClearTimer.start(5000, this);
    m_initialListing = false;
}

void FolderView::listingError(const QString &message)
{
    m_errorMessage = message;
    markEverythingDirty();
    update();
}

void FolderView::commitData(QWidget *editor)
{
    m_delegate->setModelData(editor, m_model, m_editorIndex);
}

void FolderView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    Q_UNUSED(hint)

    if (editor->hasFocus()) {
        setFocus();
    }
    editor->hide();
    editor->removeEventFilter(m_delegate);
    editor->deleteLater();

    markEverythingDirty();
}

void FolderView::filterChanged(int index)
{
	uiFilter.filterFilesPattern->setEnabled(index != 0);
	uiFilter.searchMimetype->setEnabled(index != 0);
	uiFilter.filterFilesList->setEnabled(index != 0);
	uiFilter.selectAll->setEnabled(index != 0);
	uiFilter.deselectAll->setEnabled(index != 0);
}

void FolderView::selectUnselectAll()
{
    Qt::CheckState state = sender() == uiFilter.selectAll ? Qt::Checked : Qt::Unchecked;
    for (int i = 0; i < uiFilter.filterFilesList->model()->rowCount(); i++) {
        const QModelIndex index = uiFilter.filterFilesList->model()->index(i, 0);
        uiFilter.filterFilesList->model()->setData(index, state, Qt::CheckStateRole);
    }
}

void FolderView::moveToTrash(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(buttons)    

    KonqOperations::Operation op = (modifiers & Qt::ShiftModifier) ?
            KonqOperations::DEL : KonqOperations::TRASH;

    KonqOperations::del(view(), op, selectedUrls());
}

void FolderView::deleteSelectedIcons()
{
    KonqOperations::del(view(), KonqOperations::DEL, selectedUrls());
}

void FolderView::undoTextChanged(const QString &text)
{
    if (QAction *action = m_actionCollection.action("undo")) {
        action->setText(text);
    }
}

void FolderView::showContextMenu(QWidget *widget, const QPoint &pos, const QModelIndexList &indexes)
{
    if (!KAuthorized::authorize("action/kdesktop_rmb")) {
        return;
    }

    KFileItemList items;
    bool hasRemoteFiles = false;
    bool isTrashLink = false;

    foreach (const QModelIndex &index, indexes) {
        KFileItem item = m_model->itemForIndex(index);
        hasRemoteFiles |= item.localPath().isEmpty();
        items.append(item);
    }

    // Check if we're showing the menu for the trash link
    if (items.count() == 1 && items.at(0).isDesktopFile()) {
        KDesktopFile file(items.at(0).localPath());
        if (file.readType() == "Link" && file.readUrl() == "trash:/") {
            isTrashLink = true;
        }
    }

    QAction* pasteTo = m_actionCollection.action("pasteto");
    if (pasteTo) {
        pasteTo->setEnabled(m_actionCollection.action("paste")->isEnabled());
        pasteTo->setText(m_actionCollection.action("paste")->text());
    }

    QList<QAction*> editActions;
    editActions.append(m_actionCollection.action("rename"));

    KConfigGroup configGroup(KGlobal::config(), "KDE");
    bool showDeleteCommand = configGroup.readEntry("ShowDeleteCommand", false);

    // Don't add the "Move to Trash" action if we're showing the menu for the trash link
    if (!isTrashLink) {
        if (!hasRemoteFiles) {
            editActions.append(m_actionCollection.action("trash"));
        } else {
            showDeleteCommand = true;
        }
    }
    if (showDeleteCommand) {
        editActions.append(m_actionCollection.action("del"));
    }

    KParts::BrowserExtension::ActionGroupMap actionGroups;
    actionGroups.insert("editactions", editActions);

    KParts::BrowserExtension::PopupFlags flags = KParts::BrowserExtension::ShowProperties;

    // Use the Dolphin setting for showing the "Copy To" and "Move To" actions
    KConfig dolphin("dolphinrc");
    if (KConfigGroup(&dolphin, "General").readEntry("ShowCopyMoveMenu", false)) {
        flags |= KParts::BrowserExtension::ShowUrlOperations;
    }

    // m_newMenu can be NULL here but KonqPopupMenu does handle this.
    KonqPopupMenu *contextMenu = new KonqPopupMenu(items, m_url, m_actionCollection, m_newMenu,
                                                   KonqPopupMenu::ShowNewWindow, flags, widget,
                                                   KBookmarkManager::userBookmarksManager(),
                                                   actionGroups);

    contextMenu->exec(pos);
    delete contextMenu;

    if (pasteTo) {
        pasteTo->setEnabled(false);
    }
}

void FolderView::focusInEvent(QFocusEvent *event)
{
    Plasma::Applet::focusInEvent(event);
    markEverythingDirty();
}

void FolderView::focusOutEvent(QFocusEvent *event)
{
    Plasma::Applet::focusOutEvent(event);
    markEverythingDirty();
}

void FolderView::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    const QModelIndex index = indexAt(mapToViewport(event->pos()));
    if (index.isValid()) {
        m_hoveredIndex = index;
        markAreaDirty(visualRect(index));
    }
}

void FolderView::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    if (m_hoveredIndex.isValid()) {
        markAreaDirty(visualRect(m_hoveredIndex));
        m_hoveredIndex = QModelIndex();
    }
}

void FolderView::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const QModelIndex index = indexAt(mapToViewport(event->pos()));
    if (index != m_hoveredIndex) {
        markAreaDirty(visualRect(index));
        markAreaDirty(visualRect(m_hoveredIndex));
        m_hoveredIndex = index;
    }
}

void FolderView::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (!contentsRect().contains(event->pos()) ||
        event->pos().y() < contentsRect().top() + m_titleHeight ||
        !m_errorMessage.isEmpty())
    {
        Plasma::Applet::mousePressEvent(event);
        return;
    }

    const QPointF pos = mapToViewport(event->pos());
    setFocus(Qt::MouseFocusReason);

    if (event->button() == Qt::RightButton) {
        const QModelIndex index = indexAt(pos);
        if (index.isValid()) {
            if (!m_selectionModel->isSelected(index)) {
                m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markEverythingDirty();
            }
            showContextMenu(event->widget(), event->screenPos(), m_selectionModel->selectedIndexes());
        } else if (m_selectionModel->hasSelection()) {
            m_selectionModel->clearSelection();
            markEverythingDirty();
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(pos);

        // If an icon was pressed
        if (index.isValid())
        {
            if (event->modifiers() & Qt::ControlModifier) {
                m_selectionModel->select(index, QItemSelectionModel::Toggle);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markAreaDirty(visualRect(index));
            } else if (!m_selectionModel->isSelected(index)) {
                m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markEverythingDirty();
            }
            m_pressedIndex = index;
            m_buttonDownPos = pos;
            event->accept();
            return;
        } else if (event->modifiers() & Qt::ControlModifier) {
            // make current selection persistent
            m_selectionModel->select(m_selectionModel->selection(), QItemSelectionModel::Select);
        }

        // If empty space was pressed
        m_pressedIndex = QModelIndex();
        m_buttonDownPos = pos;
        if (m_selectionModel->hasSelection()) {
            if (!(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
                m_selectionModel->clearSelection();
                markEverythingDirty();
            }
        }
        event->accept();
    }
}

void FolderView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (m_rubberBand.isValid()) {
            markAreaDirty(m_rubberBand);
            m_rubberBand = QRect();
            return;
        }

        const QPointF pos = mapToViewport(event->pos());
        const QModelIndex index = indexAt(pos);

        if (index.isValid() && index == m_pressedIndex && !(event->modifiers() & Qt::ControlModifier)) {
            if (!m_doubleClick && KGlobalSettings::singleClick()) {
                const KFileItem item = m_model->itemForIndex(index);
                item.run();
                emit releaseVisualFocus();
                m_selectionModel->clearSelection();
                markEverythingDirty();
            }
            // We don't clear and update the selection and current index in
            // mousePressEvent() if the item is already selected when it's pressed,
            // so we need to do that here.
            if (m_selectionModel->currentIndex() != index ||
                m_selectionModel->selectedIndexes().count() > 1) {
                m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markEverythingDirty();
            }
            event->accept();
            m_doubleClick = false;
            return;
        }
    }

    m_doubleClick = false;
    m_pressedIndex = QModelIndex();
    Plasma::Applet::mouseReleaseEvent(event);
}

void FolderView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        Plasma::Applet::mouseDoubleClickEvent(event);
        return;
    }

    // So we don't activate the item again on the release event
    m_doubleClick = true;

    // We don't want to invoke the default implementation in this case, since it
    // calls mousePressEvent().
    if (KGlobalSettings::singleClick()) {
        return;
    }

    const QModelIndex index = indexAt(mapToViewport(event->pos()));
    if (!index.isValid()) {
        return;
    }

    // Activate the item
    const KFileItem item = m_model->itemForIndex(index);
    item.run();
    emit releaseVisualFocus();

    m_selectionModel->clearSelection();
    markEverythingDirty();
}

void FolderView::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // If an item is pressed
    if (m_pressedIndex.isValid())
    {
        const QPointF point = event->pos() - event->buttonDownPos(Qt::LeftButton);

        if (point.toPoint().manhattanLength() >= QApplication::startDragDistance())
        {
            startDrag(m_buttonDownPos, event->widget());
        }
        event->accept();
        return;
    }

    const QPointF pos = mapToViewport(event->pos());
    const QRectF rubberBand = QRectF(m_buttonDownPos, pos).normalized();
    const QRect r = QRectF(rubberBand & m_viewportRect).toAlignedRect();

    if (r != m_rubberBand)
    {
        const QPoint pt = pos.toPoint();
        QRectF dirtyRect = m_rubberBand | r;
        m_rubberBand = r;

        dirtyRect |= visualRect(m_hoveredIndex);
        m_hoveredIndex = QModelIndex();

        foreach (const QModelIndex &index, m_selectionModel->selectedIndexes())
            dirtyRect |= visualRect(index);

        // Select the indexes inside the rubber band
        QItemSelection selection;
        for (int i = 0; i < m_items.size(); i++)
        {
            QModelIndex index = m_model->index(i, 0);
            if (!indexIntersectsRect(index, m_rubberBand))
                continue;

            int start = i;

            do {
                dirtyRect |= m_items[i].rect;
                if (m_items[i].rect.contains(pt) && visualRegion(index).contains(pt)) {
                    m_hoveredIndex = index;
                }
                index = m_model->index(++i, 0);
            } while (i < m_items.size() && indexIntersectsRect(index, m_rubberBand));

            selection.select(m_model->index(start, 0), m_model->index(i - 1, 0));
        }
        m_selectionModel->select(selection, QItemSelectionModel::ToggleCurrent);

        // Update the current index
        if (m_hoveredIndex.isValid()) {
            if (m_hoveredIndex != m_selectionModel->currentIndex()) {
                dirtyRect |= visualRect(m_selectionModel->currentIndex());
            }
            m_selectionModel->setCurrentIndex(m_hoveredIndex, QItemSelectionModel::NoUpdate);
        }
        markAreaDirty(dirtyRect);
    }

    event->accept();
}

void FolderView::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (event->modifiers() == Qt::CTRL) {
        Containment::wheelEvent(event);
        return;
    }

    if (event->orientation() == Qt::Horizontal) {
        return;
    }

    int pixels = 40 * event->delta() / 120;
    m_scrollBar->setValue(m_scrollBar->value() - pixels);
}

void FolderView::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    const QString appletMimeType = static_cast<Plasma::Corona*>(scene())->appletMimeType();
    event->setAccepted(event->mimeData()->hasUrls() || (isContainment() &&
                       immutability() == Plasma::Mutable && event->mimeData()->hasFormat(appletMimeType)));
}

void FolderView::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    const QModelIndex index = indexAt(mapToViewport(event->pos()));
    if (index == m_hoveredIndex) {
        return;
    }

    const QString appletMimeType = static_cast<Plasma::Corona*>(scene())->appletMimeType();
    QRectF dirtyRect = visualRect(m_hoveredIndex);
    m_hoveredIndex = QModelIndex();

    if (index.isValid() && (m_model->flags(index) & Qt::ItemIsDropEnabled) &&
        !event->mimeData()->hasFormat(appletMimeType))
    {
        dirtyRect |= visualRect(index);
        bool onOurself = false;

        foreach (const QModelIndex &selected, m_selectionModel->selectedIndexes()) {
            if (selected == index) {
                onOurself = true;
                break;
            }
        }

        if (!onOurself) {
            m_hoveredIndex = index;
            dirtyRect |= visualRect(index);
        }
    }

    markAreaDirty(dirtyRect);
    event->accept();
}

void FolderView::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    // If the dropped item is an applet, let Containment handle it
    const QString appletMimeType = static_cast<Plasma::Corona*>(scene())->appletMimeType();
    if (isContainment() && event->mimeData()->hasFormat(appletMimeType)) {
        Containment::dropEvent(event);
        return;
    }

    // Check if the drop event originated from this applet.
    // Normally we'd do this by checking if the source widget matches the target widget
    // in the drag and drop operation, but since two QGraphicsItems can be part of the
    // same widget, we can't use that method here.
    KFileItem item;
    if ((!m_dragInProgress && !m_hoveredIndex.isValid()) ||
        ((!m_dragInProgress || m_hoveredIndex.isValid()) &&
         m_model->flags(m_hoveredIndex) & Qt::ItemIsDropEnabled))
    {
        item = m_model->itemForIndex(m_hoveredIndex);
    }

    if (!item.isNull()) {
        QDropEvent ev(event->screenPos(), event->dropAction(), event->mimeData(),
                      event->buttons(), event->modifiers());
        //kDebug() << "dropping to" << m_url << "with" << view() << event->modifiers();
        KonqOperations::doDrop(item, m_url, &ev, event->widget());
        //kDebug() << "all done!";
        return;
    }

    // If we get to this point, the drag was started from within the applet,
    // so instead of moving/copying/linking the dropped URL's to the folder,
    // we'll move the items in the view.
    QPoint delta = (mapToViewport(event->pos()) - m_buttonDownPos).toPoint();
    if (delta.isNull() || m_iconsLocked) {
        return;
    }

    // If this option is set, we'll assume the dragged icons were aligned
    // to the grid before the drag started, and just adjust the delta we use
    // to move all of them.
    if (m_alignToGrid) {
        const QSize size = gridSize() + QSize(10, 10);
        if ((qAbs(delta.x()) < size.width() / 2) && (qAbs(delta.y()) < size.height() / 2)) {
            return;
        }

        delta.rx() = qRound(delta.x() / qreal(size.width()))  * size.width();
        delta.ry() = qRound(delta.y() / qreal(size.height())) * size.height();
    }

    foreach (const QUrl &url, event->mimeData()->urls()) {
        const QModelIndex index = m_model->indexForUrl(url);
        if (index.isValid()) {
            m_items[index.row()].rect.translate(delta);
        }
    }

    // Make sure no icons have negative coordinates etc.
    doLayoutSanityCheck();
    updateScrollBar();
    markEverythingDirty();
    m_regionCache.clear();

    m_layoutBroken = true;
    updateSortActionsState();
    m_delayedSaveTimer.start(5000, this);
}

// pos is the position where the mouse was clicked in the applet.
// widget is the widget that sent the mouse event that triggered the drag.
void FolderView::startDrag(const QPointF &pos, QWidget *widget)
{
    QModelIndexList indexes = m_selectionModel->selectedIndexes();
    QRect boundingRect;
    foreach (const QModelIndex &index, indexes) {
        boundingRect |= visualRect(index);
    }

    QPixmap pixmap(boundingRect.size());
    pixmap.fill(Qt::transparent);

    QStyleOptionViewItemV4 option = viewOptions(); 
    option.state |= QStyle::State_Selected;

    updateTextShadows(palette().color(QPalette::HighlightedText));

    QPainter p(&pixmap);
    foreach (const QModelIndex &index, indexes)
    {
        option.rect = visualRect(index).translated(-boundingRect.topLeft());
        if (index == m_hoveredIndex)
            option.state |= QStyle::State_MouseOver;
        else
            option.state &= ~QStyle::State_MouseOver;
        m_delegate->paint(&p, option, index);
    }
    p.end();

    // Mark the area containing the about-to-be-dragged items as dirty, so they
    // will be erased from the view on the next repaint.  We have to do this
    // before calling QDrag::exec(), since it's a blocking call.
    markAreaDirty(boundingRect);

    // Unset the hovered index so dropEvent won't think the icons are being
    // dropped on a dragged folder.
    m_hoveredIndex = QModelIndex();
    m_dragInProgress = true;

    QDrag *drag = new QDrag(widget);
    drag->setMimeData(m_model->mimeData(indexes));
    drag->setPixmap(pixmap);
    drag->setHotSpot((pos - boundingRect.topLeft()).toPoint());
    drag->exec(m_model->supportedDragActions());

    m_dragInProgress = false;

    // Repaint the dragged icons in case the drag did not remove the file 
    markAreaDirty(boundingRect); 
}

QSize FolderView::iconSize() const
{
    const int size = (m_customIconSize != 0) ? m_customIconSize : KIconLoader::global()->currentSize(KIconLoader::Desktop);
    return QSize(size, size);
}

QSize FolderView::gridSize() const
{
    const QFontMetrics fm(m_font);
    const int textHeight = fm.lineSpacing() * m_numTextLines;
    QSize size = iconSize();
    size.rheight() = size.height() + textHeight + 16;
    size.rwidth() = qMax(size.width() * 2, fm.averageCharWidth() * 10);
    return size;
}

QStyleOptionViewItemV4 FolderView::viewOptions() const
{
    QStyleOptionViewItemV4 option;
    initStyleOption(&option);

    option.font                = m_font;
    option.fontMetrics         = QFontMetrics(m_font);
    option.decorationAlignment = Qt::AlignTop | Qt::AlignHCenter;
    option.decorationPosition  = QStyleOptionViewItem::Top;
    option.decorationSize      = iconSize();
    option.displayAlignment    = Qt::AlignHCenter;
    option.textElideMode       = Qt::ElideRight;
    option.locale              = QLocale::system();
    option.widget              = 0;
    option.viewItemPosition    = QStyleOptionViewItemV4::OnlyOne;

    if (m_numTextLines > 1) {
        option.features = QStyleOptionViewItemV2::WrapText;
    }

    return option;
}

void FolderView::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_delayedSaveTimer.timerId()) {
        m_delayedSaveTimer.stop();
        saveIconPositions();
        emit configNeedsSaving();
    } else if (event->timerId() == m_delayedCacheClearTimer.timerId()) {
        m_delayedCacheClearTimer.stop();
        m_savedPositions.clear();
    } else if (event->timerId() == m_delayedLayoutTimer.timerId()) {
        m_delayedLayoutTimer.stop();
        layoutItems();
    } else if (event->timerId() == m_animTimer.timerId()) {
        if (++m_animFrame >= 20) {
            m_animFrame = 0;
        }
        const QSizeF size(m_animFrames.width(), m_animFrames.width());
        update(QRectF(contentsRect().center() - QPointF(size.width() / 2, size.width() / 2), size));
    }

    Containment::timerEvent(event);
}

#include "folderview.moc"
