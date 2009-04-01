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

#ifndef PROXYMODEL_H
#define PROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QStringList>

class KDirModel;
class KFileItem;
class KUrl;

class ProxyModel : public QSortFilterProxyModel
{
public:
    enum FilterMode {
        NoFilter = 0,
        FilterShowMatches,
        FilterHideMatches
    };

    ProxyModel(QObject *parent = 0);
    ~ProxyModel();

    void setFilterMode(FilterMode filterMode);
    FilterMode filterMode() const;

    void setMimeTypeFilterList(const QStringList &mimeList);
    const QStringList &mimeTypeFilterList() const;

    void setSortDirectoriesFirst(bool enable);
    bool sortDirectoriesFirst() const;

    void setParseDesktopFiles(bool enable);
    bool parseDesktopFiles() const;

    QModelIndex indexForUrl(const KUrl &url) const;
    KFileItem itemForIndex(const QModelIndex &index) const;
    bool isDir(const QModelIndex &index, const KDirModel *dirModel) const;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

    static FilterMode filterModeFromInt(int filterMode);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;

private:
    FilterMode m_filterMode;
    QStringList m_mimeList;
    bool m_sortDirsFirst;
    bool m_parseDesktopFiles;
};

#endif
