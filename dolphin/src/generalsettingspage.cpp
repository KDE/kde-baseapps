/***************************************************************************
 *   Copyright (C) 2006 by Peter Penz (peter.penz@gmx.at) and              *
 *   and Patrice Tremblay                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "generalsettingspage.h"

#include "dolphinsettings.h"

#include "dolphin_generalsettings.h"

#include <kdialog.h>
#include <klocale.h>
#include <kvbox.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

GeneralSettingsPage::GeneralSettingsPage(DolphinMainWindow* mainWin, QWidget* parent) :
    SettingsPageBase(parent),
    m_confirmMoveToTrash(0),
    m_confirmDelete(0),
    m_showDeleteCommand(0),
    m_showCopyMoveMenu(0),
    m_showZoomSlider(0),
    m_showSpaceInfo(0),
    m_browseThroughArchives(0),
    m_renameInline(0),
    m_autoExpandFolders(0)
{
    Q_UNUSED(mainWin);

    const int spacing = KDialog::spacingHint();

    QVBoxLayout* topLayout = new QVBoxLayout(this);
    KVBox* vBox = new KVBox(this);
    vBox->setSpacing(spacing);

    // create 'Ask Confirmation For' group
    QGroupBox* confirmBox = new QGroupBox(i18nc("@title:group", "Ask For Confirmation When"), vBox);
    m_confirmMoveToTrash = new QCheckBox(i18nc("@option:check Ask for Confirmation When",
                                               "Moving files or folders to trash"), confirmBox);
    connect(m_confirmMoveToTrash, SIGNAL(toggled(bool)), this, SIGNAL(changed()));
    m_confirmDelete = new QCheckBox(i18nc("@option:check Ask for Confirmation When",
                                          "Deleting files or folders"), confirmBox);
    connect(m_confirmDelete, SIGNAL(toggled(bool)), this, SIGNAL(changed()));

    QVBoxLayout* confirmBoxLayout = new QVBoxLayout(confirmBox);
    confirmBoxLayout->addWidget(m_confirmMoveToTrash);
    confirmBoxLayout->addWidget(m_confirmDelete);

    QGroupBox* contextMenuBox = new QGroupBox(i18nc("@title:group", "Context Menu"), vBox);

    // create 'Show the command 'Delete' in context menu' checkbox
    m_showDeleteCommand = new QCheckBox(i18nc("@option:check", "Show 'Delete' command"), contextMenuBox);
    connect(m_showDeleteCommand, SIGNAL(toggled(bool)), this, SIGNAL(changed()));

    m_showCopyMoveMenu = new QCheckBox(i18nc("@option:check", "Show 'Copy To' and 'Move To' commands"), contextMenuBox);
    connect(m_showCopyMoveMenu, SIGNAL(toggled(bool)), this, SIGNAL(changed()));

    QVBoxLayout* contextMenuBoxLayout = new QVBoxLayout(contextMenuBox);
    contextMenuBoxLayout->addWidget(m_showDeleteCommand);
    contextMenuBoxLayout->addWidget(m_showCopyMoveMenu);

    QGroupBox* statusBarBox = new QGroupBox(i18nc("@title:group", "Status Bar"), vBox);

    m_showZoomSlider = new QCheckBox(i18nc("@option:check", "Show zoom slider"), statusBarBox);
    connect(m_showZoomSlider, SIGNAL(toggled(bool)), this, SIGNAL(changed()));

    m_showSpaceInfo = new QCheckBox(i18nc("@option:check", "Show space information"), statusBarBox);
    connect(m_showSpaceInfo, SIGNAL(toggled(bool)), this, SIGNAL(changed()));

    QVBoxLayout* statusBarBoxLayout = new QVBoxLayout(statusBarBox);
    statusBarBoxLayout->addWidget(m_showZoomSlider);
    statusBarBoxLayout->addWidget(m_showSpaceInfo);

    m_browseThroughArchives = new QCheckBox(i18nc("@option:check", "Browse through archives"), vBox);
    connect(m_browseThroughArchives, SIGNAL(toggled(bool)), this, SIGNAL(changed()));

    m_renameInline = new QCheckBox(i18nc("@option:check", "Rename inline"), vBox);
    connect(m_renameInline, SIGNAL(toggled(bool)), this, SIGNAL(changed()));

    m_autoExpandFolders = new QCheckBox(i18nc("option:check", "Open folders during drag operations"), vBox);
    connect(m_autoExpandFolders, SIGNAL(toggled(bool)), this, SIGNAL(changed()));

    // Add a dummy widget with no restriction regarding
    // a vertical resizing. This assures that the dialog layout
    // is not stretched vertically.
    new QWidget(vBox);

    topLayout->addWidget(vBox);

    loadSettings();
}

GeneralSettingsPage::~GeneralSettingsPage()
{
}

void GeneralSettingsPage::applySettings()
{
    GeneralSettings* settings = DolphinSettings::instance().generalSettings();

    KSharedConfig::Ptr kioConfig = KSharedConfig::openConfig("kiorc", KConfig::NoGlobals);
    KConfigGroup confirmationGroup(kioConfig, "Confirmations");
    confirmationGroup.writeEntry("ConfirmTrash", m_confirmMoveToTrash->isChecked());
    confirmationGroup.writeEntry("ConfirmDelete", m_confirmDelete->isChecked());
    confirmationGroup.sync();

    KSharedConfig::Ptr globalConfig = KSharedConfig::openConfig("kdeglobals", KConfig::NoGlobals);
    KConfigGroup configGroup(globalConfig, "KDE");
    configGroup.writeEntry("ShowDeleteCommand", m_showDeleteCommand->isChecked());
    configGroup.sync();

    settings->setShowCopyMoveMenu(m_showCopyMoveMenu->isChecked());
    settings->setShowZoomSlider(m_showZoomSlider->isChecked());
    settings->setShowSpaceInfo(m_showSpaceInfo->isChecked());
    settings->setBrowseThroughArchives(m_browseThroughArchives->isChecked());
    settings->setRenameInline(m_renameInline->isChecked());
    settings->setAutoExpandFolders(m_autoExpandFolders->isChecked());
    
    settings->writeConfig();
}

void GeneralSettingsPage::restoreDefaults()
{
    GeneralSettings* settings = DolphinSettings::instance().generalSettings();
    settings->setDefaults();

    // TODO: reset default settings for trash and show delete command...

    loadSettings();
}

void GeneralSettingsPage::loadSettings()
{
    KSharedConfig::Ptr kioConfig = KSharedConfig::openConfig("kiorc", KConfig::IncludeGlobals);
    const KConfigGroup confirmationGroup(kioConfig, "Confirmations");
    m_confirmMoveToTrash->setChecked(confirmationGroup.readEntry("ConfirmTrash", false));
    m_confirmDelete->setChecked(confirmationGroup.readEntry("ConfirmDelete", true));

    KSharedConfig::Ptr globalConfig = KSharedConfig::openConfig("kdeglobals", KConfig::IncludeGlobals);
    KConfigGroup configGroup(globalConfig, "KDE");
    m_showDeleteCommand->setChecked(configGroup.readEntry("ShowDeleteCommand", false));

    GeneralSettings* settings = DolphinSettings::instance().generalSettings();
    m_showCopyMoveMenu->setChecked(settings->showCopyMoveMenu());
    m_showZoomSlider->setChecked(settings->showZoomSlider());
    m_showSpaceInfo->setChecked(settings->showSpaceInfo());
    m_browseThroughArchives->setChecked(settings->browseThroughArchives());
    m_renameInline->setChecked(settings->renameInline());
    m_autoExpandFolders->setChecked(settings->autoExpandFolders());
}

#include "generalsettingspage.moc"
