/*
 *   Copyright © 2008, 2009, 2010 Fredrik Höglund <fredrik@kde.org>
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
#include <QDrag>
#include <QGraphicsView>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsProxyWidget>
#include <QImageReader>
#include <QItemSelectionModel>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <KDirModel>
#include <KGlobalSettings>
#include <KIcon>
#include <KIconEffect>
#include <KStringHandler>
#include <KFileItemDelegate>

#include <KIO/NetAccess>

#include <konqmimedata.h>
#include <konq_operations.h>

#include "dirlister.h"
#include "folderview.h"
#include "proxymodel.h"
#include "previewpluginsmodel.h"
#include "tooltipwidget.h"
#include "animator.h"
#include "asyncfiletester.h"

#include <Plasma/Containment>
#include <Plasma/ContainmentActions>
#include <Plasma/Corona>
#include <Plasma/PaintUtils>
#include <Plasma/Theme>
#include <Plasma/ToolTipManager>


IconView::IconView(QGraphicsWidget *parent)
    : AbstractItemView(parent),
      m_columns(0),
      m_rows(0),
      m_validRows(0),
      m_numTextLines(2),
      m_layoutBroken(false),
      m_needPostLayoutPass(false),
      m_positionsLoaded(false),
      m_doubleClick(false),
      m_dragInProgress(false),
      m_hoverDrag(false),
      m_iconsLocked(false),
      m_alignToGrid(false),
      m_wordWrap(false),
      m_popupShowPreview(true),
      m_folderIsEmpty(false),
      m_clickToViewFolders(true),
      m_showSelectionMarker(true),
      m_drawIconShrinked(false),
      m_layout(Rows),
      m_alignment(layoutDirection() == Qt::LeftToRight ? Left : Right),
      m_popupCausedWidget(0),
      m_dropOperation(0),
      m_dropActions(0),
      m_editor(0)
{
    m_actionOverlay = new ActionOverlay(this);

    setAcceptHoverEvents(true);
    setAcceptDrops(true);
    setCacheMode(NoCache);
    setFocusPolicy(Qt::StrongFocus);

    m_scrollBar->hide();
    connect(m_scrollBar, SIGNAL(valueChanged(int)), SLOT(repositionWidgetsManually()));
    connect(m_scrollBar, SIGNAL(valueChanged(int)), SLOT(viewScrolled()));

    m_toolTipWidget = new ToolTipWidget(this);
    m_toolTipWidget->hide();

    m_animator = new Animator(this);

    // set a default for popup preview plugins
    m_popupPreviewPlugins = QStringList() << "imagethumbnail" << "jpegthumbnail";

    int size = style()->pixelMetric(QStyle::PM_LargeIconSize);
    setIconSize(QSize(size, size));

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
    emit modelChanged();
}

void IconView::setIconSize(const QSize &size)
{
    if (size != m_iconSize) {
        m_iconSize = size;
        updateGridSize();
        updateActionButtons();
    }
}

void IconView::setTextLineCount(int count)
{
    if (count != m_numTextLines) {
        m_numTextLines = count;
        updateGridSize();
    }
}

int IconView::textLineCount() const
{
    return m_numTextLines;
}

void IconView::setPopupPreviewSettings(const bool &showPreview, const QStringList &plugins)
{
    m_popupShowPreview = showPreview;
    m_popupPreviewPlugins = plugins;
}

bool IconView::popupShowPreview() const
{
    return m_popupShowPreview;
}

QStringList IconView::popupPreviewPlugins() const
{
    return m_popupPreviewPlugins;
}

// #### remove
void IconView::setGridSize(const QSize &)
{
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

void IconView::setLayout(IconView::Layout layout)
{
    if (m_layout != layout) {
        m_layout = layout;

        // Schedule a full relayout
        if (!m_layoutBroken && m_validRows > 0) {
            m_validRows = 0;
            m_delayedLayoutTimer.start(10, this);
            emit busy(true);
        }
    }
}

IconView::Layout IconView::layout() const
{
    return m_layout;
}

void IconView::setAlignment(IconView::Alignment alignment)
{
    if (m_alignment != alignment) {
        m_alignment = alignment;

        // Schedule a full relayout
        if (!m_layoutBroken && m_validRows > 0) {
            m_validRows = 0;
            m_delayedLayoutTimer.start(10, this);
            emit busy(true);
        }
    }
}

IconView::Alignment IconView::alignment() const
{
    return m_alignment;
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
    if (data.size() < 5 ||                               // is there data stored for at least 1 icon?
        data.at(0).toInt() != 1 ||                       // is format version number 1?
        ((data.size() - 2) % 3) ||                       // are there 3 strings stored for every icon?
        data.at(1).toInt() != ((data.size() - 2) / 3)) { // is the specified number of icons equal to the stored number of icon entries?
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

    if (m_layoutBroken && !listingInProgress() && m_validRows == m_items.size()) {
        int version = 1;
        data << QString::number(version);
        data << QString::number(m_items.size());

        const QPoint offset = contentsRect().topLeft().toPoint();
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

void IconView::updateGridSize()
{
    // Recompute the grid size
    qreal left, top, right, bottom;
    m_itemFrame->getMargins(left, top, right, bottom);

    QFontMetrics fm(font());
    int w = qMin(fm.width('x') * 15, m_iconSize.width() * 2);

    QSize size;
    size.rwidth() = qMax(w, m_iconSize.width()) + left + right;
    size.rheight() = top + bottom + m_iconSize.height() + fm.lineSpacing() * textLineCount() + 4;

    // Update the minimum size hint
    const Plasma::Containment *containment = qobject_cast<Plasma::Containment*>(parentWidget());
    if (!containment || !containment->isContainment()) {
        getContentsMargins(&left, &top, &right, &bottom);
        QSize minSize = size + QSize(20 + left + right, 20 + top + bottom);
        if (m_layout == Rows) {
            minSize.rwidth() += m_scrollBar->geometry().width();
        }
        setMinimumSize(minSize);
    }

    if (size != m_gridSize) {
        const Plasma::Containment *containment = qobject_cast<Plasma::Containment*>(parentWidget());

        if (m_layoutBroken && containment && containment->isContainment()) {
            const int margin = 10;
            const int spacing = 10;

            if (m_alignToGrid)
            {
                int oldRowCount, oldColCount, newRowCount, newColCount;
                const QRect oldCr = adjustedContentsRect(m_gridSize, &oldRowCount, &oldColCount);
                const QRect newCr = adjustedContentsRect(size, &newRowCount, &newColCount);
                const int lastRow = newRowCount - 1;
                const int lastCol = newColCount - 1;
                const QSize oldAdjustedGridSize = m_gridSize + QSize(spacing, spacing);
                const QSize newAdjustedGridSize = size + QSize(spacing, spacing);
                const int oldTopMargin = margin + oldCr.top();
                const int oldLeftMargin = margin + oldCr.left();
                const int newTopMargin = margin + newCr.top();
                const int newLeftMargin = margin + newCr.left();

                for (int i = 0; i < m_items.size(); i++) {
                    const QPoint topLeft = m_items[i].rect.topLeft() - QPoint(oldLeftMargin, oldTopMargin);
                    const int row = qBound(0, topLeft.y() / oldAdjustedGridSize.height(), lastRow);
                    int col = topLeft.x() / oldAdjustedGridSize.width();

                    if (m_alignment == Right) {
                        col = lastCol - ((oldColCount - 1) - col);
                    }

                    col = qBound(0, col, lastCol);

                    m_items[i].rect = QRect(QPoint(newLeftMargin + col * newAdjustedGridSize.width(),
                        newTopMargin + row * newAdjustedGridSize.height()), size);
                    m_items[i].needSizeAdjust = true;
                }
            } else {
                const QRect cr = contentsRect().toRect();
                const int leftMargin = margin + cr.left();
                const int topMargin = margin + cr.top();
                const qreal scaleX = qreal(size.width()) / qreal(m_gridSize.width());
                const qreal scaleY = qreal(size.height()) / qreal(m_gridSize.height());

                for (int i = 0; i < m_items.size(); i++) {
                    const QPoint topLeft = m_items[i].rect.topLeft();
                    const int y = qBound(topMargin, qRound(scaleY * topLeft.y()),
                        cr.bottom() - size.height() - margin);
                    int x = topLeft.x();

                    if (m_alignment == Right) {
                        x = cr.right() - qRound(scaleX * (cr.right() - x));
                    } else {
                        x = scaleX * x;
                    }

                    x = qBound(leftMargin, x, cr.right() - size.width() - margin);

                    m_items[i].rect = QRect(QPoint(x, y), size);
                    m_items[i].needSizeAdjust = true;
                }
            }

            doLayoutSanityCheck();
            m_regionCache.clear();
            markAreaDirty(visibleArea());
        } else if (m_validRows > 0) {
            m_validRows = 0;
            m_delayedLayoutTimer.start(10, this);
            emit busy(true);
        }
    }

    m_gridSize = size;
}

bool IconView::listingInProgress() const
{
    if (m_dirModel) {
        if (KDirLister *lister = m_dirModel->dirLister()) {
            return !lister->isFinished();
        }
    }

    return false;
}

void IconView::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    m_regionCache.clear();

    if (!m_layoutBroken || !m_savedPositions.isEmpty()) {
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
    if (!m_errorMessage.isEmpty() || m_folderIsEmpty) {
        m_errorMessage.clear();
        m_folderIsEmpty = false;
        update();
    }

    emit busy(true);
}

void IconView::listingClear()
{
    markAreaDirty(visibleArea());
    updateScrollBar();
    update();
}

void IconView::listingCompleted()
{
    m_delayedCacheClearTimer.start(5000, this);

    if (m_validRows == m_model->rowCount()) {
        emit busy(false);
    }

    if (!m_model->rowCount() && !m_folderIsEmpty) {
        m_folderIsEmpty = true;
        update();
    } else if (m_model->rowCount() && m_folderIsEmpty) {
        m_folderIsEmpty = false;
        update();
    }
}

void IconView::listingCanceled()
{
    m_delayedCacheClearTimer.start(5000, this);

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

    if (!m_model->rowCount()) {
        m_folderIsEmpty = true;
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
    if (m_layout == Rows) {
        if (layoutDirection() == Qt::LeftToRight) {
            r.adjust(0, 0, -m_scrollBar->geometry().width(), 0);
        } else {
            r.adjust(m_scrollBar->geometry().width(), 0, 0, 0);
        }
    }

    const int xOrigin = (m_alignment == Left) ?
                    r.left() :  r.right() - grid.width() + 1;

    if (lastPos.isNull()) {
        return QPoint(xOrigin, r.top());
    }

    QPoint pos = lastPos;

    if (m_layout == Rows) {
        if (m_alignment == Left) {
            pos.rx() += grid.width() + spacing;
            if (pos.x() + grid.width() >= r.right()) {
                pos.ry() += grid.height() + spacing;
                pos.rx() = xOrigin;
            }
        } else { // Right
            pos.rx() -= grid.width() + spacing;
            if (pos.x() < r.left()) {
                pos.ry() += grid.height() + spacing;
                pos.rx() = xOrigin;
            }
        }
    } else { // Columns
        pos.ry() += grid.height() + spacing;
        if (pos.y() + grid.height() >= r.bottom()) {
            if (m_alignment == Left) {
                pos.rx() += grid.width() + spacing;
            } else { // Right
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
            if (m_items.at(i).layouted && m_items.at(i).rect.intersects(r)) {
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
    if (m_layout == Rows) {
        maxWidth -= m_scrollBar->geometry().width();
    }
    m_columns = columnsForWidth(maxWidth);
    m_rows = rowsForHeight(maxHeight);
    bool needUpdate = false;

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
            if (!m_needPostLayoutPass && count == m_items.size() && !listingInProgress()) {
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
            m_items[i].layouted = true;
            if (m_items[i].rect.intersects(visibleRect)) {
                needUpdate = true;
            }
        }
        needUpdate |= doLayoutSanityCheck();
        m_needPostLayoutPass = false;
        emit busy(false);
    }

    if (m_validRows < m_items.size() || m_needPostLayoutPass) {
        m_delayedLayoutTimer.start(10, this);
    } else if (!listingInProgress()) {
        emit busy(false);
    }

    if (needUpdate) {
        m_dirtyRegion = QRegion(visibleRect);
        update();
    }

    updateScrollBar();
}

// Returns the contents rect with the width and height snapped to the grid
// and aligned according to the direction of the flow.
QRect IconView::adjustedContentsRect(const QSize &gridSize, int *rowCount, int *colCount) const
{
    QRect r = contentsRect().toRect();

    const QSize size = gridSize + QSize(10, 10);
    *colCount = qMax(1, (r.width() - 10) / size.width());
    *rowCount = qMax(1, (r.height() - 10) / size.height());
    int dx = r.width() - (*colCount * size.width() + 10);
    int dy = r.height() - (*rowCount * size.height() + 10);
    r.setWidth(r.width() - dx);
    r.setHeight(r.height() - dy);

    // Take the origin into account
    if (m_alignment == Right) {
        r.translate(dx, 0);
    }

    return r;
}

void IconView::alignIconsToGrid()
{
    int rowCount, colCount;
    const QRect cr = adjustedContentsRect(gridSize(), &rowCount, &colCount);

    int lastRow = rowCount - 1;
    int lastCol = colCount - 1;

    const Plasma::Containment *containment = qobject_cast<Plasma::Containment*>(parentWidget());
    if (!containment || !containment->isContainment()) {
        // Don't limit the max rows/columns in the scrolling direction
        if (m_layout == Rows) {
            lastRow = INT_MAX;
        } else {
            lastCol = INT_MAX;
        }
    }

    int margin = 10;
    int spacing = 10;
    const QSize size = gridSize() + QSize(spacing, spacing);

    int topMargin = margin + cr.top();
    int leftMargin = margin + cr.left();
    int vOffset = topMargin + size.height() / 2;
    int hOffset = leftMargin + size.width() / 2;
    bool layoutChanged = false;

    for (int i = 0; i < m_items.size(); i++) {
        const QPoint center = m_items[i].rect.center();
        const int col = qBound(0, qRound((center.x() - hOffset) / qreal(size.width())), lastCol);
        const int row = qBound(0, qRound((center.y() - vOffset) / qreal(size.height())), lastRow);

        const QPoint pos(leftMargin + col * size.width(), topMargin + row * size.height());

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
            // Make sure we use the full space available to the item.
            // The height of the item rect is adjusted to the number
            // of text lines actually used.
            boundingRect |= QRect(m_items[i].rect.topLeft(), gridSize());
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

QSize IconView::itemSize(const QStyleOptionViewItemV4 &option, const QModelIndex &index) const
{
    QSize size = option.decorationSize;
    QSize grid = gridSize();

    qreal left, top, right, bottom;
    m_itemFrame->getMargins(left, top, right, bottom);

    size.rwidth() += left + right;
    size.rheight() += top + bottom + 4;

    QFont font = option.font;

    KFileItem item = qvariant_cast<KFileItem>(index.data(KDirModel::FileItemRole));
    if (item.isLink()) {
        font.setItalic(true);
    }

    QTextLayout layout;
    layout.setText(index.data(Qt::DisplayRole).toString());
    layout.setFont(font);

    const QSize ts = doTextLayout(layout, QSize(grid.width() - left - right, grid.height() - size.height()),
                                  Qt::AlignHCenter, QTextOption::WrapAtWordBoundaryOrAnywhere);

    const int hm = left+right;
    size.rwidth() = qMax(size.width(), ts.width() + hm);
    size.rheight() += ts.height();
    return size;
}

void IconView::paintItem(QPainter *painter, const QStyleOptionViewItemV4 &option, const QModelIndex &index) const
{
    // Draw the item background
    // ========================
    const bool selected = (option.state & QStyle::State_Selected);
    const bool hovered = (option.state & QStyle::State_MouseOver);
    const qreal hoverProgress = m_animator->hoverProgress(index);

    QPixmap from(option.rect.size());
    QPixmap to(option.rect.size());
    from.fill(Qt::transparent);
    to.fill(Qt::transparent);

    if (selected) {
        QPainter p(&from);
        m_itemFrame->setElementPrefix("selected");
        m_itemFrame->resizeFrame(option.rect.size());
        m_itemFrame->paintFrame(&p, QPoint());
    }

    if (hovered && hoverProgress > 0.0) {
        QPainter p(&to);
        m_itemFrame->setElementPrefix(selected ? "selected+hover" : "hover");
        m_itemFrame->resizeFrame(option.rect.size());
        m_itemFrame->paintFrame(&p, QPoint());
        p.end();

        QPixmap result = Plasma::PaintUtils::transition(from, to, hoverProgress);
        painter->drawPixmap(option.rect.topLeft(), result);
    } else if (selected) {
        painter->drawPixmap(option.rect.topLeft(), from);
    }

    qreal left, top, right, bottom;
    m_itemFrame->getMargins(left, top, right, bottom);
    const QRect r = option.rect.adjusted(left, top, -right, -bottom);


    // Draw the icon
    // =============
    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    const QRect ir = QStyle::alignedRect(option.direction, Qt::AlignTop | Qt::AlignHCenter,
                                         option.decorationSize, r);

    if (selected) {
        const QColor color = option.palette.brush(QPalette::Normal, QPalette::Highlight).color();
        QImage image = icon.pixmap(ir.size()).toImage();
        KIconEffect::colorize(image, color, 0.8f);
        icon = QIcon(QPixmap::fromImage(image));
    }

    icon.paint(painter, ir);

    const QRect tr = r.adjusted(0, ir.bottom() - r.top() + 2, 0, 0);

    QFont font = option.font;

    KFileItem item = qvariant_cast<KFileItem>(index.data(KDirModel::FileItemRole));
    if (item.isLink()) {
        font.setItalic(true);
    }

    // Draw the text label
    // ===================
    const QString text = index.data(Qt::DisplayRole).toString();

    QTextLayout layout;
    layout.setText(KStringHandler::preProcessWrap(text));
    layout.setFont(font);
    const QSize size = doTextLayout(layout, tr.size(), Qt::AlignHCenter,
                                    QTextOption::WrapAtWordBoundaryOrAnywhere);

    painter->setPen(option.palette.color(QPalette::Text));
    drawTextLayout(painter, layout, tr);


    // Draw the focus rect
    // ===================
    if (option.state & QStyle::State_HasFocus) {
        QRect fr = QStyle::alignedRect(layoutDirection(), Qt::AlignTop | Qt::AlignHCenter, size, tr.translated(0,2));
        fr.adjust(-2, -2, 2, 2);

        QColor color = Qt::white;
        color.setAlphaF(.33);

        QColor transparent = color;
        transparent.setAlphaF(0);

        QLinearGradient g1(0, fr.top(), 0, fr.bottom());
        g1.setColorAt(0, color);
        g1.setColorAt(1, transparent);

        QLinearGradient g2(fr.left(), 0, fr.right(), 0);
        g2.setColorAt(0, transparent);
        g2.setColorAt(.5, color);
        g2.setColorAt(1, transparent);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(g1, 0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(fr).adjusted(.5, .5, -.5, -.5), 2, 2);
        painter->setPen(QPen(g2, 0));
        painter->drawLine(QLineF(fr.left() + 2, fr.bottom() + .5,
                                 fr.right() - 2, fr.bottom() + .5));
        painter->restore();
    }
}

void IconView::paintMessage(QPainter *painter, const QRect &rect, const QString &message,
                            const QIcon &icon) const
{
    const QSize iconSize = icon.isNull() ? QSize() :
                               icon.actualSize(QSize(KIconLoader::SizeHuge, KIconLoader::SizeHuge));
    const QSize textConstraints = rect.size() - QSize(iconSize.width() + 4, 0);

    QTextLayout layout;
    layout.setText(message);
    layout.setFont(font());

    const QSize textSize = doTextLayout(layout, textConstraints, Qt::AlignLeft, QTextOption::WordWrap);
    const QSize size(iconSize.width() + 4 + textSize.width(), qMax(iconSize.height(), textSize.height()));
    const QRect r = QStyle::alignedRect(layoutDirection(), Qt::AlignCenter, size, rect);
    const QRect textRect = QStyle::alignedRect(layoutDirection(), Qt::AlignRight | Qt::AlignVCenter,
                                               textSize, r);
    const QRect iconRect = QStyle::alignedRect(layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter,
                                               iconSize, r);

    painter->setPen(palette().color(QPalette::Text));
    drawTextLayout(painter, layout, textRect);
    if (!icon.isNull()) {
        icon.paint(painter, iconRect);
    }
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

    painter->setClipRect(clipRect, Qt::IntersectClip);

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
                opt.state |= QStyle::State_Selected;
            }

            if (hasFocus() && index == m_selectionModel->currentIndex()) {
                opt.state |= QStyle::State_HasFocus;
            }

            if (m_items[i].needSizeAdjust) {
                const QSize size = itemSize(opt, index);
                m_items[i].rect.setHeight(size.height());
                m_items[i].needSizeAdjust = false;
                opt.rect = m_items[i].rect;
            }

            if (m_pressedIndex == index && m_drawIconShrinked) {
                opt.state |= QStyle::State_Sunken;
                oldDecorationSize = opt.decorationSize;
                opt.decorationSize *= 0.9;
            }

            paintItem(&p, opt, index);
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
        paintMessage(painter, cr, m_errorMessage, KIcon("dialog-error"));
    } else if (m_folderIsEmpty) {
        Plasma::Containment *containment = qobject_cast<Plasma::Containment*>(parentWidget());
        if (!containment || !containment->isContainment()) {
            paintMessage(painter, cr, i18n( "This folder is empty." ), KIcon() );
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
    const quint64 key = index.row();
    if (QRegion *region = m_regionCache.object(key)) {
        return *region;
    }

    QStyleOptionViewItemV4 option = viewOptions();
    option.rect = m_items[index.row()].rect;

    qreal left, top, right, bottom;
    m_itemFrame->getMargins(left, top, right, bottom);

    const QRect r = option.rect.adjusted(left, top, -right, -bottom);
    QRect ir = QStyle::alignedRect(option.direction, Qt::AlignTop | Qt::AlignHCenter,
                                   option.decorationSize, r);
    QRect tr = r.adjusted(0, ir.bottom() - r.top() + 2, 0, 0);

    KFileItem item = qvariant_cast<KFileItem>(index.data(KDirModel::FileItemRole));
    QFont fnt = font();
    if (item.isLink())
        fnt.setBold(true);

    QTextLayout layout;
    layout.setText(index.data(Qt::DisplayRole).toString());
    layout.setFont(font());
    const QSize ts = doTextLayout(layout, tr.size(), Qt::AlignHCenter,
                                  QTextOption::WrapAtWordBoundaryOrAnywhere);
    tr = QStyle::alignedRect(layoutDirection(), Qt::AlignTop | Qt::AlignHCenter, ts, tr);

    // Extend the icon rect so it touches the text rect
    if (ir.width() < tr.width()) {
        ir.setBottom(tr.top());
    } else {
        tr.setTop(ir.bottom());
    }

    QRegion region;
    region += ir;
    region += tr;

    m_regionCache.insert(key, new QRegion(region));
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

void IconView::updateEditorGeometry()
{
    QStyleOptionViewItemV4 option = viewOptions();
    option.rect = visualRect(m_editorIndex);

    const int frameWidth = m_editor->nativeWidget()->frameWidth();
    qreal left, top, right, bottom;
    m_itemFrame->getMargins(left, top, right, bottom);
    const QRect r = option.rect.adjusted(-frameWidth, top + option.decorationSize.height() + 2 - frameWidth,
                                         frameWidth, frameWidth);

    m_editor->nativeWidget()->setGeometry(r);
    m_editor->setPos(mapFromViewport(m_editor->nativeWidget()->pos()));
}

void IconView::renameSelectedIcon()
{
    QModelIndex index = m_selectionModel->currentIndex();
    if (!index.isValid()) {
        return;
    }

    // Don't allow renaming of files the aren't visible in the view
    const QRect rect = visualRect(index);
    if (!mapToViewport(contentsRect()).contains(rect)) {
        return;
    }

    QStyleOptionViewItemV4 option = viewOptions();
    option.rect = rect;

    m_editorIndex = index;

    m_editor = new ItemEditor(this, option, index);
    connect(m_editor, SIGNAL(closeEditor(QGraphicsWidget*,QAbstractItemDelegate::EndEditHint)),
                      SLOT(closeEditor(QGraphicsWidget*,QAbstractItemDelegate::EndEditHint)));

    updateEditorGeometry();

    m_editor->nativeWidget()->setFocus();
    m_editor->show();
    m_editor->setFocus();
}

void IconView::selectFirstIcon()
{
    if (!m_layoutBroken) {
        selectIcon(m_model->index(0, 0));
    }
    else {        //In case the user has made the view unsorted
        selectFirstOrLastIcon(true);
    }
}

void IconView::selectLastIcon()
{
    if (!m_layoutBroken) {
        selectIcon(m_model->index(m_model->rowCount()-1, 0));
    }
    else {        //In case the user has made the view unsorted
        selectFirstOrLastIcon(false);
    }
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

// ### Remove
void IconView::commitData(QWidget *editor)
{
    m_delegate->setModelData(editor, m_model, m_editorIndex);
}

// ### Remove
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

void IconView::closeEditor(QGraphicsWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    Q_UNUSED(hint)

    bool hadFocus = editor->hasFocus();

    editor->hide();
    editor->deleteLater();

    if (hadFocus) {
        setFocus();
    }

    m_editorIndex = QModelIndex();
    markAreaDirty(visibleArea());
}

void IconView::resizeEvent(QGraphicsSceneResizeEvent *e)
{
    updateScrollBarGeometry();

    if (m_validRows > 0) {
        if (m_alignment == Right) {
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
    if (m_editor) {
        updateEditorGeometry();
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

// Updates the tooltip for the hovered index
void IconView::updateToolTip()
{
    // Close the popup view if one is open
    m_toolTipShowTimer.stop();
    m_popupCausedWidget = 0;
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

void IconView::updateRubberband()
{
    const int scrollBarOffset = m_scrollBar->isVisible() ? m_scrollBar->geometry().width() : 0;
    const QRect realContentsRect = itemsBoundingRect().adjusted(-10, -10, 10, 10) | contentsRect().toAlignedRect();
    const QRect viewportRect = layoutDirection() == Qt::LeftToRight ?
                   realContentsRect.adjusted(0, 0, int(-scrollBarOffset), 0) :
                   realContentsRect.adjusted(int(scrollBarOffset), 0, 0, 0);
    const QPointF pos = mapToViewport(m_mouseMovedPos);
    const QRectF rubberBand = QRectF(m_buttonDownPos, pos).normalized();
    const QRect r = QRectF(rubberBand & viewportRect).toAlignedRect();

    const QModelIndex prevHoveredIndex = m_hoveredIndex;

    if (r != m_rubberBand) {
        const QPoint pt = pos.toPoint();
        QRectF dirtyRect = m_rubberBand | r;
        m_rubberBand = r;

        dirtyRect |= visualRect(m_hoveredIndex);
        m_hoveredIndex = QModelIndex();

        repaintSelectedIcons();
        selectIconsInArea(m_rubberBand, pt);

        markAreaDirty(dirtyRect);
    }

    if (prevHoveredIndex != m_hoveredIndex) {
        if (prevHoveredIndex.isValid()) {
            emit left(prevHoveredIndex);
        }
        if (m_hoveredIndex.isValid()) {
            emit entered(m_hoveredIndex);
        }
    }
}

void IconView::updateActionButtons()
{
    m_actionOverlay->setShowFolderButton(clickToViewFolders());
    m_actionOverlay->setShowSelectionButton(showSelectionMarker());
}

void IconView::checkIfFolderResult(const QModelIndex &index, bool isFolder)
{
    m_toolTipShowTimer.stop();
    if (index != m_hoveredIndex) {
        return;
    }

    // Start the timer if the index turned out to be a folder
    if (isFolder && index.isValid()) {
        // Use a longer delay if we don't have any popup view open or if it's been
        // longer than 1.5 seconds since the last popup view was opened or closed.
        if ((m_popupView && m_hoveredIndex != m_popupIndex) ||
            PopupView::lastOpenCloseTime().elapsed() < 1500) {
            m_toolTipShowTimer.start(500, this);
        } else {
            m_toolTipShowTimer.start(1000, this);
        }
    } else if (m_popupView) {
        m_popupView->delayedHide();
    }
}

void IconView::svgChanged()
{
    for (int i = 0; i < m_validRows; ++i) {
        m_items[i].needSizeAdjust = true;
    }

    // this updates the grid size, then calls layoutItems() which in turn repaints the view
    updateGridSize();
    updateActionButtons();
}

void IconView::viewScrolled()
{
    if (m_rubberBand.isValid()) {
        updateRubberband();
    }
}

void IconView::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    const QModelIndex index = indexAt(mapToViewport(event->pos()));
    if (index.isValid()) {
        emit entered(index);
        m_hoveredIndex = index;
        markAreaDirty(visualRect(index));
        if (!clickToViewFolders()) {
            AsyncFileTester::checkIfFolder(m_hoveredIndex, this, "checkIfFolderResult");
        }
    }
    updateToolTip();
}

void IconView::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    if (m_hoveredIndex.isValid()) {
        emit left(m_hoveredIndex);
        markAreaDirty(visualRect(m_hoveredIndex));
        m_hoveredIndex = QModelIndex();
        updateToolTip();
    }

    m_actionOverlay->forceHide(ActionOverlay::FadeOut);
}

void IconView::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const QModelIndex index = indexAt(mapToViewport(event->pos()));
    if (index != m_hoveredIndex) {
        if (m_hoveredIndex.isValid()) {
            emit left(m_hoveredIndex);
        }
        if (index.isValid()) {
            emit entered(index);
        }
        markAreaDirty(visualRect(index));
        markAreaDirty(visualRect(m_hoveredIndex));
        m_hoveredIndex = index;
        updateToolTip();

        if (!clickToViewFolders()) {
            AsyncFileTester::checkIfFolder(m_hoveredIndex, this, "checkIfFolderResult");
        }
    }
}

void IconView::keyPressEvent(QKeyEvent *event)
{
    if (m_columns == 0)  {
        //The layout isn't done until items are actually inserted into the model, so until that happens, m_columns will be 0
        return;
    }

    int hdirection = 0;
    int vdirection = 0;

    QModelIndex currentIndex = m_selectionModel->currentIndex();

    if (!event->text().isEmpty()) {
        bool sameKeyWasPressed = m_searchQuery.endsWith(event->text());
        m_searchQuery.append(event->text());
        m_searchQueryTimer.start(1500, this);   //clears search query when the user doesn't press any key

        // First try to match the exact icon string
        QModelIndexList matches = m_model->match(currentIndex, Qt::DisplayRole, m_searchQuery, 1,
                                                 Qt::MatchFixedString | Qt::MatchWrap);

        if (matches.count() <= 0) {
            // Exact match failed, try matching the beginning of the icon string
            matches = m_model->match(currentIndex, Qt::DisplayRole, m_searchQuery, 1,
                                     Qt::MatchStartsWith | Qt::MatchWrap);

            if (matches.count() <= 0 && sameKeyWasPressed) {
                // Didn't even match beginning, try next icon string starting with the same letter
                matches = m_model->match(currentIndex.sibling(currentIndex.row()+1, currentIndex.column()),
                        Qt::DisplayRole, event->text(), 1, Qt::MatchStartsWith | Qt::MatchWrap);
            }
        }

        if (matches.count() > 0) {
            selectIcon(matches.at(0));
        }
    }

    switch (event->key()) {
    case Qt::Key_Home:
        selectFirstIcon();
        return;
    case Qt::Key_End:
        selectLastIcon();
        return;
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
        if (!currentIndex.isValid()) {
            selectFirstIcon();
            return;
        }
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

        //Perform flow related calculations
        if (m_layout == Rows) {
            vMultiplier = m_columns;
            if (m_alignment == Right) {
                hMultiplier = -1;
            }
        } else { // Columns
            hMultiplier = m_rows;
            if (m_alignment == Right) {
                hMultiplier *= -1;
            }
        }

        newItem = currentIndex.row() + hdirection*hMultiplier + vdirection*vMultiplier;

        if ( (newItem < 0) || (newItem >= m_model->rowCount()) ) {
            newItem = prevItem;
        }

        nextIndex = currentIndex.sibling(newItem, currentIndex.column());
    } else {
        //If the user has moved the icons, i.e. the view is no longer sorted
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
    selectIcon(nextIndex);
}

void IconView::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // Make sure that any visible tooltip is hidden
    Plasma::ToolTipManager::self()->hide(m_toolTipWidget);
    delete m_popupView;

    m_toolTipShowTimer.stop();

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
                // A previously unselected icon clicked, clear and repaint selection
                // then select and repaint the icon at index
                const QRect dirtyRect = selectedItemsBoundingRect();
                m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markAreaDirty(dirtyRect);
                markAreaDirty(visualRect(index));
            }
        } else if (m_selectionModel->hasSelection()) {
            // Empty space clicked, clear and repaint selection
            const QRect dirtyRect = selectedItemsBoundingRect();
            m_selectionModel->clearSelection();
            markAreaDirty(dirtyRect);
        }
        event->ignore(); // Causes contextMenuEvent() to get called
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(pos);

        // If an icon was pressed
        if (index.isValid()) {
            // If ctrl is held
            if (event->modifiers() & Qt::ControlModifier) {
                m_selectionModel->select(index, QItemSelectionModel::Toggle);
                m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                markAreaDirty(visualRect(index));
            } else if (event->modifiers() & Qt::ShiftModifier) {        //If shift is held
                QModelIndex current = m_selectionModel->currentIndex();
                if (m_layoutBroken) {
                    selectIconsInArea(QRect(visualRect(current).center(), visualRect(index).center()), pos.toPoint());
                }
                else {
                    selectIconRange(current, index);
                }
            } else {
                if (!m_selectionModel->isSelected(index)) {
                    // If not already selected
                    const QRect dirtyRect = selectedItemsBoundingRect();
                    m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
                    m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                    markAreaDirty(dirtyRect);
                }
                m_drawIconShrinked = KGlobalSettings::singleClick();
                markAreaDirty(visualRect(index));
            }

            m_pressedIndex = index;
            m_buttonDownPos = pos;
            return;
        }

        // If empty space was pressed
        m_pressedIndex = QModelIndex();
        m_buttonDownPos = pos;

        // If a containment action is assigned to the left mouse button,
        // give it precedence over rubberband-selections
        Plasma::Containment *containment = qobject_cast<Plasma::Containment*>(parentWidget());
        if (containment && containment->isContainment()) {
            const QString trigger = Plasma::ContainmentActions::eventToString(event);
            if (!containment->containmentActions(trigger).isEmpty()) {
                event->ignore();
                return;
            }
        }

        if (event->modifiers() & Qt::ControlModifier) {
            // Make the current selection persistent
            m_selectionModel->select(m_selectionModel->selection(), QItemSelectionModel::Select);
        }

        if (m_selectionModel->hasSelection()) {
            if (!(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
                const QRect dirtyRect = selectedItemsBoundingRect();
                m_selectionModel->clearSelection();
                markAreaDirty(dirtyRect);
            }
        }
    }

    if (event->button() == Qt::MidButton) {
        event->ignore();
    }
}

void IconView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_rubberBand.isValid()) {
            markAreaDirty(m_rubberBand);
            m_rubberBand = QRect();
            stopAutoScrolling();
            return;
        }

        const QPointF pos = mapToViewport(event->pos());
        const QModelIndex index = indexAt(pos);

        bool ctrlOrShiftPressed = (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));
        if (index.isValid() && index == m_pressedIndex) {
            if (ctrlOrShiftPressed) {
                markAreaDirty(visibleArea());
            } else {
                if (KGlobalSettings::singleClick()) {
                    // we ignore double clicks when in single click mode
                    if (!m_doubleClick) {
                        emit activated(index);
                        markAreaDirty(visualRect(index));
                    }
                } else {
                    // since icon shrinking is delayed, we always repaint after mouse release
                    markAreaDirty(visualRect(index));
                }

                // We don't clear and update the selection and current index in
                // mousePressEvent() if the item is already selected when it's pressed,
                // so we need to do that here.
                if (m_selectionModel->currentIndex() != index ||
                    m_selectionModel->selectedIndexes().count() > 1) {
                    const QRect dirtyRect = selectedItemsBoundingRect();
                    m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
                    m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                    markAreaDirty(dirtyRect);
                }
            }
        }
    }

    m_doubleClick = false;
    m_pressedIndex = QModelIndex();
    m_drawIconShrinked = false;
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

    m_pressedIndex = index;
    m_drawIconShrinked = true;

    // Activate the item
    emit activated(index);

    // A double-click implies a single-click, which means that the selection will
    // be reset and repainted on mousePressEvent
    // so we only need to repant the icon itself to get the "sunken" effect
    markAreaDirty(visualRect(index));
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

    m_mouseMovedPos = event->pos();

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

    updateRubberband();
}

void IconView::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    Plasma::Containment *containment = qobject_cast<Plasma::Containment*>(parentWidget());
    if ((containment && containment->isContainment() && !m_scrollBar->isVisible()) ||
        (event->modifiers() & Qt::CTRL) ||
        (event->orientation() == Qt::Horizontal)) {
        // Let the event propagate to the parent widget
        event->ignore();
        return;
    }

    stopAutoScrolling();
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
    if (m_rubberBand.isValid()) {
        markAreaDirty(m_rubberBand);
        m_rubberBand = QRect();
    }
}

void IconView::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    const bool accepted = KUrl::List::canDecode(event->mimeData()) ||
                          (event->mimeData()->hasFormat(QLatin1String("application/x-kde-ark-dndextract-service")) &&
                           event->mimeData()->hasFormat(QLatin1String("application/x-kde-ark-dndextract-path")));
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

    // Open a popup view for this index if it's a folder
    if (!m_popupView || m_hoveredIndex != m_popupIndex) {
        // If we're not already showing a popup view for this index
        m_popupCausedWidget = event->widget();
        AsyncFileTester::checkIfFolder(m_hoveredIndex, this, "checkIfFolderResult");
    }

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

        if (containment->immutability() == Plasma::Mutable && !appletList.isEmpty()) {
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
        if (mimeName.startsWith(QLatin1String("image/")) ||
                QImageReader::supportedImageFormats().contains(suffix)) {
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
        const QMimeData *mimeData = event->mimeData();

        QDropEvent ev(event->screenPos(), event->possibleActions(), mimeData,
                      event->buttons(), event->modifiers());
        ev.setDropAction(event->dropAction());
        //kDebug() << "dropping to" << m_url << "with" << view() << event->modifiers();

        if (mimeData->hasFormat(QLatin1String("application/x-kde-ark-dndextract-service")) &&
            mimeData->hasFormat(QLatin1String("application/x-kde-ark-dndextract-service"))) {
            const QString remoteDBusClient = mimeData->data(QLatin1String("application/x-kde-ark-dndextract-service"));
            const QString remoteDBusPath = mimeData->data(QLatin1String("application/x-kde-ark-dndextract-path"));

            QDBusMessage message =
                QDBusMessage::createMethodCall(remoteDBusClient, remoteDBusPath,
                                               QLatin1String("org.kde.ark.DndExtract"),
                                               QLatin1String("extractSelectedFilesTo"));
            message.setArguments(QVariantList() << m_dirModel->dirLister()->url().pathOrUrl());

            QDBusConnection::sessionBus().call(message);
            return;
        }

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
            const QRect rect(m_items[index.row()].rect.topLeft(), gridSize());
            boundingRect |= rect;
            indexes.append(index);
        }
    }

    int rowCount, colCount;
    const QRect cr = m_alignToGrid ? adjustedContentsRect(gridSize(), &rowCount, &colCount)
                            : contentsRect().toRect();

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
    if (m_layout == Columns) {
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

        if (!m_savedPositions.isEmpty()) {
            // If the contents margins change while a layout with saved positions is
            // in progress, we have to adjust all the saved positions and restart the
            // layout process. The saved positions are relative to the top left corner
            // of the content area.
            const QPoint delta(left - m_margins[Plasma::LeftMargin], top - m_margins[Plasma::TopMargin]);
            QMutableHashIterator<QString, QPoint> i(m_savedPositions);
            while (i.hasNext()) {
                i.next();
                i.setValue(i.value() + delta);
            }
            m_validRows = 0;
            m_delayedLayoutTimer.start(10, this);
            if (m_delayedCacheClearTimer.isActive()) {
                m_delayedCacheClearTimer.start(5000, this);
            }
        }

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
        bool horizontalFlow = (m_layout == Rows);
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

            if (m_alignment == Left) {
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
                m_regionCache.clear();
                markAreaDirty(mapToViewport(rect()).toAlignedRect());
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
        updateGridSize();
        // Fallthrough intended

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
    m_actionOverlay->forceHide(ActionOverlay::HideNow);

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
        paintItem(&p, option, index);
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

void IconView::selectIcon(QModelIndex index)
{
    if (!index.isValid()) {
        return;
    }
    repaintSelectedIcons();
    m_selectionModel->select(index, QItemSelectionModel::ClearAndSelect);
    m_selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    scrollTo(index);
    m_pressedIndex = index;
    markAreaDirty(visualRect(index));
}

void IconView::selectFirstOrLastIcon(bool firstIcon)
{
    int minVertical=0;
    int minHorizontal=0;
    int dirn=1;    //Useful in calculations as it stores whether view is LeftToRight or RightToLeft
    int isFirst = firstIcon ? 1 : -1;    //Useful in calculations to decide whether to select First or Last icon
    QModelIndex toSelect;

    if (m_alignment == Right) {
        dirn=-1;
    }

    for (int i = 0; i < m_validRows; i++) {
        const QModelIndex tempIndex = m_model->index(i, 0);
        const QPoint pos = visualRect(tempIndex).center();
        //i==0 is used so that minHorizontal and minVertical are set to some value at the first item
        if (((pos.x()*dirn*isFirst) < (minHorizontal*dirn*isFirst) && (pos.y()*isFirst) <= (minVertical*isFirst)) || i==0) {
            minHorizontal = pos.x();
            toSelect = tempIndex;
        }
        if (((pos.y()*isFirst) < (minVertical*isFirst) && (pos.x()*dirn*isFirst) <= (minHorizontal*dirn*isFirst)) || i==0) {
            minVertical = pos.y();
            toSelect = tempIndex;
        }
    }
    selectIcon(toSelect);
}

void IconView::selectIconsInArea(const QRect &area, const QPoint &finalPos)
{
    QModelIndex m;
    QRect dirtyRect;

    // Select the indexes inside the area
    QItemSelection selection;
    for (int i = 0; i < m_items.size(); i++) {
        QModelIndex index = m_model->index(i, 0);
        if (!indexIntersectsRect(index, area))
            continue;

        int start = i;

        do {
            dirtyRect |= m_items[i].rect;
            if (m_items[i].rect.contains(finalPos) && visualRegion(index).contains(finalPos)) {
               m_hoveredIndex = index;
            }
            index = m_model->index(++i, 0);
        } while (i < m_items.size() && indexIntersectsRect(index, area));

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

void IconView::selectIconRange(const QModelIndex &begin, const QModelIndex &end)
{
    m_selectionModel->select(QItemSelection(begin, end), QItemSelectionModel::Select);
    repaintSelectedIcons();
}

QRect IconView::selectedItemsBoundingRect() const
{
    QRect rect;
    foreach (const QModelIndex &index, m_selectionModel->selectedIndexes()) {
            rect |= visualRect(index);
    }
    return rect;
}

void IconView::repaintSelectedIcons()
{
    markAreaDirty(selectedItemsBoundingRect());
}

void IconView::popupCloseRequested()
{
    if (m_popupView && (!m_hoveredIndex.isValid() || m_hoveredIndex != m_popupIndex)) {
        m_popupView->hide();
        m_popupView->deleteLater();
    }
}

void IconView::setClickToViewFolders(bool click)
{
    m_clickToViewFolders = click;
    m_actionOverlay->setShowFolderButton(clickToViewFolders());
}

bool IconView::clickToViewFolders() const
{
    return overlayEnabled() ? m_clickToViewFolders : false;
}

void IconView::openPopup(const QModelIndex &index)
{
    if (m_popupView && m_popupIndex == index) {
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

    // Make sure the index is valid
    if (!index.isValid()) {
        return;
    }

    const QPointF viewPos = mapFromViewport(visualRect(index)).center();
    const QPoint scenePos = mapToScene(viewPos).toPoint();
    QGraphicsView *gv = 0;

    if (m_popupCausedWidget) {
        gv = qobject_cast<QGraphicsView*>(m_popupCausedWidget->parentWidget());
    } else {
        // We position the popup relative to the view under the mouse cursor
        gv = Plasma::viewFor(m_actionOverlay);
    }

    const QPoint pos = gv ? gv->mapToGlobal(gv->mapFromScene(scenePos)) : QPoint();
    m_popupIndex = index;
    m_popupView = new PopupView(m_popupIndex, pos, m_popupShowPreview, m_popupPreviewPlugins, this);
    connect(m_popupView, SIGNAL(destroyed(QObject*)), SIGNAL(popupViewClosed()));
    connect(m_popupView, SIGNAL(requestClose()), SLOT(popupCloseRequested()));
}

void IconView::setShowSelectionMarker(bool show)
{
    m_showSelectionMarker = show;
    m_actionOverlay->setShowSelectionButton(showSelectionMarker());
}

bool IconView::showSelectionMarker() const
{
    return overlayEnabled() ? m_showSelectionMarker : false;
}

bool IconView::overlayEnabled() const
{
    // Do not let the action overlay cover the icon
    return gridSize().width() - m_iconSize.width() > 2*qMin(m_actionOverlay->iconSize().width(), m_actionOverlay->iconSize().height());
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

        bool horizontalFlow = (m_layout == Rows);
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
        openPopup(m_hoveredIndex);
    } else if (event->timerId() == m_searchQueryTimer.timerId()) {
        m_searchQuery.clear();
        m_searchQueryTimer.stop();
    }
}

#include "iconview.moc"

