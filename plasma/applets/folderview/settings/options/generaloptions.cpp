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

#include "generaloptions.h"


GeneralOptions::GeneralOptions()
{
    loadDefaults();
}

void GeneralOptions::loadDefaults()
{
    m_url                   = KUrl();
    m_labelType             = FolderView::None;
    m_customLabel           = "";
}

void GeneralOptions::writeSettings(KConfigGroup& cg)
{
    cg.writeEntry("url", m_url);
    cg.writeEntry("labelType", static_cast<int>(m_labelType));
//     cg.writeEntry("labelType", labelTypeEnumToString(m_labelType)); // TODO - kconfigupdate
    cg.writeEntry("customLabel", m_customLabel);
}

void GeneralOptions::loadSettings(KConfigGroup& cg)
{
    m_url = cg.readEntry("url", m_url);
    m_labelType = static_cast<FolderView::LabelType>(cg.readEntry("labelType", static_cast<int>(m_labelType)));
//     m_labelType = labelTypeStringToEnum(cg.readEntry("labelType", labelTypeEnumToString(m_labelType))); // TODO - kconfigupdate
    m_customLabel = cg.readEntry("customLabel", m_customLabel);
}
