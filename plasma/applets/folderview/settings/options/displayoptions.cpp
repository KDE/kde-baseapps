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

#include "displayoptions.h"

#include <KDirModel>

DisplayOptions::DisplayOptions()
{
    loadDefaults();
}

void DisplayOptions::loadDefaults()
{
    m_sortColumn            = KDirModel::Name;
    m_sortOrder             = Qt::AscendingOrder;
    m_sortDirsFirst         = true;

    m_flow                  = IconView::TopToBottom;
    m_showPreviews          = true;
    m_previewPlugins        = QStringList() << "imagethumbnail" << "jpegthumbnail";

    m_textColor             = QColor(Qt::transparent);
    m_drawShadows           = true;
    m_iconsLocked           = false;
    m_alignToGrid           = false;
    m_clickToView           = true;
    m_showSelectionMarker   = KGlobalSettings::singleClick();
    m_customIconSize        = 0;
    m_numTextLines          = 2;
}

void DisplayOptions::loadSettings(KConfigGroup& cg)
{
    m_sortColumn = cg.readEntry("sortColumn", m_sortColumn);
    m_sortOrder = sortOrderStringToEnum(cg.readEntry("sortOrder", sortOrderEnumToString(m_sortOrder)));
    m_sortDirsFirst = cg.readEntry("sortDirsFirst", m_sortDirsFirst);

    m_flow = static_cast<IconView::Flow>(cg.readEntry("flow", static_cast<int>(m_flow)));
//     m_flow = iconFlowStringToEnum(cg.readEntry("flow", iconFlowEnumToString(m_flow))); // TODO - kconfigupdate
    m_showPreviews = cg.readEntry("showPreviews", m_showPreviews);
    m_previewPlugins = cg.readEntry("previewPlugins", m_previewPlugins);

    m_textColor = cg.readEntry("textColor", m_textColor);
    m_drawShadows = cg.readEntry("drawShadows", m_drawShadows);
    m_iconsLocked = cg.readEntry("iconsLocked", m_iconsLocked);
    m_alignToGrid = cg.readEntry("alignToGrid", m_alignToGrid);
    m_clickToView = cg.readEntry("clickForFolderPreviews", m_clickToView);
    m_showSelectionMarker = KGlobalSettings::singleClick();
    m_customIconSize = cg.readEntry("customIconSize", m_customIconSize);
    m_numTextLines = cg.readEntry("numTextLines", m_numTextLines);
}

void DisplayOptions::writeSettings(KConfigGroup& cg)
{
    cg.writeEntry("sortColumn", m_sortColumn);
    cg.writeEntry("sortOrder", sortOrderEnumToString(m_sortOrder));
    cg.writeEntry("sortDirsFirst", m_sortDirsFirst);

    cg.writeEntry("flow", static_cast<int>(m_flow));
//     cg.writeEntry("flow", iconFlowEnumToString(m_flow)); // TODO -kconfigupdate
    cg.writeEntry("showPreviews", m_showPreviews);
    cg.writeEntry("previewPlugins", m_previewPlugins);

    cg.writeEntry("textColor", m_textColor);
    cg.writeEntry("drawShadows", m_drawShadows);
    cg.writeEntry("iconsLocked", m_iconsLocked);
    cg.writeEntry("alignToGrid", m_alignToGrid);
    cg.writeEntry("clickForFolderPreviews", m_clickToView);
    cg.writeEntry("showSelectionMarker", m_showSelectionMarker);
    cg.writeEntry("customIconSize", m_customIconSize);
    cg.writeEntry("numTextLines", m_numTextLines);
}

