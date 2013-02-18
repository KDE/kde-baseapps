/*
 *   Copyright © 2012 Ignat Semenov <ignat.semenov@blue-systems.com>
 *   Copyright © 2008, 2009 Fredrik Höglund <fredrik@kde.org>
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

#ifndef GENERALOPTIONS_H
#define GENERALOPTIONS_H

#include "settings/optionsbase.h"

class FilterOptions : public OptionsBase
{
    Q_OBJECT

public:
    FilterOptions(KConfigGroup *group);

public:
    ProxyModel::FilterMode filterMode() const { return m_filterType; }
    QString filterFiles() const { return m_filterFiles; }
    QStringList filterFilesMimeList() const { return m_filterFilesMimeList; }

    void setFilterMode(ProxyModel::FilterMode mode);
    void setFilterFiles(const QString& files);
    void setFilterFilesMimeList(const QStringList& list);

    virtual void loadDefaults();
    virtual void loadSettings();
    virtual void writeSettings();

protected:
    ProxyModel::FilterMode m_filterType;
    QString m_filterFiles;
    QStringList m_filterFilesMimeList;
};

#endif
