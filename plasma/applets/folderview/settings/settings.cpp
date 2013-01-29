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

#include "settings.h"

#include <KGlobalSettings>
#include <KDirModel>


QString Settings::sortOrderEnumToString(Qt::SortOrder order)
{
    if (order == Qt::AscendingOrder) {
        return "ascending";
    } else {
        return "descending";
    }
}

Qt::SortOrder Settings::sortOrderStringToEnum(const QString& order)
{
    if (order == "ascending") {
       return Qt::AscendingOrder;
    } else {
       return Qt::DescendingOrder;
    }
}

QString Settings::filterModeEnumToString(ProxyModel::FilterMode mode)
{
    switch (mode) {
        case ProxyModel::NoFilter:
            return "noFilter";
            break;
        case ProxyModel::FilterShowMatches:
            return "filterShowMatches";
            break;
        default:
            return "filterHideMatches";
            break;
    }
}

ProxyModel::FilterMode Settings::filterModeStringToEnum(const QString& mode)
{
    if (mode == "noFilter") {
        return ProxyModel::NoFilter;
    } else if (mode == "filterShowMatches") {
        return ProxyModel::FilterShowMatches;
    } else {
        return ProxyModel::FilterHideMatches;
    }
}


Settings::Settings()
    : QObject()
{
}

void Settings::loadDefaults()
{
    m_customLabel         = QString();
    m_customIconSize      = 0;
    m_showPreviews        = true;
    m_drawShadows         = true;
    m_numTextLines        = 2;
    m_textColor           = QColor(Qt::transparent);
    m_iconsLocked         = false;
    m_alignToGrid         = false;
    m_clickToView         = true;
    m_previewPlugins      = QStringList() << "imagethumbnail" << "jpegthumbnail";
    m_sortDirsFirst       = true;
    m_sortColumn          = int(KDirModel::Name);
    m_sortOrder           = Qt::AscendingOrder;
    m_filterFiles         = "*";
    m_filterType          = ProxyModel::NoFilter;
    m_filterFilesMimeList = QStringList();
    m_blankLabel          = false;
    m_userSelectedShowAllFiles = m_filterType;
    m_showSelectionMarker = KGlobalSettings::singleClick();
}

void Settings::loadSettings(KConfigGroup& cg)
{
    m_customLabel         = cg.readEntry("customLabel", m_customLabel);
    m_customIconSize      = cg.readEntry("customIconSize", m_customIconSize);
    m_showPreviews        = cg.readEntry("showPreviews", m_showPreviews);
    m_drawShadows         = cg.readEntry("drawShadows", m_drawShadows);
    m_numTextLines        = cg.readEntry("numTextLines", m_numTextLines);
    m_textColor           = cg.readEntry("textColor", m_textColor);
    m_iconsLocked         = cg.readEntry("iconsLocked", m_iconsLocked);
    m_alignToGrid         = cg.readEntry("alignToGrid", m_alignToGrid);
    m_clickToView         = cg.readEntry("clickForFolderPreviews", m_clickToView);
    m_previewPlugins      = cg.readEntry("previewPlugins", m_previewPlugins);
    m_sortDirsFirst       = cg.readEntry("sortDirsFirst", m_sortDirsFirst);
    m_sortColumn          = cg.readEntry("sortColumn", m_sortColumn);
    m_sortOrder           = sortOrderStringToEnum(cg.readEntry("sortOrder", sortOrderEnumToString(m_sortOrder)));
    m_filterFiles         = cg.readEntry("filterFiles", m_filterFiles);
    m_filterType          = filterModeStringToEnum(cg.readEntry("filter", filterModeEnumToString(m_filterType)));
    m_filterFilesMimeList = cg.readEntry("mimeFilter", m_filterFilesMimeList);
    m_blankLabel          = cg.readEntry("blankLabel", m_blankLabel);
    m_userSelectedShowAllFiles = m_filterType;
    m_showSelectionMarker = KGlobalSettings::singleClick();
}


#include "settings.moc"