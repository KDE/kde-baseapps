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
      m_parseDesktopFiles(false),
      m_patternMatchAll(true)
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
    m_mimeSet = QSet<QString>::fromList(mimeList);
    invalidateFilter();
}

QStringList ProxyModel::mimeTypeFilterList() const
{
    return m_mimeSet.toList();
}

void ProxyModel::setFileNameFilter(const QString &pattern)
{
    m_pattern = pattern;
    m_patternMatchAll = (pattern == "*");

    const QStringList patterns = pattern.split(' ');
    m_regExps.clear();

    foreach (const QString &pattern, patterns) {
        QRegExp rx(pattern);
        rx.setPatternSyntax(QRegExp::Wildcard);
        rx.setCaseSensitivity(Qt::CaseInsensitive);
        m_regExps.append(rx);
    }
}

QString ProxyModel::fileNameFilter() const
{
    return m_pattern;
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

#include <KDebug>
bool ProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const KDirModel *dirModel = static_cast<KDirModel*>(sourceModel());

    // When sorting by size, folders are compared using the number of items in them,
    // so they need to be given precedence over regular files as the comparison criteria is different
    if (m_sortDirsFirst || left.column() == KDirModel::Size) {
        bool leftIsDir = isDir(left, dirModel);
        bool rightIsDir = isDir(right, dirModel);
        if (leftIsDir && !rightIsDir) {
            return (sortOrder() == Qt::AscendingOrder); // folders > files independent of the sorting order
        }
        if (!leftIsDir && rightIsDir) {
            return (sortOrder() == Qt::DescendingOrder); // same here
        }
    }

    const KFileItem leftItem = dirModel->data(left, KDirModel::FileItemRole).value<KFileItem>();
    const KFileItem rightItem = dirModel->data(right, KDirModel::FileItemRole).value<KFileItem>();
    const int column = left.column();
    int result = 0;

    switch (column) {
      case KDirModel::Name:
            // fall through to the naturalCompare call
            break;
        case KDirModel::ModifiedTime: {
            const KDateTime leftTime = leftItem.time(KFileItem::ModificationTime);
            const KDateTime rightTime = rightItem.time(KFileItem::ModificationTime);
            if (leftTime < rightTime)
                result = -1;
            else if (leftTime > rightTime)
                result = +1;
            break;
            }
        case KDirModel::Size: {
            if (isDir(left, dirModel) && isDir(right, dirModel)) {
                const int leftChildCount = dirModel->data(left, KDirModel::ChildCountRole).toInt();
                const int rightChildCount = dirModel->data(right, KDirModel::ChildCountRole).toInt();
                if (leftChildCount < rightChildCount)
                    result = -1;
                else if (leftChildCount > rightChildCount)
                    result = +1;
            } else {
                const KIO::filesize_t leftSize = leftItem.size();
                const KIO::filesize_t rightSize = rightItem.size();
                if (leftSize < rightSize)
                    result = -1;
                else if (leftSize > rightSize)
                    result = +1;
            }
            break;
            }
        case KDirModel::Type:
            // add other sorting modes here
            // KDirModel::data(index, Qt::DisplayRole) returns the data in index.column()
            result = QString::compare(dirModel->data(left, Qt::DisplayRole).toString(),
                                      dirModel->data(right, Qt::DisplayRole).toString());
            break;
    }

    if (result != 0)
        return result < 0;

    // The following code is taken from dolphin/src/kfileitemmodel.cpp
    // and ensures that the sorting order is always determined
    // Copyright (C) 2011 by Peter Penz <peter.penz91@gmail.com>
    result = KStringHandler::naturalCompare(leftItem.text(), rightItem.text(), Qt::CaseSensitive);

    if (result != 0)
        return result < 0;

    result = KStringHandler::naturalCompare(leftItem.name(), rightItem.name(), Qt::CaseSensitive);

    if (result != 0)
        return result < 0;

    return QString::compare(leftItem.url().url(), rightItem.url().url(), Qt::CaseSensitive);
}

inline bool ProxyModel::matchMimeType(const KFileItem &item) const
{
    if (m_mimeSet.isEmpty()) {
        return false;
    }

    const QString mimeType = item.determineMimeType()->name();
    return m_mimeSet.contains(mimeType);
}

inline bool ProxyModel::matchPattern(const KFileItem &item) const
{
    if (m_patternMatchAll) {
        return true;
    }

    const QString name = item.name();
    QListIterator<QRegExp> i(m_regExps);
    while (i.hasNext()) {
        if (i.next().exactMatch(name)) {
            return true;
        }
    }

    return false;
}

bool ProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filterMode == NoFilter) {
        return true;
    }

    const KDirModel *dirModel = static_cast<KDirModel*>(sourceModel());
    const KFileItem item = dirModel->itemForIndex(dirModel->index(sourceRow, KDirModel::Name, sourceParent));

    if (m_filterMode == FilterShowMatches) {
        return (matchPattern(item) && matchMimeType(item));
    } else {
        return !(matchPattern(item) && matchMimeType(item));
    }
}

