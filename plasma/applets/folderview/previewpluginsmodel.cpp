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

#include "previewpluginsmodel.h"

#include <QStringList>

#include <KServiceTypeTrader>

static bool lessThan(const KService::Ptr &a, const KService::Ptr &b)
{
    return QString::localeAwareCompare(a->name(), b->name()) < 0;
}

PreviewPluginsModel::PreviewPluginsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    plugins = KServiceTypeTrader::self()->query("ThumbCreator");
    checkedRows = QVector<bool>(plugins.size(), false);

    // Sort the list alphabetially
    qStableSort(plugins.begin(), plugins.end(), lessThan);
}

PreviewPluginsModel::~PreviewPluginsModel()
{
}

Qt::ItemFlags PreviewPluginsModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index)

    return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
}

QVariant PreviewPluginsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= plugins.size()) {
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
        return plugins.at(index.row())->name();

    case Qt::CheckStateRole:
        return checkedRows.at(index.row()) ? Qt::Checked : Qt::Unchecked;
    }

    return QVariant();
}

bool PreviewPluginsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::CheckStateRole) {
        return false;
    }

    const Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
    if (state == Qt::Checked) {
        checkedRows[(index.row())] = true;
    } else {
        checkedRows[(index.row())] = false;
    }

    emit dataChanged(index, index);
    return true;
}

int PreviewPluginsModel::indexOfPlugin(const QString &name) const
{
    for (int i = 0; i < plugins.size(); i++) {
        if (plugins.at(i)->desktopEntryName() == name) {
            return i;
        }
    }
    return -1;
}

void PreviewPluginsModel::setCheckedPlugins(const QStringList &list)
{
    foreach (const QString &name, list) {
        const int row = indexOfPlugin(name);
        if (row != -1) {
            checkedRows[row] = true;
            emit dataChanged(index(row, 0), index(row, 0));
        }
    }
}

QStringList PreviewPluginsModel::checkedPlugins() const
{
    QStringList list;
    for (int i =0; i < checkedRows.size(); ++i) {
        if (checkedRows.at(i)) {
            list.append(plugins.at(i)->desktopEntryName());
        }
    }
    return list;
}


