/*
 *   Copyright © 2008 Fredrik Höglund <fredrik@kde.org>
 *
 *   The smooth scrolling code is based on the code in KHTMLView,
 *   Copyright © 2006-2008 Germain Garand <germain@ebooksfrance.org>
 *   Copyright © 2008 Allan Sandfeld Jensen <kde@carewolf.com>
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

#include "abstractitemview.h"
#include "proxymodel.h"
#include "style.h"

#include <QItemSelectionModel>
#include <QPaintEngine>
#include <KDirModel>
#include <KFileItemDelegate>

#ifdef Q_WS_X11
#  include <QX11Info>
#  include <X11/Xlib.h>
#endif


static const int sSmoothScrollTime = 140;
static const int sSmoothScrollTick = 14;


AbstractItemView::AbstractItemView(QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
      m_delegate(0),
      m_lastScrollValue(0),
      m_viewScrolled(false),
      m_dx(0),
      m_ddx(0),
      m_dddx(0),
      m_rdx(0),
      m_dy(0),
      m_ddy(0),
      m_dddy(0),
      m_rdy(0),
      m_smoothScrolling(false),
      m_autoScrollSpeed(0)
{
    m_scrollBar = new Plasma::ScrollBar(this);
    connect(m_scrollBar, SIGNAL(valueChanged(int)), SLOT(scrollBarValueChanged(int)));
    connect(m_scrollBar->nativeWidget(), SIGNAL(actionTriggered(int)), SLOT(scrollBarActionTriggered(int)));
    connect(m_scrollBar->nativeWidget(), SIGNAL(sliderReleased()), SLOT(scrollBarSliderReleased()));

    // This is a dummy widget that's never shown - it's just passed to
    // KFileItemDelegate in the style options, so it will use the widget's
    // style to draw the view item backgrounds.
    m_styleWidget = new QWidget;
    m_style = new FolderViewStyle;
    m_styleWidget->setStyle(m_style);

    const int size = style()->pixelMetric(QStyle::PM_LargeIconSize);
    m_iconSize = QSize(size, size);
}

AbstractItemView::~AbstractItemView()
{
    delete m_styleWidget;
    delete m_style;
}

void AbstractItemView::setModel(QAbstractItemModel *model)
{
    m_model = static_cast<ProxyModel*>(model);
    m_dirModel = static_cast<KDirModel*>(m_model->sourceModel());

    connect(m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(rowsInserted(QModelIndex,int,int)));
    connect(m_model, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(rowsRemoved(QModelIndex,int,int)));
    connect(m_model, SIGNAL(modelReset()), SLOT(modelReset()));
    connect(m_model, SIGNAL(layoutChanged()), SLOT(layoutChanged()));
    connect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(dataChanged(QModelIndex,QModelIndex)));
}

QAbstractItemModel *AbstractItemView::model() const
{
    return m_model;
}

void AbstractItemView::setSelectionModel(QItemSelectionModel *model)
{
    m_selectionModel = model;
}

QItemSelectionModel *AbstractItemView::selectionModel() const
{
    return m_selectionModel;
}

void AbstractItemView::setItemDelegate(KFileItemDelegate *delegate)
{
    m_delegate = static_cast<KFileItemDelegate*>(delegate);

    connect(m_delegate, SIGNAL(closeEditor(QWidget*,QAbstractItemDelegate::EndEditHint)),
            SLOT(closeEditor(QWidget*,QAbstractItemDelegate::EndEditHint)));
    connect(m_delegate, SIGNAL(commitData(QWidget*)), SLOT(commitData(QWidget*)));
}

KFileItemDelegate *AbstractItemView::itemDelegate() const
{
    return m_delegate;
}

void AbstractItemView::setIconSize(const QSize &iconSize)
{
    m_iconSize = iconSize;
}

QSize AbstractItemView::iconSize() const
{
    return m_iconSize;
}

QScrollBar *AbstractItemView::verticalScrollBar() const
{
    return m_scrollBar->nativeWidget();
}

QRect AbstractItemView::visibleArea() const
{
    return mapToViewport(contentsRect()).toAlignedRect();
}

// Marks the given rect in viewport coordinates, as dirty and schedules a repaint.
void AbstractItemView::markAreaDirty(const QRect &rect)
{
    if (!rect.isEmpty() && rect.intersects(visibleArea())) {
        m_dirtyRegion += rect;
        update(mapFromViewport(rect));
    }
}

// This function scrolls the contents of the backbuffer the distance the scrollbar
// has moved since the last time this function was called.
QRect AbstractItemView::scrollBackBuffer()
{
    const int value = m_scrollBar->value();
    const int delta = m_lastScrollValue - value;
    m_lastScrollValue = value;

    if (qAbs(delta) >= m_pixmap.height()) {
        return visibleArea();
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

#if defined(Q_WS_X11)
    const QPaintEngine::Type type = m_pixmap.paintEngine()->type();
    if (type == QPaintEngine::X11) {
        Display *dpy = QX11Info::display();
        GC gc = XCreateGC(dpy, m_pixmap.handle(), 0, 0);
        XCopyArea(dpy, m_pixmap.handle(), m_pixmap.handle(), gc, 0, sy, m_pixmap.width(), h, 0, dy);
        XFreeGC(dpy, gc);
    } else if (type == QPaintEngine::Raster) {
        // Hack to prevent the image from detaching
        const QImage image = m_pixmap.toImage();
        const uchar *src = image.scanLine(sy);
        uchar *dst = const_cast<uchar*>(image.scanLine(dy));
        memmove((void*)dst, (const void*)src, h * image.bytesPerLine());
    } else
#endif
    {
        dirty = m_pixmap.rect();
    }

    return mapToViewport(dirty.translated(contentsRect().topLeft().toPoint())).toAlignedRect();
}

void AbstractItemView::prepareBackBuffer()
{
    const QRect cr = contentsRect().toRect();

    // Make sure the backbuffer pixmap has the same size as the content rect
    if (m_pixmap.size() != cr.size()) {
        QPixmap pixmap(cr.size());
        pixmap.fill(Qt::transparent);
        if (!m_pixmap.isNull()) {
            // Static content optimization
#ifdef Q_WS_X11
            if (m_pixmap.paintEngine()->type() == QPaintEngine::X11) {
                GC gc = XCreateGC(QX11Info::display(), pixmap.handle(), 0, NULL);
                XCopyArea(QX11Info::display(), m_pixmap.handle(), pixmap.handle(), gc, 0, 0,
                          m_pixmap.width(), m_pixmap.height(), 0, 0);
                XFreeGC(QX11Info::display(), gc);
            } else
#endif
            {
                QPainter p(&pixmap);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.drawPixmap(0, 0, m_pixmap);
            }
            QRegion region(pixmap.rect());
            region -= m_pixmap.rect();
            region.translate(0, m_scrollBar->value());
            m_dirtyRegion |= region;
        } else {
            m_dirtyRegion = QRegion(visibleArea());
        }
        m_pixmap = pixmap;
    }

    if (m_viewScrolled) {
        m_dirtyRegion += scrollBackBuffer();
        m_viewScrolled = false;
    }
}

// This function draws the backbuffer pixmap on the widget, and fades out the top
// and bottom if as needed.
void AbstractItemView::syncBackBuffer(QPainter *painter, const QRect &clipRect)
{
    const QRect cr = contentsRect().toRect();

    const int fadeHeight = 16;
    const QRect topFadeRect(cr.x(), cr.y(), cr.width(), fadeHeight);
    const QRect bottomFadeRect(cr.bottomLeft() - QPoint(0, fadeHeight), QSize(cr.width(), fadeHeight));
    int scrollValue = m_scrollBar->value();
    int maxValue = m_scrollBar->maximum();

    // Draw the backbuffer on the widget
    // =================================
    if ((scrollValue > 0 && topFadeRect.intersects(clipRect)) ||
        (scrollValue < maxValue && bottomFadeRect.intersects(clipRect)))
    {
        QPixmap pixmap = m_pixmap;
        QPainter p(&pixmap);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);

        // Fade out the top section of the pixmap if the scrollbar slider isn't at the top
        if (scrollValue > 0 && topFadeRect.intersects(clipRect))
        {
            if (m_topFadeTile.isNull())
            {
                m_topFadeTile = QPixmap(256, fadeHeight);
                m_topFadeTile.fill(Qt::transparent);
                QLinearGradient g(0, 0, 0, fadeHeight);
                g.setColorAt(0, Qt::transparent);
                g.setColorAt(1, Qt::black);
                QPainter p(&m_topFadeTile);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.fillRect(0, 0, 256, fadeHeight, g);
                p.end();
            }
            p.drawTiledPixmap(0, 0, m_pixmap.width(), fadeHeight, m_topFadeTile);
        }

        // Fade out the bottom part of the pixmap if the scrollbar slider isn't at the bottom
        if (scrollValue < maxValue && bottomFadeRect.intersects(clipRect))
        {
            if (m_topFadeTile.isNull())
            {
                m_bottomFadeTile = QPixmap(256, fadeHeight);
                m_bottomFadeTile.fill(Qt::transparent);
                QLinearGradient g(0, 0, 0, fadeHeight);
                g.setColorAt(0, Qt::black);
                g.setColorAt(1, Qt::transparent);
                QPainter p(&m_bottomFadeTile);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.fillRect(0, 0, 256, fadeHeight, g);
                p.end();
            }
            p.drawTiledPixmap(0, m_pixmap.height() - fadeHeight, m_pixmap.width(), fadeHeight, m_bottomFadeTile);
        }
        p.end();

        painter->drawPixmap(cr.topLeft(), pixmap);
    }
    else
    {
        painter->drawPixmap(cr.topLeft(), m_pixmap);
    }
}

void AbstractItemView::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)
}

void AbstractItemView::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)
}

void AbstractItemView::modelReset()
{
}

void AbstractItemView::layoutChanged()
{
}

void AbstractItemView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)
}

void AbstractItemView::commitData(QWidget *editor)
{
    Q_UNUSED(editor)
}

void AbstractItemView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    Q_UNUSED(editor)
    Q_UNUSED(hint)
}


void AbstractItemView::scrollTo(const QModelIndex &index,  QAbstractItemView::ScrollHint hint)
{
    Q_UNUSED(hint)

    const QRectF r = mapFromViewport(visualRect(index));

    if (r.top() < 0) {
        smoothScroll(0, r.top());
    } else if (r.bottom() > geometry().height()) {
        smoothScroll(0, r.bottom() - geometry().height());
    }
}

void AbstractItemView::autoScroll(ScrollDirection direction, int pixelsPerSecond)
{
    m_scrollDirection = direction;
    m_autoScrollSetSpeed = direction == ScrollUp ? -pixelsPerSecond : pixelsPerSecond;

    if (!m_autoScrollTimer.isActive()) {
        m_autoScrollSpeed = 1;
        m_autoScrollTime.restart();
        m_autoScrollTimer.start(1000 / 30, this);
    }
}

void AbstractItemView::stopAutoScrolling()
{
    m_autoScrollSetSpeed = 0;
}

void AbstractItemView::scrollBarValueChanged(int value)
{
    Q_UNUSED(value)

    m_viewScrolled = true;
    update();
}

void AbstractItemView::scrollBarActionTriggered(int action)
{
    switch (action)
    {
    case QAbstractSlider::SliderSingleStepAdd:
    case QAbstractSlider::SliderSingleStepSub:
    case QAbstractSlider::SliderPageStepAdd:
    case QAbstractSlider::SliderPageStepSub:
        smoothScroll(0, m_scrollBar->nativeWidget()->sliderPosition() - m_scrollBar->value());
        break;

    case QAbstractSlider::SliderToMinimum:
    case QAbstractSlider::SliderToMaximum:
        // Use a delayed call since the value won't propagate until after this function returns
        QMetaObject::invokeMethod(this, "finishedScrolling", Qt::QueuedConnection);
        break;
    }
}

void AbstractItemView::scrollBarSliderReleased()
{
    finishedScrolling();
}

void AbstractItemView::finishedScrolling()
{
}

void AbstractItemView::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_smoothScrollTimer.timerId()) {
        scrollTick();
    } else if (event->timerId() == m_autoScrollTimer.timerId()) {
        int step = qRound(m_autoScrollTime.elapsed() * (m_autoScrollSpeed / 1000.));
        m_autoScrollTime.restart();

        if (m_scrollDirection == ScrollUp && m_scrollBar->value() > m_scrollBar->minimum()) {
            m_scrollBar->setValue(qMax(m_scrollBar->minimum(), m_scrollBar->value() + step));
        } else if (m_scrollDirection == ScrollDown && m_scrollBar->value() < m_scrollBar->maximum()) {
            m_scrollBar->setValue(qMin(m_scrollBar->maximum(), m_scrollBar->value() + step));
        } else {
            m_autoScrollSetSpeed = 0;
            m_autoScrollSpeed = 0;
        }

        if (m_autoScrollSetSpeed > m_autoScrollSpeed) {
            int delta;
            if (m_autoScrollSpeed >= 0) {
                delta = qBound(2, m_autoScrollSpeed * 2, 30);
            } else {
                delta = qBound(2, qAbs(m_autoScrollSpeed) / 2, 30);
            }
            m_autoScrollSpeed = qMin(m_autoScrollSpeed + delta, m_autoScrollSetSpeed);
        } else if (m_autoScrollSetSpeed < m_autoScrollSpeed) {
            int delta;
            if (m_autoScrollSpeed >= 0) {
                delta = qBound(2, m_autoScrollSpeed / 2, 30);
            } else {
                delta = qBound(2, qAbs(m_autoScrollSpeed * 2), 30);
            }
            m_autoScrollSpeed = qMax(m_autoScrollSetSpeed, m_autoScrollSpeed - delta);
        }

        if (m_autoScrollSpeed == 0 && m_autoScrollSetSpeed == 0) {
            m_autoScrollTimer.stop();
        }
    }
}

void AbstractItemView::startScrolling()
{
    m_smoothScrolling = true;
    m_smoothScrollTimer.start(sSmoothScrollTick, this);
}

void AbstractItemView::stopScrolling()
{
    m_smoothScrollTimer.stop();
    m_dx = m_dy = 0;
    m_ddx = m_ddy = 0;
    m_rdx = m_rdy = 0;
    m_dddx = m_dddy = 0;
    m_smoothScrolling = false;
    finishedScrolling();
}

void AbstractItemView::smoothScroll(int dx, int dy)
{
    // full scroll is remaining scroll plus new scroll
    m_dx = m_dx + dx;
    m_dy = m_dy + dy;

    if (m_dx == 0 && m_dy == 0) return;

    int steps = sSmoothScrollTime/sSmoothScrollTick;

    // average step size (stored in 1/16 px/step)
    m_ddx = (m_dx*16)/(steps+1);
    m_ddy = (m_dy*16)/(steps+1);

    if (qAbs(m_ddx) < 64 && qAbs(m_ddy) < 64) {
        // Don't move slower than average 4px/step in minimum one direction
        if (m_ddx > 0) m_ddx = qMax(m_ddx, 64);
        if (m_ddy > 0) m_ddy = qMax(m_ddy, 64);
        if (m_ddx < 0) m_ddx = qMin(m_ddx, -64);
        if (m_ddy < 0) m_ddy = qMin(m_ddy, -64);
        // This means fewer than normal steps
        steps = qMax(m_ddx ? (m_dx*16)/m_ddx : 0, m_ddy ? (m_dy*16)/m_ddy : 0);
        if (steps < 1) steps = 1;
        m_ddx = (m_dx*16)/(steps+1);
        m_ddy = (m_dy*16)/(steps+1);
    }

    // step size starts at double average speed and ends at 0
    m_ddx *= 2;
    m_ddy *= 2;

    // deacceleration speed
    m_dddx = (m_ddx+1)/steps;
    m_dddy = (m_ddy+1)/steps;

    if (!m_smoothScrolling) {
        startScrolling();
        scrollTick();
    }
    m_smoothScrollStopwatch.start();
}

void AbstractItemView::scrollTick() {
    if (m_dx == 0 && m_dy == 0) {
        stopScrolling();
        return;
    }

    // step size + remaining partial step
    int tddx = m_ddx + m_rdx;
    int tddy = m_ddy + m_rdy;

    // don't go under 1px/step
    if (tddx > 0 && tddx < 16) tddx = 16;
    if (tddy > 0 && tddy < 16) tddy = 16;
    if (tddx < 0 && tddx > -16) tddx = -16;
    if (tddy < 0 && tddy > -16) tddy = -16;

    // full pixel steps to scroll in this step
    int ddx = tddx / 16;
    int ddy = tddy / 16;
    // remaining partial step (this is especially needed for 1.x sized steps)
    m_rdx = tddx % 16;
    m_rdy = tddy % 16;

    // limit step to requested scrolling distance
    if (qAbs(ddx) > qAbs(m_dx)) ddx = m_dx;
    if (qAbs(ddy) > qAbs(m_dy)) ddy = m_dy;

    // Don't stop if deaccelerated too fast
    if (!ddx) ddx = m_dx;
    if (!ddy) ddy = m_dy;

    // update remaining scroll
    m_dx -= ddx;
    m_dy -= ddy;

    m_scrollBar->setValue(m_scrollBar->value() + ddy);

    // only consider decelerating if we aren't too far behind schedule
    if (m_smoothScrollStopwatch.elapsed() < 2 * sSmoothScrollTick) {
        // update scrolling speed
        int dddx = m_dddx;
        int dddy = m_dddy;
        // don't change direction
        if (abs(dddx) > abs(m_ddx)) dddx = m_ddx;
        if (abs(dddy) > abs(m_ddy)) dddy = m_ddy;

        m_ddx -= dddx;
        m_ddy -= dddy;
    }
    m_smoothScrollStopwatch.start();
}

#include "abstractitemview.moc"
