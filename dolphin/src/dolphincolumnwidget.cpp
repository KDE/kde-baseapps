/***************************************************************************
 *   Copyright (C) 2007 by Peter Penz <peter.penz@gmx.at>                  *
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

#include "dolphincolumnwidget.h"

#include "dolphinmodel.h"
#include "dolphincolumnview.h"
#include "dolphincontroller.h"
#include "dolphindirlister.h"
#include "dolphinsortfilterproxymodel.h"
#include "settings/dolphinsettings.h"
#include "dolphinviewautoscroller.h"
#include "dolphin_columnmodesettings.h"
#include "dolphin_generalsettings.h"
#include "draganddrophelper.h"
#include "folderexpander.h"
#include "selectionmanager.h"
#include "tooltips/tooltipmanager.h"

#include <kcolorscheme.h>
#include <kdirlister.h>
#include <kfileitem.h>
#include <kfilepreviewgenerator.h>
#include <kio/previewjob.h>
#include <kiconeffect.h>
#include <kjob.h>
#include <konqmimedata.h>

#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QPoint>
#include <QScrollBar>

DolphinColumnWidget::DolphinColumnWidget(QWidget* parent,
                                         DolphinColumnView* columnView,
                                         const KUrl& url) :
    QListView(parent),
    m_active(true),
    m_view(columnView),
    m_selectionManager(0),
    m_autoScroller(0),
    m_url(url),
    m_childUrl(),
    m_font(),
    m_decorationSize(),
    m_dirLister(0),
    m_dolphinModel(0),
    m_proxyModel(0),
    m_previewGenerator(0),
    m_toolTipManager(0),
    m_dropRect()
{
    setMouseTracking(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setSelectionBehavior(SelectItems);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDropIndicatorShown(false);
    setSelectionRectVisible(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    setVerticalScrollMode(QListView::ScrollPerPixel);
    setHorizontalScrollMode(QListView::ScrollPerPixel);

    m_autoScroller = new DolphinViewAutoScroller(this);

    // apply the column mode settings to the widget
    const ColumnModeSettings* settings = DolphinSettings::instance().columnModeSettings();
    Q_ASSERT(settings != 0);

    if (settings->useSystemFont()) {
        m_font = KGlobalSettings::generalFont();
    } else {
        m_font = QFont(settings->fontFamily(),
                       settings->fontSize(),
                       settings->fontWeight(),
                       settings->italicFont());
    }

    const int iconSize = settings->iconSize();
    setDecorationSize(QSize(iconSize, iconSize));

    KFileItemDelegate* delegate = new KFileItemDelegate(this);
    delegate->setShowToolTipWhenElided(false);
    setItemDelegate(delegate);

    activate();

    connect(this, SIGNAL(viewportEntered()),
            m_view->m_controller, SLOT(emitViewportEntered()));
    connect(this, SIGNAL(entered(const QModelIndex&)),
            this, SLOT(slotEntered(const QModelIndex&)));

    m_dirLister = new DolphinDirLister();
    m_dirLister->setAutoUpdate(true);
    m_dirLister->setMainWindow(window());
    m_dirLister->setDelayedMimeTypes(true);
    const bool showHiddenFiles = m_view->m_controller->dolphinView()->showHiddenFiles();
    m_dirLister->setShowingDotFiles(showHiddenFiles);

    m_dolphinModel = new DolphinModel(this);
    m_dolphinModel->setDirLister(m_dirLister);
    m_dolphinModel->setDropsAllowed(DolphinModel::DropOnDirectory);

    m_proxyModel = new DolphinSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_dolphinModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    const DolphinView* dolphinView = m_view->m_controller->dolphinView();
    m_proxyModel->setSorting(dolphinView->sorting());
    m_proxyModel->setSortOrder(dolphinView->sortOrder());
    m_proxyModel->setSortFoldersFirst(dolphinView->sortFoldersFirst());

    setModel(m_proxyModel);

    if (DolphinSettings::instance().generalSettings()->showSelectionToggle()) {
        m_selectionManager = new SelectionManager(this);
        connect(m_selectionManager, SIGNAL(selectionChanged()),
                this, SLOT(requestActivation()));
        connect(m_view->m_controller, SIGNAL(urlChanged(const KUrl&)),
                m_selectionManager, SLOT(reset()));
    }

    m_previewGenerator = new KFilePreviewGenerator(this);
    m_previewGenerator->setPreviewShown(m_view->m_controller->dolphinView()->showPreview());

    if (DolphinSettings::instance().generalSettings()->showToolTips()) {
        m_toolTipManager = new ToolTipManager(this, m_proxyModel);
    }

    m_dirLister->openUrl(url, KDirLister::NoFlags);

    connect(KGlobalSettings::self(), SIGNAL(kdisplayFontChanged()),
            this, SLOT(updateFont()));

    FolderExpander* folderExpander = new FolderExpander(this, m_proxyModel);
    folderExpander->setEnabled(DolphinSettings::instance().generalSettings()->autoExpandFolders());
    connect (folderExpander, SIGNAL(enterDir(const QModelIndex&)),
             m_view->m_controller, SLOT(triggerItem(const QModelIndex&)));
}

DolphinColumnWidget::~DolphinColumnWidget()
{
    delete m_proxyModel;
    m_proxyModel = 0;
    delete m_dolphinModel;
    m_dolphinModel = 0;
    m_dirLister = 0; // deleted by m_dolphinModel
}

void DolphinColumnWidget::setDecorationSize(const QSize& size)
{
    setIconSize(size);
    m_decorationSize = size;
    doItemsLayout();
    if (m_previewGenerator != 0) {
        m_previewGenerator->updateIcons();
    }
    if (m_selectionManager != 0) {
        m_selectionManager->reset();
    }
}

void DolphinColumnWidget::setActive(bool active)
{
    if (active && (m_view->focusProxy() != this)) {
        m_view->setFocusProxy(this);
    }

    if (m_active != active) {
        m_active = active;

        if (active) {
            activate();
        } else {
            deactivate();
        }
    }
}

void DolphinColumnWidget::reload()
{
    m_dirLister->stop();
    m_dirLister->openUrl(m_url, KDirLister::Reload);
}

void DolphinColumnWidget::setSorting(DolphinView::Sorting sorting)
{
    m_proxyModel->setSorting(sorting);
}

void DolphinColumnWidget::setSortOrder(Qt::SortOrder order)
{
    m_proxyModel->setSortOrder(order);
}

void DolphinColumnWidget::setSortFoldersFirst(bool foldersFirst)
{
    m_proxyModel->setSortFoldersFirst(foldersFirst);
}

void DolphinColumnWidget::setShowHiddenFiles(bool show)
{
    if (show != m_dirLister->showingDotFiles()) {
        m_dirLister->setShowingDotFiles(show);
        m_dirLister->stop();
        m_dirLister->openUrl(m_url, KDirLister::Reload);
    }
}

void DolphinColumnWidget::setShowPreview(bool show)
{
    m_previewGenerator->setPreviewShown(show);

    m_dirLister->stop();
    m_dirLister->openUrl(m_url, KDirLister::Reload);
}

void DolphinColumnWidget::updateBackground()
{
    // TODO: The alpha-value 150 is copied from DolphinView::setActive(). When
    // cleaning up the cut-indication of DolphinColumnWidget with the code from
    // DolphinView a common helper-class should be available which can be shared
    // by all view implementations -> no hardcoded value anymore
    const QPalette::ColorRole role = viewport()->backgroundRole();
    QColor color = viewport()->palette().color(role);
    color.setAlpha((m_active && m_view->m_active) ? 255 : 150);

    QPalette palette = viewport()->palette();
    palette.setColor(role, color);
    viewport()->setPalette(palette);

    update();
}

void DolphinColumnWidget::setNameFilter(const QString& nameFilter)
{
    m_proxyModel->setFilterRegExp(nameFilter);
}

void DolphinColumnWidget::editItem(const KFileItem& item)
{
    const QModelIndex dirIndex = m_dolphinModel->indexForItem(item);
    const QModelIndex proxyIndex = m_proxyModel->mapFromSource(dirIndex);
    if (proxyIndex.isValid()) {
        edit(proxyIndex);
    }
}

KFileItem DolphinColumnWidget::itemAt(const QPoint& pos) const
{
    KFileItem item;
    const QModelIndex index = indexAt(pos);
    if (index.isValid() && (index.column() == DolphinModel::Name)) {
        const QModelIndex dolphinModelIndex = m_proxyModel->mapToSource(index);
        item = m_dolphinModel->itemForIndex(dolphinModelIndex);
    }
    return item;
}

KFileItemList DolphinColumnWidget::selectedItems() const
{
    const QItemSelection selection = m_proxyModel->mapSelectionToSource(selectionModel()->selection());
    KFileItemList itemList;

    const QModelIndexList indexList = selection.indexes();
    foreach (const QModelIndex &index, indexList) {
        KFileItem item = m_dolphinModel->itemForIndex(index);
        if (!item.isNull()) {
            itemList.append(item);
        }
    }

    return itemList;
}

QMimeData* DolphinColumnWidget::selectionMimeData() const
{
    const QItemSelection selection = m_proxyModel->mapSelectionToSource(selectionModel()->selection());
    return m_dolphinModel->mimeData(selection.indexes());
}

QStyleOptionViewItem DolphinColumnWidget::viewOptions() const
{
    QStyleOptionViewItem viewOptions = QListView::viewOptions();
    viewOptions.font = m_font;
    viewOptions.decorationSize = m_decorationSize;
    viewOptions.showDecorationSelected = true;
    return viewOptions;
}

void DolphinColumnWidget::startDrag(Qt::DropActions supportedActions)
{
    DragAndDropHelper::instance().startDrag(this, supportedActions, m_view->m_controller);
}

void DolphinColumnWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (DragAndDropHelper::instance().isMimeDataSupported(event->mimeData())) {
        event->acceptProposedAction();
        requestActivation();
    }
}

void DolphinColumnWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    QListView::dragLeaveEvent(event);
    setDirtyRegion(m_dropRect);
}

void DolphinColumnWidget::dragMoveEvent(QDragMoveEvent* event)
{
    QListView::dragMoveEvent(event);

    // TODO: remove this code when the issue #160611 is solved in Qt 4.4
    const QModelIndex index = indexAt(event->pos());
    setDirtyRegion(m_dropRect);

    m_dropRect.setSize(QSize()); // set as invalid
    if (index.isValid()) {
        m_view->m_controller->setItemView(this);
        const KFileItem item = m_view->m_controller->itemForIndex(index);
        if (!item.isNull() && item.isDir()) {
            m_dropRect = visualRect(index);
        }
    }
    setDirtyRegion(m_dropRect);

    if (DragAndDropHelper::instance().isMimeDataSupported(event->mimeData())) {
        // accept url drops, independently from the destination item
        event->acceptProposedAction();
    }
}

void DolphinColumnWidget::dropEvent(QDropEvent* event)
{
    const QModelIndex index = indexAt(event->pos());
    m_view->m_controller->setItemView(this);
    const KFileItem item = m_view->m_controller->itemForIndex(index);
    m_view->m_controller->indicateDroppedUrls(item, url(), event);
    QListView::dropEvent(event);
}

void DolphinColumnWidget::paintEvent(QPaintEvent* event)
{
    if (!m_childUrl.isEmpty()) {
        // indicate the shown URL of the next column by highlighting the shown folder item
        const QModelIndex dirIndex = m_dolphinModel->indexForUrl(m_childUrl);
        const QModelIndex proxyIndex = m_proxyModel->mapFromSource(dirIndex);
        if (proxyIndex.isValid() && !selectionModel()->isSelected(proxyIndex)) {
            const QRect itemRect = visualRect(proxyIndex);
            QPainter painter(viewport());
            QColor color = KColorScheme(QPalette::Active, KColorScheme::View).foreground().color();
            color.setAlpha(32);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawRect(itemRect);
        }
    }

    QListView::paintEvent(event);
}

void DolphinColumnWidget::mousePressEvent(QMouseEvent* event)
{
    requestActivation();
    if (!indexAt(event->pos()).isValid()) {
        if (QApplication::mouseButtons() & Qt::MidButton) {
            m_view->m_controller->replaceUrlByClipboard();
        }
    } else if (event->button() == Qt::LeftButton) {
        // TODO: see comment in DolphinIconsView::mousePressEvent()
        setState(QAbstractItemView::DraggingState);
    }
    QListView::mousePressEvent(event);
}

void DolphinColumnWidget::keyPressEvent(QKeyEvent* event)
{
    QListView::keyPressEvent(event);
    requestActivation();

    DolphinController* controller = m_view->m_controller;
    controller->handleKeyPressEvent(event);
    switch (event->key()) {
    case Qt::Key_Right: {
        // Special key handling for the column: A Key_Right should
        // open a new column for the currently selected folder.
        const QModelIndex index = currentIndex();
        const KFileItem item = controller->itemForIndex(index);
        if (!item.isNull() && item.isDir()) {
            controller->emitItemTriggered(item);
        }
        break;
    }

    case Qt::Key_Escape:
        selectionModel()->setCurrentIndex(selectionModel()->currentIndex(),
                                          QItemSelectionModel::Current |
                                          QItemSelectionModel::Clear);
        break;

    default:
        break;
    }

    if (m_toolTipManager != 0) {
        m_toolTipManager->hideTip();
    }
}

void DolphinColumnWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_active) {
        m_view->requestActivation(this);
        Q_ASSERT(m_view->m_controller->itemView() == this);
        m_view->m_controller->triggerUrlChangeRequest(m_url);
    }
    Q_ASSERT(m_active);

    QListView::contextMenuEvent(event);

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        clearSelection();
    }

    if (m_toolTipManager != 0) {
        m_toolTipManager->hideTip();
    }

    const QPoint pos = m_view->viewport()->mapFromGlobal(event->globalPos());
    Q_ASSERT(m_view->m_controller->itemView() == this);
    m_view->m_controller->triggerContextMenuRequest(pos);
}

void DolphinColumnWidget::wheelEvent(QWheelEvent* event)
{
    if (m_selectionManager != 0) {
        m_selectionManager->reset();
    }

    // let Ctrl+wheel events propagate to the DolphinView for icon zooming
    if (event->modifiers() & Qt::ControlModifier) {
        event->ignore();
        return;
    }

    const int height = m_decorationSize.height();
    const int step = (height >= KIconLoader::SizeHuge) ? height / 10 : (KIconLoader::SizeHuge - height) / 2;
    verticalScrollBar()->setSingleStep(step);

    QListView::wheelEvent(event);
}

void DolphinColumnWidget::leaveEvent(QEvent* event)
{
    QListView::leaveEvent(event);
    // if the mouse is above an item and moved very fast outside the widget,
    // no viewportEntered() signal might be emitted although the mouse has been moved
    // above the viewport
    m_view->m_controller->emitViewportEntered();
}

void DolphinColumnWidget::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    QListView::selectionChanged(selected, deselected);

    QItemSelectionModel* selModel = m_view->selectionModel();
    selModel->select(selected, QItemSelectionModel::Select);
    selModel->select(deselected, QItemSelectionModel::Deselect);
}

void DolphinColumnWidget::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    QListView::currentChanged(current, previous);
    m_autoScroller->handleCurrentIndexChange(current, previous);
}

void DolphinColumnWidget::slotEntered(const QModelIndex& index)
{
    m_view->m_controller->setItemView(this);
    m_view->m_controller->emitItemEntered(index);
}

void DolphinColumnWidget::requestActivation()
{
    m_view->m_controller->setItemView(this);
    m_view->m_controller->requestActivation();
    if (!m_active) {
        m_view->requestActivation(this);
        m_view->m_controller->triggerUrlChangeRequest(m_url);
        selectionModel()->clear();
    }
}

void DolphinColumnWidget::updateFont()
{
    const ColumnModeSettings* settings = DolphinSettings::instance().columnModeSettings();
    Q_ASSERT(settings != 0);

    if (settings->useSystemFont()) {
        m_font = KGlobalSettings::generalFont();
    }
}

void DolphinColumnWidget::activate()
{
    setFocus(Qt::OtherFocusReason);

    if (KGlobalSettings::singleClick()) {
        connect(this, SIGNAL(clicked(const QModelIndex&)),
                m_view->m_controller, SLOT(triggerItem(const QModelIndex&)));
    } else {
        connect(this, SIGNAL(doubleClicked(const QModelIndex&)),
                m_view->m_controller, SLOT(triggerItem(const QModelIndex&)));
    }

    if (selectionModel() && selectionModel()->currentIndex().isValid()) {
        selectionModel()->setCurrentIndex(selectionModel()->currentIndex(), QItemSelectionModel::SelectCurrent);
    }

    updateBackground();
}

void DolphinColumnWidget::deactivate()
{
    clearFocus();
    if (KGlobalSettings::singleClick()) {
        disconnect(this, SIGNAL(clicked(const QModelIndex&)),
                   m_view->m_controller, SLOT(triggerItem(const QModelIndex&)));
    } else {
        disconnect(this, SIGNAL(doubleClicked(const QModelIndex&)),
                   m_view->m_controller, SLOT(triggerItem(const QModelIndex&)));
    }

    const QModelIndex current = selectionModel()->currentIndex();
    selectionModel()->clear();
    selectionModel()->setCurrentIndex(current, QItemSelectionModel::NoUpdate);
    updateBackground();
}

#include "dolphincolumnwidget.moc"
