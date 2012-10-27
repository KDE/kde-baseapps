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

#ifndef PLACESFILTERMODEL_H
#define PLACESFILTERMODEL_H

#include <QSortFilterProxyModel>

#include <KFilePlacesModel>

// Proxy model for KFilePlacesModel that filters out hidden items.
class PlacesFilterModel : public QSortFilterProxyModel
{
public:
    PlacesFilterModel(QObject *parent = 0) : QSortFilterProxyModel(parent) {}

    bool filterAcceptsRow(int row, const QModelIndex &parent) const
    {
        KFilePlacesModel *model = static_cast<KFilePlacesModel*>(sourceModel());
        const QModelIndex index = model->index(row, 0, parent);
        return !model->isHidden(index);
    }
};

#endif
