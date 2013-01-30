/*
 *   Copyright © 2013 Ignat Semenov <ignat.semenov@blue-systems.org>
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

#include "displaypage.h"

DisplayPage::DisplayPage(KConfigDialog* parent, Settings* settings): PageBase(parent, settings)
{
    uiDisplay.setupUi(this);

    m_titleLineEdit = new KLineEdit(this);
    ledit->setClearButtonShown(false);
    ledit->setClickMessage(i18n("Title"));
    uiDisplay.titleEdit->setLineEdit(ledit);

    
}


#include "displaypage.moc"
