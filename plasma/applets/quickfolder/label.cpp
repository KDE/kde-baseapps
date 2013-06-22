/*
 *   Copyright © 2008, 2009 Fredrik Höglund <fredrik@kde.org>
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
    QFontMetrics fm(font());
    setMinimumHeight(fm.height() + 4);
    setMaximumHeight(fm.height() + 4);
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

    QColor color = palette().color(QPalette::Text);
    color.setAlphaF(.75);

    QFontMetrics fm(font());
    const QString text = fm.elidedText(m_text, Qt::ElideMiddle, contentsRect().width());

    painter->save();
    painter->setFont(font());
    painter->setPen(color);
    painter->drawText(contentsRect(), Qt::AlignCenter, text);
    painter->restore();
}

#include "label.moc"

