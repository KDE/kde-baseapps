/*
 *   Copyright © 2008 Fredrik Höglund <fredrik@kde.org>
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

#include "proxymodel.h"

#include <KDesktopFile>
#include <KDirModel>
#include <KStringHandler>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <kde_file.h>


ProxyModel::ProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent),
      m_filterMode(NoFilter),
      m_sortDirsFirst(true),
      m_parseDesktopFiles(false)
{
    setSupportedDragActions(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction);
}

ProxyModel::~ProxyModel()
{
}

void ProxyModel::setFilterMode(FilterMode filterMode)
{
    m_filterMode = filterMode;
    invalidateFilter();
}

ProxyModel::FilterMode ProxyModel::filterMode() const
{
    return m_filterMode;
}

void ProxyModel::setMimeTypeFilterList(const QStringList &mimeList)
{
    m_mimeList = mimeList;
    invalidateFilter();
}

const QStringList &ProxyModel::mimeTypeFilterList() const
{
    return m_mimeList;
}

void ProxyModel::setSortDirectoriesFirst(bool enable)
{
    m_sortDirsFirst = enable;
}

bool ProxyModel::sortDirectoriesFirst() const
{
    return m_sortDirsFirst;
}

void ProxyModel::setParseDesktopFiles(bool enable)
{
    m_parseDesktopFiles = enable;
}

bool ProxyModel::parseDesktopFiles() const
{
    return m_parseDesktopFiles;
}

QModelIndex ProxyModel::indexForUrl(const KUrl &url) const
{
    const KDirModel *dirModel = static_cast<KDirModel*>(sourceModel());
    return mapFromSource(dirModel->indexForUrl(url));
}

KFileItem ProxyModel::itemForIndex(const QModelIndex &index) const
{
    const KDirModel *dirModel = static_cast<KDirModel*>(sourceModel());
    return dirModel->itemForIndex(mapToSource(index));
}

bool ProxyModel::isDir(const QModelIndex &index, const KDirModel *dirModel) const
{
    KFileItem item = dirModel->itemForIndex(index);
    if (item.isDir()) {
        return true;
    }

    if (m_parseDesktopFiles && item.isDesktopFile()) {
        // Check if the desktop file is a link to a directory
        KDesktopFile file(item.targetUrl().path());
        if (file.readType() == "Link") {
            const KUrl url(file.readUrl());
            if (url.isLocalFile()) {
                KDE_struct_stat buf;
                const QString path = url.toLocalFile(KUrl::RemoveTrailingSlash);
                if (KDE::stat(path, &buf) == 0) {
                    return S_ISDIR(buf.st_mode);
                }
            }
        }
    }

    return false;
}

bool ProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const KDirModel *dirModel = static_cast<KDirModel*>(sourceModel());

    // Sort directories first
    if (m_sortDirsFirst) {
        bool leftIsDir = isDir(left, dirModel);
        bool rightIsDir = isDir(right, dirModel);
        if (leftIsDir && !rightIsDir) {
            return true;
        }
        if (!leftIsDir && rightIsDir) {
            return false;
        }
    }

    const QString name1 = dirModel->data(left).toString();
    const QString name2 = dirModel->data(right).toString();

    return KStringHandler::naturalCompare(name1, name2, Qt::CaseInsensitive) < 0;
}

ProxyModel::FilterMode ProxyModel::filterModeFromInt(int filterMode)
{
    switch (filterMode) {
        case 0:
            return ProxyModel::NoFilter;
        case 1:
            return ProxyModel::FilterShowMatches;
        default:
            return ProxyModel::FilterHideMatches;
    }
}

bool ProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const KDirModel *dirModel = static_cast<KDirModel*>(sourceModel());
    const KFileItem item = dirModel->itemForIndex(dirModel->index(sourceRow, KDirModel::Name, sourceParent));

    bool invertResult = false;
    switch (m_filterMode) {
        case NoFilter:
            return true;
        case FilterHideMatches:
            invertResult = true; // fall through
        case FilterShowMatches: {
            // Mime type check
            bool ret = m_mimeList.contains(item.determineMimeType()->name());
            if (!ret) {
                return invertResult ? true : false;
            }
            // Pattern check
            const QString regExpOrig = filterRegExp().pattern();
            const QStringList regExps = regExpOrig.split(' ');
            foreach (const QString &regExpStr, regExps) {
                QRegExp regExp(regExpStr);
                regExp.setPatternSyntax(QRegExp::Wildcard);
                regExp.setCaseSensitivity(Qt::CaseInsensitive);

                if (regExp.indexIn(item.name()) != -1) {
                    return invertResult ? false : true;
                }
            }
            break;
        }
    }

    return invertResult ? true : false;
}

