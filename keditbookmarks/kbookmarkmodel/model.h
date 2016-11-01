/* This file is part of the KDE project
   Copyright (C) 2005 Daniel Teske <teske@squorn.de>
   Copyright (C) 2010 David Faure <faure@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#ifndef BOOKMARKMODEL_MODEL_H
#define BOOKMARKMODEL_MODEL_H

#include <QtCore/QAbstractItemModel>
#include "kbookmarkmodel_export.h"

class CommandHistory;
class KBookmarkGroup;
class KBookmarkManager;
class KBookmark;

class KBOOKMARKMODEL_EXPORT KBookmarkModel : public QAbstractItemModel
{
    Q_OBJECT

    enum ColumnIds
    {
        NameColumnId = 0,
        UrlColumnId = 1,
        CommentColumnId = 2,
        StatusColumnId = 3,
        LastColumnId = 3,
        NoOfColumnIds = LastColumnId+1
    };

public:
    KBookmarkModel(const KBookmark& root, CommandHistory* commandHistory, QObject* parent = 0);
    void setRoot(const KBookmark& root);

    virtual ~KBookmarkModel();

    KBookmarkManager* bookmarkManager();
    CommandHistory* commandHistory();

    enum AdditionalRoles {
        // Note: use   printf "0x%08X\n" $(($RANDOM*$RANDOM))
        // to define additional roles.
        KBookmarkRole = 0x161BEC30
    };

    //reimplemented functions
    QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;
    Qt::ItemFlags flags(const QModelIndex &index) const Q_DECL_OVERRIDE;
    QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    QModelIndex parent(const QModelIndex &index) const Q_DECL_OVERRIDE;
    bool setData(const QModelIndex &index, const QVariant &value, int role) Q_DECL_OVERRIDE;
    int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    virtual void resetModel();

    QModelIndex indexForBookmark(const KBookmark& bk) const;
    KBookmark bookmarkForIndex(const QModelIndex& index) const;
    void emitDataChanged(const KBookmark& bk);

    /// Call this before inserting items into the bookmark group
    void beginInsert(const KBookmarkGroup& group, int first, int last);
    /// Call this after item insertion is done
    void endInsert();

    /// Remove the bookmark
    void removeBookmark(KBookmark bookmark);

    //drag and drop
    bool dropMimeData(const QMimeData * data, Qt::DropAction action, int row, int column, const QModelIndex & parent) Q_DECL_OVERRIDE;
    QStringList mimeTypes() const Q_DECL_OVERRIDE;
    QMimeData * mimeData(const QModelIndexList & indexes) const Q_DECL_OVERRIDE;
    Qt::DropActions supportedDropActions() const Q_DECL_OVERRIDE;

public Q_SLOTS:
    void notifyManagers(const KBookmarkGroup& grp);

private:
    class Private;
    Private * const d;
    Q_PRIVATE_SLOT(d, void _kd_slotBookmarksChanged(const QString &groupAddress, const QString &caller = QString()))
};

#endif
