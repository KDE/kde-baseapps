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

#include "settings/proxymimemodel.h"

#include <KStringHandler>
#include <KMimeType>

ProxyMimeModel::ProxyMimeModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void ProxyMimeModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    QSortFilterProxyModel::setSourceModel(sourceModel);
    sort(0);
}

void ProxyMimeModel::setFilter(const QString &filter)
{
    m_filter = filter;
    invalidateFilter();
}

bool ProxyMimeModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    KMimeType *leftPtr = static_cast<KMimeType*>(left.internalPointer());
    KMimeType *rightPtr = static_cast<KMimeType*>(right.internalPointer());

    if (!leftPtr) {
        return true;
    } else if (!rightPtr) {
        return false;
    }

    return KStringHandler::naturalCompare(leftPtr->comment(), rightPtr->comment()) < 0;
}

bool ProxyMimeModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex sourceIndex = sourceModel()->index(source_row, 0, source_parent);
    KMimeType *mime = static_cast<KMimeType*>(sourceIndex.internalPointer());
    if (!mime) {
        return false;
    }

    if (m_filter.isEmpty()) {
        return true;
    }

    const bool fastRet = mime->comment().contains(m_filter, Qt::CaseInsensitive) ||
                   ((!mime->patterns().count() || mime->comment().isEmpty()) && mime->name().contains(m_filter, Qt::CaseInsensitive));

    if (fastRet) {
        return true;
    }

    foreach (const QString &pattern, mime->patterns()) {
        if (pattern.contains(m_filter, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

#include "proxymimemodel.moc"