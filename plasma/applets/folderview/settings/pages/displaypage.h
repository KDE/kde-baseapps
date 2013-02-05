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

#ifndef DISPLAYPAGE_H
#define DISPLAYPAGE_H

#include "../pagebase.h"

#include <KLineEdit>

#include "ui_folderviewDisplayConfig.h"
#include "ui_folderviewPreviewConfig.h"
#include "settings/options/displayoptions.h"


class DisplayPage : public PageBase
{
    Q_OBJECT

public:
    DisplayPage(KConfigDialog *parent, DisplayOptions *options);

protected:
    virtual void setupUi();
    virtual void loadSettings();
    virtual void setupModificationSignals();
    void saveSettings();

protected slots:
    void showPreviewConfigDialog();

protected:
    QList<int> m_iconSizes;
    Ui::folderviewDisplayConfig uiDisplay;
    Ui::folderviewPreviewConfig uiPreviewConfig;

    DisplayOptions* m_options;
};


/**
 * Helper class, encapsulates the absence of the Flow configuration UI in the applet version of the page
 */
class AppletDisplayPage : public DisplayPage
{
public:
    AppletDisplayPage(KConfigDialog* parent, DisplayOptions* options);
protected:
    virtual void setupUi();
};

class ContainmentDisplayPage : public DisplayPage
{
public:
    ContainmentDisplayPage(KConfigDialog* parent, DisplayOptions* options);
};

#endif
