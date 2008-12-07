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

#include "folderviewadapter.h"

FolderViewAdapter::FolderViewAdapter(AbstractItemView *view)
    : KAbstractViewAdapter(view), m_view(view)
{
}

QAbstractItemModel *FolderViewAdapter::model() const
{
    return m_view->model();
}

QSize FolderViewAdapter::iconSize() const
{
   return m_view->iconSize();
}

QPalette FolderViewAdapter::palette() const
{
    return m_view->palette();
}

QRect FolderViewAdapter::visibleArea() const
{
    return m_view->visibleArea();
}

QRect FolderViewAdapter::visualRect(const QModelIndex &index) const
{
    return m_view->visualRect(index);
}

void FolderViewAdapter::connect(Signal signal, QObject *receiver, const char *slot)
{
    if (signal == ScrollBarValueChanged) {
        QObject::connect(m_view->verticalScrollBar(), SIGNAL(valueChanged(int)), receiver, slot);
    }
}

