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
#include <QDesktopWidget>
#include <QGraphicsLinearLayout>
#include <QGraphicsView>
#include <QGraphicsSceneDragDropEvent>
#include <QImageReader>
#include <QItemSelectionModel>
#include <QCache>

#include <KAction>
#include <KAuthorized>
#include <KBookmarkManager>
#include <KConfigDialog>
#include <KDebug>
#include <KDesktopFile>
#include <KDirModel>
#include <KFileItemDelegate>
#include <KFilePreviewGenerator>
#include <KGlobalSettings>
#include <KProtocolInfo>
#include <KStandardShortcut>
#include <KMenu>
#include <KCompletionBox>

#include <kio/copyjob.h>
#include <kio/fileundomanager.h>
#include <kio/paste.h>

#include <kfileitemactions.h>
#include <kfileitemlistproperties.h>

#include <knewfilemenu.h>
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
#include <Plasma/Theme>
#include <Plasma/ToolTipManager>
#include <Plasma/Wallpaper>
#include <Plasma/WindowEffects>
#include <Plasma/Applet>

#include "proxymodel.h"
#include "dirlister.h"
#include "folderviewadapter.h"
#include "iconview.h"
#include "listview.h"
#include "dialog.h"
#include "iconwidget.h"
#include "label.h"
#include "settings/settings.h"
#include "settings/previewpluginsmodel.h"
#include "settings/mimemodel.h"
#include "settings/proxymimemodel.h"
#include "settings/placesfiltermodel.h"


K_EXPORT_PLASMA_APPLET(folderview, FolderView)

Q_DECLARE_METATYPE(Qt::SortOrder)
Q_DECLARE_METATYPE(IconView::Flow)
Q_DECLARE_METATYPE(ProxyModel::FilterMode)
Q_DECLARE_METATYPE(FolderView::LabelType)


// ---------------------------------------------------------------------------



FolderView::FolderView(QObject *parent, const QVariantList &args)
    : Plasma::Containment(parent, args),
      m_iconView(0),
      m_listView(0),
      m_label(0),
      m_previewGenerator(0),
      m_placesModel(0),
      m_iconWidget(0),
      m_dialog(0),
      m_newMenu(0),
      m_itemActions(0),
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

    // we are not calling setUrl here since m_dirLister does not exist
    if (args.count() > 0) {
        m_url = KUrl(args.value(0).toString());
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
    connect(KGlobalSettings::self(), SIGNAL(settingsChanged(int)), SLOT(clickSettingsChanged(int)));

    // Find out about theme changes
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), SLOT(plasmaThemeChanged()));

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
    m_clickToView         = cg.readEntry("clickForFolderPreviews", true);
    m_previewPlugins      = cg.readEntry("previewPlugins", QStringList() << "imagethumbnail" << "jpegthumbnail");
    m_sortDirsFirst       = cg.readEntry("sortDirsFirst", true);
    m_sortColumn          = cg.readEntry("sortColumn", int(KDirModel::Name));
    m_sortOrder           = Settings::sortOrderStringToEnum(cg.readEntry("sortOrder", "ascending"));
    m_filterFiles         = cg.readEntry("filterFiles", "*");
    m_filterType          = static_cast<ProxyModel::FilterMode>(cg.readEntry("filter", static_cast<int>(ProxyModel::NoFilter)));
    m_filterFilesMimeList = cg.readEntry("mimeFilter", QStringList());
//     m_blankLabel          = cg.readEntry("blankLabel", false);
    m_labelType           = static_cast<LabelType>(cg.readEntry("labelType", static_cast<int>(FolderView::None)));
    m_userSelectedShowAllFiles = m_filterType;
    m_showSelectionMarker = KGlobalSettings::singleClick();

    if (isContainment()) {
        m_flow = layoutDirection() == Qt::LeftToRight ? IconView::TopToBottom : IconView::TopToBottomRightToLeft;
    } else {
        m_flow = layoutDirection() == Qt::LeftToRight ? IconView::LeftToRight : IconView::RightToLeft;
    }
    m_flow = static_cast<IconView::Flow>(cg.readEntry("flow", static_cast<int>(m_flow)));

    m_model->setFilterMode(m_filterType);
    m_model->setMimeTypeFilterList(m_filterFilesMimeList);
    m_model->setFileNameFilter(m_filterFiles);
    m_model->setSortDirectoriesFirst(m_sortDirsFirst);
    m_model->setDynamicSortFilter(m_sortColumn != -1);
    m_model->sort(m_sortColumn != -1 ? m_sortColumn : KDirModel::Name, m_sortOrder);

    m_dirLister = new DirLister(this);
    m_dirLister->setDelayedMimeTypes(true);
    m_dirLister->setAutoErrorHandlingEnabled(false, 0);

    m_dirModel->setDirLister(m_dirLister);

    if (m_url.isValid()) {
        // this means that we were passed a URL via the args list in the constructor
        // we need to set it and save it in the config file
        setUrl(m_url);
        KConfigGroup cg = config();
        cg.writeEntry("url", m_url);
    } else {
        QString path = QDir::homePath();
        if (isContainment()) {
            const QString desktopPath = KGlobalSettings::desktopPath();
            const QDir desktopFolder(desktopPath);

            if (desktopPath != QDir::homePath() && desktopFolder.exists()) {
                path = QString("desktop:/");
            }
        }
        setUrl(cg.readEntry("url", KUrl(path)));
    }

    createActions();

    if (isContainment()) {
        setupIconView();

        // Set a low Z value so applets don't end up below the icon view
        m_iconView->setZValue(INT_MIN);
    }

    // Set the associated application
    KService::List offers = KFileItemActions::associatedApplications(QStringList() << "inode/directory", QString());
    if (!offers.isEmpty()) {
        setAssociatedApplication(offers.first()->exec());
        setAssociatedApplicationUrls(KUrl::List() << m_url);

        // contextualActions() includes KFileItemActions::preferredOpenWithAction(),
        // so we'll hide the one Plasma provides.
        if (QAction *runAction = action("run associated application")) {
            runAction->setVisible(false);
        }
    }

    /*
    TODO Mark the cut icons as cut
    connect(QApplication::clipboard(), SIGNAL(dataChanged()), SLOT(clipboardDataChanged()));
    */
}

void FolderView::networkAvailable()
{
    if (KProtocolInfo::protocolClass(m_url.protocol()) == ":local") {
        m_dirLister->openUrl(m_url);
    }
}

void FolderView::configChanged()
{
    KConfigGroup cg = config();

    //Declare some variables that are used afterwards
    bool needReload = false;
    bool preserveIconPositions = false;

    const LabelType labelType = static_cast<LabelType>(cg.readEntry("labelType", static_cast<int>(m_labelType)));
    if (labelType != m_labelType) {
        m_labelType = labelType;
        needReload = true;
    }

    //Reload m_customLabel values
    const QString label = cg.readEntry("customLabel", m_customLabel);
    if (label != m_customLabel) {
        m_customLabel = label;
        needReload = true;
    }

    //Reload m_customIconSize values
    const int size = m_customIconSize;
    m_customIconSize = cg.readEntry("customIconSize", m_customIconSize);
    if (size != iconSize().width()) {
        needReload = true;
    }

    m_drawShadows  = cg.readEntry("drawShadows", m_drawShadows);
    m_clickToView  = cg.readEntry("clickForFolderPreviews", m_clickToView);
    m_numTextLines = cg.readEntry("numTextLines", m_numTextLines);
    m_alignToGrid  = cg.readEntry("alignToGrid", m_alignToGrid);

    if (QAction *action = m_actionCollection.action("auto_align")) {
        action->setChecked(m_alignToGrid);
    }

    m_iconsLocked  = cg.readEntry("iconsLocked", m_iconsLocked);
    if (QAction *action = m_actionCollection.action("lock_icons")) {
        action->setChecked(m_iconsLocked);
    }

    const QColor color = cg.readEntry("textColor", m_textColor);
    if (color != m_textColor) {
        m_textColor = color;
        needReload = true;
    }

    const bool showPreviews = cg.readEntry("showPreviews", m_showPreviews);
    if (showPreviews != m_showPreviews) {
        m_showPreviews = showPreviews;

        //As disabling the previews will force a rearrangement, we need to manually
        //save and restore the icons positions

        //Enable/disable the previews
        m_previewGenerator->setPreviewShown(m_showPreviews);
        if (m_iconView)
            m_iconView->update(m_iconView->visibleArea());
        if (m_listView)
            m_listView->update(m_listView->visibleArea());
    }

    m_previewPlugins = cg.readEntry("previewPlugins", m_previewPlugins);

    if (m_previewGenerator && m_previewPlugins != m_previewGenerator->enabledPlugins()) {
        m_previewGenerator->setEnabledPlugins(m_previewPlugins);

        //Changing the preview plugins will also need a reload to work, so we need to preserve
        //the icons position
        needReload = true;
        preserveIconPositions = true;
    }

    const bool sortDirsFirst = cg.readEntry("sortDirsFirst", m_sortDirsFirst);
    if (sortDirsFirst != m_sortDirsFirst) {
        m_sortDirsFirst = sortDirsFirst;

        m_model->setSortDirectoriesFirst(m_sortDirsFirst);
        if (m_sortColumn != -1) {
            m_model->invalidate();
        }
    }

    const int sortColumn = cg.readEntry("sortColumn", m_sortColumn);
    const Qt::SortOrder sortOrder = Settings::sortOrderStringToEnum(cg.readEntry("sortOrder", Settings::sortOrderEnumToString(m_sortOrder)));
    if ((m_sortColumn != sortColumn) || (m_sortOrder != m_sortOrder)) {
        m_sortColumn = sortColumn;
        m_sortOrder = sortOrder;
        if (m_sortColumn != -1) {
            m_model->invalidate();
            m_model->sort(m_sortColumn, m_sortOrder);
            m_model->setDynamicSortFilter(true);
        } else if (m_iconView) {
            m_iconView->setCustomLayout(true);
            m_model->setDynamicSortFilter(false);
        }
        updateSortActionsState();
    }

    const int flow = static_cast<IconView::Flow>(cg.readEntry("flow", static_cast<int>(m_flow)));
    if (flow != m_flow) {
        m_flow = static_cast<IconView::Flow>(flow);
    }

    const ProxyModel::FilterMode filterType = static_cast<ProxyModel::FilterMode>(cg.readEntry("filter", static_cast<int>(m_filterType)));
    if (filterType != m_filterType) {
        m_filterType = filterType;
        m_model->setFilterMode(m_filterType);
        needReload = true;
    }

    m_userSelectedShowAllFiles = m_filterType;

    const QString filterFiles = cg.readEntry("filterFiles", m_filterFiles);
    if (filterFiles != m_filterFiles) {
        m_filterFiles = filterFiles;
        m_model->setFileNameFilter(m_filterFiles);
        needReload = true;
    }

    const QStringList mimeFilter = cg.readEntry("mimeFilter", m_filterFilesMimeList);
    if (mimeFilter != m_filterFilesMimeList) {
        m_filterFilesMimeList = mimeFilter;
        m_model->setMimeTypeFilterList(m_filterFilesMimeList);
        needReload = true;
    }

    const KUrl url = cg.readEntry("url", m_url);
    if (url != m_url) {
        m_url = url;
        needReload = true;
    }

    if (m_iconView) {
        updateIconViewState();
    }

    if (m_listView) {
        updateListViewState();
    }

    if (needReload) {
        //Manually save and restore the icon positions if we need it
        if (preserveIconPositions && m_iconView) {
            m_iconView->setIconPositionsData(m_iconView->iconPositionsData());
        }

        // So the KFileItemActions will be recreated for the new URL.
        delete m_itemActions;
        m_itemActions = 0;

        setUrl(m_url);
    }
}

FolderView::~FolderView()
{
    if (m_dialog)
        delete m_dialog;
    delete m_newMenu;
}

void FolderView::saveState(KConfigGroup &config) const
{
    Containment::saveState(config);
    saveIconPositions();
}

void FolderView::addUrls(const KUrl::List& urls)
{
    KIO::CopyJob *job;

    foreach (KUrl url, urls) {
        job = KIO::link(url.url(), m_url);
        KIO::FileUndoManager::self()->recordCopyJob(job);
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
    } else if (m_url == KUrl("activities:/current/")) {
        uiLocation.showActivity->setChecked(true);
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

    uiLocation.titleCombo->addItem(i18n("None"), FolderView::None);
    uiLocation.titleCombo->addItem(i18n("Default"), FolderView::PlaceName);
    uiLocation.titleCombo->addItem(i18n("Full Path"), FolderView::FullPath);
    uiLocation.titleCombo->addItem(i18n("Custom title"), FolderView::Custom);

    uiLocation.titleEdit->setEnabled(m_labelType == FolderView::Custom);

    if (m_labelType == FolderView::Custom) {
        uiLocation.titleEdit->setText(m_customLabel);
    }

    // The label is not shown when the applet is acting as a containment,
    // so don't confuse the user by making it editable.
    if (isContainment()) {
        uiLocation.titleLabel->hide();
        uiLocation.titleCombo->hide();
        uiLocation.titleEdit->hide();
    }

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

    uiFilter.filterCombo->addItem(i18n("Show All Files"), ProxyModel::NoFilter);
    uiFilter.filterCombo->addItem(i18n("Show Files Matching"), ProxyModel::FilterShowMatches);
    uiFilter.filterCombo->addItem(i18n("Hide Files Matching"), ProxyModel::FilterHideMatches);

    uiDisplay.alignToGrid->setChecked(m_alignToGrid);
    uiDisplay.clickToView->setChecked(m_clickToView);
    uiDisplay.lockInPlace->setChecked(m_iconsLocked);
    uiDisplay.drawShadows->setChecked(m_drawShadows);
    uiDisplay.showPreviews->setChecked(m_showPreviews);
    uiDisplay.previewsAdvanced->setEnabled(m_showPreviews);
    uiDisplay.numLinesEdit->setValue(m_numTextLines);
    uiDisplay.colorButton->setColor(textColor());

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

    for (int i = 0; i < uiLocation.titleCombo->maxCount(); i++) {
       if (m_labelType == uiLocation.titleCombo->itemData(i).toInt()) {
           uiLocation.titleCombo->setCurrentIndex(i);
           break;
       }
    }

    for (int i = 0; i < uiFilter.filterCombo->maxCount(); i++) {
       if (m_filterType == uiFilter.filterCombo->itemData(i).toInt()) {
           uiFilter.filterCombo->setCurrentIndex(i);
           break;
       }
    }

    filterChanged(m_filterType);

    if (m_filterFilesMimeList.count()) {
        for (int i = 0; i < pMimeModel->rowCount(); i++) {
            const QModelIndex index = pMimeModel->index(i, 0);
            const KMimeType *mime = static_cast<KMimeType*>(pMimeModel->mapToSource(index).internalPointer());
            if (mime && m_filterFilesMimeList.contains(mime->name())) {
                m_filterFilesMimeList.removeAll(mime->name());
                uiFilter.filterFilesList->model()->setData(index, Qt::Checked, Qt::CheckStateRole);
            }
        }
    }

    // Hide the icon arrangement controls when we're not acting as a containment,
    // since this option doesn't make much sense in the applet.
    if (!isContainment()) {
        uiDisplay.flowLabel->hide();
        uiDisplay.flowCombo->hide();
    }

    parent->addPage(widgetLocation, i18nc("Title of the page that lets the user choose which location should the folderview show", "Location"), "folder");
    parent->addPage(widgetDisplay, i18nc("Title of the page that lets the user choose how the folderview should be shown", "Display"), "preferences-desktop-display");
    parent->addPage(widgetFilter, i18nc("Title of the page that lets the user choose how to filter the folderview contents", "Filter"), "view-filter");

    connect(parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(configAccepted()));

    connect(uiFilter.searchMimetype, SIGNAL(textChanged(QString)), pMimeModel, SLOT(setFilter(QString)));
    connect(uiLocation.showPlace, SIGNAL(toggled(bool)), uiLocation.placesCombo, SLOT(setEnabled(bool)));
    connect(uiLocation.showCustomFolder, SIGNAL(toggled(bool)), uiLocation.lineEdit, SLOT(setEnabled(bool)));
    connect(uiLocation.titleCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setTitleEditEnabled(int)));
    connect(uiFilter.filterCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(filterChanged(int)));
    connect(uiFilter.selectAll, SIGNAL(clicked(bool)), this, SLOT(selectUnselectAll()));
    connect(uiFilter.deselectAll, SIGNAL(clicked(bool)), this, SLOT(selectUnselectAll()));
    connect(uiDisplay.previewsAdvanced, SIGNAL(clicked()), this, SLOT(showPreviewConfigDialog()));
    connect(uiDisplay.showPreviews, SIGNAL(toggled(bool)), uiDisplay.previewsAdvanced, SLOT(setEnabled(bool)));


    connect(uiDisplay.flowCombo, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
    connect(uiDisplay.sortCombo, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
    connect(uiDisplay.sizeSlider, SIGNAL(valueChanged(int)), parent, SLOT(settingsModified()));
    connect(uiDisplay.showPreviews, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
    connect(uiDisplay.lockInPlace, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
    connect(uiDisplay.alignToGrid, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
    connect(uiDisplay.clickToView, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
    connect(uiDisplay.numLinesEdit, SIGNAL(valueChanged(int)), parent, SLOT(settingsModified()));
    connect(uiDisplay.colorButton, SIGNAL(changed(QColor)), parent, SLOT(settingsModified()));
    connect(uiDisplay.drawShadows, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));

    connect(uiFilter.filterCombo, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
    connect(uiFilter.filterFilesPattern, SIGNAL(textChanged(QString)), parent, SLOT(settingsModified()));
    connect(uiFilter.filterFilesList->model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), parent, SLOT(settingsModified()));

    connect(uiLocation.showDesktopFolder, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
    connect(uiLocation.showActivity, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
    connect(uiLocation.showPlace, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
    connect(uiLocation.titleCombo, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
    connect(uiLocation.titleEdit, SIGNAL(textChanged(QString)), parent, SLOT(settingsModified()));
    connect(uiLocation.showCustomFolder, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
    connect(uiLocation.placesCombo, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
    connect(uiLocation.lineEdit, SIGNAL(textChanged(QString)), parent, SLOT(settingsModified()));
}

void FolderView::configAccepted()
{
    KUrl url;

    if (uiLocation.showDesktopFolder->isChecked()) {
        url = KUrl("desktop:/");
    } else if (uiLocation.showActivity->isChecked()) {
        url = KUrl("activities:/current/");
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

    KConfigGroup cg = config();

    cg.writeEntry("drawShadows", uiDisplay.drawShadows->isChecked());

    cg.writeEntry("showPreviews", uiDisplay.showPreviews->isChecked());

    if (m_previewGenerator && m_previewPlugins != m_previewGenerator->enabledPlugins()) {
        cg.writeEntry("previewPlugins", m_previewPlugins);
    }

    const QColor defaultColor = isContainment() ? Qt::white
                                                : Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
    const QColor color = uiDisplay.colorButton->color();
    if ((m_textColor != Qt::transparent && color != m_textColor) ||
        (m_textColor == Qt::transparent && color != defaultColor))
    {
        cg.writeEntry("textColor", color);
    }

    cg.writeEntry("numTextLines", uiDisplay.numLinesEdit->value());

    const QList<int> iconSizes = QList<int>() << 16 << 22 << 32 << 48 << 64 << 128;
    const int size = iconSizes.at(uiDisplay.sizeSlider->value());
    cg.writeEntry("customIconSize", size);

    const int sortColumn = uiDisplay.sortCombo->itemData(uiDisplay.sortCombo->currentIndex()).toInt();
    cg.writeEntry("sortColumn", sortColumn);

    const int flow = uiDisplay.flowCombo->itemData(uiDisplay.flowCombo->currentIndex()).toInt();
    cg.writeEntry("flow", flow);

    cg.writeEntry("alignToGrid", uiDisplay.alignToGrid->isChecked());
    cg.writeEntry("clickForFolderPreviews", uiDisplay.clickToView->isChecked());
    cg.writeEntry("iconsLocked", uiDisplay.lockInPlace->isChecked());

    cg.writeEntry("url", url);
    cg.writeEntry("filterFiles", uiFilter.filterFilesPattern->text());
    cg.writeEntry("filter", uiFilter.filterCombo->currentIndex());

    const FolderView::LabelType labelType = static_cast<FolderView::LabelType>(uiLocation.titleCombo->itemData(uiLocation.titleCombo->currentIndex()).toInt());
    QString customTitle;
    if (labelType == FolderView::Custom) {
        customTitle = uiLocation.titleEdit->text();
    } else {
        customTitle.clear();
    }
    cg.writeEntry("labelType", static_cast<int>(labelType));
    cg.writeEntry("customLabel", customTitle);

    // Now, we have to iterate over all items (not only the filtered ones). For that reason we have
    // to ask the source model, not the proxy model.
    QStringList selectedItems;
    ProxyMimeModel *proxyModel = static_cast<ProxyMimeModel*>(uiFilter.filterFilesList->model());
    for (int i = 0; i < proxyModel->sourceModel()->rowCount(); i++) {
        const QModelIndex index = proxyModel->sourceModel()->index(i, 0);
        if (index.model()->data(index, Qt::CheckStateRole).toInt() == Qt::Checked) {
            KMimeType *mime = static_cast<KMimeType*>(index.internalPointer());
            if (mime) {
                selectedItems << mime->name();
            }
        }
    }
    cg.writeEntry("mimeFilter", selectedItems);

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

QColor FolderView::textColor() const
{
    if (m_textColor != Qt::transparent) {
        return m_textColor;
    }

    // Default to white text on the desktop
    if (isContainment()) {
        return Qt::white;
    }

    return Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
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
    m_listView->setTextLineCount(m_numTextLines);
}

void FolderView::updateIconViewState()
{
    QPalette palette = m_iconView->palette();
    palette.setColor(QPalette::Text, textColor());
    m_iconView->setPalette(palette);

    m_iconView->setDrawShadows(m_drawShadows);
    m_iconView->setIconSize(iconSize());
    m_iconView->setTextLineCount(m_numTextLines);
    m_iconView->setFlow(m_flow);
    m_iconView->setWordWrap(m_numTextLines > 1);
    m_iconView->setAlignToGrid(m_alignToGrid);
    m_iconView->setIconsMoveable(!m_iconsLocked);
    m_iconView->setClickToViewFolders(m_clickToView);
    m_iconView->setShowSelectionMarker(m_showSelectionMarker);

    if (m_label) {
        m_label->setPalette(palette);
        m_label->setDrawShadow(m_drawShadows);
    }
    // make popup inherit file preview settings:
    m_iconView->setPopupPreviewSettings(m_showPreviews, m_previewPlugins);
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

    const QStringList data = config().readEntry("savedPositions", QStringList());
    m_iconView->setIconPositionsData(data);

    m_iconView->setModel(m_model);
    m_iconView->setItemDelegate(m_delegate);
    m_iconView->setSelectionModel(m_selectionModel);
    m_iconView->setFont(Plasma::Theme::defaultTheme()->font(Plasma::Theme::DesktopFont));

    // Add widget specific actions with shortcuts to the view
    addActions(m_iconView);

    if (!isContainment() ) {
        m_label = new Label(this);
        m_label->setText(m_titleText);

        QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DesktopFont);
        font.setPointSize(font.pointSize() + 1);
        font.setBold(true);
        m_label->setFont(font);
    }

    updateIconViewState();

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
    if (m_label && (m_labelType != FolderView::None)) {
        layout->addItem(m_label);
    }
    layout->addItem(m_iconView);

    setLayout(layout);
}

void FolderView::fontSettingsChanged()
{
    QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DesktopFont);

    if (m_iconView) {
        m_iconView->setFont(font);
    }

    if (m_label) {
        font.setPointSize(font.pointSize() + 1);
        font.setBold(true);
        m_label->setFont(font);
    }
}

void FolderView::iconSettingsChanged(int group)
{
    if (group == KIconLoader::Desktop && m_iconView) {
        const int size = (m_customIconSize != 0) ?
                m_customIconSize : KIconLoader::global()->currentSize(KIconLoader::Desktop);

        m_iconView->setIconSize(QSize(size, size));
        m_iconView->markAreaDirty(m_iconView->visibleArea());
        m_iconView->update();
    } else if (group == KIconLoader::Panel && m_listView) {
        const int size = (m_customIconSize != 0) ?
                m_customIconSize : KIconLoader::global()->currentSize(KIconLoader::Panel);

        m_listView->setIconSize(QSize(size, size));
        m_listView->markAreaDirty(m_listView->visibleArea());
        m_listView->update();

        updateGeometry();
    }
}

void FolderView::clickSettingsChanged(int category)
{
  if (category == KGlobalSettings::SETTINGS_MOUSE && m_iconView) {
    m_iconView->setShowSelectionMarker(KGlobalSettings::singleClick());
  }
}

void FolderView::plasmaThemeChanged()
{
    if (m_textColor != Qt::transparent) {
        return;
    }

    if (m_iconView) {
        QPalette palette = m_iconView->palette();
        palette.setColor(QPalette::Text, textColor());
        m_iconView->setPalette(palette);
    }

    if (m_listView) {
        updateListViewState();
    }

    if (m_label) {
        QPalette palette = m_label->palette();
        palette.setColor(QPalette::Text, textColor());
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

/*
// TODO Mark the cut icons as cut, but test performance!
void FolderView::clipboardDataChanged()
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (KonqMimeData::decodeIsCutSelection(mimeData)) {
        KUrl::List urls = KUrl::List::fromMimeData(mimeData);
        .. marking items as cut code would go here ..
    }
}
*/

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
            const bool wasIconified = m_iconWidget != 0;
            if (wasIconified) {
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

            if (wasIconified) {
                // if we're coming out of an iconified state, let's reset to a reasonable sane state
                // NOTE: usually one NEVER resizes outside of the constructor as that overrides the
                // user settings, but in this case we are changing applet state completely and there
                // is no user state for size in that case for folderview (by defintion)
                resize(600, 400);
            }
            setAspectRatioMode(Plasma::IgnoreAspectRatio);
        } else if (!m_iconWidget) {
            // Clean up the icon view
            delete m_label;
            delete m_iconView;
            m_label = 0;
            m_iconView = 0;

            // Set up the icon widget
            m_iconWidget = new IconWidget(this);
            m_iconWidget->setModel(m_dirModel);
            m_iconWidget->setIcon(m_icon.isNull() ? KIcon("folder-blue") : m_icon);
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

            QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(this);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            layout->addItem(m_iconWidget);

            setLayout(layout);
            int iconSize = IconSize(KIconLoader::Panel);
            setPreferredSize(QSizeF(iconSize, iconSize));
            setAspectRatioMode(Plasma::ConstrainedSquare);
            setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        }
    }

    if (constraints & Plasma::ScreenConstraint) {
        Plasma::Corona *c = corona();
        disconnect(c, SIGNAL(availableScreenRegionChanged()), this, SLOT(updateScreenRegion()));
        if (isContainment() && screen() > -1) {
            updateScreenRegion();
            connect(c, SIGNAL(availableScreenRegionChanged()), this, SLOT(updateScreenRegion()));
        }
    }
}

void FolderView::updateScreenRegion()
{
    if (!m_iconView) {
        return;
    }

    Plasma::Corona *c = corona();
    if (!c) {
        return;
    }

    const QRect screenRect = c->screenGeometry(screen());
    QRect availRect;
    //FIXME: a pretty horrible hack, but there we go; should do something more elegant in 4.5
    if (c->metaObject()->indexOfSlot("availableScreenRect(int)") != -1) {
        QMetaObject::invokeMethod(c, "availableScreenRect",
                                  Qt::DirectConnection, Q_RETURN_ARG(QRect, availRect), Q_ARG(int, screen()));
    } else {
        kDebug() << "using qdesktopwidget";
        availRect = QApplication::desktop()->availableGeometry(screen());
    }

    m_iconView->setContentsMargins(availRect.x() - screenRect.x(),
                                   availRect.y() - screenRect.y(),
                                   screenRect.right() - availRect.right(),
                                   screenRect.bottom() - availRect.bottom());
}

void FolderView::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (isContainment()) {
        Containment::dragEnterEvent(event);
    }
}

void FolderView::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (isContainment()) {
        QGraphicsItem *item = scene()->itemAt(event->scenePos());
        if (item == m_iconView) {
            event->setAccepted(true);
        } else {
            Containment::dragMoveEvent(event);
        }
    }
}

void FolderView::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (isContainment()) {
        Containment::dropEvent(event);
    }
}

void FolderView::setUrl(const KUrl &url)
{
    m_url = url;
    setAssociatedApplicationUrls(KUrl::List() << m_url);

    if (KProtocolInfo::protocolClass(m_url.protocol()) == ":local") {
        disconnect(Solid::Networking::notifier(), 0, this, 0);
        m_dirLister->openUrl(m_url);
    } else {
        //If host is connected to the network and url is remote, list the remote files
        connect(Solid::Networking::notifier(), SIGNAL(shouldConnect()), this,
                SLOT(networkAvailable()), Qt::UniqueConnection);
        //Check if network is not in the "Connected" state
        if (Solid::Networking::status() == Solid::Networking::Connected) {
             //FIXME: Remove the comment when KDE 4.9 development starts
            //QString networkStatus(i18n("Network is not reachable"));
            //showMessage(KIcon("dialog-warning"), networkStatus, Plasma::ButtonOk);
            m_dirLister->openUrl(m_url);
        }
    }

    // Only parse desktop files when sorting if we're showing the desktop folder
    m_model->setParseDesktopFiles(m_url.protocol() == "desktop");
    setAppletTitle();
}

void FolderView::setAppletTitle()
{
    if (m_labelType == FolderView::None) {
        m_titleText.clear();
    } else if (m_labelType == FolderView::FullPath) {
        m_titleText = m_url.path();
    } else if (m_labelType == FolderView::PlaceName) {
        if (m_url == KUrl("desktop:/")) {
            m_titleText = i18n("Desktop Folder");
        } else {
            m_titleText = m_url.pathOrUrl();

            if (!m_placesModel) {
                m_placesModel = new KFilePlacesModel(this);
            }
            const QModelIndex index = m_placesModel->closestItem(m_url);
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
    } else {
        m_titleText = m_customLabel;
    }
    kDebug() << "WORKING WITH" << m_labelType << m_customLabel << "WE GOT" << m_titleText;

    if (m_labelType == FolderView::None) {
        if (m_label) {
            m_label->hide();
        }
        recreateLayout();
    } else {
        if (m_label) {
            m_label->setText(m_titleText);
            m_label->show();
        }
        recreateLayout();
    }
    updateIconWidget();
}

void FolderView::recreateLayout()
{
    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    if (m_labelType != FolderView::None) {
        layout->addItem(m_label);
    }
    layout->addItem(m_iconView);

    setLayout(layout);
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
    connect(trash, SIGNAL(triggered(Qt::MouseButtons,Qt::KeyboardModifiers)),
            SLOT(moveToTrash(Qt::MouseButtons,Qt::KeyboardModifiers)));

    KAction *emptyTrash = new KAction(KIcon("trash-empty"), i18n("&Empty Trash Bin"), this);
    KConfig trashConfig("trashrc", KConfig::SimpleConfig);
    emptyTrash->setEnabled(!trashConfig.group("Status").readEntry("Empty", true));
    connect(emptyTrash, SIGNAL(triggered()), SLOT(emptyTrashBin()));

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
    m_actionCollection.addAction("empty_trash", emptyTrash);

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

        m_sortingOrderGroup = new QActionGroup(this);
        connect(m_sortingOrderGroup, SIGNAL(triggered(QAction*)), SLOT(sortingOrderChanged(QAction*)));
        QAction *sortAscending = m_sortingOrderGroup->addAction(i18nc("Sort icons", "Ascending"));
        QAction *sortDescending = m_sortingOrderGroup->addAction(i18nc("Sort icons", "Descending"));

        sortAscending->setCheckable(true);
        sortAscending->setData(QVariant::fromValue(Qt::AscendingOrder));

        sortDescending->setCheckable(true);
        sortDescending->setData(QVariant::fromValue(Qt::DescendingOrder));

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
        sortMenu->addAction(sortAscending);
        sortMenu->addAction(sortDescending);
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
        m_newMenu = new KNewFileMenu(&m_actionCollection, "new_menu", QApplication::desktop());
        m_newMenu->setModal(false);

        connect(m_newMenu->menu(), SIGNAL(aboutToShow()), this, SLOT(aboutToShowCreateNew()));

        m_actionCollection.addAction("lock_icons", lockIcons);
        m_actionCollection.addAction("auto_align", alignToGrid);
        m_actionCollection.addAction("icons_menu", iconsMenuAction);
        m_actionCollection.addAction("sort_name", sortByName);
        m_actionCollection.addAction("sort_size", sortBySize);
        m_actionCollection.addAction("sort_type", sortByType);
        m_actionCollection.addAction("sort_date", sortByDate);
        m_actionCollection.addAction("sort_ascending", sortAscending);
        m_actionCollection.addAction("sort_descending", sortDescending);

        updateSortActionsState();
    }
}

void FolderView::updatePasteAction()
{
    if (QAction *paste = m_actionCollection.action("paste")) {
        const QString pasteText = KIO::pasteActionText();
        if (pasteText.isEmpty()) {
            paste->setText(i18n("&Paste"));
            paste->setEnabled(false);
        } else {
            paste->setText(pasteText);
            paste->setEnabled(true);
        }
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
        if (QAction *paste = m_actionCollection.action("paste")) {
            updatePasteAction();
            actions.append(paste);
        }

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
        if (m_url.protocol() == "trash") {
            KConfig trashConfig("trashrc", KConfig::SimpleConfig);
            m_actionCollection.action("empty_trash")->setEnabled(!trashConfig.group("Status")
                                      .readEntry("Empty", true));
            actions.append(m_actionCollection.action("empty_trash"));
        }

        separator = new QAction(this);
        separator->setSeparator(true);
        actions.append(separator);
    }

    return actions;
}

void FolderView::aboutToShowCreateNew()
{
    if (m_newMenu) {
        m_newMenu->checkUpToDate();
        m_newMenu->setPopupFiles(m_url);
    }
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
    KonqOperations::doPaste(QApplication::desktop(), m_url);
}

void FolderView::pasteTo()
{
    const KUrl::List urls = selectedUrls(false);
    Q_ASSERT(urls.count() == 1);
    KonqOperations::doPaste(QApplication::desktop(), urls.first());
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

void FolderView::toggleClickToViewFolders(bool enable)
{
   m_clickToView = enable;

    if (m_iconView) {
        m_iconView->setClickToViewFolders(enable);
    }

    config().writeEntry("clickForFolderPreviews", enable);
    emit configNeedsSaving();

    m_delayedSaveTimer.start(5000, this);
}

void FolderView::toggleDirectoriesFirst(bool enable)
{
    m_sortDirsFirst = enable;

    m_model->setSortDirectoriesFirst(m_sortDirsFirst);
    if (m_sortColumn != -1) {
        m_model->invalidate();
    }

    config().writeEntry("sortDirsFirst", m_sortDirsFirst);
    emit configNeedsSaving();

    m_delayedSaveTimer.start(5000, this);
}

void FolderView::sortingChanged(QAction *action)
{
    const int column = action->data().toInt();

    if (column != m_sortColumn) {
        m_model->invalidate();
        m_model->sort(column, m_sortOrder);
        m_model->setDynamicSortFilter(true);
        m_sortColumn = column;
        config().writeEntry("sortColumn", m_sortColumn);
        emit configNeedsSaving();
        m_delayedSaveTimer.start(5000, this);
    }
}

void FolderView::sortingOrderChanged(QAction *action)
{
    const Qt::SortOrder order = action->data().value<Qt::SortOrder>();

    if (order != m_sortOrder) {
        m_model->invalidate();
        m_model->sort(m_sortColumn, order);
        m_model->setDynamicSortFilter(true);
        m_sortOrder = order;
        config().writeEntry("sortOrder", Settings::sortOrderEnumToString(m_sortOrder));
        emit configNeedsSaving();
        m_delayedSaveTimer.start(5000, this);
    }
}

void FolderView::updateSortActionsState()
{
    foreach (QAction *action, m_sortingGroup->actions()) {
        action->setChecked(action->data() == int(m_sortColumn));
    }
    foreach (QAction *action, m_sortingOrderGroup->actions()) {
        action->setChecked(action->data().value<Qt::SortOrder>() == m_sortOrder);
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

    KonqOperations::del(QApplication::desktop(), op, selectedUrls(op == KonqOperations::TRASH));
}

void FolderView::deleteSelectedIcons()
{
    if (m_iconView && m_iconView->renameInProgress()) {
        return;
    }

    KonqOperations::del(QApplication::desktop(), KonqOperations::DEL, selectedUrls(false));
}

void FolderView::renameSelectedIcon()
{
    if (m_iconView) {
        m_iconView->renameSelectedIcon();
    }
}

void FolderView::emptyTrashBin()
{
    KonqOperations::emptyTrash(QApplication::desktop());
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
        Plasma::WindowEffects::slideWindow(m_dialog, location());
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
        m_model->setDynamicSortFilter(false);
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
        if (!item.isNull()) {
            hasRemoteFiles |= item.localPath().isEmpty();
            items.append(item);
        }
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
        if (QAction *paste = m_actionCollection.action("paste")) {
            updatePasteAction();
            pasteTo->setEnabled(paste->isEnabled());
            pasteTo->setText(paste->text());
        }
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
        m_icon = KIcon("folder-blue");
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
    Plasma::WindowEffects::slideWindow(m_dialog, location());
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

void FolderView::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_delayedSaveTimer.timerId()) {
        m_delayedSaveTimer.stop();
        saveIconPositions();
        emit configNeedsSaving();
    }

    Containment::timerEvent(event);
}

QSizeF FolderView::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    if (which == Qt::PreferredSize) {
        QSizeF size;

        switch (formFactor()) {
          case Plasma::Planar:
          case Plasma::MediaCenter:
              if (!constraint.isEmpty()) {
                  size = QSizeF(600, 400).boundedTo(constraint);
              } else {
                  size = QSizeF(600, 400);
              }
              break;
          case Plasma::Horizontal:
          case Plasma::Vertical:
              const int iconSize = IconSize(KIconLoader::Panel);
              size = QSizeF(iconSize, iconSize);
              break;
        }

        return size;
    }

    return Containment::sizeHint(which, constraint);

}

void FolderView::setTitleEditEnabled(int index)
{
    if (uiLocation.titleCombo->itemData(index).toInt() == FolderView::Custom) {
        uiLocation.titleEdit->setEnabled(true);
        uiLocation.titleEdit->setFocus();
    } else {
        uiLocation.titleEdit->setEnabled(false);
    }
}

#include "folderview.moc"
