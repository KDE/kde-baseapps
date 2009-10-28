
/**
 *  Copyright (C) 2004 Frans Englich <frans.englich@telia.com>
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
 *
 *
 *  Please see the README
 *
 */

#include "main.h"

#include <QtGui/QLayout>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtCore/QEvent>
#include <QtGui/QPixmap>
#include <QtCore/QStringList>
#include <QtGui/QDragEnterEvent>

#include <kpushbutton.h>
#include <kguiitem.h>
#include <kpassworddialog.h>
#include <kuser.h>
#include <kdialog.h>
#include <kimageio.h>
#include <kmimetype.h>
#include <kstandarddirs.h>
#include <kaboutdata.h>
#include <kmessagebox.h>
#include <QProcess>
#include <kio/netaccess.h>
#include <kurl.h>

#include "settings.h"
#include "pass.h"
#include "chfnprocess.h"
#include <KPluginFactory>
#include <KPluginLoader>

K_PLUGIN_FACTORY(Factory,
        registerPlugin<KCMUserAccount>();
        )
K_EXPORT_PLUGIN(Factory("useraccount"))

KCMUserAccount::KCMUserAccount( QWidget *parent, const QVariantList &)
	: KCModule( Factory::componentData(), parent)
{
	QVBoxLayout *topLayout = new QVBoxLayout(this);
        topLayout->setSpacing(KDialog::spacingHint());
        topLayout->setMargin(0);

	_mw = new MainWidget(this);
	topLayout->addWidget( _mw );

	connect( _mw->btnChangeFace, SIGNAL(clicked()), SLOT(slotFaceButtonClicked()));
	connect( _mw->btnChangePassword, SIGNAL(clicked()), SLOT(slotChangePassword()));
	_mw->btnChangePassword->setGuiItem( KGuiItem( i18n("Change &Password..."), "preferences-desktop-user-password" ));

	connect( _mw->leRealname, SIGNAL(textChanged(const QString&)), SLOT(changed()));
	connect( _mw->leOrganization, SIGNAL(textChanged(const QString&)), SLOT(changed()));
	connect( _mw->leEmail, SIGNAL(textChanged(const QString&)), SLOT(changed()));
	connect( _mw->leSMTP, SIGNAL(textChanged(const QString&)), SLOT(changed()));

	_ku = new KUser();
	_kes = new KEMailSettings();

	_mw->lblUsername->setText( _ku->loginName() );
	QFont font( _mw->lblUsername->font() );
	font.setPointSizeF( font.pointSizeF()  * 1.41 );
	font.setBold( true );
	_mw->lblUsername->setFont( font );
	_mw->lblUID->setText( QString().number(_ku->uid()) );

	KAboutData *about = new KAboutData("kcm_useraccount", 0,
		ki18n("Password & User Information"), 0, KLocalizedString(),
		KAboutData::License_GPL,
		ki18n("(C) 2002, Braden MacDonald, "
			"(C) 2004 Ravikiran Rajagopal"));

	about->addAuthor(ki18n("Frans Englich"), ki18n("Maintainer"), "frans.englich@telia.com");
	about->addAuthor(ki18n("Ravikiran Rajagopal"), KLocalizedString(), "ravi@kde.org");
	about->addAuthor(ki18n("Michael H\303\244ckel"), KLocalizedString(), "haeckel@kde.org" );

	about->addAuthor(ki18n("Braden MacDonald"), ki18n("Face editor"), "bradenm_k@shaw.ca");
	about->addAuthor(ki18n("Geert Jansen"), ki18n("Password changer"), "jansen@kde.org",
			"http://www.stack.nl/~geertj/");
	about->addAuthor(ki18n("Daniel Molkentin"));
	about->addAuthor(ki18n("Alex Zepeda"));
	about->addAuthor(ki18n("Hans Karlsson"), ki18n("Icons"), "karlsson.h@home.se");
	about->addAuthor(ki18n("Hermann Thomas"), ki18n("Icons"), "h.thomas@gmx.de");
	setAboutData(about);

	setQuickHelp( i18n("<qt>Here you can change your personal information, which "
			"will be used, for instance, in mail programs and word processors. You can "
			"change your login password by clicking <em>Change Password...</em>.</qt>") );

	addConfig( KCFGPassword::self(), this );
	load();
}

void KCMUserAccount::slotChangePassword()
{
	QString bin = KGlobal::dirs()->findExe("kdepasswd");
	if ( bin.isNull() )
	{
		kDebug() << "kcm_useraccount: kdepasswd was not found.";
		KMessageBox::sorry ( this, i18n( "A program error occurred: the internal "
			"program 'kdepasswd' could not be found. You will "
			"not be able to change your password."));

		_mw->btnChangePassword->setEnabled(false);
		return;
	}
	QStringList lst;
	lst << _ku->loginName();
	QProcess::startDetached(bin,lst);

}


KCMUserAccount::~KCMUserAccount()
{
	delete _ku;
	delete _kes;
}

void KCMUserAccount::load()
{
	_mw->lblUsername->setText(_ku->loginName());

	_kes->setProfile(_kes->defaultProfileName());

	_mw->leRealname->setText( _kes->getSetting( KEMailSettings::RealName ));
	_mw->leEmail->setText( _kes->getSetting( KEMailSettings::EmailAddress ));
	_mw->leOrganization->setText( _kes->getSetting( KEMailSettings::Organization ));
	_mw->leSMTP->setText( _kes->getSetting( KEMailSettings::OutServer ));

	QString _userPicsDir = KCFGUserAccount::faceDir() +
		KGlobal::dirs()->resourceDirs("data").last() + "kdm/faces/";

	QString fs = KCFGUserAccount::faceSource();
	if (fs == QLatin1String("UserOnly"))
		_facePerm = userOnly;
	else if (fs == QLatin1String("PreferUser"))
		_facePerm = userFirst;
	else if (fs == QLatin1String("PreferAdmin"))
		_facePerm = adminFirst;
	else
		_facePerm = adminOnly; // Admin Only

	if ( _facePerm == adminFirst )
	{ 	// If the administrator's choice takes preference
		_facePixmap = QPixmap( _userPicsDir + _ku->loginName() + ".face.icon" );

		if ( _facePixmap.isNull() )
			_facePerm = userFirst;
		else
			_mw->btnChangeFace->setIcon( KIcon(_facePixmap) );
	}

	if ( _facePerm >= userFirst )
	{
		// If the user's choice takes preference
		_facePixmap = QPixmap( KCFGUserAccount::faceFile() );

		// The user has no face, should we check for the admin's setting?
		if ( _facePixmap.isNull() && _facePerm == userFirst )
			_facePixmap = QPixmap( _userPicsDir + _ku->loginName() + ".face.icon" );

		if ( _facePixmap.isNull() )
			_facePixmap = QPixmap( _userPicsDir + KCFGUserAccount::defaultFile() );

		_mw->btnChangeFace->setIcon( KIcon(_facePixmap) );
	}
	else if ( _facePerm <= adminOnly )
	{
		// Admin only
		_facePixmap = QPixmap( _userPicsDir + _ku->loginName() + ".face.icon" );
		if ( _facePixmap.isNull() )
			_facePixmap = QPixmap( _userPicsDir + KCFGUserAccount::defaultFile() );
		_mw->btnChangeFace->setIcon( KIcon(_facePixmap) );
	}

        if (!_facePixmap.isNull()) {
            _mw->btnChangeFace->setIconSize(_facePixmap.size());
        }

	KCModule::load(); /* KConfigXT */

}

void KCMUserAccount::save()
{
	KCModule::save(); /* KConfigXT */

	/* Save KDE's homebrewn settings */
	_kes->setSetting( KEMailSettings::RealName, _mw->leRealname->text() );
	_kes->setSetting( KEMailSettings::EmailAddress, _mw->leEmail->text() );
	_kes->setSetting( KEMailSettings::Organization, _mw->leOrganization->text() );
	_kes->setSetting( KEMailSettings::OutServer, _mw->leSMTP->text() );

	/* Save realname to /etc/passwd */
	if ( _mw->leRealname->isModified() )
	{
		KPasswordDialog dlg( _mw );
		dlg.setPrompt(i18n("Please enter your password in order to save your settings:"));
		if ( !dlg.exec() )
		{
			KMessageBox::sorry( this, i18n("You must enter "
				"your password in order to change your information."));
			return;
		}

		ChfnProcess *proc = new ChfnProcess();
		int ret = proc->exec(dlg.password().toLocal8Bit() , _mw->leRealname->text().toLocal8Bit() );
		if ( ret )
			{
			if ( ret == ChfnProcess::PasswordError )
				KMessageBox::sorry( this, i18n("You must enter a correct password."));

			else
			{
				KMessageBox::sorry( this, i18n("An error occurred and your password has "
							"probably not been changed. The error "
							"message was:\n%1", QString::fromLocal8Bit(proc->error())));
				kDebug() << "ChfnProcess->exec() failed. Error code: " << ret
					<< "\nOutput:" << proc->error() << endl;
			}
		}

		delete proc;
	}

	/* Save the image */
	if( !_facePixmap.isNull() ) {
		if( !_facePixmap.save( KCFGUserAccount::faceFile(), "PNG" ))
			KMessageBox::error( this, i18n("There was an error saving the image: %1" ,
				KCFGUserAccount::faceFile()) );
	}

}

void KCMUserAccount::changeFace(const QPixmap &pix)
{
  if ( _facePerm < userFirst )
    return; // If the user isn't allowed to change their face, don't!

  if ( pix.isNull() ) {
    KMessageBox::sorry( this, i18n("There was an error loading the image.") );
    return;
  }

  _facePixmap = pix;
  _mw->btnChangeFace->setIcon( KIcon(_facePixmap) );
  _mw->btnChangeFace->setIconSize(_facePixmap.size());
  emit changed( true );
}

void KCMUserAccount::slotFaceButtonClicked()
{
  if ( _facePerm < userFirst )
  {
    KMessageBox::sorry( this, i18n("Your administrator has disallowed changing your image.") );
    return;
  }

  ChFaceDlg* pDlg = new ChFaceDlg( KGlobal::dirs()->resourceDirs("data").last() +
	"/kdm/pics/users/", this );

  if ( pDlg->exec() == QDialog::Accepted && !pDlg->getFaceImage().isNull() )
      changeFace( pDlg->getFaceImage() );

  delete pDlg;
}

/**
 * I merged faceButtonDropEvent into this /Frans
 * The function was called after checking event type and
 * the code is now below that if statement
 */
bool KCMUserAccount::eventFilter(QObject *, QEvent *e)
{
	if (e->type() == QEvent::DragEnter)
		{
		QDragEnterEvent *ee = (QDragEnterEvent *) e;
    if (!KUrl::List::fromMimeData( ee->mimeData() ).isEmpty())
      ee->accept();
    else
      ee->ignore();
		return true;
	}

	if (e->type() == QEvent::Drop)
	{
		if ( _facePerm < userFirst )
		{
			KMessageBox::sorry( this, i18n("Your administrator "
				"has disallowed changing your image.") );
			return true;
		}

		KUrl *url = decodeImgDrop( (QDropEvent *) e, this);
		if (url)
		{
			QString pixPath;
			KIO::NetAccess::download(*url, pixPath, this);
			changeFace( QPixmap( pixPath ) );
			KIO::NetAccess::removeTempFile(pixPath);
			delete url;
		}
		return true;
	}
	return false;
}

inline KUrl *KCMUserAccount::decodeImgDrop(QDropEvent *e, QWidget *wdg)
{
  KUrl::List uris = KUrl::List::fromMimeData(e->mimeData());
  if (!uris.isEmpty())
  {
    KUrl *url = new KUrl(uris.first());

    KMimeType::Ptr mime = KMimeType::findByUrl( *url );
    if ( mime && KImageIO::isSupported( mime->name(), KImageIO::Reading ) )
      return url;

    QStringList qs = KImageIO::pattern().split( '\n');
    qs.erase(qs.begin());

    QString msg = i18n( "%1 does not appear to be an image file.\n"
			  "Please use files with these extensions:\n"
			  "%2", url->fileName(), qs.join("\n"));
    KMessageBox::sorry( wdg, msg);
    delete url;
  }
  return 0;
}

#include "main.moc"

