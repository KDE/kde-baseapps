/*
 *   Copyright © 2008, 2009 Fredrik Höglund <fredrik@kde.org>
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

#include "style.h"

#include <QPainter>
#include <QStyleOption>

#include <plasma/framesvg.h>

FolderViewStyle::FolderViewStyle()
    : QCommonStyle()
{
    m_frame = new Plasma::FrameSvg(this);
    m_frame->setImagePath("widgets/viewitem");
    m_frame->setCacheAllRenderedFrames(true);
    m_frame->setElementPrefix("normal");
}

FolderViewStyle::~FolderViewStyle()
{
}

void FolderViewStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                                    QPainter *painter, const QWidget *widget) const
{
    switch (element)
    {
    case PE_PanelItemViewItem:
    {
        const bool hover = (option->state & State_MouseOver);
        const bool selected = (option->state & State_Selected);

        if (selected && hover) {
            m_frame->setElementPrefix("selected+hover");
        } else if (selected) {
            m_frame->setElementPrefix("selected");
        } else if (hover) {
            m_frame->setElementPrefix("hover");
        } else {
            m_frame->setElementPrefix("normal");
        }
    
        if (selected || hover) {
            m_frame->resizeFrame(option->rect.size());
            m_frame->paintFrame(painter, option->rect.topLeft());
        }
        break;
    }

    case PE_FrameFocusRect:
    {
        //QColor color = option->palette.color(QPalette::Text);
        QColor color = Qt::white;
        color.setAlphaF(.33);

        QColor transparent = color;
        transparent.setAlphaF(0);

        QLinearGradient g1(0, option->rect.top(), 0, option->rect.bottom());
        g1.setColorAt(0, color);
        g1.setColorAt(1, transparent);

        QLinearGradient g2(option->rect.left(), 0, option->rect.right(), 0);
        g2.setColorAt(0, transparent);
        g2.setColorAt(.5, color);
        g2.setColorAt(1, transparent);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(g1, 0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(option->rect).adjusted(.5, .5, -.5, -.5), 2, 2);
        painter->setPen(QPen(g2, 0));
        painter->drawLine(QLineF(option->rect.left() + 2, option->rect.bottom() + .5,
                                 option->rect.right() - 2, option->rect.bottom() + .5));
        painter->restore();
        break;
    }

    default:
        return QCommonStyle::drawPrimitive(element, option, painter, widget);
    }
}

