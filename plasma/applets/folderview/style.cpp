/*
 *   Copyright © 2008 Fredrik Höglund <fredrik@kde.org>
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
    if (element != PE_PanelItemViewItem) {
        return QCommonStyle::drawPrimitive(element, option, painter, widget);
    }

    bool hover = (option->state & State_MouseOver);
    bool selected = (option->state & State_Selected);

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
}

