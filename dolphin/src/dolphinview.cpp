/***************************************************************************
 *   Copyright (C) 2006-2009 by Peter Penz <peter.penz@gmx.at>             *
 *   Copyright (C) 2006 by Gregor Kališnik <gregor@podnapisi.net>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "dolphinview.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QItemSelection>
#include <QBoxLayout>
#include <QTimer>
#include <QScrollBar>

#include <kactioncollection.h>
#include <kcolorscheme.h>
#include <kdirlister.h>
#include <kiconeffect.h>
#include <kfileitem.h>
#include <klocale.h>
#include <kio/deletejob.h>
#include <kio/netaccess.h>
#include <kio/previewjob.h>
#include <kjob.h>
#include <kmenu.h>
#include <kmessagebox.h>
#include <kmimetyperesolver.h>
#include <konq_fileitemcapabilities.h>
#include <konq_operations.h>
#include <konqmimedata.h>
#include <kstringhandler.h>
#include <ktoggleaction.h>
#include <kurl.h>

#include "additionalinfoaccessor.h"
#include "dolphinmodel.h"
#include "dolphincolumnviewcontainer.h"
#include "dolphinviewcontroller.h"
#include "dolphindetailsview.h"
#include "dolphinfileitemdelegate.h"
#include "dolphinnewmenuobserver.h"
#include "dolphinsortfilterproxymodel.h"
#include "dolphin_detailsmodesettings.h"
#include "dolphiniconsview.h"
#include "dolphin_generalsettings.h"
#include "draganddrophelper.h"
#include "renamedialog.h"
#include "settings/dolphinsettings.h"
#include "viewmodecontroller.h"
#include "viewproperties.h"
#include "zoomlevelinfo.h"
#include "dolphindetailsviewexpander.h"

/**
 * Helper function for sorting items with qSort() in
 * DolphinView::renameSelectedItems().
 */
bool lessThan(const KFileItem& item1, const KFileItem& item2)
{
    return KStringHandler::naturalCompare(item1.name(), item2.name()) < 0;
}

DolphinView::DolphinView(QWidget* parent,
                         const KUrl& url,
                         DolphinSortFilterProxyModel* proxyModel) :
    QWidget(parent),
    m_active(true),
    m_showPreview(false),
    m_storedCategorizedSorting(false),
    m_tabsForFiles(false),
    m_isContextMenuOpen(false),
    m_ignoreViewProperties(false),
    m_assureVisibleCurrentIndex(false),
    m_mode(DolphinView::IconsView),
    m_topLayout(0),
    m_dolphinViewController(0),
    m_viewModeController(0),
    m_viewAccessor(proxyModel),
    m_selectionChangedTimer(0),
    m_rootUrl(),
    m_activeItemUrl(),
    m_restoredContentsPosition(),
    m_createdItemUrl(),
    m_selectedItems(),
    m_newFileNames()
{
    m_topLayout = new QVBoxLayout(this);
    m_topLayout->setSpacing(0);
    m_topLayout->setMargin(0);

    m_dolphinViewController = new DolphinViewController(this);

    m_viewModeController = new ViewModeController(this);
    m_viewModeController->setUrl(url);

    connect(m_viewModeController, SIGNAL(urlChanged(const KUrl&)),
            this, SIGNAL(urlChanged(const KUrl&)));

    connect(m_dolphinViewController, SIGNAL(requestContextMenu(const QPoint&, const QList<QAction*>&)),
            this, SLOT(openContextMenu(const QPoint&, const QList<QAction*>&)));
    connect(m_dolphinViewController, SIGNAL(urlsDropped(const KFileItem&, const KUrl&, QDropEvent*)),
            this, SLOT(dropUrls(const KFileItem&, const KUrl&, QDropEvent*)));
    connect(m_dolphinViewController, SIGNAL(sortingChanged(DolphinView::Sorting)),
            this, SLOT(updateSorting(DolphinView::Sorting)));
    connect(m_dolphinViewController, SIGNAL(sortOrderChanged(Qt::SortOrder)),
            this, SLOT(updateSortOrder(Qt::SortOrder)));
    connect(m_dolphinViewController, SIGNAL(sortFoldersFirstChanged(bool)),
            this, SLOT(updateSortFoldersFirst(bool)));
    connect(m_dolphinViewController, SIGNAL(additionalInfoChanged(const KFileItemDelegate::InformationList&)),
            this, SLOT(updateAdditionalInfo(const KFileItemDelegate::InformationList&)));
    connect(m_dolphinViewController, SIGNAL(itemTriggered(const KFileItem&)),
            this, SLOT(triggerItem(const KFileItem&)));
    connect(m_dolphinViewController, SIGNAL(tabRequested(const KUrl&)),
            this, SIGNAL(tabRequested(const KUrl&)));
    connect(m_dolphinViewController, SIGNAL(activated()),
            this, SLOT(activate()));
    connect(m_dolphinViewController, SIGNAL(itemEntered(const KFileItem&)),
            this, SLOT(showHoverInformation(const KFileItem&)));
    connect(m_dolphinViewController, SIGNAL(viewportEntered()),
            this, SLOT(clearHoverInformation()));
    connect(m_dolphinViewController, SIGNAL(urlChangeRequested(KUrl)),
            m_viewModeController, SLOT(setUrl(KUrl)));

    KDirLister* dirLister = m_viewAccessor.dirLister();
    connect(dirLister, SIGNAL(redirection(KUrl,KUrl)),
            this, SLOT(slotRedirection(KUrl,KUrl)));
    connect(dirLister, SIGNAL(completed()),
            this, SLOT(slotDirListerCompleted()));
    connect(dirLister, SIGNAL(refreshItems(const QList<QPair<KFileItem,KFileItem>>&)),
            this, SLOT(slotRefreshItems()));

    // When a new item has been created by the "Create New..." menu, the item should
    // get selected and it must be assured that the item will get visible. As the
    // creation is done asynchronously, several signals must be checked:
    connect(&DolphinNewMenuObserver::instance(), SIGNAL(itemCreated(const KUrl&)),
            this, SLOT(observeCreatedItem(const KUrl&)));

    m_selectionChangedTimer = new QTimer(this);
    m_selectionChangedTimer->setSingleShot(true);
    m_selectionChangedTimer->setInterval(300);
    connect(m_selectionChangedTimer, SIGNAL(timeout()),
            this, SLOT(emitSelectionChangedSignal()));

    applyViewProperties();
    m_topLayout->addWidget(m_viewAccessor.layoutTarget());
}

DolphinView::~DolphinView()
{
}

KUrl DolphinView::url() const
{
    return m_viewModeController->url();
}

KUrl DolphinView::rootUrl() const
{
    const KUrl viewUrl = url();
    const KUrl root = m_viewAccessor.rootUrl();
    if (root.isEmpty() || !root.isParentOf(viewUrl)) {
        return viewUrl;
    }
    return root;
}

void DolphinView::setActive(bool active)
{
    if (active == m_active) {
        return;
    }

    m_active = active;

    QColor color = KColorScheme(QPalette::Active, KColorScheme::View).background().color();
    if (active) {
        emitSelectionChangedSignal();
    } else {
        color.setAlpha(150);
    }

    QWidget* viewport = m_viewAccessor.itemView()->viewport();
    QPalette palette;
    palette.setColor(viewport->backgroundRole(), color);
    viewport->setPalette(palette);

    update();

    if (active) {
        m_viewAccessor.itemView()->setFocus();
        emit activated();
    }

    m_viewModeController->indicateActivationChange(active);
}

bool DolphinView::isActive() const
{
    return m_active;
}

void DolphinView::setMode(Mode mode)
{
    if (mode == m_mode) {
        return; // the wished mode is already set
    }

    const int oldZoomLevel = m_viewModeController->zoomLevel();
    m_mode = mode;

    // remember the currently selected items, so that they will
    // be restored after reloading the directory
    m_selectedItems = selectedItems();

    deleteView();

    const KUrl viewPropsUrl = rootUrl();
    ViewProperties props(viewPropsUrl);
    props.setViewMode(m_mode);
    createView();

    // the file item delegate has been recreated, apply the current
    // additional information manually
    const KFileItemDelegate::InformationList infoList = props.additionalInfo();
    m_viewAccessor.itemDelegate()->setShowInformation(infoList);
    emit additionalInfoChanged();

    // Not all view modes support categorized sorting. Adjust the sorting model
    // if changing the view mode results in a change of the categorized sorting
    // capabilities.
    m_storedCategorizedSorting = props.categorizedSorting();
    const bool categorized = m_storedCategorizedSorting && supportsCategorizedSorting();
    if (categorized != m_viewAccessor.proxyModel()->isCategorizedModel()) {
        m_viewAccessor.proxyModel()->setCategorizedModel(categorized);
        emit categorizedSortingChanged();
    }

    emit modeChanged();

    updateZoomLevel(oldZoomLevel);
    loadDirectory(viewPropsUrl);
}

DolphinView::Mode DolphinView::mode() const
{
    return m_mode;
}

bool DolphinView::showPreview() const
{
    return m_showPreview;
}

bool DolphinView::showHiddenFiles() const
{
    return m_viewAccessor.dirLister()->showingDotFiles();
}

bool DolphinView::categorizedSorting() const
{
    // If all view modes would support categorized sorting, returning
    // m_viewAccessor.proxyModel()->isCategorizedModel() would be the way to go. As
    // currently only the icons view supports caterized sorting, we remember
    // the stored view properties state in m_storedCategorizedSorting and
    // return this state. The application takes care to disable the corresponding
    // checkbox by checking DolphinView::supportsCategorizedSorting() to indicate
    // that this setting is not applied to the current view mode.
    return m_storedCategorizedSorting;
}

bool DolphinView::supportsCategorizedSorting() const
{
    return m_viewAccessor.supportsCategorizedSorting();
}

bool DolphinView::hasSelection() const
{
    const QAbstractItemView* view = m_viewAccessor.itemView();
    return (view != 0) && view->selectionModel()->hasSelection();
}

void DolphinView::markUrlsAsSelected(const QList<KUrl>& urls)
{
    foreach (const KUrl& url, urls) {
        KFileItem item(KFileItem::Unknown, KFileItem::Unknown, url);
        m_selectedItems.append(item);
    }
}

KFileItemList DolphinView::selectedItems() const
{
    KFileItemList itemList;
    const QAbstractItemView* view = m_viewAccessor.itemView();
    if (view == 0) {
        return itemList;
    }

    const QItemSelection selection = m_viewAccessor.proxyModel()->mapSelectionToSource(view->selectionModel()->selection());

    const QModelIndexList indexList = selection.indexes();
    foreach (const QModelIndex &index, indexList) {
        KFileItem item = m_viewAccessor.dirModel()->itemForIndex(index);
        if (!item.isNull()) {
            itemList.append(item);
        }
    }

    return itemList;
}

KUrl::List DolphinView::selectedUrls() const
{
    KUrl::List urls;
    const KFileItemList list = selectedItems();
    foreach (const KFileItem &item, list) {
        urls.append(item.url());
    }
    return urls;
}

int DolphinView::selectedItemsCount() const
{
    const QAbstractItemView* view = m_viewAccessor.itemView();
    if (view == 0) {
        return 0;
    }

    return view->selectionModel()->selectedIndexes().count();
}

QItemSelectionModel* DolphinView::selectionModel() const
{
    return m_viewAccessor.itemView()->selectionModel();
}

void DolphinView::setZoomLevel(int level)
{
    if (level < ZoomLevelInfo::minimumLevel()) {
        level = ZoomLevelInfo::minimumLevel();
    } else if (level > ZoomLevelInfo::maximumLevel()) {
        level = ZoomLevelInfo::maximumLevel();
    }

    if (level != zoomLevel()) {
        m_viewModeController->setZoomLevel(level);
        emit zoomLevelChanged(level);
    }
}

int DolphinView::zoomLevel() const
{
    return m_viewModeController->zoomLevel();
}

void DolphinView::setSorting(Sorting sorting)
{
    if (sorting != this->sorting()) {
        updateSorting(sorting);
    }
}

DolphinView::Sorting DolphinView::sorting() const
{
    return m_viewAccessor.proxyModel()->sorting();
}

void DolphinView::setSortOrder(Qt::SortOrder order)
{
    if (sortOrder() != order) {
        updateSortOrder(order);
    }
}

Qt::SortOrder DolphinView::sortOrder() const
{
    return m_viewAccessor.proxyModel()->sortOrder();
}

void DolphinView::setSortFoldersFirst(bool foldersFirst)
{
    if (sortFoldersFirst() != foldersFirst) {
        updateSortFoldersFirst(foldersFirst);
    }
}

bool DolphinView::sortFoldersFirst() const
{
    return m_viewAccessor.proxyModel()->sortFoldersFirst();
}

void DolphinView::setAdditionalInfo(KFileItemDelegate::InformationList info)
{
    const KUrl viewPropsUrl = rootUrl();
    ViewProperties props(viewPropsUrl);
    props.setAdditionalInfo(info);
    m_viewAccessor.itemDelegate()->setShowInformation(info);

    emit additionalInfoChanged();

    if (m_viewAccessor.reloadOnAdditionalInfoChange()) {
        loadDirectory(viewPropsUrl);
    }
}

KFileItemDelegate::InformationList DolphinView::additionalInfo() const
{
    return m_viewAccessor.itemDelegate()->showInformation();
}

void DolphinView::reload()
{
    QByteArray viewState;
    QDataStream saveStream(&viewState, QIODevice::WriteOnly);
    saveState(saveStream);
    m_selectedItems= selectedItems();

    setUrl(url());
    loadDirectory(url(), true);

    QDataStream restoreStream(viewState);
    restoreState(restoreStream);
}

void DolphinView::refresh()
{
    m_ignoreViewProperties = false;

    const bool oldActivationState = m_active;
    const int oldZoomLevel = m_viewModeController->zoomLevel();
    m_active = true;

    createView();
    applyViewProperties();
    reload();

    setActive(oldActivationState);
    updateZoomLevel(oldZoomLevel);
}

void DolphinView::setNameFilter(const QString& nameFilter)
{
    m_viewModeController->setNameFilter(nameFilter);
}

void DolphinView::calculateItemCount(int& fileCount,
                                     int& folderCount,
                                     KIO::filesize_t& totalFileSize) const
{
    foreach (const KFileItem& item, m_viewAccessor.dirLister()->items()) {
        if (item.isDir()) {
            ++folderCount;
        } else {
            ++fileCount;
            totalFileSize += item.size();
        }
    }
}

QString DolphinView::statusBarText() const
{
    QString text;
    int folderCount = 0;
    int fileCount = 0;
    KIO::filesize_t totalFileSize = 0;

    if (hasSelection()) {
        // give a summary of the status of the selected files
        const KFileItemList list = selectedItems();
        if (list.isEmpty()) {
            // when an item is triggered, it is temporary selected but selectedItems()
            // will return an empty list
            return text;
        }

        KFileItemList::const_iterator it = list.begin();
        const KFileItemList::const_iterator end = list.end();
        while (it != end) {
            const KFileItem& item = *it;
            if (item.isDir()) {
                ++folderCount;
            } else {
                ++fileCount;
                totalFileSize += item.size();
            }
            ++it;
        }

        if (folderCount + fileCount == 1) {
            // if only one item is selected, show the filename
            const QString name = list.first().text();
            text = (folderCount == 1) ? i18nc("@info:status", "<filename>%1</filename> selected", name) :
                                        i18nc("@info:status", "<filename>%1</filename> selected (%2)",
                                              name, KIO::convertSize(totalFileSize));
        } else {
            // at least 2 items are selected
            const QString foldersText = i18ncp("@info:status", "1 Folder selected", "%1 Folders selected", folderCount);
            const QString filesText = i18ncp("@info:status", "1 File selected", "%1 Files selected", fileCount);
            if ((folderCount > 0) && (fileCount > 0)) {
                text = i18nc("@info:status folders, files (size)", "%1, %2 (%3)",
                             foldersText, filesText, KIO::convertSize(totalFileSize));
            } else if (fileCount > 0) {
                text = i18nc("@info:status files (size)", "%1 (%2)", filesText, KIO::convertSize(totalFileSize));
            } else {
                Q_ASSERT(folderCount > 0);
                text = foldersText;
            }
        }
    } else {
        calculateItemCount(fileCount, folderCount, totalFileSize);
        text = KIO::itemsSummaryString(fileCount + folderCount,
                                       fileCount, folderCount,
                                       totalFileSize, true);
    }

    return text;
}

QList<QAction*> DolphinView::versionControlActions(const KFileItemList& items) const
{
    return m_dolphinViewController->versionControlActions(items);
}

void DolphinView::setUrl(const KUrl& url)
{
    if (m_viewModeController->url() == url) {
        return;
    }

    // The selection model might change in the case of the column view. Disconnect
    // from the current selection model and reconnect later after the URL switch.
    QAbstractItemView* view = m_viewAccessor.itemView();
    disconnect(view->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
               this, SLOT(slotSelectionChanged(QItemSelection, QItemSelection)));

    m_newFileNames.clear();

    m_viewModeController->setUrl(url); // emits urlChanged, which we forward
    m_viewAccessor.prepareUrlChange(url);
    applyViewProperties();
    loadDirectory(url);

    // When changing the URL there is no need to keep the version
    // data of the previous URL.
    m_viewAccessor.dirModel()->clearVersionData();

    emit startedPathLoading(url);

    // Reconnect to the (probably) new selection model
    view = m_viewAccessor.itemView();
    connect(view->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            this, SLOT(slotSelectionChanged(QItemSelection, QItemSelection)));
}

void DolphinView::selectAll()
{
    m_viewAccessor.itemView()->selectAll();
}

void DolphinView::invertSelection()
{
    QItemSelectionModel* selectionModel = m_viewAccessor.itemView()->selectionModel();
    const QAbstractItemModel* itemModel = selectionModel->model();

    const QModelIndex topLeft = itemModel->index(0, 0);
    const QModelIndex bottomRight = itemModel->index(itemModel->rowCount() - 1,
                                                     itemModel->columnCount() - 1);

    const QItemSelection selection(topLeft, bottomRight);
    selectionModel->select(selection, QItemSelectionModel::Toggle);
}

void DolphinView::clearSelection()
{
    m_viewAccessor.itemView()->clearSelection();
}

void DolphinView::renameSelectedItems()
{
    KFileItemList items = selectedItems();
    const int itemCount = items.count();
    if (itemCount < 1) {
        return;
    }

    if (itemCount > 1) {
        // More than one item has been selected for renaming. Open
        // a rename dialog and rename all items afterwards.
        QPointer<RenameDialog> dialog = new RenameDialog(this, items);
        if (dialog->exec() == QDialog::Rejected) {
            delete dialog;
            return;
        }

        const QString newName = dialog->newName();
        if (newName.isEmpty()) {
            emit errorMessage(dialog->errorString());
            delete dialog;
            return;
        }
        delete dialog;

        // the selection would be invalid after renaming the items, so just clear
        // it before
        clearSelection();

        // TODO: check how this can be integrated into KIO::FileUndoManager/KonqOperations
        // as one operation instead of n rename operations like it is done now...
        Q_ASSERT(newName.contains('#'));

        // currently the items are sorted by the selection order, resort
        // them by the file name
        qSort(items.begin(), items.end(), lessThan);

        // iterate through all selected items and rename them...
        int index = 1;
        foreach (const KFileItem& item, items) {
            const KUrl& oldUrl = item.url();
            QString number;
            number.setNum(index++);

            QString name = newName;
            name.replace('#', number);

            if (oldUrl.fileName() != name) {
                KUrl newUrl = oldUrl;
                newUrl.setFileName(name);
                KonqOperations::rename(this, oldUrl, newUrl);
            }
        }
    } else if (DolphinSettings::instance().generalSettings()->renameInline()) {
        Q_ASSERT(itemCount == 1);
        const QModelIndex dirIndex = m_viewAccessor.dirModel()->indexForItem(items.first());
        const QModelIndex proxyIndex = m_viewAccessor.proxyModel()->mapFromSource(dirIndex);
        m_viewAccessor.itemView()->edit(proxyIndex);
    } else {
        Q_ASSERT(itemCount == 1);

        QPointer<RenameDialog> dialog = new RenameDialog(this, items);
        if (dialog->exec() == QDialog::Rejected) {
            delete dialog;
            return;
        }

        const QString newName = dialog->newName();
        if (newName.isEmpty()) {
            emit errorMessage(dialog->errorString());
            delete dialog;
            return;
        }
        delete dialog;

        const KUrl& oldUrl = items.first().url();
        KUrl newUrl = oldUrl;
        newUrl.setFileName(newName);
        KonqOperations::rename(this, oldUrl, newUrl);
    }

    // assure that the current index remains visible when KDirLister
    // will notify the view about changed items
    m_assureVisibleCurrentIndex = true;
}

void DolphinView::trashSelectedItems()
{
    const KUrl::List list = simplifiedSelectedUrls();
    KonqOperations::del(this, KonqOperations::TRASH, list);
}

void DolphinView::deleteSelectedItems()
{
    const KUrl::List list = simplifiedSelectedUrls();
    const bool del = KonqOperations::askDeleteConfirmation(list,
                     KonqOperations::DEL,
                     KonqOperations::DEFAULT_CONFIRMATION,
                     this);

    if (del) {
        KIO::Job* job = KIO::del(list);
        connect(job, SIGNAL(result(KJob*)),
                this, SLOT(slotDeleteFileFinished(KJob*)));
    }
}

void DolphinView::cutSelectedItems()
{
    QMimeData* mimeData = selectionMimeData();
    KonqMimeData::addIsCutSelection(mimeData, true);
    QApplication::clipboard()->setMimeData(mimeData);
}

void DolphinView::copySelectedItems()
{
    QMimeData* mimeData = selectionMimeData();
    QApplication::clipboard()->setMimeData(mimeData);
}

void DolphinView::paste()
{
    pasteToUrl(url());
}

void DolphinView::pasteIntoFolder()
{
    const KFileItemList items = selectedItems();
    if ((items.count() == 1) && items.first().isDir()) {
        pasteToUrl(items.first().url());
    }
}

void DolphinView::setShowPreview(bool show)
{
    if (m_showPreview == show) {
        return;
    }

    const KUrl viewPropsUrl = rootUrl();
    ViewProperties props(viewPropsUrl);
    props.setShowPreview(show);

    m_showPreview = show;
    const int oldZoomLevel = m_viewModeController->zoomLevel();
    emit showPreviewChanged();

    // Enabling or disabling the preview might change the icon size of the view.
    // As the view does not emit a signal when the icon size has been changed,
    // the used zoom level of the controller must be adjusted manually:
    updateZoomLevel(oldZoomLevel);
}

void DolphinView::setShowHiddenFiles(bool show)
{
    if (m_viewAccessor.dirLister()->showingDotFiles() == show) {
        return;
    }

    const KUrl viewPropsUrl = rootUrl();
    ViewProperties props(viewPropsUrl);
    props.setShowHiddenFiles(show);

    m_viewAccessor.dirLister()->setShowingDotFiles(show);
    emit showHiddenFilesChanged();
}

void DolphinView::setCategorizedSorting(bool categorized)
{
    if (categorized == categorizedSorting()) {
        return;
    }

    // setCategorizedSorting(true) may only get invoked
    // if the view supports categorized sorting
    Q_ASSERT(!categorized || supportsCategorizedSorting());

    ViewProperties props(rootUrl());
    props.setCategorizedSorting(categorized);
    props.save();

    m_storedCategorizedSorting = categorized;
    m_viewAccessor.proxyModel()->setCategorizedModel(categorized);

    emit categorizedSortingChanged();
}

void DolphinView::toggleSortOrder()
{
    const Qt::SortOrder order = (sortOrder() == Qt::AscendingOrder) ?
                                Qt::DescendingOrder :
                                Qt::AscendingOrder;
    setSortOrder(order);
}

void DolphinView::toggleSortFoldersFirst()
{
    setSortFoldersFirst(!sortFoldersFirst());
}

void DolphinView::toggleAdditionalInfo(QAction* action)
{
    const KFileItemDelegate::Information info =
        static_cast<KFileItemDelegate::Information>(action->data().toInt());

    KFileItemDelegate::InformationList list = additionalInfo();

    const bool show = action->isChecked();

    const int index = list.indexOf(info);
    const bool containsInfo = (index >= 0);
    if (show && !containsInfo) {
        list.append(info);
        setAdditionalInfo(list);
    } else if (!show && containsInfo) {
        list.removeAt(index);
        setAdditionalInfo(list);
        Q_ASSERT(list.indexOf(info) < 0);
    }
}

void DolphinView::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
    setActive(true);
}

bool DolphinView::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::FocusIn:
        if (watched == m_viewAccessor.itemView()) {
            m_dolphinViewController->requestActivation();
        }
        break;

    case QEvent::DragEnter:
        if (watched == m_viewAccessor.itemView()->viewport()) {
            setActive(true);
        }
        break;

    case QEvent::KeyPress:
        if (watched == m_viewAccessor.itemView()) {
            // clear the selection when Escape has been pressed
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                clearSelection();
            }
        }
        break;

    case QEvent::Wheel:
        if (watched == m_viewAccessor.itemView()->viewport()) {
            // Ctrl+wheel events should cause icon zooming, but not if the left mouse button is pressed
            // (the user is probably trying to scroll during a selection in that case)
            QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
            if (wheelEvent->modifiers() & Qt::ControlModifier && !(wheelEvent->buttons() & Qt::LeftButton)) {
                const int delta = wheelEvent->delta();
                const int level = zoomLevel();
                if (delta > 0) {
                    setZoomLevel(level + 1);
                } else if (delta < 0) {
                    setZoomLevel(level - 1);
                }
                return true;
            }
        }
        break;

    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

void DolphinView::activate()
{
    setActive(true);
}

void DolphinView::triggerItem(const KFileItem& item)
{
    const Qt::KeyboardModifiers modifier = QApplication::keyboardModifiers();
    if ((modifier & Qt::ShiftModifier) || (modifier & Qt::ControlModifier)) {
        // items are selected by the user, hence don't trigger the
        // item specified by 'index'
        return;
    }

    // TODO: the m_isContextMenuOpen check is a workaround for Qt-issue 207192
    if (item.isNull() || m_isContextMenuOpen) {
        return;
    }

    emit itemTriggered(item); // caught by DolphinViewContainer or DolphinPart
}

void DolphinView::slotSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    const int count = selectedItemsCount();
    const bool selectionStateChanged = ((count >  0) && (selected.count() == count)) ||
                                       ((count == 0) && !deselected.isEmpty());

    // If nothing has been selected before and something got selected (or if something
    // was selected before and now nothing is selected) the selectionChangedSignal must
    // be emitted asynchronously as fast as possible to update the edit-actions.
    m_selectionChangedTimer->setInterval(selectionStateChanged ? 0 : 300);
    m_selectionChangedTimer->start();
}

void DolphinView::emitSelectionChangedSignal()
{
    emit selectionChanged(DolphinView::selectedItems());
}

void DolphinView::openContextMenu(const QPoint& pos,
                                  const QList<QAction*>& customActions)
{
    KFileItem item;
    const QModelIndex index = m_viewAccessor.itemView()->indexAt(pos);
    if (index.isValid() && (index.column() == DolphinModel::Name)) {
        const QModelIndex dolphinModelIndex = m_viewAccessor.proxyModel()->mapToSource(index);
        item = m_viewAccessor.dirModel()->itemForIndex(dolphinModelIndex);
    }

    m_isContextMenuOpen = true; // TODO: workaround for Qt-issue 207192
    emit requestContextMenu(item, url(), customActions);
    m_isContextMenuOpen = false;
}

void DolphinView::dropUrls(const KFileItem& destItem,
                           const KUrl& destPath,
                           QDropEvent* event)
{
    addNewFileNames(event->mimeData());
    DragAndDropHelper::instance().dropUrls(destItem, destPath, event, this);
}

void DolphinView::updateSorting(DolphinView::Sorting sorting)
{
    ViewProperties props(rootUrl());
    props.setSorting(sorting);

    m_viewAccessor.proxyModel()->setSorting(sorting);

    emit sortingChanged(sorting);
}

void DolphinView::updateSortOrder(Qt::SortOrder order)
{
    ViewProperties props(rootUrl());
    props.setSortOrder(order);

    m_viewAccessor.proxyModel()->setSortOrder(order);

    emit sortOrderChanged(order);
}

void DolphinView::updateSortFoldersFirst(bool foldersFirst)
{
    ViewProperties props(rootUrl());
    props.setSortFoldersFirst(foldersFirst);

    m_viewAccessor.proxyModel()->setSortFoldersFirst(foldersFirst);

    emit sortFoldersFirstChanged(foldersFirst);
}

void DolphinView::updateAdditionalInfo(const KFileItemDelegate::InformationList& info)
{
    ViewProperties props(rootUrl());
    props.setAdditionalInfo(info);
    props.save();

    m_viewAccessor.itemDelegate()->setShowInformation(info);

    emit additionalInfoChanged();
}

void DolphinView::updateAdditionalInfoActions(KActionCollection* collection)
{
    const AdditionalInfoAccessor& infoAccessor = AdditionalInfoAccessor::instance();

    const KFileItemDelegate::InformationList checkedInfos = m_viewAccessor.itemDelegate()->showInformation();
    const KFileItemDelegate::InformationList infos = infoAccessor.keys();

    const bool enable = (m_mode == DolphinView::DetailsView) ||
                        (m_mode == DolphinView::IconsView);

    foreach (const KFileItemDelegate::Information& info, infos) {
        const QString name = infoAccessor.actionCollectionName(info, AdditionalInfoAccessor::AdditionalInfoType);
        QAction* action = collection->action(name);
        Q_ASSERT(action != 0);
        action->setEnabled(enable);
        action->setChecked(checkedInfos.contains(info));
    }
}

QPair<bool, QString> DolphinView::pasteInfo() const
{
    return KonqOperations::pasteInfo(url());
}

void DolphinView::setTabsForFilesEnabled(bool tabsForFiles)
{
    m_tabsForFiles = tabsForFiles;
}

bool DolphinView::isTabsForFilesEnabled() const
{
    return m_tabsForFiles;
}

bool DolphinView::itemsExpandable() const
{
    return m_viewAccessor.itemsExpandable();
}

void DolphinView::restoreState(QDataStream& stream)
{
    // current item
    stream >> m_activeItemUrl;

    // view position
    stream >> m_restoredContentsPosition;

    // expanded folders (only relevant for the details view - will be ignored by the view in other view modes)
    QSet<KUrl> urlsToExpand;
    stream >> urlsToExpand;
    const DolphinDetailsViewExpander* expander = m_viewAccessor.setExpandedUrls(urlsToExpand);
    if (expander != 0) {
        m_expanderActive = true;
        connect (expander, SIGNAL(completed()), this, SLOT(slotLoadingCompleted()));
    }
    else {
        m_expanderActive = false;
    }
}

void DolphinView::saveState(QDataStream& stream)
{
    // current item
    KFileItem currentItem;
    const QAbstractItemView* view = m_viewAccessor.itemView();

    if (view != 0) {
        const QModelIndex proxyIndex = view->currentIndex();
        const QModelIndex dirModelIndex = m_viewAccessor.proxyModel()->mapToSource(proxyIndex);
        currentItem = m_viewAccessor.dirModel()->itemForIndex(dirModelIndex);
    }

    KUrl currentUrl;
    if (!currentItem.isNull()) {
        currentUrl = currentItem.url();
    }

    stream << currentUrl;

    // view position
    const int x = view->horizontalScrollBar()->value();
    const int y = view->verticalScrollBar()->value();
    stream << QPoint(x, y);

    // expanded folders (only relevant for the details view - the set will be empty in other view modes)
    stream << m_viewAccessor.expandedUrls();
}

void DolphinView::observeCreatedItem(const KUrl& url)
{
    m_createdItemUrl = url;
    connect(m_viewAccessor.dirModel(), SIGNAL(rowsInserted(const QModelIndex&, int, int)),
            this, SLOT(selectAndScrollToCreatedItem()));
}

void DolphinView::selectAndScrollToCreatedItem()
{
    const QModelIndex dirIndex = m_viewAccessor.dirModel()->indexForUrl(m_createdItemUrl);
    if (dirIndex.isValid()) {
        const QModelIndex proxyIndex = m_viewAccessor.proxyModel()->mapFromSource(dirIndex);
        m_viewAccessor.itemView()->setCurrentIndex(proxyIndex);
    }

    disconnect(m_viewAccessor.dirModel(), SIGNAL(rowsInserted(const QModelIndex&, int, int)),
               this, SLOT(selectAndScrollToCreatedItem()));
    m_createdItemUrl = KUrl();
}

void DolphinView::showHoverInformation(const KFileItem& item)
{
    emit requestItemInfo(item);
}

void DolphinView::clearHoverInformation()
{
    emit requestItemInfo(KFileItem());
}

void DolphinView::slotDeleteFileFinished(KJob* job)
{
    if (job->error() == 0) {
        emit operationCompletedMessage(i18nc("@info:status", "Delete operation completed."));
    } else if (job->error() != KIO::ERR_USER_CANCELED) {
        emit errorMessage(job->errorString());
    }
}

void DolphinView::slotDirListerCompleted()
{
    if (!m_expanderActive) {
        slotLoadingCompleted();
    }

    if (!m_newFileNames.isEmpty()) {
        // select all newly added items created by a paste operation or
        // a drag & drop operation, and clear the previous selection
        m_viewAccessor.itemView()->clearSelection();
        const int rowCount = m_viewAccessor.proxyModel()->rowCount();
        QItemSelection selection;
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex proxyIndex = m_viewAccessor.proxyModel()->index(row, 0);
            const QModelIndex dirIndex = m_viewAccessor.proxyModel()->mapToSource(proxyIndex);
            const KUrl url = m_viewAccessor.dirModel()->itemForIndex(dirIndex).url();
            if (m_newFileNames.contains(url.fileName())) {
                selection.merge(QItemSelection(proxyIndex, proxyIndex), QItemSelectionModel::Select);
            }
        }
        m_viewAccessor.itemView()->selectionModel()->select(selection, QItemSelectionModel::Select);

        m_newFileNames.clear();
    }
}

void DolphinView::slotLoadingCompleted()
{
    m_expanderActive = false;

    if (!m_activeItemUrl.isEmpty()) {
        // assure that the current item remains visible
        const QModelIndex dirIndex = m_viewAccessor.dirModel()->indexForUrl(m_activeItemUrl);
        if (dirIndex.isValid()) {
            const QModelIndex proxyIndex = m_viewAccessor.proxyModel()->mapFromSource(dirIndex);
            QAbstractItemView* view = m_viewAccessor.itemView();
            const bool clearSelection = !hasSelection();
            view->setCurrentIndex(proxyIndex);
            if (clearSelection) {
                view->clearSelection();
            }
            m_activeItemUrl.clear();
        }
    }

    if (!m_selectedItems.isEmpty()) {
        const KUrl& baseUrl = url();
        KUrl url;
        QItemSelection newSelection;
        foreach(const KFileItem& item, m_selectedItems) {
            url = item.url().upUrl();
            if (baseUrl.equals(url, KUrl::CompareWithoutTrailingSlash)) {
                QModelIndex index = m_viewAccessor.proxyModel()->mapFromSource(m_viewAccessor.dirModel()->indexForItem(item));
                newSelection.select(index, index);
            }
        }
        m_viewAccessor.itemView()->selectionModel()->select(newSelection,
                                                            QItemSelectionModel::ClearAndSelect
                                                            | QItemSelectionModel::Current);
        m_selectedItems.clear();
    }

    // Restore the contents position. This has to be done using a Qt::QueuedConnection
    // because the view might not be in its final state yet.
    QMetaObject::invokeMethod(this, "restoreContentsPosition", Qt::QueuedConnection);
}

void DolphinView::slotRefreshItems()
{
    if (m_assureVisibleCurrentIndex) {
        m_assureVisibleCurrentIndex = false;
        m_viewAccessor.itemView()->scrollTo(m_viewAccessor.itemView()->currentIndex());
    }
}

void DolphinView::loadDirectory(const KUrl& url, bool reload)
{
    if (!url.isValid()) {
        const QString location(url.pathOrUrl());
        if (location.isEmpty()) {
            emit errorMessage(i18nc("@info:status", "The location is empty."));
        } else {
            emit errorMessage(i18nc("@info:status", "The location '%1' is invalid.", location));
        }
        return;
    }

    KDirLister* dirLister = m_viewAccessor.dirLister();
    dirLister->openUrl(url, reload ? KDirLister::Reload : KDirLister::NoFlags);
}

void DolphinView::applyViewProperties()
{
    if (m_ignoreViewProperties) {
        return;
    }

    const ViewProperties props(rootUrl());

    const Mode mode = props.viewMode();
    if (m_mode != mode) {
        const int oldZoomLevel = m_viewModeController->zoomLevel();

        m_mode = mode;
        createView();
        emit modeChanged();

        updateZoomLevel(oldZoomLevel);
    }
    if (m_viewAccessor.itemView() == 0) {
        createView();
    }
    Q_ASSERT(m_viewAccessor.itemView() != 0);
    Q_ASSERT(m_viewAccessor.itemDelegate() != 0);

    const bool showHiddenFiles = props.showHiddenFiles();
    if (showHiddenFiles != m_viewAccessor.dirLister()->showingDotFiles()) {
        m_viewAccessor.dirLister()->setShowingDotFiles(showHiddenFiles);
        emit showHiddenFilesChanged();
    }

    m_storedCategorizedSorting = props.categorizedSorting();
    const bool categorized = m_storedCategorizedSorting && supportsCategorizedSorting();
    if (categorized != m_viewAccessor.proxyModel()->isCategorizedModel()) {
        m_viewAccessor.proxyModel()->setCategorizedModel(categorized);
        emit categorizedSortingChanged();
    }

    const DolphinView::Sorting sorting = props.sorting();
    if (sorting != m_viewAccessor.proxyModel()->sorting()) {
        m_viewAccessor.proxyModel()->setSorting(sorting);
        emit sortingChanged(sorting);
    }

    const Qt::SortOrder sortOrder = props.sortOrder();
    if (sortOrder != m_viewAccessor.proxyModel()->sortOrder()) {
        m_viewAccessor.proxyModel()->setSortOrder(sortOrder);
        emit sortOrderChanged(sortOrder);
    }

    const bool sortFoldersFirst = props.sortFoldersFirst();
    if (sortFoldersFirst != m_viewAccessor.proxyModel()->sortFoldersFirst()) {
        m_viewAccessor.proxyModel()->setSortFoldersFirst(sortFoldersFirst);
        emit sortFoldersFirstChanged(sortFoldersFirst);
    }

    KFileItemDelegate::InformationList info = props.additionalInfo();
    if (info != m_viewAccessor.itemDelegate()->showInformation()) {
        m_viewAccessor.itemDelegate()->setShowInformation(info);
        emit additionalInfoChanged();
    }

    const bool showPreview = props.showPreview();
    if (showPreview != m_showPreview) {
        m_showPreview = showPreview;
        const int oldZoomLevel = m_viewModeController->zoomLevel();
        emit showPreviewChanged();

        // Enabling or disabling the preview might change the icon size of the view.
        // As the view does not emit a signal when the icon size has been changed,
        // the used zoom level of the controller must be adjusted manually:
        updateZoomLevel(oldZoomLevel);
    }

    if (DolphinSettings::instance().generalSettings()->globalViewProps()) {
        // During the lifetime of a DolphinView instance the global view properties
        // should not be changed. This allows e. g. to split a view and use different
        // view properties for each view.
        m_ignoreViewProperties = true;
    }
}

void DolphinView::createView()
{
    QAbstractItemView* view = m_viewAccessor.itemView();
    if ((view != 0) && (view->selectionModel() != 0)) {
        disconnect(view->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
                   this, SLOT(slotSelectionChanged(QItemSelection, QItemSelection)));
    }

    deleteView();

    Q_ASSERT(m_viewAccessor.itemView() == 0);
    m_viewAccessor.createView(this, m_dolphinViewController, m_viewModeController, m_mode);

    view = m_viewAccessor.itemView();
    Q_ASSERT(view != 0);
    view->installEventFilter(this);
    view->viewport()->installEventFilter(this);

    m_dolphinViewController->setItemView(view);

    const int zoomLevel = ZoomLevelInfo::zoomLevelForIconSize(view->iconSize());
    m_viewModeController->setZoomLevel(zoomLevel);

    connect(view->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            this, SLOT(slotSelectionChanged(QItemSelection, QItemSelection)));

    setFocusProxy(m_viewAccessor.layoutTarget());
    m_topLayout->insertWidget(1, m_viewAccessor.layoutTarget());
}

void DolphinView::deleteView()
{
    QAbstractItemView* view = m_viewAccessor.itemView();
    if (view != 0) {
        // It's important to set the keyboard focus to the parent
        // before deleting the view: Otherwise when having a split
        // view the other view will get the focus and will request
        // an activation (see DolphinView::eventFilter()).
        setFocusProxy(0);
        setFocus();

        m_topLayout->removeWidget(view);
        view->close();

        // disconnect all signal/slots
        disconnect(view);
        m_viewModeController->disconnect(view);
        view->disconnect();

        m_viewAccessor.deleteView();
    }
}

void DolphinView::pasteToUrl(const KUrl& url)
{
    addNewFileNames(QApplication::clipboard()->mimeData());
    KonqOperations::doPaste(this, url);
}

void DolphinView::updateZoomLevel(int oldZoomLevel)
{
    const int newZoomLevel = ZoomLevelInfo::zoomLevelForIconSize(m_viewAccessor.itemView()->iconSize());
    if (oldZoomLevel != newZoomLevel) {
        m_viewModeController->setZoomLevel(newZoomLevel);
        emit zoomLevelChanged(newZoomLevel);
    }
}

KUrl::List DolphinView::simplifiedSelectedUrls() const
{
    KUrl::List list = selectedUrls();
    if (itemsExpandable() ) {
        list = KDirModel::simplifiedUrlList(list);
    }
    return list;
}

QMimeData* DolphinView::selectionMimeData() const
{
    const QAbstractItemView* view = m_viewAccessor.itemView();
    Q_ASSERT((view != 0) && (view->selectionModel() != 0));
    const QItemSelection selection = m_viewAccessor.proxyModel()->mapSelectionToSource(view->selectionModel()->selection());
    return m_viewAccessor.dirModel()->mimeData(selection.indexes());
}

void DolphinView::addNewFileNames(const QMimeData* mimeData)
{
    const KUrl::List urls = KUrl::List::fromMimeData(mimeData);
    foreach (const KUrl& url, urls) {
        m_newFileNames.insert(url.fileName());
    }
}

DolphinView::ViewAccessor::ViewAccessor(DolphinSortFilterProxyModel* proxyModel) :
    m_iconsView(0),
    m_detailsView(0),
    m_columnsContainer(0),
    m_proxyModel(proxyModel),
    m_dragSource(0)
{
}

DolphinView::ViewAccessor::~ViewAccessor()
{
    delete m_dragSource;
    m_dragSource = 0;
}

void DolphinView::ViewAccessor::createView(QWidget* parent,
                                           DolphinViewController* dolphinViewController,
                                           const ViewModeController* viewModeController,
                                           Mode mode)
{
    Q_ASSERT(itemView() == 0);

    switch (mode) {
    case IconsView:
        m_iconsView = new DolphinIconsView(parent,
                                           dolphinViewController,
                                           viewModeController,
                                           m_proxyModel);
        break;

    case DetailsView:
        m_detailsView = new DolphinDetailsView(parent,
                                               dolphinViewController,
                                               viewModeController,
                                               m_proxyModel);
        break;

    case ColumnView:
        m_columnsContainer = new DolphinColumnViewContainer(parent,
                                                            dolphinViewController,
                                                            viewModeController);
        break;

    default:
        Q_ASSERT(false);
    }
}

void DolphinView::ViewAccessor::deleteView()
{
    QAbstractItemView* view = itemView();
    if (view != 0) {
        if (DragAndDropHelper::instance().isDragSource(view)) {
            // The view is a drag source (the feature "Open folders
            // during drag operations" is used). Deleting the view
            // during an ongoing drag operation is not allowed, so
            // this will postponed.
            if (m_dragSource != 0) {
                // the old stored view is obviously not the drag source anymore
                m_dragSource->deleteLater();
                m_dragSource = 0;
            }
            view->hide();
            m_dragSource = view;
        } else {
            view->deleteLater();
        }
    }

    m_iconsView = 0;
    m_detailsView = 0;

    if (m_columnsContainer != 0) {
        m_columnsContainer->deleteLater();
    }
    m_columnsContainer = 0;
}


void DolphinView::ViewAccessor::prepareUrlChange(const KUrl& url)
{
    if (m_columnsContainer != 0) {
        m_columnsContainer->showColumn(url);
    }
}

QAbstractItemView* DolphinView::ViewAccessor::itemView() const
{
    if (m_iconsView != 0) {
        return m_iconsView;
    }

    if (m_detailsView != 0) {
        return m_detailsView;
    }

    if (m_columnsContainer != 0) {
        return m_columnsContainer->activeColumn();
    }

    return 0;
}

KFileItemDelegate* DolphinView::ViewAccessor::itemDelegate() const
{
    return static_cast<KFileItemDelegate*>(itemView()->itemDelegate());
}

QWidget* DolphinView::ViewAccessor::layoutTarget() const
{
    if (m_columnsContainer != 0) {
        return m_columnsContainer;
    }
    return itemView();
}

KUrl DolphinView::ViewAccessor::rootUrl() const
{
    return (m_columnsContainer != 0) ? m_columnsContainer->rootUrl() : KUrl();
}

bool DolphinView::ViewAccessor::supportsCategorizedSorting() const
{
    return m_iconsView != 0;
}

bool DolphinView::ViewAccessor::itemsExpandable() const
{
    return (m_detailsView != 0) && m_detailsView->itemsExpandable();
}


QSet<KUrl> DolphinView::ViewAccessor::expandedUrls() const
{
    if (m_detailsView != 0) {
        return m_detailsView->expandedUrls();
    }

    return QSet<KUrl>();
}

const DolphinDetailsViewExpander* DolphinView::ViewAccessor::setExpandedUrls(const QSet<KUrl>& urlsToExpand)
{
    if ((m_detailsView != 0) && m_detailsView->itemsExpandable() && !urlsToExpand.isEmpty()) {
        // Check if another expander is already active and stop it if necessary.
        if(!m_detailsViewExpander.isNull()) {
            m_detailsViewExpander->stop();
        }

        m_detailsViewExpander = new DolphinDetailsViewExpander(m_detailsView, urlsToExpand);
        return m_detailsViewExpander;
    }
    else {
        return 0;
    }
}

bool DolphinView::ViewAccessor::reloadOnAdditionalInfoChange() const
{
    // the details view requires no reloading of the directory, as it maps
    // the file item delegate info to its columns internally
    return m_detailsView != 0;
}

DolphinModel* DolphinView::ViewAccessor::dirModel() const
{
    return static_cast<DolphinModel*>(proxyModel()->sourceModel());
}

DolphinSortFilterProxyModel* DolphinView::ViewAccessor::proxyModel() const
{
    if (m_columnsContainer != 0) {
        return static_cast<DolphinSortFilterProxyModel*>(m_columnsContainer->activeColumn()->model());
    }
    return m_proxyModel;
}

KDirLister* DolphinView::ViewAccessor::dirLister() const
{
    return dirModel()->dirLister();
}

void DolphinView::slotRedirection(const KUrl& oldUrl, const KUrl& newUrl)
{
    if (oldUrl.equals(url(), KUrl::CompareWithoutTrailingSlash)) {
        emit redirection(oldUrl, newUrl);
        m_viewModeController->redirectToUrl(newUrl); // #186947
    }
}

void DolphinView::restoreContentsPosition()
{
    if (!m_restoredContentsPosition.isNull()) {
        const int x = m_restoredContentsPosition.x();
        const int y = m_restoredContentsPosition.y();
        m_restoredContentsPosition = QPoint();

        QAbstractItemView* view = m_viewAccessor.itemView();
        Q_ASSERT(view != 0);
        view->horizontalScrollBar()->setValue(x);
        view->verticalScrollBar()->setValue(y);
    }
}

#include "dolphinview.moc"
