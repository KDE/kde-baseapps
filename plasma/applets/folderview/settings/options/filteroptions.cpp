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

#include "filteroptions.h"


FilterOptions::FilterOptions(KConfigGroup *group) : OptionsBase(group)
{
}

void FilterOptions::loadDefaults()
{
    m_filterType            = ProxyModel::NoFilter;
    m_filterFiles           = "*";
    m_filterFilesMimeList   = QStringList();
}

void FilterOptions::loadSettings()
{
    m_filterType = static_cast<ProxyModel::FilterMode>(m_cg->readEntry("filter", static_cast<int>(m_filterType)));
//     m_filterType = filterModeStringToEnum(m_cg->readEntry("filter", filterModeEnumToString(m_filterType))); TODO - kconfigupdate
    m_filterFiles = m_cg->readEntry("filterFiles", m_filterFiles);
    m_filterFilesMimeList = m_cg->readEntry("mimeFilter", m_filterFilesMimeList);
}

void FilterOptions::setFilterMode(ProxyModel::FilterMode mode)
{
    m_filterType = mode;
    m_cg->writeEntry("filter", static_cast<int>(m_filterType));
//     m_cg->writeEntry("filter", filterModeEnumToString(m_filterType)); // TODO - kconfigupdate
}

void FilterOptions::setFilterFiles(const QString& files)
{
    m_filterFiles = files;
    m_cg->writeEntry("flterFiles", m_filterFiles);
}

void FilterOptions::setFilterFilesMimeList(const QStringList& list)
{
    if (m_filterFilesMimeList != list) {
        m_filterFilesMimeList = list;
        m_cg->writeEntry("mimeFilter", m_filterFilesMimeList);
    }
}

#include "filteroptions.moc"
