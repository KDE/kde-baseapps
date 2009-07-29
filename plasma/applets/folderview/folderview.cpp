/*
 *   Copyright © 2008, 2009 Fredrik Höglund <fredrik@kde.org>
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

#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QDrag>
#include <QGraphicsLinearLayout>
#include <QGraphicsView>
#include <QGraphicsSceneDragDropEvent>
#include <QImageReader>
#include <QItemSelectionModel>
#include <QSignalMapper>

#include <KAction>
#include <KApplication>
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
#include <KStandardDirs>
#include <KStandardShortcut>
#include <KStringHandler>
#include <KTemporaryFile>

#include <kio/copyjob.h>
#include <kio/fileundomanager.h>
#include <kio/paste.h>
#include <KParts/BrowserExtension>

#include <kfileitemactions.h>
#include <kfileitemlistproperties.h>

#include <knewmenu.h>
#include <konqmimedata.h>
#include <konq_operations.h>
#include <konq_popupmenu.h>
#include <konq_popupmenuinformation.h>

#include <limits.h>

#ifdef Q_OS_WIN
#  define _WIN32_WINNT 0x0500 // require NT 5.0 (win 2k pro)
#  include <windows.h>
#endif // Q_OS_WIN

#include <Plasma/Corona>
#include <Plasma/PaintUtils>
#include <Plasma/Theme>
#include <Plasma/ToolTipManager>
#include <Plasma/Wallpaper>

#ifdef HAVE_KWORKSPACE
#  include <kworkspace/kwindowlistmenu.h>
#endif

#include "dirlister.h"
#include "dialog.h"
#include "folderviewadapter.h"
#include "iconwidget.h"
#include "label.h"
#include "previewpluginsmodel.h"
#include "proxymodel.h"
#include "listview.h"


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

    const bool fastRet = mime->comment().contains(m_filter, Qt::CaseInsensitive) ||
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



RemoteWallpaperSetter::RemoteWallpaperSetter(const KUrl &url, FolderView *containment)
    : QObject(containment)
{
    const QString suffix = QFileInfo(url.fileName()).suffix();

    KTemporaryFile file;
    file.setPrefix(KGlobal::dirs()->saveLocation("wallpaper"));
    file.setSuffix(QString(".") + suffix);
    file.setAutoRemove(false);

    if (file.open()) {
        KIO::FileCopyJob *job = KIO::file_copy(url, KUrl::fromPath(file.fileName()), -1, KIO::Overwrite);
        connect(job, SIGNAL(result(KJob*)), SLOT(result(KJob*)));
    } else {
        deleteLater();
    }
}

void RemoteWallpaperSetter::result(KJob *job)
{
    if (!job->error()) {
        FolderView *containment = static_cast<FolderView*>(parent());
        KIO::FileCopyJob *copyJob = static_cast<KIO::FileCopyJob*>(job);
        containment->setWallpaper(copyJob->destUrl());
    }
    deleteLater();
}



// ---------------------------------------------------------------------------



FolderView::FolderView(QObject *parent, const QVariantList &args)
    : Plasma::Containment(parent, args),
      m_previewGenerator(0),
      m_placesModel(0),
      m_itemActions(0),
      m_iconView(0),
      m_listView(0),
      m_label(0),
      m_iconWidget(0),
      m_dialog(0),
      m_newMenu(0),
#ifdef HAVE_KWORKSPACE
      m_windowListMenu(0),
#endif      
      m_actionCollection(this)
{
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    setAcceptHoverEvents(true);
    setAcceptDrops(true);

    m_dirModel = new KDirModel(this);
    m_dirModel->setDropsAllowed(KDirModel::DropOnDirectory | KDirModel::DropOnLocalExecutable);

    m_model = new ProxyModel(this);
    m_model->setSourceModel(m_dirModel);
    m_model->setSortLocaleAware(true);
    m_model->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_delegate = new KFileItemDelegate(this);
    m_selectionModel = new QItemSelectionModel(m_model, this);

    if (args.count() > 0) {
        setUrl(KUrl(args.value(0).toString()));
    }

    resize(600, 400);

    // As we use some part of konqueror libkonq must be added to have translations
    KGlobal::locale()->insertCatalog("libkonq");
}

void FolderView::init()
{
    Containment::init();
    setContainmentType(DesktopContainment);

    // Find out about icon and font settings changes
    connect(KGlobalSettings::self(), SIGNAL(kdisplayFontChanged()), SLOT(fontSettingsChanged()));
    connect(KGlobalSettings::self(), SIGNAL(iconChanged(int)), SLOT(iconSettingsChanged(int)));

    // Find out about theme changes
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), SLOT(themeChanged()));

    // Find out about network availability changes
    connect(Solid::Networking::notifier(), SIGNAL(statusChanged(Solid::Networking::Status)),
            SLOT(networkStatusChanged(Solid::Networking::Status)));

    KConfigGroup cg = config();
    m_customLabel         = cg.readEntry("customLabel", "");
    m_customIconSize      = cg.readEntry("customIconSize", 0);
    m_showPreviews        = cg.readEntry("showPreviews", true);
    m_drawShadows         = cg.readEntry("drawShadows", true);
    m_numTextLines        = cg.readEntry("numTextLines", 2);
    m_textColor           = cg.readEntry("textColor", QColor(Qt::transparent));
    m_iconsLocked         = cg.readEntry("iconsLocked", false);
    m_alignToGrid         = cg.readEntry("alignToGrid", false);
    m_previewPlugins      = cg.readEntry("previewPlugins", QStringList() << "imagethumbnail" << "jpegthumbnail");
    m_sortDirsFirst       = cg.readEntry("sortDirsFirst", true);
    m_sortColumn          = cg.readEntry("sortColumn", int(KDirModel::Name));
    m_filterFiles         = cg.readEntry("filterFiles", "*");
    m_filterType          = cg.readEntry("filter", 0);
    m_filterFilesMimeList = cg.readEntry("mimeFilter", QStringList());

    m_userSelectedShowAllFiles = m_filterType;
    if (isContainment()) {
        m_flow = layoutDirection() == Qt::LeftToRight ? IconView::TopToBottom : IconView::TopToBottomRightToLeft;
    } else {
        m_flow = layoutDirection() == Qt::LeftToRight ? IconView::LeftToRight : IconView::RightToLeft;
    }
    m_flow = static_cast<IconView::Flow>(cg.readEntry("flow", static_cast<int>(m_flow)));

    m_model->setFilterMode(ProxyModel::filterModeFromInt(m_filterType));
    m_model->setMimeTypeFilterList(m_filterFilesMimeList);
    m_model->setFilterFixedString(m_filterFiles);
    m_model->setSortDirectoriesFirst(m_sortDirsFirst);
    m_model->sort(m_sortColumn != -1 ? m_sortColumn : KDirModel::Name, Qt::AscendingOrder);

    DirLister *lister = new DirLister(this);
    lister->setDelayedMimeTypes(true);
    lister->setAutoErrorHandlingEnabled(false, 0);

    m_dirModel->setDirLister(lister);

    if (!m_url.isValid()) {

        //FIXME: 4.3 Need to update folderview's description
        QString path = QDir::homePath();
        if (isContainment()) {
            const QString desktopPath = KGlobalSettings::desktopPath();
            const QDir desktopFolder(desktopPath);

            if (desktopPath != QDir::homePath() && desktopFolder.exists()) {
                path = QString("desktop:/");
            }
        }
        setUrl(cg.readEntry("url", KUrl(path)));

    } else {
        KConfigGroup cg = config();
        cg.writeEntry("url", m_url);
    }

    // TODO: 4.3 Check if the URL is a remote URL, and if it is check the network status
    //       and display a message saying it's not available, instead of trying to open
    //       the URL and waiting for the job to time out.
    lister->openUrl(m_url);

    createActions();
    
    if (isContainment()) {
        setupIconView();

        // Set a low Z value so applets don't end up below the icon view
        m_iconView->setZValue(INT_MIN);
    }

    connect(QApplication::clipboard(), SIGNAL(dataChanged()), SLOT(clipboardDataChanged()));
}

FolderView::~FolderView()
{
    delete m_newMenu;
#ifdef HAVE_KWORKSPACE
    delete m_windowListMenu;
#endif
}

void FolderView::saveState(KConfigGroup &config) const
{
    Q_UNUSED(config)
    saveIconPositions();
}

void FolderView::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget *widgetFilter = new QWidget;
    QWidget *widgetDisplay = new QWidget;
    QWidget *widgetLocation = new QWidget;
    uiFilter.setupUi(widgetFilter);
    uiDisplay.setupUi(widgetDisplay);
    uiLocation.setupUi(widgetLocation);

    if (!m_placesModel) {
        m_placesModel = new KFilePlacesModel(this);
    }

    PlacesFilterModel *placesFilter = new PlacesFilterModel(parent);
    placesFilter->setSourceModel(m_placesModel);
    uiLocation.placesCombo->setModel(placesFilter);

    QString desktopPath = KGlobalSettings::desktopPath();
    QDir desktopFolder(desktopPath);

    const bool desktopVisible = desktopPath != QDir::homePath() && desktopFolder.exists();
    uiLocation.showDesktopFolder->setVisible(desktopVisible);

    if (desktopVisible && m_url == KUrl("desktop:/")) {
        uiLocation.showDesktopFolder->setChecked(true);
        uiLocation.placesCombo->setEnabled(false);
        uiLocation.lineEdit->setEnabled(false);
    } else {
        QModelIndex index;
        for (int i = 0; i < placesFilter->rowCount(); i++) {
            const KUrl url = m_placesModel->url(placesFilter->mapToSource(placesFilter->index(i, 0)));
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
        uiDisplay.lblCustomLabel->hide();
        uiDisplay.labelEdit->hide();
    }

    uiDisplay.labelEdit->setText(m_titleText);

    const QList<int> iconSizes = QList<int>() << 16 << 22 << 32 << 48 << 64 << 128;
    uiDisplay.sizeSlider->setRange(0, iconSizes.size() - 1);
    uiDisplay.sizeSlider->setValue(iconSizes.indexOf(iconSize().width()));

    // Only add "Unsorted" as an option when we're showing an icon view, since the list view
    // doesn't allow the user to rearrange the icons.
    if (m_iconView) {
        uiDisplay.sortCombo->addItem(i18nc("Sort Icons", "Unsorted"), -1);
    }
    uiDisplay.sortCombo->addItem(KGlobal::locale()->removeAcceleratorMarker(m_actionCollection.action("sort_name")->text()), KDirModel::Name);
    uiDisplay.sortCombo->addItem(KGlobal::locale()->removeAcceleratorMarker(m_actionCollection.action("sort_size")->text()), KDirModel::Size);
    uiDisplay.sortCombo->addItem(KGlobal::locale()->removeAcceleratorMarker(m_actionCollection.action("sort_type")->text()), KDirModel::Type);
    uiDisplay.sortCombo->addItem(KGlobal::locale()->removeAcceleratorMarker(m_actionCollection.action("sort_date")->text()), KDirModel::ModifiedTime);

    uiDisplay.flowCombo->addItem(i18n("Top to Bottom, Left to Right"), IconView::TopToBottom);
    uiDisplay.flowCombo->addItem(i18n("Top to Bottom, Right to Left"), IconView::TopToBottomRightToLeft);
    uiDisplay.flowCombo->addItem(i18n("Left to Right, Top to Bottom"), IconView::LeftToRight);
    uiDisplay.flowCombo->addItem(i18n("Right to Left, Top to Bottom"), IconView::RightToLeft);

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

    parent->addPage(widgetLocation, i18nc("Title of the page that lets the user choose which location should the folderview show", "Location"), "folder");
    parent->addPage(widgetDisplay, i18nc("Title of the page that lets the user choose how the folderview should be shown", "Display"), "preferences-desktop-display");
    parent->addPage(widgetFilter, i18nc("Title of the page that lets the user choose how to filter the folderview contents", "Filter"), "view-filter");
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

    KConfigGroup cg = config();
    const int filter = cg.readEntry("filter", 0);
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

    if (url.isEmpty()) {
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

    const int filterType = uiFilter.filterType->currentIndex();
    KConfigGroup cg = config();
    bool needReload = false;

    if (m_drawShadows != uiDisplay.drawShadows->isChecked()) {
        m_drawShadows = uiDisplay.drawShadows->isChecked();
        cg.writeEntry("drawShadows", m_drawShadows);
    }

    if (m_showPreviews != uiDisplay.showPreviews->isChecked() ||
        (m_previewGenerator && m_previewPlugins != m_previewGenerator->enabledPlugins()))
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
    }

    if (m_numTextLines != uiDisplay.numLinesEdit->value()) {
        m_numTextLines = uiDisplay.numLinesEdit->value();
        cg.writeEntry("numTextLines", m_numTextLines);
    }

    const QList<int> iconSizes = QList<int>() << 16 << 22 << 32 << 48 << 64 << 128;
    const int size = iconSizes.at(uiDisplay.sizeSlider->value());
    if (size != iconSize().width())
    {
        m_customIconSize = size;
        cg.writeEntry("customIconSize", m_customIconSize);

        // This is to force the preview images to be regenerated with the new size
        if (m_showPreviews) {
            needReload = true;
        }
    }

    const int sortColumn = uiDisplay.sortCombo->itemData(uiDisplay.sortCombo->currentIndex()).toInt();
    if (m_sortColumn != sortColumn) {
        m_sortColumn = sortColumn;
        if (m_sortColumn != -1) {
            m_model->invalidate();
            m_model->sort(m_sortColumn, Qt::AscendingOrder);
        } else if (m_iconView) {
            m_iconView->setCustomLayout(true);
        }
        updateSortActionsState();
        cg.writeEntry("sortColumn", m_sortColumn);
    }

    const int flow = uiDisplay.flowCombo->itemData(uiDisplay.flowCombo->currentIndex()).toInt();
    if (m_flow != flow) {
        m_flow = static_cast<IconView::Flow>(flow);
        cg.writeEntry("flow", flow);
    }

    if (m_alignToGrid != uiDisplay.alignToGrid->isChecked()) {
        m_alignToGrid = uiDisplay.alignToGrid->isChecked();
        cg.writeEntry("alignToGrid", m_alignToGrid);
        m_actionCollection.action("auto_align")->setChecked(m_alignToGrid);
    }

    if (m_iconsLocked != uiDisplay.lockInPlace->isChecked()) {
        m_iconsLocked = uiDisplay.lockInPlace->isChecked();
        cg.writeEntry("iconsLocked", m_iconsLocked);
        m_actionCollection.action("lock_icons")->setChecked(m_iconsLocked);
    }

    const QString label = uiDisplay.labelEdit->text();
    if ((m_customLabel.isEmpty() && label != m_titleText) ||
        (!m_customLabel.isEmpty() && label != m_customLabel))
    {
        m_customLabel = label;
        setUrl(url);
        cg.writeEntry("customLabel", m_customLabel);
    }

    if (m_url != url || m_filterFiles != uiFilter.filterFilesPattern->text() ||
        m_filterFilesMimeList != selectedItems || m_filterType != filterType)
    {
        m_model->setFilterFixedString(uiFilter.filterFilesPattern->text());
        m_filterFiles = uiFilter.filterFilesPattern->text();
        m_filterFilesMimeList = selectedItems;
        m_filterType = filterType;
        setUrl(url);

        cg.writeEntry("url", m_url);
        cg.writeEntry("filterFiles", m_filterFiles);
        cg.writeEntry("filter", m_filterType);
	m_userSelectedShowAllFiles = m_filterType;
        cg.writeEntry("mimeFilter", m_filterFilesMimeList);

        m_model->setMimeTypeFilterList(m_filterFilesMimeList);
        m_model->setFilterMode(ProxyModel::filterModeFromInt(m_filterType));
        needReload = true;
    }

    if (m_iconView) {
        updateIconViewState();
    }

    if (m_listView) {
        updateListViewState();
    }

    if (needReload) {
        m_dirModel->dirLister()->openUrl(m_url);

        // So the KFileItemActions will be recreated for the new URL.
        delete m_itemActions;
        m_itemActions = 0;
    }

    m_delayedSaveTimer.start(5000, this);
    emit configNeedsSaving();
}

void FolderView::setWallpaper(const KUrl &url)
{
    if (!url.isLocalFile()) {
        return;
    }

    const QString wallpaper = url.toLocalFile();
    Plasma::Wallpaper::ResizeMethod resizeMethod = Plasma::Wallpaper::MaxpectResize;

    // Try to read the image size without loading the image
    QImageReader reader(wallpaper);
    QSize size = reader.size();

    if (!size.isEmpty()) {
        if (size.width() < geometry().width() / 2 && size.height() < geometry().height() / 2) {
            // If the image size is less than a quarter of the size of the containment,
            // center it instead of scaling it.
           resizeMethod = Plasma::Wallpaper::CenteredResize;
        } else {
            // Permit up to 10% of the image to be cropped in either dimension as a result of scaling.
            size.scale(geometry().size().toSize(), Qt::KeepAspectRatioByExpanding);
            if (size.width() / geometry().width() < 1.1 && size.height() / geometry().height() < 1.1) {
                resizeMethod = Plasma::Wallpaper::ScaledAndCroppedResize;
            } else {
                resizeMethod = Plasma::Wallpaper::MaxpectResize;
            }
        }
    }

    KConfigGroup cg = config();
    cg = KConfigGroup(&cg, "Wallpaper");
    cg = KConfigGroup(&cg, "image");

    QStringList userWallpapers = cg.readEntry("userswallpapers", QStringList());
    if (!userWallpapers.contains(wallpaper)) {
        userWallpapers.append(wallpaper);
        cg.writeEntry("userswallpapers", userWallpapers);
    }

    cg.writeEntry("wallpaper", wallpaper);
    cg.writeEntry("wallpaperposition", int(resizeMethod));
    cg.sync();

    Plasma::Containment::setWallpaper("image", "SingleImage");
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

void FolderView::updateListViewState()
{
    QPalette palette = m_listView->palette();
    palette.setColor(QPalette::Text, m_textColor != Qt::transparent ? m_textColor :
                     Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
    m_listView->setPalette(palette);

    const QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DesktopFont);
    if (m_listView->font() != font) {
        m_listView->setFont(font);
    }
    m_listView->setDrawShadows(m_drawShadows);
    m_listView->setIconSize(iconSize());
    m_listView->setWordWrap(m_numTextLines > 1);
}

void FolderView::updateIconViewState()
{
    QPalette palette = m_iconView->palette();
    palette.setColor(QPalette::Text, m_textColor != Qt::transparent ? m_textColor :
                     Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
    m_iconView->setPalette(palette);

    m_iconView->setDrawShadows(m_drawShadows);
    m_iconView->setIconSize(iconSize());
    m_iconView->setGridSize(gridSize());
    m_iconView->setFlow(m_flow);
    m_iconView->setWordWrap(m_numTextLines > 1);
    m_iconView->setAlignToGrid(m_alignToGrid);
    m_iconView->setIconsMoveable(!m_iconsLocked);

    if (m_label) {
        m_label->setPalette(palette);
        m_label->setDrawShadow(m_drawShadows);
    }
}

void FolderView::addActions(AbstractItemView *view)
{
    view->addAction(m_actionCollection.action("rename"));
    view->addAction(m_actionCollection.action("cut"));
    view->addAction(m_actionCollection.action("undo"));
    view->addAction(m_actionCollection.action("copy"));
    view->addAction(m_actionCollection.action("paste"));
    view->addAction(m_actionCollection.action("pasteto"));
    view->addAction(m_actionCollection.action("refresh"));
    view->addAction(m_actionCollection.action("trash"));
    view->addAction(m_actionCollection.action("del"));
}

void FolderView::setupIconView()
{
    if (m_iconView) {
        return;
    }

    m_iconView = new IconView(this);
    m_iconView->setModel(m_model);
    m_iconView->setItemDelegate(m_delegate);
    m_iconView->setSelectionModel(m_selectionModel);
    m_iconView->setFont(Plasma::Theme::defaultTheme()->font(Plasma::Theme::DesktopFont));

    // Add widget specific actions with shortcuts to the view
    addActions(m_iconView);

    if (!isContainment()) {
        m_label = new Label(this);
        m_label->setText(m_titleText);
    }

    updateIconViewState();

    const QStringList data = config().readEntry("savedPositions", QStringList());
    m_iconView->setIconPositionsData(data);

    connect(m_iconView, SIGNAL(activated(QModelIndex)), SLOT(activated(QModelIndex)));
    connect(m_iconView, SIGNAL(indexesMoved(QModelIndexList)), SLOT(indexesMoved(QModelIndexList)));
    connect(m_iconView, SIGNAL(contextMenuRequest(QWidget*,QPoint)), SLOT(contextMenuRequest(QWidget*,QPoint)));
    connect(m_iconView, SIGNAL(busy(bool)), SLOT(setBusy(bool)));

    FolderViewAdapter *adapter = new FolderViewAdapter(m_iconView);
    m_previewGenerator = new KFilePreviewGenerator(adapter, m_model);
    m_previewGenerator->setPreviewShown(m_showPreviews);
    m_previewGenerator->setEnabledPlugins(m_previewPlugins);

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    if (m_label) {
        layout->addItem(m_label);
    }
    layout->addItem(m_iconView);

    setLayout(layout);
}

void FolderView::fontSettingsChanged()
{
    const QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DesktopFont);

    if (m_iconView && m_iconView->font() != font) {
        m_iconView->setFont(font);
        m_iconView->setGridSize(gridSize());
    }

    if (m_label && m_label->font() != font) {
        m_label->setFont(font);
    }
}

void FolderView::iconSettingsChanged(int group)
{
    if (group == KIconLoader::Desktop && m_iconView)
    {
        const int size = (m_customIconSize != 0) ?
                m_customIconSize : KIconLoader::global()->currentSize(KIconLoader::Desktop);

        m_iconView->setIconSize(QSize(size, size));
    }
    else if (group == KIconLoader::Panel && m_listView)
    {
        const int size = (m_customIconSize != 0) ?
                m_customIconSize : KIconLoader::global()->currentSize(KIconLoader::Panel);

        m_listView->setIconSize(QSize(size, size));
    }
}

void FolderView::themeChanged()
{
    if (m_textColor != Qt::transparent) {
        return;
    }

    if (m_iconView) {
        QPalette palette = m_iconView->palette();
        palette.setColor(QPalette::Text, Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
        m_iconView->setPalette(palette);
    }

    if (m_listView) {
        updateListViewState();
    }

    if (m_label) {
        QPalette palette = m_label->palette();
        palette.setColor(QPalette::Text, Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
        m_label->setPalette(palette);
    }
}

void FolderView::networkStatusChanged(Solid::Networking::Status status)
{
    if (status == Solid::Networking::Connected && !m_url.isLocalFile() &&
        m_url.protocol() != "desktop") {
        refreshIcons();
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

void FolderView::saveIconPositions() const
{
    if (!m_iconView) {
        return;
    }

    const QStringList data = m_iconView->iconPositionsData();
    if (!data.isEmpty()) {
        config().writeEntry("savedPositions", data);
    } else {
        config().deleteEntry("savedPositions");
    }
}

void FolderView::paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect &contentRect)
{
    Q_UNUSED(painter)
    Q_UNUSED(option)
    Q_UNUSED(contentRect)
}

void FolderView::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints & Plasma::FormFactorConstraint) {
        if (isContainment()) {
            setBackgroundHints(Applet::NoBackground);
        } else if (formFactor() == Plasma::Planar || formFactor() == Plasma::MediaCenter) {
            setBackgroundHints(Applet::TranslucentBackground);
        }

        if (formFactor() == Plasma::Planar || formFactor() == Plasma::MediaCenter) {
            // Clean up the icon widget
            if (m_iconWidget) {
                disconnect(m_dirModel->dirLister(), SIGNAL(newItems(KFileItemList)), this, SLOT(updateIconWidget()));
                disconnect(m_dirModel->dirLister(), SIGNAL(itemsDeleted(KFileItemList)), this, SLOT(updateIconWidget()));
                disconnect(m_dirModel->dirLister(), SIGNAL(clear()), this, SLOT(updateIconWidget()));
            }
            delete m_iconWidget;
            delete m_dialog;
            m_iconWidget = 0;
            m_dialog = 0;
            m_listView = 0;

            if (!isContainment()) {
                // Give the applet a sane size
                setupIconView();
            }
            setAspectRatioMode(Plasma::IgnoreAspectRatio);
        } else {
            // Clean up the icon view
            delete m_label;
            delete m_iconView;
            m_label = 0;
            m_iconView = 0;

            // Set up the icon widget
            m_iconWidget = new IconWidget(this);
            m_iconWidget->setModel(m_dirModel);
            m_iconWidget->setIcon(m_icon.isNull() ? KIcon("user-folder") : m_icon);
            connect(m_iconWidget, SIGNAL(clicked()), SLOT(iconWidgetClicked()));

            updateIconWidget();

            // We need to update the tooltip (and maybe the icon) when the contents of the folder changes
            connect(m_dirModel->dirLister(), SIGNAL(newItems(KFileItemList)), SLOT(updateIconWidget()));
            connect(m_dirModel->dirLister(), SIGNAL(itemsDeleted(KFileItemList)), SLOT(updateIconWidget()));
            connect(m_dirModel->dirLister(), SIGNAL(clear()), SLOT(updateIconWidget()));

            m_listView = new ListView;
            m_listView->setItemDelegate(m_delegate);
            m_listView->setModel(m_model);
            m_listView->setSelectionModel(m_selectionModel);

            // Add widget specific actions with shortcuts to the view
            addActions(m_listView);

            connect(m_listView, SIGNAL(activated(QModelIndex)), SLOT(activated(QModelIndex)));
            connect(m_listView, SIGNAL(contextMenuRequest(QWidget*,QPoint)), SLOT(contextMenuRequest(QWidget*,QPoint)));

            FolderViewAdapter *adapter = new FolderViewAdapter(m_listView);
            m_previewGenerator = new KFilePreviewGenerator(adapter, m_model);
            m_previewGenerator->setPreviewShown(m_showPreviews);
            m_previewGenerator->setEnabledPlugins(m_previewPlugins);

            updateListViewState();

            m_dialog = new Dialog;
            m_dialog->setGraphicsWidget(m_listView); // Ownership is transferred to the scene in the dialog

            QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical, this);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            layout->addItem(m_iconWidget);

            setLayout(layout);
            setAspectRatioMode(Plasma::ConstrainedSquare);
        }
    }

    if (constraints & Plasma::ImmutableConstraint) {
        // We need to update the menu items that have already been created
        if (QAction *addPanel = m_actionCollection.action("add panel")) {
            addPanel->setVisible(immutability() == Plasma::Mutable);
        }
    }

    if (constraints & Plasma::ScreenConstraint) {
        Plasma::Corona *c = corona();
        disconnect(c, SIGNAL(availableScreenRegionChanged()), this, SLOT(updateScreenRegion()));
        if (isContainment()) {
            if (screen() >= 0) {
                updateScreenRegion();
                connect(c, SIGNAL(availableScreenRegionChanged()), this, SLOT(updateScreenRegion()));
            }
        }
    }
}

void FolderView::updateScreenRegion()
{
    Plasma::Corona *c = corona();

    if (!c) {
        return;
    }

    QRect availRect;
    const QRect screenRect = c->screenGeometry(screen());
    // we pick the biggest rect from the available screen region; all the screen used bits
    // are on the edges, which means that the "middle" part which is free of panels and other
    // such strut-claimers will aways be the biggest rect barring some really messed
    // up configuration - aseigo
    foreach (const QRect &rect, c->availableScreenRegion(screen()).rects()) {
        if (rect.width() * rect.height() > availRect.width() * availRect.height()) {
            availRect = rect;
        }
    }

    //kDebug() << c->availableScreenRegion(screen()).rects();
    m_iconView->setContentsMargins(availRect.x() - screenRect.x(),
                                   availRect.y() - screenRect.y(),
                                   screenRect.right() - availRect.right(),
                                   screenRect.bottom() - availRect.bottom());
}

void FolderView::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (isContainment()) {
        if (event->widget()->window()->inherits("DashboardView")) {
            emit releaseVisualFocus();
        }
#ifdef HAVE_KWORKSPACE
        else if (event->button() == Qt::MidButton) {
            if (!m_windowListMenu) {
                m_windowListMenu = new KWindowListMenu;
                connect(m_windowListMenu, SIGNAL(aboutToShow()), SLOT(aboutToShowWindowList()));
            }
            m_windowListMenu->exec(event->screenPos());
        }
#endif
        return;
    }

    Containment::mousePressEvent(event);
}

void FolderView::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    const QString appletMimeType = static_cast<Plasma::Corona*>(scene())->appletMimeType();
    event->setAccepted(isContainment() && immutability() == Plasma::Mutable &&
                       event->mimeData()->hasFormat(appletMimeType));
}

void FolderView::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    const QString appletMimeType = static_cast<Plasma::Corona*>(scene())->appletMimeType();
    event->setAccepted(isContainment() && event->mimeData()->hasFormat(appletMimeType));
}

void FolderView::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    const QString appletMimeType = static_cast<Plasma::Corona*>(scene())->appletMimeType();
    if (isContainment() && event->mimeData()->hasFormat(appletMimeType)) {
        Containment::dropEvent(event);
    }
}

void FolderView::setUrl(const KUrl &url)
{
    m_url = url;

    // Only parse desktop files when sorting if we're showing the desktop folder
    m_model->setParseDesktopFiles(m_url.protocol() == "desktop");

    if (!m_customLabel.isEmpty()) {
        m_titleText = m_customLabel;
    } else if (m_url == KUrl("desktop:/")) {
        m_titleText = i18n("Desktop Folder");
    } else {
        m_titleText = m_url.pathOrUrl();

        if (!m_placesModel) {
            m_placesModel = new KFilePlacesModel(this);
        }
        const QModelIndex index = m_placesModel->closestItem(url);
        if (index.isValid()) {
            m_titleText = m_titleText.right(m_titleText.length() - m_placesModel->url(index).pathOrUrl().length());

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

            m_titleText.prepend(m_placesModel->text(index));
        }
    }

    if (m_label) {
        m_label->setText(m_titleText);
    }

    updateIconWidget();
}

void FolderView::createActions()
{
    KIO::FileUndoManager *manager = KIO::FileUndoManager::self();

    // Remove the Shift+Delete shortcut from the cut action, since it's used for deleting files
    KAction *cut = KStandardAction::cut(this, SLOT(cut()), this);
    KShortcut cutShortCut = cut->shortcut();
    cutShortCut.remove(Qt::SHIFT + Qt::Key_Delete);
    cut->setShortcut(cutShortCut);
    cut->setShortcutContext(Qt::WidgetShortcut);

    KAction *copy = KStandardAction::copy(this, SLOT(copy()), this);
    copy->setShortcutContext(Qt::WidgetShortcut);

    KAction *undo = KStandardAction::undo(manager, SLOT(undo()), this);
    undo->setEnabled(manager->undoAvailable());
    undo->setShortcutContext(Qt::WidgetShortcut);
    connect(manager, SIGNAL(undoAvailable(bool)), undo, SLOT(setEnabled(bool)));
    connect(manager, SIGNAL(undoTextChanged(QString)), SLOT(undoTextChanged(QString)));

    KAction *paste = KStandardAction::paste(this, SLOT(paste()), this);
    paste->setShortcutContext(Qt::WidgetShortcut);

    QString actionText = KIO::pasteActionText();
    if (!actionText.isEmpty()) {
       paste->setText(actionText);
    } else {
       paste->setEnabled(false);
    }

    KAction *pasteTo = KStandardAction::paste(this, SLOT(pasteTo()), this);
    pasteTo->setEnabled(false); // Only enabled during popupMenu()
    pasteTo->setShortcutContext(Qt::WidgetShortcut);

    KAction *reload = new KAction(i18n("&Reload"), this);
    connect(reload, SIGNAL(triggered()), SLOT(refreshIcons()));

    KAction *refresh = new KAction(isContainment() ? i18n("&Refresh Desktop") : i18n("&Refresh View"), this);
    refresh->setShortcut(KStandardShortcut::reload());
    refresh->setShortcutContext(Qt::WidgetShortcut);
    if (isContainment()) {
        refresh->setIcon(KIcon("user-desktop"));
    }
    connect(refresh, SIGNAL(triggered()), SLOT(refreshIcons()));

    KAction *rename = new KAction(KIcon("edit-rename"), i18n("&Rename"), this);
    rename->setShortcut(Qt::Key_F2);
    rename->setShortcutContext(Qt::WidgetShortcut);
    connect(rename, SIGNAL(triggered()), SLOT(renameSelectedIcon()));

    KAction *trash = new KAction(KIcon("user-trash"), i18n("&Move to Trash"), this);
    trash->setShortcut(Qt::Key_Delete);
    trash->setShortcutContext(Qt::WidgetShortcut);
    connect(trash, SIGNAL(triggered(Qt::MouseButtons, Qt::KeyboardModifiers)),
            SLOT(moveToTrash(Qt::MouseButtons, Qt::KeyboardModifiers)));

    KAction *del = new KAction(i18n("&Delete"), this);
    del->setIcon(KIcon("edit-delete"));
    del->setShortcut(Qt::SHIFT + Qt::Key_Delete);
    del->setShortcutContext(Qt::WidgetShortcut);
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

    QAction *addPanelAction = new QAction(i18n("Add Panel"), this);
    addPanelAction->setIcon(KIcon("list-add"));
    addPanelAction->setVisible(immutability() == Plasma::Mutable);

    KPluginInfo::List panelPlugins = listContainmentsOfType("panel");
    if (panelPlugins.size() == 1) {
        connect(addPanelAction, SIGNAL(triggered(bool)), this, SLOT(addPanel()));
    } else if (!panelPlugins.isEmpty()) {
        QSignalMapper *mapper = new QSignalMapper(this);
        connect(mapper, SIGNAL(mapped(QString)), SLOT(addPanel(QString)));
 
        QMenu *menu = new QMenu();
        foreach (const KPluginInfo &plugin, panelPlugins) {
            QAction *action = new QAction(plugin.name(), this);
            if (!plugin.icon().isEmpty()) {
                action->setIcon(KIcon(plugin.icon()));
            }
            mapper->setMapping(action, plugin.pluginName());
            connect(action, SIGNAL(triggered(bool)), mapper, SLOT(map()));
            menu->addAction(action);
        }
        addPanelAction->setMenu(menu);
    }

    QAction *runCommandAction = new QAction(i18n("Run Command..."), this);
    connect(runCommandAction, SIGNAL(triggered(bool)), this, SLOT(runCommand()));
    runCommandAction->setIcon(KIcon("system-run"));

    QAction *lockScreenAction = new QAction(i18n("Lock Screen"), this);
    lockScreenAction->setIcon(KIcon("system-lock-screen"));
    connect(lockScreenAction, SIGNAL(triggered(bool)), this, SLOT(lockScreen()));

    QAction *logoutAction = new QAction(i18n("Leave..."), this);
    logoutAction->setIcon(KIcon("system-shutdown"));
    connect(logoutAction, SIGNAL(triggered(bool)), this, SLOT(logout()));

    m_actionCollection.addAction("add panel", addPanelAction);
    m_actionCollection.addAction("run command", runCommandAction);
    m_actionCollection.addAction("lock screen", lockScreenAction);
    m_actionCollection.addAction("logout", logoutAction);

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
        sortByName->setData(int(KDirModel::Name));

        sortBySize->setCheckable(true);
        sortBySize->setData(int(KDirModel::Size));

        sortByType->setCheckable(true);
        sortByType->setData(int(KDirModel::Type));

        sortByDate->setCheckable(true);
        sortByDate->setData(int(KDirModel::ModifiedTime));

        KAction *dirsFirst = new KAction(i18nc("Sort icons", "Folders First"), this);
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
}

QList<QAction*> FolderView::contextualActions()
{
    QList<QAction*> actions;

    KFileItem rootItem = m_model->itemForIndex(QModelIndex());
    if (KAuthorized::authorize("action/kdesktop_rmb") && !rootItem.isNull())
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

        if (m_iconView) {
            if (QAction *iconsMenu = m_actionCollection.action("icons_menu")) {
                actions.append(iconsMenu);
            }
        }

        actions.append(m_actionCollection.action("refresh"));

        // Add an action for opening the folder in the preferred application.
        if (!m_itemActions) {
            // Create a new KFileItem to prevent the target URL in the root item
            // from being used. In this case we want the configured URL instead.
            KFileItem item(rootItem.mode(), rootItem.permissions(), m_url);

            KFileItemListProperties itemList(KFileItemList() << item);

            m_itemActions = new KFileItemActions(this);
            m_itemActions->setItemListProperties(itemList);
        }
        actions.append(m_itemActions->preferredOpenWithAction(QString()));

        separator = new QAction(this);
        separator->setSeparator(true);
        actions.append(separator);
    }

    if (isContainment()) {
        actions.append(action("add widgets"));
        actions.append(m_actionCollection.action("add panel"));
        if (KAuthorized::authorizeKAction("run_command")) {
            actions.append(m_actionCollection.action("run command"));
        }
        if (screen() == -1) {
            actions.append(action("remove"));
        }   
        actions.append(action("lock widgets"));

        QAction *separator = new QAction(this);
        separator->setSeparator(true);
        actions.append(separator);
 
        if (KAuthorized::authorizeKAction("lock_screen")) {
            actions.append(m_actionCollection.action("lock screen"));
        }
        if (KAuthorized::authorizeKAction("logout")) {
            actions.append(m_actionCollection.action("logout"));
        }

        separator = new QAction(this);
        separator->setSeparator(true);
        actions.append(separator);
    } 

    return actions;
}

void FolderView::addPanel()
{
    const KPluginInfo::List panelPlugins = listContainmentsOfType("panel");

    if (!panelPlugins.isEmpty()) {
        addPanel(panelPlugins.first().pluginName());
    }
}

void FolderView::addPanel(const QString &plugin)
{
    if (corona()) {
        // make a panel at the top
        Containment* panel = corona()->addContainment(plugin);
        panel->showConfigurationInterface();

        panel->setScreen(screen());

        QList<Plasma::Location> freeEdges = corona()->freeEdges(screen());
        //kDebug() << freeEdges;
        Plasma::Location destination;
        if (freeEdges.contains(Plasma::TopEdge)) {
            destination = Plasma::TopEdge;
        } else if (freeEdges.contains(Plasma::BottomEdge)) {
            destination = Plasma::BottomEdge;
        } else if (freeEdges.contains(Plasma::LeftEdge)) {
            destination = Plasma::LeftEdge;
        } else if (freeEdges.contains(Plasma::RightEdge)) {
            destination = Plasma::RightEdge;
        } else destination = Plasma::TopEdge;

        panel->setLocation(destination);

        // trigger an instant layout so we immediately have a proper geometry
        // rather than waiting around for the event loop
        panel->updateConstraints(Plasma::StartupCompletedConstraint);
        panel->flushPendingConstraintsEvents();

        const QRect screenGeom = corona()->screenGeometry(screen());
        const QRegion availGeom = corona()->availableScreenRegion(screen());
        int minH = 10;
        int minW = 10;
        int w = 35;
        int h = 35;

        if (destination == Plasma::LeftEdge) {
            QRect r = availGeom.intersected(QRect(0, 0, w, screenGeom.height())).boundingRect();
            h = r.height();
            minW = 35;
        } else if (destination == Plasma::RightEdge) {
            QRect r = availGeom.intersected(QRect(screenGeom.width() - w, 0, w, screenGeom.height())).boundingRect();
            h = r.height();
            minW = 35;
        } else if (destination == Plasma::TopEdge) {
            QRect r = availGeom.intersected(QRect(0, 0, screenGeom.width(), h)).boundingRect();
            w = r.width();
            minH = 35;
        } else if (destination == Plasma::BottomEdge) {
            QRect r = availGeom.intersected(QRect(0, screenGeom.height() - h, screenGeom.width(), h)).boundingRect();
            w = r.width();
            minH = 35;
        }

        panel->setMinimumSize(minW, minH);
        panel->setMaximumSize(w, h);
        panel->resize(w, h);
    }
}

void FolderView::runCommand()
{
    if (!KAuthorized::authorizeKAction("run_command")) {
        return;
    }

    QDBusInterface krunner("org.kde.krunner", "/App", "org.kde.krunner.App", QDBusConnection::sessionBus());
    krunner.call("display");
}

void FolderView::lockScreen()
{
    if (!KAuthorized::authorizeKAction("lock_screen")) {
        return;
    }

#ifndef Q_OS_WIN
    QDBusInterface screensaver("org.freedesktop.ScreenSaver", "/ScreenSaver",
                               "org.freedesktop.ScreenSaver", QDBusConnection::sessionBus());
    if (screensaver.isValid()) {
        screensaver.call("Lock");
    }
#else
    LockWorkStation();
#endif
}

void FolderView::logout()
{
    if (!KAuthorized::authorizeKAction("logout")) {
        return;
    }

#ifndef Q_WS_WIN
    QApplication::syncX();
    static_cast<KApplication*>(QApplication::instance())->updateRemoteUserTimestamp("org.kde.ksmserver");
    QDBusInterface ksmserver("org.kde.ksmserver", "/KSMServer", "org.kde.KSMServerInterface", QDBusConnection::sessionBus());
    ksmserver.call("logout", -1, -1, -1);
#endif
}

void FolderView::aboutToShowCreateNew()
{
    if (m_newMenu) {
        m_newMenu->slotCheckUpToDate();
        m_newMenu->setPopupFiles(m_url);
    }
}

void FolderView::aboutToShowWindowList()
{
#ifdef HAVE_KWORKSPACE
    if (m_windowListMenu) {
        m_windowListMenu->init();
    }
#endif
}

KUrl::List FolderView::selectedUrls(bool forTrash) const
{
    KUrl::List urls;
    foreach (const QModelIndex &index, m_selectionModel->selectedIndexes())
    {
        KFileItem item = m_model->itemForIndex(index);

        if (forTrash) {
            // Prefer the local URL if there is one, since we can't trash remote URL's
            const QString path = item.localPath();
            if (!path.isEmpty()) {
                urls.append(path);
            } else {
                urls.append(item.url());
            }
        } else {
            urls.append(item.url());
        }
    }
    return urls;
}

void FolderView::copy()
{
    QMimeData *mimeData = m_model->mimeData(m_selectionModel->selectedIndexes());
    QApplication::clipboard()->setMimeData(mimeData);
}

void FolderView::cut()
{
    QMimeData *mimeData = m_model->mimeData(m_selectionModel->selectedIndexes());
    KonqMimeData::addIsCutSelection(mimeData, true);
    QApplication::clipboard()->setMimeData(mimeData);
}

void FolderView::paste()
{
    KonqOperations::doPaste(view(), m_url);
}

void FolderView::pasteTo()
{
    const KUrl::List urls = selectedUrls(false);
    Q_ASSERT(urls.count() == 1);
    KonqOperations::doPaste(view(), urls.first());
}

void FolderView::refreshIcons()
{
    m_dirModel->dirLister()->updateDirectory(m_url);
}

void FolderView::toggleIconsLocked(bool locked)
{
    m_iconsLocked = locked;

    if (m_iconView) {
        m_iconView->setIconsMoveable(!locked);
    }

    config().writeEntry("iconsLocked", locked);
    emit configNeedsSaving();
}

void FolderView::toggleAlignToGrid(bool align)
{
    m_alignToGrid = align;

    if (m_iconView) {
        m_iconView->setAlignToGrid(align);
    }

    config().writeEntry("alignToGrid", align);
    emit configNeedsSaving();

    m_delayedSaveTimer.start(5000, this);
}

void FolderView::toggleDirectoriesFirst(bool enable)
{
    m_sortDirsFirst = enable;

    m_model->setSortDirectoriesFirst(m_sortDirsFirst);
    if (m_sortColumn != -1) {
        m_model->invalidate();
        m_delayedSaveTimer.start(5000, this);
    }

    config().writeEntry("sortDirsFirst", m_sortDirsFirst);
    emit configNeedsSaving();
}

void FolderView::sortingChanged(QAction *action)
{
    const int column = action->data().toInt();

    if (column != m_sortColumn) {
        m_model->invalidate();
        m_model->sort(column, Qt::AscendingOrder);
        m_sortColumn = column;
        config().writeEntry("sortColumn", m_sortColumn);
        emit configNeedsSaving();
        m_delayedSaveTimer.start(5000, this);
    }
}

void FolderView::updateSortActionsState()
{
    foreach (QAction *action, m_sortingGroup->actions()) {
        action->setChecked(action->data() == int(m_sortColumn));
    }
}


void FolderView::filterChanged(int index)
{
    uiFilter.filterFilesPattern->setEnabled(index != 0);
    uiFilter.searchMimetype->setEnabled(index != 0);
    uiFilter.filterFilesList->setEnabled(index != 0);
    uiFilter.selectAll->setEnabled(index != 0);
    uiFilter.deselectAll->setEnabled(index != 0);
    if ((index != 0) && (m_userSelectedShowAllFiles == 0)) {
      for (int i = 0; i < uiFilter.filterFilesList->model()->rowCount(); i++) {
        const QModelIndex index = uiFilter.filterFilesList->model()->index(i, 0);
        uiFilter.filterFilesList->model()->setData(index, Qt::Checked, Qt::CheckStateRole);
      }
    }
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
    if (m_iconView && m_iconView->renameInProgress()) {
        return;
    }

    KonqOperations::Operation op = (modifiers & Qt::ShiftModifier) ?
            KonqOperations::DEL : KonqOperations::TRASH;

    KonqOperations::del(view(), op, selectedUrls(op == KonqOperations::TRASH));
}

void FolderView::deleteSelectedIcons()
{
    if (m_iconView && m_iconView->renameInProgress()) {
        return;
    }

    KonqOperations::del(view(), KonqOperations::DEL, selectedUrls(false));
}

void FolderView::renameSelectedIcon()
{
    if (m_iconView) {
        m_iconView->renameSelectedIcon();
    }
}

void FolderView::undoTextChanged(const QString &text)
{
    if (QAction *action = m_actionCollection.action("undo")) {
        action->setText(text);
    }
}

void FolderView::activated(const QModelIndex &index)
{
    const KFileItem item = m_model->itemForIndex(index);
    item.run();

    if (m_dialog && m_dialog->isVisible()) {
        m_dialog->hide();
    }

    emit releaseVisualFocus();
}

void FolderView::indexesMoved(const QModelIndexList &indexes)
{
    Q_UNUSED(indexes)

    // If the user has rearranged the icons, the view is no longer sorted
    if (m_sortColumn != -1) {
        m_sortColumn = -1;
        updateSortActionsState();
        config().writeEntry("sortColumn", m_sortColumn);
        emit configNeedsSaving();
    }

    m_delayedSaveTimer.start(5000, this);
}

void FolderView::contextMenuRequest(QWidget *widget, const QPoint &screenPos)
{
    showContextMenu(widget, screenPos, m_selectionModel->selectedIndexes());
}

void FolderView::showContextMenu(QWidget *widget, const QPoint &pos, const QModelIndexList &indexes)
{
    if (!KAuthorized::authorize("action/kdesktop_rmb") || indexes.isEmpty()) {
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
    flags |= KParts::BrowserExtension::ShowUrlOperations;

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

void FolderView::updateIconWidget()
{
    if (!m_iconWidget) {
        return;
    }

    if (!m_placesModel) {
        m_placesModel = new KFilePlacesModel(this);
    }

    const QModelIndex index = m_placesModel->closestItem(m_url);

    // TODO: Custom icon

    KFileItem item = m_dirModel->itemForIndex(QModelIndex());
    if (!item.isNull() && item.iconName() != "inode-directory") {
        m_icon = KIcon(item.iconName(), 0, item.overlays());
    } else if (m_url.protocol() == "desktop") {
        m_icon = KIcon("user-desktop");
    } else if (m_url.protocol() == "trash") {
        m_icon = m_model->rowCount() > 0 ? KIcon("user-trash-full") : KIcon("user-trash");
    } else if (index.isValid()) {
        m_icon = m_placesModel->icon(index);
    } else {
        m_icon = KIcon("user-folder");
    }

    m_iconWidget->setIcon(m_icon);
    m_iconWidget->update();

    int nFolders = 0;
    int nFiles = 0;
    for (int row = 0; row < m_model->rowCount(); row++) {
        const QModelIndex index = m_model->index(row, 0);
        KFileItem item = m_model->itemForIndex(index);
        if (item.isDir()) {
            nFolders++;
        } else {
            nFiles++;
        }
    }

    const QString str1 = i18ncp("Inserted as %1 in the message below.", "1 folder", "%1 folders", nFolders);
    const QString str2 = i18ncp("Inserted as %2 in the message below.", "1 file", "%1 files", nFiles);

    QString subText;
    if (nFolders > 0) {
        subText = i18nc("%1 and %2 are the messages translated above.", "%1, %2.", str1, str2);
    } else {
        subText = i18np("1 file.", "%1 files.", nFiles);
    }

    // Update the tooltip
    Plasma::ToolTipContent data;
    data.setMainText(m_titleText);
    data.setSubText(subText);
    data.setImage(m_icon);
    Plasma::ToolTipManager::self()->setContent(m_iconWidget, data);
}

void FolderView::iconWidgetClicked()
{
    if (m_dialog->isVisible()) {
        m_dialog->hide();
    } else {
        m_dialog->show(this);
    }
}

QSize FolderView::iconSize() const
{
    const int defaultSize = KIconLoader::global()->currentSize(m_listView ? KIconLoader::Panel : KIconLoader::Desktop);
    const int size = (m_customIconSize != 0) ? m_customIconSize : defaultSize;
    return QSize(size, size);
}

QSize FolderView::gridSize() const
{
    const QFontMetrics fm(Plasma::Theme::defaultTheme()->font(Plasma::Theme::DesktopFont));
    const int textHeight = fm.lineSpacing() * m_numTextLines;
    QSize size = iconSize();
    size.rheight() = size.height() + textHeight + 16;
    size.rwidth() = qMax(size.width() * 2, fm.averageCharWidth() * 10);
    return size;
}

void FolderView::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_delayedSaveTimer.timerId()) {
        m_delayedSaveTimer.stop();
        saveIconPositions();
        emit configNeedsSaving();
    }

    Containment::timerEvent(event);
}

#include "folderview.moc"
