/***************************************************************************
 *   Copyright 2012 by Sebastian Kügler <sebas@kde.org>                    *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#ifndef DIRMODEL_H
#define DIRMODEL_H

#include <QAbstractListModel>

#include <KFileItem>

#include "../proxymodel.h"

class DirModelPrivate;

class DirModel : public ProxyModel
{
    Q_OBJECT

public:
    enum FileItemRoles {
        NameRole = Qt::UserRole + 1,
        IconNameRole,
        CommentRole,
        IsDirRole,
        IsFileRole,
        IsLinkRole,
        IsLocalFileRole,
        MimetypeRole,
        ModificationTimeRole,
        AccessTimeRole,
        CreationTimeRole,
        UrlRole
    };

    DirModel(QObject *parent = 0);
    virtual ~DirModel();

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;

private:
    DirModelPrivate *d;
};

#endif // DIRMODEL_H
