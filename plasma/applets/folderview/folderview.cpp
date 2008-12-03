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
#include <QDesktopWidget>
#include <QDrag>
#include <QGraphicsLinearLayout>
#include <QGraphicsView>
#include <QGraphicsSceneDragDropEvent>
#include <QItemSelectionModel>
#include <QScrollBar>

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

#include <limits.h>

#include "plasma/corona.h"
#include "plasma/paintutils.h"
#include "plasma/theme.h"
#include "plasma/tooltipmanager.h"

#include "dirlister.h"
#include "dialog.h"
#include "folderviewadapter.h"
#include "iconview.h"
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
      m_placesModel(0),
      m_iconView(0),
      m_listView(0),
      m_label(0),
      m_iconWidget(0),
      m_dialog(0),
      m_newMenu(0),
      m_actionCollection(this)
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

    m_delegate = new KFileItemDelegate(this);
    m_selectionModel = new QItemSelectionModel(m_model, this);

    if (args.count() > 0) {
        setUrl(KUrl(args.value(0).toString()));
    }

    // As we use some part of konqueror libkonq must be added to have translations
    KGlobal::locale()->insertCatalog("libkonq");
}

void FolderView::init()
{
    Containment::init();

    // Find out about icon and font settings changes
    connect(KGlobalSettings::self(), SIGNAL(kdisplayFontChanged()), SLOT(fontSettingsChanged()));
    connect(KGlobalSettings::self(), SIGNAL(iconChanged(int)), SLOT(iconSettingsChanged(int)));

    // Find out about theme changes
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), SLOT(themeChanged()));

    KConfigGroup cg = config();
    m_customLabel         = cg.readEntry("customLabel", "");
    m_customIconSize      = cg.readEntry("customIconSize", 0);
    m_showPreviews        = cg.readEntry("showPreviews", true);
    m_drawShadows         = cg.readEntry("drawShadows", true);
    m_numTextLines        = cg.readEntry("numTextLines", 2);
    m_textColor           = cg.readEntry("textColor", QColor(Qt::transparent));
    m_iconsLocked         = cg.readEntry("iconsLocked", false);
    m_alignToGrid         = cg.readEntry("alignToGrid", false);
    m_previewPlugins      = cg.readEntry("previewPlugins", QStringList() << "imagethumbnail");
    m_sortDirsFirst       = cg.readEntry("sortDirsFirst", true);
    m_sortColumn          = cg.readEntry("sortColumn", int(KDirModel::Name));
    m_filterFiles         = cg.readEntry("filterFiles", "*");
    m_filterType          = cg.readEntry("filter", 0);
    m_filterFilesMimeList = cg.readEntry("mimeFilter", QStringList());

    m_flow = isContainment() ? QListView::TopToBottom : QListView::LeftToRight;
    m_flow = static_cast<QListView::Flow>(cg.readEntry("flow", static_cast<int>(m_flow)));

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
        setUrl(cg.readEntry("url", KUrl(isContainment() ? QString("desktop:/") : QDir::homePath())));
    } else {
        KConfigGroup cg = config();
        cg.writeEntry("url", m_url);
    }

    lister->openUrl(m_url);

    if (isContainment()) {
        setupIconView();
    }

    createActions();

    if (isContainment()) {

        // Set a low Z value so applets don't end up below the icon view
        m_iconView->setZValue(INT_MIN);
    }

    connect(QApplication::clipboard(), SIGNAL(dataChanged()), SLOT(clipboardDataChanged()));
}

FolderView::~FolderView()
{
    delete m_newMenu;
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

    if (m_url == KUrl("desktop:/")) {
        uiLocation.showDesktopFolder->setChecked(true);
        uiLocation.placesCombo->setEnabled(false);
        uiLocation.lineEdit->setEnabled(false);
    } else {
        QModelIndex index;
        for (int i = 0; i < placesFilter->rowCount(); i++) {
            KUrl url = m_placesModel->url(placesFilter->mapToSource(placesFilter->index(i, 0)));
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
    int size = iconSizes.at(uiDisplay.sizeSlider->value());
    if ((m_customIconSize == 0 && size != KIconLoader::global()->currentSize(KIconLoader::Desktop)) ||
        (m_customIconSize != 0 && size != m_customIconSize))
    {
        m_customIconSize = size;
        cg.writeEntry("customIconSize", m_customIconSize);

        // This is to force the preview images to be regenerated with the new size
        if (m_showPreviews) {
            needReload = true;
        }
    }

    int sortColumn = uiDisplay.sortCombo->itemData(uiDisplay.sortCombo->currentIndex()).toInt();
    if (m_sortColumn != sortColumn) {
        m_sortColumn = sortColumn;
        if (m_sortColumn != -1) {
            m_model->invalidate();
            m_model->sort(m_sortColumn, Qt::AscendingOrder);
        }
        updateSortActionsState();
        cg.writeEntry("sortColumn", m_sortColumn);
    }

    int flow = uiDisplay.flowCombo->itemData(uiDisplay.flowCombo->currentIndex()).toInt();
    if (m_flow != flow) {
        m_flow = static_cast<QListView::Flow>(flow);
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
    }

    m_delayedSaveTimer.start(5000, this);
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

    const QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DesktopFont);
    if (m_iconView->font() != font) {
        m_iconView->setFont(font);
    }
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

void FolderView::setupIconView()
{
    if (m_iconView) {
        return;
    }

    m_iconView = new IconView(this);
    m_iconView->setModel(m_model);
    m_iconView->setItemDelegate(m_delegate);
    m_iconView->setSelectionModel(m_selectionModel);

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
    connect(m_iconView, SIGNAL(checkFolder()), SLOT(checkFolder()));

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
    }

    if (m_label && m_label->font() != font) {
        m_label->setFont(font);
    }
}

void FolderView::iconSettingsChanged(int group)
{
    if (group == KIconLoader::Desktop)
    {
        if (m_iconView) {
            const int size = (m_customIconSize != 0) ?
                    m_customIconSize : KIconLoader::global()->currentSize(KIconLoader::Desktop);

            m_iconView->setIconSize(QSize(size, size));
        }
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

    if (m_label) {
        QPalette palette = m_label->palette();
        palette.setColor(QPalette::Text, Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
        m_label->setPalette(palette);
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
            delete m_listView;
            m_iconWidget = 0;
            m_dialog = 0;
            m_listView = 0;
            if (!isContainment()) {
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
            m_iconWidget = new Plasma::IconWidget(this);
            m_iconWidget->setIcon(m_icon.isNull() ? KIcon("user-folder") : m_icon);
            connect(m_iconWidget, SIGNAL(clicked()), SLOT(iconWidgetClicked()));

            updateIconWidget();

            // We need to update the tooltip (and maybe the icon) when the contents of the folder changes
            connect(m_dirModel->dirLister(), SIGNAL(newItems(KFileItemList)), SLOT(updateIconWidget()));
            connect(m_dirModel->dirLister(), SIGNAL(itemsDeleted(KFileItemList)), SLOT(updateIconWidget()));
            connect(m_dirModel->dirLister(), SIGNAL(clear()), SLOT(updateIconWidget()));

            m_listView = new ListView;
            m_listView->setModel(m_model);
            m_listView->setItemDelegate(m_delegate);
            m_listView->setSelectionModel(m_selectionModel);

            connect(m_listView, SIGNAL(activated(QModelIndex)), SLOT(activated(QModelIndex)));
            connect(m_listView, SIGNAL(contextMenuRequest(QWidget*,QPoint)), SLOT(contextMenuRequest(QWidget*,QPoint)));

            updateListViewState();

            m_dialog = new Dialog;

            QGraphicsScene *scene = new QGraphicsScene(m_dialog);
            scene->addItem(m_listView);

            m_dialog->setGraphicsWidget(m_listView);

            QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical, this);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            layout->addItem(m_iconWidget);

            setLayout(layout);
            setAspectRatioMode(Plasma::ConstrainedSquare);
        }
    }

    if (constraints & Plasma::ScreenConstraint) {
        Plasma::Corona *c = corona();
        disconnect(c, SIGNAL(availableScreenRegionChanged()), this, SLOT(updateScreenRegion()));
        if (isContainment()) {
            if (screen() >= 0) {
                updateScreenRegion();
                connect(c, SIGNAL(availableScreenRegionChanged()), this, SLOT(updateScreenRegion()));
            } else {
                m_iconView->setContentsMargins(0, 0, 0, 0);
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

    QRect availRect = c->availableScreenRegion(screen()).boundingRect();
    QRect screenRect = c->screenGeometry(screen());
    m_iconView->setContentsMargins(availRect.x() - screenRect.x(),
                                   availRect.y() - screenRect.y(),
                                   availRect.right() - screenRect.right(),
                                   availRect.bottom() - screenRect.bottom());
}

void FolderView::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (isContainment() && event->widget()->window()->inherits("DashboardView")) {
        emit releaseVisualFocus();
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
    connect(rename, SIGNAL(triggered()), m_iconView, SLOT(renameSelectedIcon()));

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
    KUrl::List urls = selectedUrls();
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
    int column = action->data().toInt();

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

void FolderView::checkFolder()
{
    QDir dir(m_url.path());

    if (!dir.exists()) {
	if (isContainment()) {
	    setUrl(KUrl("desktop:/"));
	} else {
	    setUrl(KUrl(QDir::homePath()));
	}

        m_dirModel->dirLister()->openUrl(m_url);
        updateIconWidget();
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
    foreach (const KFileItem &item, m_dirModel->dirLister()->items()) {
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
    data.setImage(m_icon/*.pixmap(IconSize(KIconLoader::Desktop))*/);
    Plasma::ToolTipManager::self()->setContent(m_iconWidget, data);
}

void FolderView::iconWidgetClicked()
{
    if (m_dialog->isVisible()) {
        m_dialog->hide();
    } else {
        int left, top, right, bottom;
        m_dialog->getContentsMargins(&left, &top, &right, &bottom);

        const QRect rect = QApplication::desktop()->availableGeometry().adjusted(left, top, -right, -bottom);
        m_listView->resize(m_listView->preferredSize().boundedTo(rect.size()));

        m_dialog->resize(m_listView->size().toSize() + QSize(left + right, top + bottom));
        m_dialog->move(popupPosition(m_dialog->size()));
        m_dialog->show();
    }
}

QSize FolderView::iconSize() const
{
    const int size = (m_customIconSize != 0) ? m_customIconSize : KIconLoader::global()->currentSize(KIconLoader::Desktop);
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
