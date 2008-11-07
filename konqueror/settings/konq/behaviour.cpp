/**
 *  Copyright (c) 2001 David Faure <david@mandrakesoft.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

// Behaviour options for konqueror

// Own
#include "behaviour.h"

// Qt
#include <QtDBus/QtDBus>
#include <QtGui/QCheckBox>
#include <QtGui/QLayout>
#include <QtGui/QLabel>

// KDE
#include <klocale.h>
#include <kstandarddirs.h>
#include <kurlrequester.h>
#include <kconfiggroup.h>

// Local
#include "konqkcmfactory.h"

static bool DEFAULT_CONFIRMDELETE = true;
static bool DEFAULT_CONFIRMTRASH = true;

KBehaviourOptions::KBehaviourOptions(QWidget *parent, const QVariantList &)
    : KCModule(KonqKcmFactory::componentData(), parent)
    , g_pConfig(KSharedConfig::openConfig("konquerorrc", KConfig::IncludeGlobals))
    , groupname("FMSettings")
{
    setQuickHelp(i18n("<h1>Konqueror Behavior</h1> You can configure how Konqueror behaves as a file manager here."));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QGroupBox * miscGb = new QGroupBox(i18n("Misc Options"), this);
    QHBoxLayout *miscHLayout = new QHBoxLayout;
    QVBoxLayout *miscLayout = new QVBoxLayout;

    winPixmap = new QLabel(this);
    winPixmap->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    winPixmap->setPixmap(QPixmap(KStandardDirs::locate("data", "kcontrol/pics/onlyone.png")));
    winPixmap->setFixedSize(winPixmap->sizeHint());

    cbNewWin = new QCheckBox(i18n("Open folders in separate &windows"), this);
    cbNewWin->setWhatsThis(i18n("If this option is checked, Konqueror will open a new window when "
                                "you open a folder, rather than showing that folder's contents in the current window."));
    connect(cbNewWin, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(cbNewWin, SIGNAL(toggled(bool)), SLOT(updateWinPixmap(bool)));

    miscLayout->addWidget(cbNewWin);

    QHBoxLayout *previewLayout = new QHBoxLayout;
    QWidget* spacer = new QWidget(this);
    spacer->setMinimumSize( 20, 0 );
    spacer->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Minimum );

    previewLayout->addWidget(spacer);

    miscLayout->addLayout(previewLayout);

    miscHLayout->addLayout(miscLayout);
    miscHLayout->addWidget(winPixmap);

    miscGb->setLayout(miscHLayout);

    mainLayout->addWidget(miscGb);

    QGroupBox *bg = new QGroupBox(this);
    bg->setTitle(i18n("Ask Confirmation For"));
    bg->setWhatsThis(i18n("This option tells Konqueror whether to ask"
                          " for a confirmation when you \"delete\" a file."
                          " <ul><li><em>Move To Trash:</em> moves the file to your trash folder,"
                          " from where it can be recovered very easily.</li>"
                          " <li><em>Delete:</em> simply deletes the file.</li>"
                          " </ul>"));

    cbMoveToTrash = new QCheckBox(i18n("&Move to trash"), bg);
    connect(cbMoveToTrash, SIGNAL(toggled(bool)), this, SLOT(changed()));

    cbDelete = new QCheckBox(i18n("D&elete"), bg);
    connect(cbDelete, SIGNAL(toggled(bool)), this, SLOT(changed()));

    QVBoxLayout *confirmationLayout = new QVBoxLayout;
    confirmationLayout->addWidget(cbMoveToTrash);
    confirmationLayout->addWidget(cbDelete);
    bg->setLayout(confirmationLayout);

    mainLayout->addWidget(bg);

    cbShowDeleteCommand = new QCheckBox(i18n("Show 'Delete' context me&nu entries which bypass the trashcan"), this);
    mainLayout->addWidget(cbShowDeleteCommand);
    connect(cbShowDeleteCommand, SIGNAL(toggled(bool)), this, SLOT(changed()));

    cbShowDeleteCommand->setWhatsThis(i18n("Check this if you want 'Delete' menu commands to be displayed "
                                           "on the desktop and in the file manager's context menus. "
                                           "You can always delete files by holding the Shift key "
                                           "while calling 'Move to Trash'."));

    mainLayout->addStretch();
}

KBehaviourOptions::~KBehaviourOptions()
{
}

void KBehaviourOptions::load()
{
    KConfigGroup cg(g_pConfig, groupname);
    cbNewWin->setChecked( cg.readEntry("AlwaysNewWin", false) );
    updateWinPixmap(cbNewWin->isChecked());

    KSharedConfig::Ptr globalconfig = KSharedConfig::openConfig("kdeglobals", KConfig::NoGlobals);
    KConfigGroup cg2(globalconfig, "KDE");
    cbShowDeleteCommand->setChecked( cg2.readEntry("ShowDeleteCommand", false) );

    KSharedConfigPtr kioConfig = KSharedConfig::openConfig("kiorc", KConfig::NoGlobals);
    KConfigGroup confirmationGroup(kioConfig, "Confirmations");
    cbMoveToTrash->setChecked(confirmationGroup.readEntry("ConfirmTrash", DEFAULT_CONFIRMTRASH));
    cbDelete->setChecked(confirmationGroup.readEntry("ConfirmDelete", DEFAULT_CONFIRMDELETE));
}

void KBehaviourOptions::defaults()
{
    cbNewWin->setChecked(false);

    cbMoveToTrash->setChecked(DEFAULT_CONFIRMTRASH);
    cbDelete->setChecked(DEFAULT_CONFIRMDELETE);
    cbShowDeleteCommand->setChecked( false );
}

void KBehaviourOptions::save()
{
    KConfigGroup cg(g_pConfig, groupname);

    cg.writeEntry( "AlwaysNewWin", cbNewWin->isChecked() );

    KSharedConfig::Ptr globalconfig = KSharedConfig::openConfig("kdeglobals", KConfig::NoGlobals);
    KConfigGroup cg2(globalconfig, "KDE");
    cg2.writeEntry( "ShowDeleteCommand", cbShowDeleteCommand->isChecked());
    cg2.sync();

    KSharedConfigPtr kioConfig = KSharedConfig::openConfig("kiorc", KConfig::NoGlobals);
    KConfigGroup confirmationGroup(kioConfig , "Confirmations");
    confirmationGroup.writeEntry( "ConfirmTrash", cbMoveToTrash->isChecked());
    confirmationGroup.writeEntry( "ConfirmDelete", cbDelete->isChecked());
    confirmationGroup.sync();

    // Send signal to all konqueror instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KonqMain", "org.kde.Konqueror.Main", "reparseConfiguration");
    QDBusConnection::sessionBus().send(message);
}

void KBehaviourOptions::updateWinPixmap(bool b)
{
  if (b)
    winPixmap->setPixmap(QPixmap(KStandardDirs::locate("data",
                                        "kcontrol/pics/overlapping.png")));
  else
    winPixmap->setPixmap(QPixmap(KStandardDirs::locate("data",
                                        "kcontrol/pics/onlyone.png")));
}

#include "behaviour.moc"
