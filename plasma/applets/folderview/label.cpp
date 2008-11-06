/*
 *   Copyright © 2008 Fredrik Höglund <fredrik@kde.org>
 *   Copyright © 2008 Andrew Lake <jamboarder@yahoo.com>
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

#include "label.h"

#include <QString>
#include <QPainter>

#include "plasma/paintutils.h"

Label::Label(QGraphicsWidget *parent)
    : QGraphicsWidget(parent)
{
    setMinimumHeight(QFontMetrics(font()).lineSpacing() + 4);
    setMaximumHeight(QFontMetrics(font()).lineSpacing() + 4);
    setCacheMode(DeviceCoordinateCache);
}

Label::~Label()
{
}

void Label::setText(const QString &text)
{
    m_text = text;
    update();
}

QString Label::text() const
{
    return m_text;
}

void Label::setDrawShadow(bool on)
{
    m_drawShadow = on;
    update();
}

bool Label::drawShadow() const
{
    return m_drawShadow;
}

void Label::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    const QString text = QFontMetrics(font()).elidedText(m_text, Qt::ElideMiddle, contentsRect().width());

    QColor shadowColor;
    QPoint shadowOffset;
    if (!m_drawShadow) {
        shadowColor = Qt::transparent;
        shadowOffset = QPoint();
    } else if (qGray(palette().color(QPalette::Text).rgb()) > 192) {
        shadowColor = Qt::black;
        shadowOffset = QPoint(1, 1);
    } else {
        shadowColor = Qt::white;
        shadowOffset = QPoint();
    }

    QPixmap titlePixmap = Plasma::PaintUtils::shadowText(text, palette().color(QPalette::Text),
                                                         shadowColor, shadowOffset);
    painter->drawPixmap(contentsRect().topLeft(), titlePixmap);

    int width = contentsRect().width();

    //Draw underline
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
    painter->drawPixmap(0, contentsRect().height() - 2, m_divider);
}

#include "label.moc"

