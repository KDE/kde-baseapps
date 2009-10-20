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

#include "iconview.h"
#ifdef Q_WS_X11
#include <fixx11h.h>
#endif
#include <QActionGroup>
#include <QApplication>
#include <QDebug>
#include <QDesktopWidget>
#include <QDrag>
#include <QGraphicsView>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsProxyWidget>
#include <QImageReader>
#include <QItemSelectionModel>
#include <QPainter>
#include <QPaintEngine>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>

#include <KDirModel>
#include <KDesktopFile>
#include <KFileItemDelegate>
#include <KGlobalSettings>
#include <KIcon>
#include <KProtocolInfo>

#include <KIO/NetAccess>

#include <konqmimedata.h>
#include <konq_operations.h>

#include "dirlister.h"
#include "folderview.h"
#include "proxymodel.h"
#include "previewpluginsmodel.h"
#include "tooltipwidget.h"

#include <Plasma/Containment>
#include <Plasma/Corona>
#include <Plasma/PaintUtils>
#include <Plasma/Theme>
#include <Plasma/ToolTipManager>


IconView::IconView(QGraphicsWidget *parent)
    : AbstractItemView(parent),
      m_columns(0),
      m_rows(0),
      m_validRows(0),
      m_layoutBroken(false),
      m_needPostLayoutPass(false),
      m_initialListing(true),
      m_positionsLoaded(false),
      m_doubleClick(false),
      m_dragInProgress(false),
      m_hoverDrag(false),
      m_iconsLocked(false),
      m_alignToGrid(false),
      m_wordWrap(false),
      m_drawShadows(true),
      m_flow(layoutDirection() == Qt::LeftToRight ? LeftToRight : RightToLeft),
      m_popupCausedWidget(0),
      m_dropOperation(0),
      m_dropActions(0),
      m_editorProxy(0)
{
    setAcceptHoverEvents(true);
    setAcceptDrops(true);
    setCacheMode(NoCache);
    setFocusPolicy(Qt::StrongFocus);

    m_scrollBar->hide();
    connect(m_scrollBar, SIGNAL(valueChanged(int)), SLOT(repositionWidgetsManually()));

    int size = style()->pixelMetric(QStyle::PM_LargeIconSize);
    m_iconSize = QSize(size, size);
    m_gridSize = QSize(size * 2, size * 2);

    m_toolTipWidget = new ToolTipWidget(this);
    m_toolTipWidget->hide();

    getContentsMargins(&m_margins[Plasma::LeftMargin], &m_margins[Plasma::TopMargin],
                       &m_margins[Plasma::RightMargin], &m_margins[Plasma::BottomMargin]);
}

IconView::~IconView()
{
    // Make sure that we don't leave any open popup views on the screen when we're deleted
    delete m_popupView;
}

void IconView::setModel(QAbstractItemModel *model)
{
    AbstractItemView::setModel(model);

    KDirLister *lister = m_dirModel->dirLister();
    connect(lister, SIGNAL(started(KUrl)), SLOT(listingStarted(KUrl)));
    connect(lister, SIGNAL(clear()), SLOT(listingClear()));
    connect(lister, SIGNAL(completed()), SLOT(listingCompleted()));
    connect(lister, SIGNAL(canceled()), SLOT(listingCanceled()));
    connect(lister, SIGNAL(showErrorMessage(QString)), SLOT(listingError(QString)));
    connect(lister, SIGNAL(itemsDeleted(KFileItemList)), SLOT(itemsDeleted(KFileItemList)));

    m_validRows = 0;
    m_layoutBroken = false;

    if (m_model->rowCount() > 0) {
        m_delayedLayoutTimer.start(10, this);
        emit busy(true);
    }
}

void IconView::setIconSize(const QSize &size)
{
    if (size != m_iconSize) {
        m_iconSize = size;

        // Schedule a full relayout
        if (m_validRows > 0) {
            m_validRows = 0;
            m_delayedLayoutTimer.start(10, this);
            emit busy(true);
        }
    }
}

void IconView::setGridSize(const QSize &size)
{
    if (size != m_gridSize) {
        m_gridSize = size;

        // Schedule a full relayout
        if (m_validRows > 0) {
            m_validRows = 0;
            m_delayedLayoutTimer.start(10, this);
            emit busy(true);
        }
    }
}

QSize IconView::gridSize() const
{
    return m_gridSize;
}

void IconView::setWordWrap(bool on)
{
    if (m_wordWrap != on) {
        m_wordWrap = on;

        // Schedule a full relayout
        if (m_validRows > 0) {
            m_validRows = 0;
            m_delayedLayoutTimer.start(10, this);
            emit busy(true);
        }
    }
}

bool IconView::wordWrap() const
{
    return m_wordWrap;
}

void IconView::setFlow(Flow flow)
{
    if (m_flow != flow) {
        m_flow = flow;

        // Schedule a full relayout
        if (!m_layoutBroken && m_validRows > 0) {
            m_validRows = 0;
            m_delayedLayoutTimer.start(10, this);
            emit busy(true);
        }
    }
}

IconView::Flow IconView::flow() const
{
    return m_flow;
}

void IconView::setAlignToGrid(bool on)
{
    if (on && !m_alignToGrid && m_validRows > 0) {
        alignIconsToGrid();
    }

    m_alignToGrid = on;
}

bool IconView::alignToGrid() const
{
    return m_alignToGrid;
}

void IconView::setIconsMoveable(bool on)
{
    m_iconsLocked = !on;
}

bool IconView::iconsMoveable() const
{
    return !m_iconsLocked;
}

void IconView::setDrawShadows(bool on)
{
    if (m_drawShadows != on) {
        m_drawShadows = on;
        markAreaDirty(visibleArea());
        update();
    }
}

bool IconView::drawShadows() const
{
    return m_drawShadows;
}

void IconView::setCustomLayout(bool value)
{
    m_layoutBroken = value;
}

bool IconView::customLayout() const
{
    return m_layoutBroken;
}

void IconView::setIconPositionsData(const QStringList &data)
{
    // Sanity checks
    if (data.size() < 5 || data.at(0).toInt() != 1 || ((data.size() - 2) % 3) ||
        data.at(1).toInt() != ((data.size() - 2) / 3)) {
        return;
    }

    const QPoint offset = contentsRect().topLeft().toPoint();
    for (int i = 2; i < data.size(); i += 3) {
        const QString &name = data.at(i);
        int x = data.at(i + 1).toInt();
        int y = data.at(i + 2).toInt();
        m_savedPositions.insert(name, QPoint(x, y) + offset);
    }
}

QStringList IconView::iconPositionsData() const
{
    QStringList data;

    if (m_layoutBroken && !m_initialListing && m_validRows == m_items.size()) {
        int version = 1;
        data << QString::number(version);
        data << QString::number(m_items.size());

        const QPoint offset = contentsRect().topLeft().toPoint();
        const QSize size = gridSize();
        for (int i = 0; i < m_items.size(); i++) {
            QModelIndex index = m_model->index(i, 0);
            KFileItem item = m_model->itemForIndex(index);
            data << item.name();
            data << QString::number(m_items[i].rect.x() - offset.x());
            data << QString::number(m_items[i].rect.y() - offset.y());
        }
    }

    return data;
}

void IconView::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    m_regionCache.clear();

    if (!m_layoutBroken || m_initialListing) {
        if (first < m_validRows) {
            m_validRows = 0;
        }
        m_delayedLayoutTimer.start(10, this);
        emit busy(true);
    } else {
        const QStyleOptionViewItemV4 option = viewOptions();
        const QRect cr = contentsRect().toRect();
        const QSize grid = gridSize();
        QPoint pos = QPoint();

        m_items.insert(first, last - first + 1, ViewItem());

        // If a single item was inserted and we have a saved position from a deleted file,
        // reuse that position.
        if (first == last && !m_lastDeletedPos.isNull()) {
            m_items[first].rect = QRect(m_lastDeletedPos, grid);
            m_items[first].layouted = true;
            m_items[first].needSizeAdjust = true;
            markAreaDirty(m_items[first].rect);
            m_lastDeletedPos = QPoint();
            m_validRows = m_items.size();
            return;
        }

        // Lay out the newly inserted files
        for (int i = first; i <= last; i++) {
            pos = findNextEmptyPosition(pos, grid, cr);
            m_items[i].rect = QRect(pos, grid);
            m_items[i].layouted = true;
            m_items[i].needSizeAdjust = true;
            markAreaDirty(m_items[i].rect);
        }

        m_validRows = m_items.size();
        updateScrollBar();
    }
}

void IconView::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)

    m_regionCache.clear();

    if (!m_layoutBroken) {
        if (first < m_validRows) {
            m_validRows = 0;
        }
        if (m_model->rowCount() > 0) {
            m_delayedLayoutTimer.start(10, this);
            emit busy(true);
        } else {
            // All the items were removed
            m_items.clear();
            updateScrollBar();
            markAreaDirty(visibleArea());
        }
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
        m_validRows = m_items.size();
        updateScrollBar();
    }
}

void IconView::modelReset()
{
    m_savedPositions.clear();
    m_layoutBroken = false;
    m_validRows = 0;

    m_delayedLayoutTimer.start(10, this);
    emit busy(true);
}

void IconView::layoutChanged()
{
    if (m_validRows > 0) {
        m_savedPositions.clear();
        m_layoutBroken = false;
        m_validRows = 0;
    } else if (m_layoutBroken && m_savedPositions.isEmpty()) {
        // Make sure that the new sorting order is applied to
        // new files if the folder is empty.
        m_layoutBroken = false;
    }
    m_delayedLayoutTimer.start(10, this);
    emit busy(true);
}

void IconView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    const QStyleOptionViewItemV4 option = viewOptions();
    const QSize grid = gridSize();
    m_regionCache.clear();

    // Update the size of the items and center them in the grid cell
    for (int i = topLeft.row(); i <= bottomRight.row() && i < m_items.size(); i++) {
        if (!m_items[i].layouted) {
            continue;
        }
        m_items[i].rect.setSize(grid);
        m_items[i].needSizeAdjust = true;
        markAreaDirty(m_items[i].rect);
    }
}

void IconView::listingStarted(const KUrl &url)
{
    Q_UNUSED(url)

    // Reset any error message that may have resulted from an earlier listing
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        update();
    }

    emit busy(true);
}

void IconView::listingClear()
{
    m_initialListing = true;
    markAreaDirty(visibleArea());
    updateScrollBar();
    update();
}

void IconView::listingCompleted()
{
    m_delayedCacheClearTimer.start(5000, this);
    m_initialListing = false;

    if (m_validRows == m_model->rowCount()) {
        emit busy(false);
    }
}

void IconView::listingCanceled()
{
    m_delayedCacheClearTimer.start(5000, this);
    m_initialListing = false;

    if (m_validRows == m_model->rowCount()) {
        emit busy(false);
    }
}

void IconView::listingError(const QString &message)
{
    m_errorMessage = message;
    markAreaDirty(visibleArea());
    update();

    if (m_validRows == m_model->rowCount()) {
        emit busy(false);
    }
}

void IconView::itemsDeleted(const KFileItemList &items)
{
    // Check if the root item was deleted
    if (items.contains(m_dirModel->dirLister()->rootItem())) {
        const QString path = m_dirModel->dirLister()->url().toLocalFile();
        listingError(KIO::buildErrorString(KIO::ERR_DOES_NOT_EXIST, path));
    }
}

int IconView::columnsForWidth(qreal width) const
{
    int spacing = 10;
    int margin = 10;

    qreal available = width - 2 * margin + spacing;
    return qFloor(available / (gridSize().width() + spacing));
}

int IconView::rowsForHeight(qreal height) const
{
    int spacing = 10;
    int margin = 10;

    qreal available = height - 2 * margin + spacing;
    return qFloor(available / (gridSize().height() + spacing));
}

QPoint inline IconView::nextGridPosition(const QPoint &lastPos, const QSize &grid, const QRect &contentRect) const
{
    int spacing = 10;
    int margin = 10;

    QRect r = contentRect.adjusted(margin, margin, -margin, -margin);
    if (m_flow == LeftToRight || m_flow == RightToLeft) {
        if (layoutDirection() == Qt::LeftToRight) {
            r.adjust(0, 0, -m_scrollBar->geometry().width(), 0); 
        } else {
            r.adjust(m_scrollBar->geometry().width(), 0, 0, 0); 
        }
    }

    const int xOrigin = (m_flow == LeftToRight || m_flow == TopToBottom) ?
                    r.left() :  r.right() - grid.width();

    if (lastPos.isNull()) {
        return QPoint(xOrigin, r.top());
    }

    QPoint pos = lastPos;

    switch (m_flow) {
    case LeftToRight:
        pos.rx() += grid.width() + spacing;
        if (pos.x() + grid.width() >= r.right()) {
            pos.ry() += grid.height() + spacing;
            pos.rx() = xOrigin;
        }
        break;

    case RightToLeft:
        pos.rx() -= grid.width() + spacing;
        if (pos.x() < r.left()) {
            pos.ry() += grid.height() + spacing;
            pos.rx() = xOrigin;
        }
        break;

    case TopToBottom: 
    case TopToBottomRightToLeft: 
        pos.ry() += grid.height() + spacing;
        if (pos.y() + grid.height() >= r.bottom()) {
            if (m_flow == TopToBottom) {
                pos.rx() += grid.width() + spacing;
            } else { // RightToLeft
                pos.rx() -= grid.width() + spacing;
            }
            pos.ry() = r.top();
        }
    }

    return pos;
}

QPoint IconView::findNextEmptyPosition(const QPoint &prevPos, const QSize &gridSize, const QRect &contentRect) const
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

void IconView::layoutItems()
{
    QStyleOptionViewItemV4 option = viewOptions();
    m_items.resize(m_model->rowCount());
    m_regionCache.clear();

    const QRect visibleRect = mapToViewport(contentsRect()).toAlignedRect();
    const QRect rect = contentsRect().toRect();
    const QSize grid = gridSize();
    int maxWidth = rect.width();
    int maxHeight = rect.height();
    if (m_flow == LeftToRight || m_flow == RightToLeft) {
        maxWidth -= m_scrollBar->geometry().width();
    }
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
                const QPoint pos = m_savedPositions.value(item.name(), QPoint(-1, -1));
                if (pos != QPoint(-1, -1)) {
                    m_items[i].rect = QRect(pos, grid);
                    m_items[i].layouted = true;
                    m_items[i].needSizeAdjust = true;
                    if (m_items[i].rect.intersects(visibleRect)) {
                        needUpdate = true;
                    }
                } else {
                    // We don't have a saved position for this file, so we'll record the
                    // size and lay it out in a second layout pass.
                    m_items[i].rect = QRect(QPoint(), grid);
                    m_items[i].layouted = false;
                    m_items[i].needSizeAdjust = true;
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
                pos = nextGridPosition(pos, grid, rect);
                m_items[i].rect = QRect(pos, grid);
                m_items[i].layouted = true;
                m_items[i].needSizeAdjust = true;
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
            m_items[i].rect.moveTo(pos);
            if (m_items[i].rect.intersects(visibleRect)) {
                needUpdate = true;
            }
        }
        needUpdate |= doLayoutSanityCheck();
        m_needPostLayoutPass = false;
        emit busy(false);
        return;
    }

    if (m_validRows < m_items.size() || m_needPostLayoutPass) {
        m_delayedLayoutTimer.start(10, this);
    } else if (!m_initialListing) {
        emit busy(false);
    }

    if (needUpdate) {
        m_dirtyRegion = QRegion(visibleRect);
        update();
    }

    updateScrollBar();
}

void IconView::alignIconsToGrid()
{
    int margin = 10;
    int spacing = 10;
    const QRect cr = contentsRect().toRect();
    const QSize size = gridSize() + QSize(spacing, spacing);
    int topMargin = margin + cr.top();
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
        markAreaDirty(visibleArea());
        m_layoutBroken = true;
        m_savedPositions.clear();
        m_regionCache.clear();
    }
}

QRect IconView::itemsBoundingRect() const
{
    QRect boundingRect;
    for (int i = 0; i < m_validRows; i++) {
        if (m_items[i].layouted) {
            boundingRect |= m_items[i].rect;
        }
    }

    return boundingRect;
}

bool IconView::doLayoutSanityCheck()
{
    // Find the bounding rect of the items
    QRect boundingRect = itemsBoundingRect();

    // Add the margin
    boundingRect.adjust(-10, -10, 10, 10);

    const QRect cr = contentsRect().toRect();
    int scrollValue = m_scrollBar->value();
    QPoint delta(0, 0);

    // Make sure no items have negative coordinates
    if (boundingRect.y() < cr.top() || boundingRect.x() < cr.left()) {
        delta.rx() = qMax(0, cr.left() - boundingRect.x());
        delta.ry() = qMax(0, cr.top() - boundingRect.y());
    }

    // Remove any empty space above the visible area
    if (delta.y() == 0 && scrollValue > 0) {
        delta.ry() = -qBound(0, boundingRect.top() - cr.top(), scrollValue);
    }

    if (!delta.isNull()) {
        // Move the items
        for (int i = 0; i < m_validRows; i++) {
            if (m_items[i].layouted) {
                m_items[i].rect.translate(delta);
            }
        }

        // Adjust the bounding rect and the scrollbar value and range
        boundingRect = boundingRect.translated(delta) | cr;
        scrollValue += delta.y();

        m_scrollBar->setRange(0, qMax(boundingRect.height() - cr.height(), scrollValue));
        m_scrollBar->setValue(scrollValue);

        if (m_scrollBar->minimum() != m_scrollBar->maximum()) {
            m_scrollBar->show();
        } else {
            m_scrollBar->hide();
        }

        m_regionCache.clear();
        return true;
    }

    boundingRect |= cr;
    m_scrollBar->setRange(0, qMax(boundingRect.height() - cr.height(), scrollValue));
    m_scrollBar->setValue(scrollValue);

    if (m_scrollBar->minimum() != m_scrollBar->maximum()) {
        m_scrollBar->show();
    } else {
        m_scrollBar->hide();
    }

    return false;
}

void IconView::updateScrollBar()
{
    const QRect cr = contentsRect().toRect();
    QRect boundingRect = itemsBoundingRect();

    if (boundingRect.isValid()) {
        // Add the margin
        boundingRect.adjust(-10, -10, 10, 10);
        boundingRect |= cr;

        m_scrollBar->setRange(0, boundingRect.height() - cr.height());
        m_scrollBar->setPageStep(cr.height());
        m_scrollBar->setSingleStep(gridSize().height());
    } else {
        // The view is empty
        m_scrollBar->setRange(0, 0);
    }

    // Update the scrollbar visibility
    if (m_scrollBar->minimum() != m_scrollBar->maximum()) {
        m_scrollBar->show();
    } else {
        m_scrollBar->hide();
    }
}

void IconView::finishedScrolling()
{
    // Find the bounding rect of the items
    QRect boundingRect = itemsBoundingRect();
    
    if (boundingRect.isValid()) {
        // Add the margin
        boundingRect.adjust(-10, -10, 10, 10);

        const QRect cr = contentsRect().toRect();

        // Remove any empty space above the visible area by shifting all the items
        // and adjusting the scrollbar range.
        int deltaY = qBound(0, boundingRect.top() - cr.top(), m_scrollBar->value());
        if (deltaY > 0) {
            for (int i = 0; i < m_validRows; i++) {
                if (m_items[i].layouted) {
                    m_items[i].rect.translate(0, -deltaY);
                }
            }
            m_scrollBar->setValue(m_scrollBar->value() - deltaY);
            m_scrollBar->setRange(0, m_scrollBar->maximum() - deltaY);
            markAreaDirty(visibleArea());
            boundingRect.translate(0, -deltaY);
            m_regionCache.clear();
        }

        // Remove any empty space below the visible area by adjusting the
        // maximum value of the scrollbar.
        boundingRect |= cr;
        int max = qMax(m_scrollBar->value(), boundingRect.height() - cr.height());
        if (m_scrollBar->maximum() > max) {
            m_scrollBar->setRange(0, max);
        }
    } else {
        // The view is empty
        m_scrollBar->setRange(0, 0);
    }

    // Update the scrollbar visibility
    if (m_scrollBar->minimum() != m_scrollBar->maximum()) {
        m_scrollBar->show();
    } else {
        m_scrollBar->hide();
    }
}

void IconView::paintErrorMessage(QPainter *painter, const QRect &rect, const QString &message) const
{
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
    p.setPen(palette().color(QPalette::Text));
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

void IconView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)

    int offset = m_scrollBar->value();
    const QRect cr = contentsRect().toRect();
    if (!cr.isValid()) {
        return;
    }

    QRect clipRect = cr & option->exposedRect.toAlignedRect();
    if (clipRect.isEmpty()) {
        return;
    }

    prepareBackBuffer();

    painter->setClipRect(clipRect);

    // Update the dirty region in the backbuffer
    // =========================================
    if (!m_dirtyRegion.isEmpty()) {
        QStyleOptionViewItemV4 opt = viewOptions();
        QSize oldDecorationSize;

        QPainter p(&m_pixmap);
        p.translate(-cr.topLeft() - QPoint(0, offset));
        p.setClipRegion(m_dirtyRegion);

        // Clear the dirty region
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(mapToViewport(cr).toAlignedRect(), Qt::transparent);
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
                updateTextShadows(palette().color(QPalette::Text));
            }

            if (hasFocus() && index == m_selectionModel->currentIndex()) {
                opt.state |= QStyle::State_HasFocus;
            }

            if (m_items[i].needSizeAdjust) {
                const QSize size = m_delegate->sizeHint(opt, index);
                m_items[i].rect.setHeight(size.height());
                m_items[i].needSizeAdjust = false;
                opt.rect = m_items[i].rect;
            }

            if (m_pressedIndex == index) {
                opt.state |= QStyle::State_Sunken;
                oldDecorationSize = opt.decorationSize;
                opt.decorationSize *= 0.9;
            }

            m_delegate->paint(&p, opt, index);
            if (!oldDecorationSize.isEmpty()) {
                opt.decorationSize = oldDecorationSize;
                oldDecorationSize = QSize();
            }
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

    syncBackBuffer(painter, clipRect);

    if (!m_errorMessage.isEmpty()) {
        paintErrorMessage(painter, cr, m_errorMessage);
    }
}

void IconView::updateTextShadows(const QColor &textColor)
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

    if (m_delegate->shadowColor() != shadowColor) {
        m_delegate->setShadowColor(shadowColor);

        // Center white shadows to create a halo effect, and offset dark shadows slightly.
        if (shadowColor == Qt::white) {
            m_delegate->setShadowOffset(QPoint(0, 0));
        } else {
            m_delegate->setShadowOffset(QPoint(layoutDirection() == Qt::RightToLeft ? -1 : 1, 1));
        }
    }
}

bool IconView::indexIntersectsRect(const QModelIndex &index, const QRect &rect) const
{
    if (!index.isValid() || index.row() >= m_items.count()) {
        return false;
    }

    QRect r = m_items[index.row()].rect;
    if (!r.intersects(rect)) {
        return false;
    }

    // If the item is fully contained in the rect
    if (r.left() > rect.left() && r.right() < rect.right() &&
        r.top() > rect.top() && r.bottom() < rect.bottom()) {
        return true;
    }

    // If the item is partially inside the rect
    return visualRegion(index).intersects(rect);
}

QModelIndex IconView::indexAt(const QPointF &point) const
{
    if (!mapToViewport(contentsRect()).contains(point)) {
        return QModelIndex();
    }

    const QPoint pt = point.toPoint();

    // If we have a hovered index, check it before walking the list
    if (m_hoveredIndex.isValid() && m_items.count() > m_hoveredIndex.row()) {
        if (m_items[m_hoveredIndex.row()].rect.contains(pt) &&
            visualRegion(m_hoveredIndex).contains(pt)) {
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

QRect IconView::visualRect(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_validRows ||
        !m_items[index.row()].layouted) {
        return QRect();
    }

    return m_items[index.row()].rect;
}

QRegion IconView::visualRegion(const QModelIndex &index) const
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

    if (m_delegate) {
        // Make this a virtual function in KDE 5
        QMetaObject::invokeMethod(m_delegate, "shape", Q_RETURN_ARG(QRegion, region),
                                  Q_ARG(QStyleOptionViewItem, option),
                                  Q_ARG(QModelIndex, index));

        m_regionCache.insert(key, new QRegion(region));
    }
    return region;
}

void IconView::updateScrollBarGeometry()
{
    const QRectF cr = contentsRect();
    QRectF r = layoutDirection() == Qt::LeftToRight ?
                QRectF(cr.right() - m_scrollBar->geometry().width(), cr.top(),
                       m_scrollBar->geometry().width(), cr.height()) :
                QRectF(cr.left(), cr.top(), m_scrollBar->geometry().width(), cr.height());

    if (m_scrollBar->geometry() != r) {
        m_scrollBar->setGeometry(r);
    }
}

void IconView::renameSelectedIcon()
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
    m_delegate->updateEditorGeometry(editor, option, index);

    m_editorProxy = new QGraphicsProxyWidget(this);
    m_editorProxy->setWidget(editor);
    m_editorProxy->setPos(mapFromViewport(editor->pos()));

    m_delegate->setEditorData(editor, index);

    editor->show();
    editor->setFocus();

    m_editorIndex = index;
}

bool IconView::renameInProgress() const
{
    return m_editorIndex.isValid();
}

bool IconView::dragInProgress() const
{
    return m_dragInProgress || m_dropOperation || (m_popupView && m_popupView->dragInProgress());
}

bool IconView::popupVisible() const
{
    return !m_popupView.isNull();
}

int IconView::scrollBarExtent() const
{
    return m_scrollBar->geometry().width();
}

QSize IconView::sizeForRowsColumns(int rows, int columns) const
{
    int spacing = 10;
    int margin = 10;

    QSize size;
    size.rwidth() = 2 * margin + columns * (gridSize().width() + spacing) + scrollBarExtent();
    size.rheight() = 2 * margin + rows * (gridSize().height() + spacing) - spacing;

    return size;
}

void IconView::commitData(QWidget *editor)
{
    m_delegate->setModelData(editor, m_model, m_editorIndex);
}

void IconView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    Q_UNUSED(hint)

    editor->removeEventFilter(m_delegate);
    if (editor->hasFocus()) {
        setFocus();
    }

    editor->hide();
    editor->deleteLater();
    m_editorIndex = QModelIndex();

    markAreaDirty(visibleArea());
}

void IconView::resizeEvent(QGraphicsSceneResizeEvent *e)
{
    updateScrollBarGeometry();

    if (m_validRows > 0) {
        if (m_flow == RightToLeft || m_flow == TopToBottomRightToLeft) {
            // When the origin is the top-right corner, we need to shift all
            // the icons horizontally so they gravitate toward the right side.
            int dx = e->newSize().width() - e->oldSize().width();
            if (dx != 0) {
                for (int i = 0; i < m_validRows; i++) {
                    m_items[i].rect.translate(dx, 0);
                }
                m_regionCache.clear();
                markAreaDirty(visibleArea());
            }
        }

        // A check is done when the timer fires to make sure that a relayout
        // is really necessary.
        m_delayedRelayoutTimer.start(500, this);
        updateScrollBar();
    }
}

void IconView::repositionWidgetsManually()
{
    if (m_editorProxy) {
        QWidget *editor = m_editorProxy->widget();
        QModelIndex index = m_selectionModel->currentIndex();
        const QRect rect = visualRect(index);
        QStyleOptionViewItemV4 option = viewOptions();
        option.rect = rect;
        m_delegate->updateEditorGeometry(editor, option, index);
        m_editorProxy->setPos(mapFromViewport(editor->pos()));
    }
}

void IconView::focusInEvent(QFocusEvent *event)
{
    Q_UNUSED(event)
    markAreaDirty(visibleArea());
}

void IconView::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event)
    markAreaDirty(visibleArea());
}

void IconView::triggerToolTip(ToolTipType type)
{
    if (type == FolderTip && m_hoveredIndex.isValid()) {
        if (!m_popupView || m_hoveredIndex != m_popupIndex) {
            m_toolTipShowTimer.start(500, this);
        }
    } else {
        // Close the popup view if one is open
        m_toolTipShowTimer.stop();
        m_popupCausedWidget = 0;
        m_popupUrl = KUrl();
        if (m_popupView) {
            m_popupView->delayedHide();
        }
        // Don't show file tips when the user is dragging something over the view
        if (!m_hoverDrag) {
            m_toolTipWidget->updateToolTip(m_hoveredIndex, mapFromViewport(visualRect(m_hoveredIndex)));
        } else {
            m_toolTipWidget->updateToolTip(QModelIndex(), QRect());
        }
    }
}

// Updates the tooltip for the hovered index
// causedWidget is the widget that received the mouse event that triggered the update
void IconView::updateToolTip(QWidget *causedWidget)
{
    if (!m_hoveredIndex.isValid()) {
        triggerToolTip(FileTip); // Will close any open tips
        return;
    }

    if (m_popupView && m_hoveredIndex == m_popupIndex) {
        // If we're already showing a popup view for this index
        return;
    }

    // Decide if we're going to show a popup view or a regular tooltip
    IconView::ToolTipType type = IconView::FileTip;
    bool delayedResult = false;

    KFileItem item = m_model->itemForIndex(m_hoveredIndex);
    KUrl url = item.targetUrl();

    if (item.isDir()) {
        type = IconView::FolderTip;
    } else if (item.isDesktopFile()) {
        // Check if the desktop file is a link to a local folder
        KDesktopFile file(url.path());
        if (file.readType() == "Link") {
            url = file.readUrl();
            if (url.isLocalFile()) {
                KFileItem destItem(KFileItem::Unknown, KFileItem::Unknown, url);
                type = destItem.isDir() ? IconView::FolderTip : IconView::FileTip;
            } else if (KProtocolInfo::protocolClass(url.protocol()) == QString(":local")) {
                KIO::StatJob *job = KIO::stat(url, KIO::HideProgressInfo);
                job->setSide(KIO::StatJob::SourceSide); // We will only read the file
                connect(job, SIGNAL(result(KJob*)), SLOT(statResult(KJob*)));
                delayedResult = true;
            }
        }
    } 

    m_popupUrl = url;
    m_popupCausedWidget = causedWidget;

    if (!delayedResult) {
        triggerToolTip(type);
    }
}

void IconView::statResult(KJob *job)
{
    if (!job->error()) {
        KIO::StatJob *statJob = static_cast<KIO::StatJob*>(job);
        if (statJob->statResult().isDir()) {
            triggerToolTip(IconView::FolderTip);
        } else {
            triggerToolTip(IconView::FileTip);
        }
    }
}

void IconView::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    const QModelIndex index = indexAt(mapToViewport(event->pos()));
    if (index.isValid()) {
        m_hoveredIndex = index;
        markAreaDirty(visualRect(index));
    }
    updateToolTip(event->widget());
}

void IconView::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    if (m_hoveredIndex.isValid()) {
        markAreaDirty(visualRect(m_hoveredIndex));
        m_hoveredIndex = QModelIndex();
        updateToolTip(event->widget());
    }
}

void IconView::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const QModelIndex index = indexAt(mapToViewport(event->pos()));
    if (index != m_hoveredIndex) {
        markAreaDirty(visualRect(index));
        markAreaDirty(visualRect(m_hoveredIndex));
        m_hoveredIndex = index;
        updateToolTip(event->widget());
    }
}

void IconView::keyPressEvent(QKeyEvent *event)
{
    if (m_columns == 0)        //The layout isn't done until items are actually inserted into the model, so until that happens, m_columns will be 0
        return;

    int hdirection = 0;
    int vdirection = 0;

    QModelIndex currentIndex = m_selectionModel->currentIndex();

    switch (event->key()) {
    case Qt::Key_Up:
        vdirection = -1;
        break;
    case Qt::Key_Down:
        vdirection = 1;
        break;
    case Qt::Key_Left:
        hdirection = -1;
        break;
    case Qt::Key_Right:
        hdirection = 1;
        break;

    case Qt::Key_Return:
    case Qt::Key_Enter:        //Enter key located on the keypad
        emit activated(currentIndex);
        return;
    default:
        event->ignore();
        return;
    }

    QModelIndex nextIndex = QModelIndex();        //Will store the final index we calculate to move to, initialized with Invalid QModelIndex

    if (!m_layoutBroken) {                //If the view is sorted
        int prevItem = currentIndex.row();
        int newItem = 0;
        int hMultiplier = 1;
        int vMultiplier = 1;
        
        switch (m_flow)        //Perform flow related calculations
        {
        case RightToLeft:
            hMultiplier = -1;
        case LeftToRight:        //Fall through because in both RightToLeft and LeftToRight, we move m_columns time vertically
            vMultiplier = m_columns;
            break;
        case TopToBottom:
            hMultiplier = m_rows;
            break;
        case TopToBottomRightToLeft:
            hMultiplier = m_rows*(-1);
            break;
        }

        newItem = currentIndex.row() + hdirection*hMultiplier + vdirection*vMultiplier;

        if ( (newItem < 0) || (newItem >= m_dirModel->rowCount()) ) {
            newItem = prevItem;
        }

        nextIndex = currentIndex.sibling(newItem, currentIndex.column());
    }

    else {        //If the user has moved the icons, i.e. the view is no longer sorted
        QPoint currentPos = visualRect(currentIndex).center();

        //Store distance between the first and the current index-
        int lastDistance = (visualRect(m_model->index(0, 0)).center() - currentPos).manhattanLength();
        int distance = 0;

        for (int i = 0; i < m_validRows; i++) {
            const QModelIndex tempIndex = m_model->index(i, 0);
            const QPoint pos = visualRect(tempIndex).center();
            if (tempIndex == currentIndex)    continue;

            if (hdirection == 0) {                                //Moving in vertical direction
                if (vdirection*pos.y() > vdirection*currentPos.y()) {
                    distance = (pos - currentPos).manhattanLength();
                    if (distance < lastDistance || !nextIndex.isValid()) {
                        nextIndex = tempIndex;
                        lastDistance = distance;
                    }
                }
            }

            else if (vdirection == 0) {                //Moving in horizontal direction
                if (hdirection*pos.x() > hdirection*currentPos.x()) {
                    distance = (pos - currentPos).manhattanLength();
                    if (distance < lastDistance || !nextIndex.isValid()) {
                        nextIndex = tempIndex;
                        lastDistance = distance;
                    }
                }
            }
        }

        if (!nextIndex.isValid()) {
            return;
        }
    }

    markAreaDirty(visualRect(currentIndex));
    m_selectionModel->select(nextIndex, QItemSelectionModel::ClearAndSelect);
    m_selectionModel->setCurrentIndex(nextIndex, QItemSelectionModel::NoUpdate);
    scrollTo(nextIndex);
    markAreaDirty(visualRect(nextIndex));
    m_pressedIndex = nextIndex;
}

void IconView::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // Make sure that any visible tooltip is hidden
    Plasma::ToolTipManager::self()->hide(m_toolTipWidget);
    delete m_popupView;

    if (scene()->itemAt(event->scenePos()) != this || !contentsRect().contains(event->pos()) ||
        !m_errorMessage.isEmpty()) {
        event->ignore();
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
                markAreaDirty(visibleArea());
            }
            event->ignore(); // Causes contextMenuEvent() to get called
        } else if (m_selectionModel->hasSelection()) {
            m_selectionModel->clearSelection();
            markAreaDirty(visibleArea());
        }
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(pos);

        // If an icon was pressed
        if (index.isValid())
        {
            //if ctrl is held
            if (event->modifiers() & Qt::ControlModifier) {
                m_selectionModel->select(index, QItemSelectionModel::Toggle);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markAreaDirty(visualRect(index));
            } else if (!m_selectionModel->isSelected(index)) {
                //if not already selected
                m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markAreaDirty(visibleArea());
            }
            else {
                markAreaDirty(visualRect(index));
            }
            m_pressedIndex = index;
            m_buttonDownPos = pos;
            return;
        }

        // If empty space was pressed
        m_pressedIndex = QModelIndex();
        m_buttonDownPos = pos;

        const Plasma::Containment *parent = qobject_cast<Plasma::Containment*>(parentWidget());

        if (event->modifiers() & Qt::ControlModifier) {
            // Make the current selection persistent
            m_selectionModel->select(m_selectionModel->selection(), QItemSelectionModel::Select);
        } else if (parent && parent->isContainment() &&
                   event->widget()->window()->inherits("DashboardView")) {
            // Let the event propagate to the parent widget, which will emit releaseVisualFocus().
            // We prefer hiding the Dashboard to allowing rubber band selections in the containment
            // when the icon view is being shown on the Dashboard.
            event->ignore();
            return;
        }

        if (m_selectionModel->hasSelection()) {
            if (!(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
                m_selectionModel->clearSelection();
                markAreaDirty(visibleArea());
            }
        }
    }

    if (event->button() == Qt::MidButton) {
        event->ignore();
    }
}

void IconView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
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
                emit activated(index);
                m_selectionModel->clearSelection();
                markAreaDirty(visibleArea());
            }
            // We don't clear and update the selection and current index in
            // mousePressEvent() if the item is already selected when it's pressed,
            // so we need to do that here.
            if (m_selectionModel->currentIndex() != index ||
                m_selectionModel->selectedIndexes().count() > 1) {
                m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markAreaDirty(visibleArea());
            }
            m_doubleClick = false;
        }
    }

    m_doubleClick = false;
    m_pressedIndex = QModelIndex();
}

void IconView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
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
    emit activated(index);

    m_selectionModel->clearSelection();
    markAreaDirty(visibleArea());
}

void IconView::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        return;
    }

    // If an item is pressed
    if (m_pressedIndex.isValid()) {
        const QPointF point = event->pos() - event->buttonDownPos(Qt::LeftButton);
        if (point.toPoint().manhattanLength() >= QApplication::startDragDistance()) {
            m_pressedIndex = QModelIndex();
            startDrag(m_buttonDownPos, event->widget());
        }
        return;
    }

    const int scrollBarOffset = m_scrollBar->isVisible() ? m_scrollBar->geometry().width() : 0;
    const QRect viewportRect = layoutDirection() == Qt::LeftToRight ?
                   visibleArea().adjusted(0, 0, int(-scrollBarOffset), 0) :
                   visibleArea().adjusted(int(scrollBarOffset), 0, 0, 0);
    const QPointF pos = mapToViewport(event->pos());
    const QRectF rubberBand = QRectF(m_buttonDownPos, pos).normalized();
    const QRect r = QRectF(rubberBand & viewportRect).toAlignedRect();

    if (r != m_rubberBand) {
        const QPoint pt = pos.toPoint();
        QRectF dirtyRect = m_rubberBand | r;
        m_rubberBand = r;

        dirtyRect |= visualRect(m_hoveredIndex);
        m_hoveredIndex = QModelIndex();

        foreach (const QModelIndex &index, m_selectionModel->selectedIndexes()) {
            dirtyRect |= visualRect(index);
        }

        // Select the indexes inside the rubber band
        QItemSelection selection;
        for (int i = 0; i < m_items.size(); i++) {
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
}

void IconView::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if ((event->modifiers() & Qt::CTRL) || (event->orientation() == Qt::Horizontal)) {
        // Let the event propagate to the parent widget
        event->ignore();
        return;
    }

    int pixels = 64 * event->delta() / 120;
    smoothScroll(0, -pixels);
}

void IconView::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    const QPointF pos = mapToViewport(event->pos());
    const QModelIndex index = indexAt(pos);

    if (index.isValid()) {
        emit contextMenuRequest(event->widget(), event->screenPos());
    } else {
        // Let the event propagate to the parent widget
        event->ignore();
    }
}

void IconView::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    bool accepted = KUrl::List::canDecode(event->mimeData());
    event->setAccepted(accepted);
    m_hoverDrag = accepted;
}

void IconView::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)

    stopAutoScrolling();
    m_hoverDrag = false;
}

void IconView::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    // Auto scroll the view when the cursor is close to the top or bottom edge
    const int maxSpeed = 500; // Pixels per second
    const int distance = gridSize().height() * .75;
    if (event->pos().y() < contentsRect().y() + distance) {
        int speed = maxSpeed / distance * (distance - event->pos().y() - contentsRect().y());
        autoScroll(ScrollUp, speed);
    } else if (event->pos().y() > contentsRect().bottom() - distance) {
        int speed = maxSpeed / distance * (event->pos().y() - contentsRect().bottom() + distance);
        autoScroll(ScrollDown, speed);
    } else {
        stopAutoScrolling();
    }

    const QModelIndex index = indexAt(mapToViewport(event->pos()));
    if (index == m_hoveredIndex) {
        return;
    }

    Plasma::Corona *corona = qobject_cast<Plasma::Corona*>(scene());
    const QString appletMimeType = (corona ? corona->appletMimeType() : QString());
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
    updateToolTip(event->widget());

    markAreaDirty(dirtyRect);
    event->setAccepted(!event->mimeData()->hasFormat(appletMimeType));
}

void IconView::createDropActions(const KUrl::List &urls, QActionGroup *actions)
{
    Plasma::Containment *containment = qobject_cast<Plasma::Containment*>(parentWidget());

    // Only add actions for creating applets and setting the wallpaper if a single
    // URL has been dropped. If there are multiple URL's, we could check if they all
    // have the same mimetype, but there's no way of knowing if the applet wants all
    // the URL's, or if we should create one applet for each URL. Positioning multiple
    // applets can also be tricky.
    if (containment && containment->isContainment() && urls.count() == 1) {
        KMimeType::Ptr mime = KMimeType::findByUrl(urls.first());
        QString mimeName = mime->name();
        KPluginInfo::List appletList = Plasma::Applet::listAppletInfoForMimetype(mimeName);

        if (!appletList.isEmpty()) {
            foreach (const KPluginInfo &info, appletList) {
                QAction *action = new QAction(info.name(), actions);
                action->setData(info.pluginName());
                if (!info.icon().isEmpty()) {
                    action->setIcon(KIcon(info.icon()));
                }
            }
        }

        // Since it is possible that the URL is a remote URL, we have to rely on the file
        // extension to decide if URL refers to a file we can use as a wallpaper.
        // Unfortunately QImageReader::supportedImageFormats() returns some formats in upper case
        // and others in lower case, but all the important ones are in lower case.
        const QByteArray suffix = QFileInfo(urls.first().fileName()).suffix().toLower().toUtf8();
        if (mimeName.startsWith("image/") || QImageReader::supportedImageFormats().contains(suffix)) {
            QAction *action = new QAction(i18n("Set as &Wallpaper"), actions);
            action->setData("internal:folderview:set-as-wallpaper");
            action->setIcon(KIcon("preferences-desktop-wallpaper"));
        }
    }
}

void IconView::dropActionTriggered(QAction *action)
{
    Q_ASSERT(m_dropOperation != 0);

    FolderView *containment = qobject_cast<FolderView*>(parentWidget());
    const KUrl::List urls = m_dropOperation->droppedUrls();

    if (containment && containment->isContainment() && urls.count() == 1) {
        const QString name = action->data().toString();

        if (name == "internal:folderview:set-as-wallpaper") {
            if (!urls.first().isLocalFile()) {
                (void) new RemoteWallpaperSetter(urls.first(), containment);
            } else {
                containment->setWallpaper(urls.first());
            }
        } else {
            const QVariantList args = QVariantList() << urls.first().url();
            const QPoint pos = m_dropOperation->dropPosition();
            const QRectF geom(pos, QSize());

            containment->addApplet(name, args, geom);
        }
    }
}

void IconView::dropCompleted()
{
    delete m_dropActions;
    m_dropActions = 0;
    m_dropOperation = 0; // Auto deleted
}

void IconView::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    m_hoverDrag = false;

    // If the dropped item is an applet, let the parent widget handle it
    Plasma::Corona *corona = qobject_cast<Plasma::Corona*>(scene());
    const QString appletMimeType = (corona ? corona->appletMimeType() : QString());
    if (event->mimeData()->hasFormat(appletMimeType)) {
        event->ignore();
        return;
    }

    event->accept();

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

        // If we're dropping on the view itself
        QList<QAction *> userActions;
        if (!m_hoveredIndex.isValid()) {
            m_dropActions = new QActionGroup(this);
            createDropActions(KUrl::List::fromMimeData(event->mimeData()), m_dropActions);
            connect(m_dropActions, SIGNAL(triggered(QAction*)), SLOT(dropActionTriggered(QAction*)));
            userActions = m_dropActions->actions();
        }

        m_dropOperation = KonqOperations::doDrop(item, m_dirModel->dirLister()->url(), &ev, event->widget(), userActions);
        if (m_dropOperation) {
            connect(m_dropOperation, SIGNAL(destroyed(QObject*)), SLOT(dropCompleted()));
        } else {
            dropCompleted();
        }
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

    QModelIndexList indexes;
    QRect boundingRect;
    foreach (const KUrl &url, KUrl::List::fromMimeData(event->mimeData())) {
        const QModelIndex index = m_model->indexForUrl(url);
        if (index.isValid()) {
            boundingRect |= m_items[index.row()].rect;
            indexes.append(index);
        }
    }

    const QRect cr = contentsRect().toRect();
    boundingRect.adjust(-10, -10, 10, 10);
    boundingRect.translate(delta);

    // Don't allow the user to move icons outside the scrollable area of the view.
    // Note: This code will need to be changed if support for a horizontal scrollbar is added.

    // Check the left and right sides
    if (boundingRect.left() < cr.left()) {
        delta.rx() += cr.left() - boundingRect.left();
    }
    else if (boundingRect.right() > cr.right()) {
        delta.rx() -= boundingRect.right() - cr.right();
    }

    // If the flow is vertical, check the top and bottom sides
    if (m_flow == TopToBottom || m_flow == TopToBottomRightToLeft) {
        if (boundingRect.top() < cr.top()) {
            delta.ry() += cr.top() - boundingRect.top();
        }
        else if (boundingRect.bottom() > cr.bottom()) {
            delta.ry() -= boundingRect.bottom() - cr.bottom();
        }
    }

    // Move the items
    foreach (const QModelIndex &index, indexes) {
        m_items[index.row()].rect.translate(delta);
    }

    // Make sure no icons have negative coordinates etc.
    doLayoutSanityCheck();
    markAreaDirty(visibleArea());
    m_regionCache.clear();

    m_layoutBroken = true;
    emit indexesMoved(indexes);
}

void IconView::changeEvent(QEvent *event)
{
    QGraphicsWidget::changeEvent(event);

    switch (event->type())
    {
    case QEvent::ContentsRectChange:
    {
        qreal left, top, right, bottom;
        getContentsMargins(&left, &top, &right, &bottom);

        if (m_validRows == 0) {
            m_margins[Plasma::LeftMargin]   = left;
            m_margins[Plasma::TopMargin]    = top;
            m_margins[Plasma::RightMargin]  = right;
            m_margins[Plasma::BottomMargin] = bottom;
            break;
        }

        // Check if the margins have changed, but the contents rect still has the same size.
        // This mainly happens when the applet is used as a containment, and the user moves
        // a panel to the opposite edge, or when the user enables/disables panel autohide.
        bool widthChanged = int(m_margins[Plasma::LeftMargin] + m_margins[Plasma::RightMargin]) != int(left + right);
        bool heightChanged = int(m_margins[Plasma::TopMargin] + m_margins[Plasma::BottomMargin]) != int(top + bottom);
        bool horizontalFlow = (m_flow == LeftToRight || m_flow == RightToLeft);
        bool needRelayout = false;

        if ((horizontalFlow && widthChanged) || (!horizontalFlow && heightChanged)) {
            needRelayout = true;
        }

        // Don't throw the layout away if all items will fit in the new contents rect
        if (needRelayout) {
            const QRect cr = contentsRect().toRect();
            QRect boundingRect = itemsBoundingRect();
            boundingRect.adjust(-10, -10, 10, 10);
            if (boundingRect.width() < cr.width() && boundingRect.height() < cr.height()) {
                needRelayout = false;
            }
        }

        if (needRelayout)
        {
            m_validRows = 0;
            m_delayedLayoutTimer.start(10, this);
            emit busy(true);
        } else {
            QPoint delta;

            if (m_flow == LeftToRight || m_flow == TopToBottom) {
                // Gravitate toward the upper left corner
                delta.rx() = int(left - m_margins[Plasma::LeftMargin]);
            } else {
                // Gravitate toward the upper right corner
                delta.rx() = int(m_margins[Plasma::RightMargin] - right);
            }
            delta.ry() = int(top - m_margins[Plasma::TopMargin]);

            if (!delta.isNull()) {
                for (int i = 0; i < m_validRows; i++) {
                    if (m_items[i].layouted) {
                        m_items[i].rect.translate(delta);
                    }
                }
                markAreaDirty(visibleArea());
                updateScrollBar();
            }    
        }

        m_margins[Plasma::LeftMargin]   = left;
        m_margins[Plasma::TopMargin]    = top;
        m_margins[Plasma::RightMargin]  = right;
        m_margins[Plasma::BottomMargin] = bottom;
        break;
    }

    case QEvent::FontChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        markAreaDirty(visibleArea());
        update();
        break;

    default:
        break;
    }
}

// pos is the position where the mouse was clicked in the applet.
// widget is the widget that sent the mouse event that triggered the drag.
void IconView::startDrag(const QPointF &pos, QWidget *widget)
{
    QModelIndexList indexes = m_selectionModel->selectedIndexes();
    QRect boundingRect;
    foreach (const QModelIndex &index, indexes) {
        boundingRect |= visualRect(index);
    }

    QPixmap pixmap(boundingRect.size());
    pixmap.fill(Qt::transparent);

    QStyleOptionViewItemV4 option = viewOptions();
    // ### We can't draw the items as selected or hovered since Qt doesn't
    //     use an ARGB window for the drag pixmap.
    //option.state |= QStyle::State_Selected;
    option.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);

    updateTextShadows(palette().color(QPalette::HighlightedText));

    QPainter p(&pixmap);
    foreach (const QModelIndex &index, indexes)
    {
        option.rect = visualRect(index).translated(-boundingRect.topLeft());
#if 0
        // ### Reenable this code when Qt uses an ARGB window for the drag pixmap
        if (index == m_hoveredIndex)
            option.state |= QStyle::State_MouseOver;
        else
            option.state &= ~QStyle::State_MouseOver;
#endif
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

QStyleOptionViewItemV4 IconView::viewOptions() const
{
    QStyleOptionViewItemV4 option;
    initStyleOption(&option);

    option.font                = font();
    option.decorationAlignment = Qt::AlignTop | Qt::AlignHCenter;
    option.decorationPosition  = QStyleOptionViewItem::Top;
    option.decorationSize      = iconSize();
    option.displayAlignment    = Qt::AlignHCenter;
    option.textElideMode       = Qt::ElideRight;
    option.locale              = QLocale::system();
    option.widget              = m_styleWidget;
    option.viewItemPosition    = QStyleOptionViewItemV4::OnlyOne;

    if (m_wordWrap) {
        option.features = QStyleOptionViewItemV2::WrapText;
    }

    return option;
}

void IconView::popupCloseRequested()
{
    if (m_popupView && (!m_hoveredIndex.isValid() || m_hoveredIndex != m_popupIndex)) {
        m_popupView->hide();
        m_popupView->deleteLater();
    }
}

void IconView::timerEvent(QTimerEvent *event)
{
    AbstractItemView::timerEvent(event);

    if (event->timerId() == m_delayedCacheClearTimer.timerId()) {
        m_delayedCacheClearTimer.stop();
        m_savedPositions.clear();
    } else if (event->timerId() == m_delayedLayoutTimer.timerId()) {
        m_delayedLayoutTimer.stop();
        layoutItems();
    }
    else if (event->timerId() == m_delayedRelayoutTimer.timerId()) {
        m_delayedRelayoutTimer.stop();

        bool horizontalFlow = (m_flow == LeftToRight || m_flow == RightToLeft);
        if (m_layoutBroken) {
            // Relayout all icons that have ended up outside the view
            const QRect cr = contentsRect().toRect();
            const QSize grid = gridSize();
            QPoint pos;

            QRect r = cr.adjusted(10, 10, -10, -10);
            if (horizontalFlow) {
                if (layoutDirection() == Qt::LeftToRight) {
                    r.adjust(0, 0, int(-m_scrollBar->geometry().width()), 0);
                } else {
                    r.adjust(int(m_scrollBar->geometry().width()), 0, 0, 0);
                }
            }
 
            for (int i = 0; i < m_validRows; i++) {
                if ((horizontalFlow && m_items[i].rect.right() > r.right()) ||
                    (!horizontalFlow && m_items[i].rect.bottom() > r.height()))
                {
                    pos = findNextEmptyPosition(pos, grid, cr);
                    m_items[i].rect.moveTo(pos);
                }
            }
            m_regionCache.clear();
            markAreaDirty(visibleArea());
        } else {
            int maxWidth  = contentsRect().width();
            int maxHeight = contentsRect().height();
            if (horizontalFlow) {
                maxWidth -= m_scrollBar->geometry().width();
            }

            if ((horizontalFlow && columnsForWidth(maxWidth) != m_columns) ||
                (!horizontalFlow && rowsForHeight(maxHeight) != m_rows))
            {
                emit busy(true);
                // This is to give the busy animation a chance to appear.
                m_delayedLayoutTimer.start(10, this);
                m_validRows = 0;
            }
        }
    } else if (event->timerId() == m_toolTipShowTimer.timerId()) {
        m_toolTipShowTimer.stop();

        if (m_popupView && m_popupIndex == m_hoveredIndex) {
            // The popup is already showing the hovered index
            return;
        }

        if (m_popupView && m_popupView->dragInProgress()) {
            // Don't delete the popup view when a drag is in progress
            return;
        }

        Plasma::ToolTipManager::self()->hide(m_toolTipWidget);
        delete m_popupView;

        if (QApplication::activePopupWidget() || QApplication::activeModalWidget()) {
            // Don't open a popup view when a menu or similar widget is being shown 
            return;
        }

        if (!m_popupUrl.isEmpty()) {
            const QPointF viewPos = mapFromViewport(visualRect(m_hoveredIndex)).center();
            const QPoint scenePos = mapToScene(viewPos).toPoint();
            QGraphicsView *gv = 0;

            if (m_popupCausedWidget) {
                gv = qobject_cast<QGraphicsView*>(m_popupCausedWidget->parentWidget());
            } else {
                // We position the popup relative to the view under the mouse cursor
                foreach (QGraphicsView *view, scene()->views()) {
                    if (view->underMouse()) {
                        gv = view;
                        break;
                    }
                }
            }

            const QPoint pos = gv ? gv->mapToGlobal(gv->mapFromScene(scenePos)) : QPoint();

            m_popupView = new PopupView(m_popupUrl, pos, this);
            connect(m_popupView, SIGNAL(destroyed(QObject*)), SIGNAL(popupViewClosed()));
            connect(m_popupView, SIGNAL(requestClose()), SLOT(popupCloseRequested()));
            m_popupIndex = m_hoveredIndex;
        }
    }
}

#include "iconview.moc"

