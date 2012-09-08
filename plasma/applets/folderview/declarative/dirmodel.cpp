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

#include <KDirModel>

#include "dirmodel.h"
#include "../proxymodel.h"
#include "../dirlister.h"

#include "kdebug.h"

class DirModelPrivate {
    public:
        KDirModel* kDirModel;
        DirLister* dirLister;
        KUrl url;
};

DirModel::DirModel(QObject *parent)
    : ProxyModel(parent)
{
    d = new DirModelPrivate;
    d->kDirModel = new KDirModel(this);
    d->kDirModel->setDropsAllowed(KDirModel::DropOnDirectory | KDirModel::DropOnLocalExecutable);

    setSourceModel(d->kDirModel);
    setSortLocaleAware(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);

    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[IconNameRole] = "iconName";
    roles[UrlRole] = "url";
    setRoleNames(roles);
    kDebug() << "Set roles to : " << roles;

    //d->dirLister = new DirLister
    d->dirLister = new DirLister(this);
    d->dirLister->setDelayedMimeTypes(true);
    d->dirLister->setAutoErrorHandlingEnabled(false, 0);

    d->kDirModel->setDirLister(d->dirLister);

    //d->url = KUrl("file:///home/sebas");
    d->url = KUrl("file:///usr/lib");

    d->dirLister->openUrl(d->url);
}

DirModel::~DirModel()
{
    delete d;
}

QVariant DirModel::data(const QModelIndex & index, int role) const {
    //if (index.row() < 0 || index.row() > m_animals.count())
    //    return QVariant();

    const KFileItem fileItem = itemForIndex(index);

    /*
        CommentRole,
        IsDirRole,
        IsFileRole,
        IsLinkRole,
        IsLocalFileRole,
        MimeTypeRole,
        ModificationTimeRole,
        AccessTimeRole,
        CreationTimeRole,
     */

    if (role == NameRole) {
        return fileItem.name();
    } else if (role == IconNameRole) {
        return fileItem.iconName();
    } else if (role == IsDirRole) {
        return fileItem.isDir();
    } else if (role == IsFileRole) {
        return fileItem.isFile();
    } else if (role == IsLinkRole) {
        return fileItem.isLink();
    } else if (role == IsLocalFileRole) {
        return fileItem.isLocalFile();
    } else if (role == MimetypeRole) {
        return fileItem.mimetype();
    } else if (role == ModificationTimeRole) {
        return fileItem.time(KFileItem::ModificationTime).dateTime();
    } else if (role == CreationTimeRole) {
        return fileItem.time(KFileItem::CreationTime).dateTime();
    } else if (role == AccessTimeRole) {
        return fileItem.time(KFileItem::AccessTime).dateTime();
    } else if (role == UrlRole) {
        return fileItem.url();
    }
    return QVariant();
}

#include "dirmodel.moc"
