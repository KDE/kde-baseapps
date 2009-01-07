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

#ifndef PREVIEWPLUGINSMODEL_H
#define PREVIEWPLUGINSMODEL_H

#include <QAbstractListModel>
#include <KServiceTypeTrader>
#include <KService>
#include <QList>

class QStringList;

class PreviewPluginsModel : public QAbstractListModel
{
public:
    PreviewPluginsModel(QObject *parent = 0);
    virtual ~PreviewPluginsModel();
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    int rowCount(const QModelIndex &parent = QModelIndex()) const { Q_UNUSED(parent) return plugins.size(); }
    void setCheckedPlugins(const QStringList &list);
    QStringList checkedPlugins() const;

private:
    int indexOfPlugin(const QString &name) const;

private:
    KService::List plugins;
    QList<int> checkedRows;
};

#endif
