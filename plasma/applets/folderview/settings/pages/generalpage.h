/*
 *   Copyright © 2013 Ignat Semenov <ignat.semenov@blue-systems.org>
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

#ifndef GENERALPAGE_H
#define GENERALPAGE_H

#include "settings/pagebase.h"

#include "settings/models/placesfiltermodel.h"

#include "ui_folderviewLocationConfig.h"
#include "settings/options/generaloptions.h"


class GeneralPage : public PageBase
{
    Q_OBJECT

public:
    GeneralPage(KConfigDialog *parent, GeneralOptions *options);

protected:
    virtual void setupUi();
    virtual void loadSettings();
    virtual void setupModificationSignals();
    void saveSettings();

protected slots:
    void setTitleEditEnabled(int);

protected:
    QPointer<KFilePlacesModel> m_placesModel;
    QPointer<PlacesFilterModel> m_placesFilterModel;

    Ui::folderviewLocationConfig uiLocation;
    GeneralOptions* m_options;
};


class AppletGeneralPage : public GeneralPage
{
public:
    AppletGeneralPage(KConfigDialog* parent, GeneralOptions* options);
};

/**
 * Helper class, encapsulates the absence of the Title configuration UI in the containment version of the page
 */
class ContainmentGeneralPage : public GeneralPage
{
public:
    ContainmentGeneralPage(KConfigDialog* parent, GeneralOptions* options);
protected:
    virtual void setupUi();
};

#endif
