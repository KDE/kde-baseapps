/* This file is part of the KDE project
   Copyright 1998, 1999 Simon Hausmann <hausmann@kde.org>
   Copyright 2000, 2006 David Faure <faure@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "KonquerorAdaptor.h"
#include "konqmisc.h"
#include "KonqMainWindowAdaptor.h"
#include "konqmainwindow.h"
#include "konqviewmanager.h"
#include "konqview.h"
#include "konqsettingsxt.h"
#include "konqsettings.h"

#include <kapplication.h>
#include <kdebug.h>
#include <kwindowsystem.h>

#include <QtCore/QFile>
#ifdef Q_WS_X11
#include <QX11Info>
#include <X11/Xlib.h>
#endif

// these DBus calls come from outside, so any windows created by these
// calls would have old user timestamps (for KWin's no-focus-stealing),
// it's better to reset the timestamp and rely on other means
// of detecting the time when the user action that triggered all this
// happened
// TODO a valid timestamp should be passed in the DBus calls that
// are not for user scripting

KonquerorAdaptor::KonquerorAdaptor()
 : QObject( kapp )
{
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject( KONQ_MAIN_PATH, this, QDBusConnection::ExportNonScriptableSlots );
}

KonquerorAdaptor::~KonquerorAdaptor()
{
}

QDBusObjectPath KonquerorAdaptor::openBrowserWindow( const QString& url, const QByteArray& startup_id )
{
    kapp->setStartupId( startup_id );
#ifdef Q_WS_X11
    QX11Info::setAppUserTime( 0 );
#endif
    KonqMainWindow *res = KonqMisc::createSimpleWindow( KUrl(url), KParts::OpenUrlArguments() );
    if ( !res )
        return QDBusObjectPath("/");
    return QDBusObjectPath( res->dbusName() );
}

QDBusObjectPath KonquerorAdaptor::createNewWindow( const QString& url, const QString& mimetype, const QByteArray& startup_id, bool tempFile )
{
    kapp->setStartupId( startup_id );
#ifdef Q_WS_X11
    QX11Info::setAppUserTime( 0 );
#endif
    KParts::OpenUrlArguments args;
    args.setMimeType( mimetype );
    // Filter the URL, so that "kfmclient openURL gg:foo" works also when konq is already running
    KUrl finalURL = KonqMisc::konqFilteredURL( 0, url );
    KonqMainWindow *res = KonqMisc::createNewWindow( finalURL, args, KParts::BrowserArguments(), false, QStringList(), tempFile );
    if ( !res )
        return QDBusObjectPath("/");
    return QDBusObjectPath( res->dbusName() );
}

QDBusObjectPath KonquerorAdaptor::createNewWindowWithSelection( const QString& url, const QStringList& filesToSelect, const QByteArray& startup_id )
{
    kapp->setStartupId( startup_id );
#ifdef Q_WS_X11
    QX11Info::setAppUserTime( 0 );
#endif
    KonqMainWindow *res = KonqMisc::createNewWindow( KUrl(url), KParts::OpenUrlArguments(), KParts::BrowserArguments(), false, filesToSelect );
    if ( !res )
        return QDBusObjectPath("/");
    return QDBusObjectPath( res->dbusName() );
}

QDBusObjectPath KonquerorAdaptor::createBrowserWindowFromProfile( const QString& path, const QString& filename, const QByteArray& startup_id )
{
    kapp->setStartupId( startup_id );
#ifdef Q_WS_X11
    QX11Info::setAppUserTime( 0 );
#endif
    kDebug(1202) << "void KonquerorAdaptor::createBrowserWindowFromProfile( path, filename ) ";
    kDebug(1202) << path << "," << filename;
    KonqMainWindow *res = KonqMisc::createBrowserWindowFromProfile( path, filename );
    if ( !res )
        return QDBusObjectPath("/");
    return QDBusObjectPath( res->dbusName() );
}

QDBusObjectPath KonquerorAdaptor::createBrowserWindowFromProfileAndUrl( const QString& path, const QString& filename, const QString& url, const QByteArray& startup_id )
{
    kapp->setStartupId( startup_id );
#ifdef Q_WS_X11
    QX11Info::setAppUserTime( 0 );
#endif
    KonqMainWindow *res = KonqMisc::createBrowserWindowFromProfile( path, filename, KUrl(url) );
    if ( !res )
        return QDBusObjectPath("/");
    return QDBusObjectPath( res->dbusName() );
}

QDBusObjectPath KonquerorAdaptor::createBrowserWindowFromProfileUrlAndMimeType( const QString& path, const QString& filename, const QString& url, const QString& mimetype, const QByteArray& startup_id )
{
    kapp->setStartupId( startup_id );
#ifdef Q_WS_X11
    QX11Info::setAppUserTime( 0 );
#endif
    KParts::OpenUrlArguments args;
    args.setMimeType( mimetype );
    KonqMainWindow *res = KonqMisc::createBrowserWindowFromProfile( path, filename, KUrl(url), args );
    if ( !res )
        return QDBusObjectPath("/");
    return QDBusObjectPath( res->dbusName() );
}

void KonquerorAdaptor::updateProfileList()
{
    QList<KonqMainWindow*> *mainWindows = KonqMainWindow::mainWindowList();
    if ( !mainWindows )
        return;

    foreach ( KonqMainWindow* window, *mainWindows )
        window->viewManager()->profileListDirty( false );
}

QString KonquerorAdaptor::crashLogFile()
{
    return KonqMainWindow::s_crashlog_file->objectName();
}

QList<QDBusObjectPath> KonquerorAdaptor::getWindows()
{
    QList<QDBusObjectPath> lst;
    QList<KonqMainWindow*> *mainWindows = KonqMainWindow::mainWindowList();
    if ( mainWindows )
    {
      foreach ( KonqMainWindow* window, *mainWindows )
        lst.append( QDBusObjectPath( window->dbusName() ) );
    }
    return lst;
}

QDBusObjectPath KonquerorAdaptor::windowForTab()
{
    QList<KonqMainWindow*> *mainWindows = KonqMainWindow::mainWindowList();
    if ( mainWindows ) {
        foreach ( KonqMainWindow* window, *mainWindows ) {
#ifdef Q_WS_X11
            KWindowInfo winfo = KWindowSystem::windowInfo( window->winId(), NET::WMDesktop );
            if( winfo.isOnCurrentDesktop() &&
#else
	    if(
#endif
                !KonqMainWindow::isPreloaded() ) { // we want a tab in an already shown window
                Q_ASSERT(!window->dbusName().isEmpty());
                return QDBusObjectPath( window->dbusName() );
            }
        }
    }
    // We can't use QDBusObjectPath(), dbus type 'o' must be a valid object path.
    // So we use "/" as an indicator for not found.
    return QDBusObjectPath("/");
}

bool KonquerorAdaptor::processCanBeReused( int screen )
{
#ifdef Q_WS_X11
	QX11Info info;
    if( info.screen() != screen )
        return false; // this instance run on different screen, and Qt apps can't migrate
#endif
    if( KonqMainWindow::isPreloaded())
        return false; // will be handled by preloading related code instead
    QList<KonqMainWindow*>* windows = KonqMainWindow::mainWindowList();
    if( windows == NULL )
        return true;
    QStringList allowed_parts = KonqSettings::safeParts();
    bool all_parts_allowed = false;

    if( allowed_parts.count() == 1 && allowed_parts.first() == QLatin1String( "SAFE" ))
    {
        allowed_parts.clear();
        // is duplicated in client/kfmclient.cc
        allowed_parts << QLatin1String( "dolphinpart.desktop" )
                      << QLatin1String( "konq_sidebartng.desktop" );
    }
    else if( allowed_parts.count() == 1 && allowed_parts.first() == QLatin1String( "ALL" ))
    {
        allowed_parts.clear();
        all_parts_allowed = true;
    }
    if( all_parts_allowed )
        return true;
    foreach ( KonqMainWindow* window, *windows )
    {
        kDebug(1202) << "processCanBeReused: count=" << window->viewCount();
        const KonqMainWindow::MapViews& views = window->viewMap();
        foreach ( KonqView* view, views )
        {
            kDebug(1202) << "processCanBeReused: part=" << view->service()->entryPath() << ", URL=" << view->url().prettyUrl();
            if( !allowed_parts.contains( view->service()->entryPath()))
                return false;
        }
    }
    return true;
}

void KonquerorAdaptor::terminatePreloaded()
{
    if( KonqMainWindow::isPreloaded())
        kapp->exit();
}

#include "KonquerorAdaptor.moc"
