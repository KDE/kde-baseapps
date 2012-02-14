/***************************************************************************
 *   Copyright (C) 2011 by Peter Penz <peter.penz19@gmail.com>             *
 *                                                                         *
 *   Based on the Itemviews NG project from Trolltech Labs:                *
 *   http://qt.gitorious.org/qt-labs/itemviews-ng                          *
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

#include "kitemmodelbase.h"

KItemRange::KItemRange(int index, int count) :
    index(index),
    count(count)
{
}

bool KItemRange::operator == (const KItemRange& other) const
{
    return index == other.index && count == other.count;
}

KItemModelBase::KItemModelBase(QObject* parent) :
    QObject(parent),
    m_groupedSorting(false),
    m_sortRole(),
    m_sortOrder(Qt::AscendingOrder)
{
}

KItemModelBase::KItemModelBase(const QByteArray& sortRole, QObject* parent) :
    QObject(parent),
    m_groupedSorting(false),
    m_sortRole(sortRole),
    m_sortOrder(Qt::AscendingOrder)
{
}

KItemModelBase::~KItemModelBase()
{
}

bool KItemModelBase::setData(int index, const QHash<QByteArray, QVariant> &values)
{
    Q_UNUSED(index);
    Q_UNUSED(values);
    return false;
}

void KItemModelBase::setGroupedSorting(bool grouped)
{
    if (m_groupedSorting != grouped) {
        m_groupedSorting = grouped;
        onGroupedSortingChanged(grouped);
        emit groupedSortingChanged(grouped);
    }
}

bool KItemModelBase::groupedSorting() const
{
    return m_groupedSorting;
}

void KItemModelBase::setSortRole(const QByteArray& role)
{
    if (role != m_sortRole) {
        const QByteArray previous = m_sortRole;
        m_sortRole = role;
        onSortRoleChanged(role, previous);
        emit sortRoleChanged(role, previous);
    }
}

QByteArray KItemModelBase::sortRole() const
{
    return m_sortRole;
}

void KItemModelBase::setSortOrder(Qt::SortOrder order)
{
    if (order != m_sortOrder) {
        const Qt::SortOrder previous = m_sortOrder;
        m_sortOrder = order;
        onSortOrderChanged(order, previous);
        emit sortOrderChanged(order, previous);
    }
}

QString KItemModelBase::roleDescription(const QByteArray& role) const
{
    return role;
}

QList<QPair<int, QVariant> > KItemModelBase::groups() const
{
    return QList<QPair<int, QVariant> >();
}

bool KItemModelBase::setExpanded(int index, bool expanded)
{
    Q_UNUSED(index);
    Q_UNUSED(expanded);
    return false;
}

bool KItemModelBase::isExpanded(int index) const
{
    Q_UNUSED(index);
    return false;
}

bool KItemModelBase::isExpandable(int index) const
{
    Q_UNUSED(index);
    return false;
}

int KItemModelBase::expandedParentsCount(int index) const
{
    Q_UNUSED(index);
    return 0;
}

QMimeData* KItemModelBase::createMimeData(const QSet<int>& indexes) const
{
    Q_UNUSED(indexes);
    return 0;
}

int KItemModelBase::indexForKeyboardSearch(const QString& text, int startFromIndex) const
{
    Q_UNUSED(text);
    Q_UNUSED(startFromIndex);
    return -1;
}

bool KItemModelBase::supportsDropping(int index) const
{
    Q_UNUSED(index);
    return false;
}

void KItemModelBase::onGroupedSortingChanged(bool current)
{
    Q_UNUSED(current);
}

void KItemModelBase::onSortRoleChanged(const QByteArray& current, const QByteArray& previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);
}

void KItemModelBase::onSortOrderChanged(Qt::SortOrder current, Qt::SortOrder previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);
}

#include "kitemmodelbase.moc"
