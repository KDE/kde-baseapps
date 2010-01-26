/* This file is part of KDE
    Copyright (c) 2006 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "favicontest.h"
#include <qtest_kde.h>
#include <kio/job.h>
#include <kconfiggroup.h>
#include <kio/netaccess.h>
#include <QDateTime>
#include <kmimetype.h>

#include "favicontest.moc"

QTEST_KDEMAIN( FavIconTest, NoGUI )

static const char* s_hostUrl = "http://www.google.com";
static const char* s_iconUrl = "http://www.google.com/favicon.ico";
static const char* s_altIconUrl = "http://www.ibm.com/favicon.ico";

static int s_downloadTime; // in ms

enum NetworkAccess { Unknown, Yes, No } s_networkAccess = Unknown;
static bool checkNetworkAccess() {
    if ( s_networkAccess == Unknown ) {
        QTime tm;
        tm.start();
        KIO::Job* job = KIO::get( KUrl( s_iconUrl ), KIO::NoReload, KIO::HideProgressInfo );
        if( KIO::NetAccess::synchronousRun( job, 0 ) ) {
            s_networkAccess = Yes;
            s_downloadTime = tm.elapsed();
	    qDebug( "Network access OK. Download time %d", s_downloadTime );
        } else {
            qWarning( "%s", qPrintable( KIO::NetAccess::lastErrorString() ) );
            s_networkAccess = No;
        }
    }
    return s_networkAccess == Yes;
}


FavIconTest::FavIconTest()
    : QObject(), m_favIconModule( "org.kde.kded", "/modules/favicons", QDBusConnection::sessionBus() )
{
}

void FavIconTest::waitForSignal()
{
    // Wait for signals - indefinitely
    QEventLoop eventLoop;
    QObject::connect(&m_favIconModule, SIGNAL( iconChanged(bool,QString,QString) ), &eventLoop, SLOT(quit()));
    eventLoop.exec( QEventLoop::ExcludeUserInputEvents );

    // or: wait for signals for a certain amount of time...
    // QTest::qWait( s_downloadTime * 2 + 1000 );
    // qDebug() << QDateTime::currentDateTime() << " waiting done";
}

void FavIconTest::initTestCase()
{
    // Disable kwallet, I don't want kwallet wizard to come up ;)
    KConfig cfg("kwalletrc");
    KConfigGroup cg( &cfg, "Wallet");
    cg.writeEntry("First Use", false);
    cg.writeEntry("Enabled", false);
}

// To avoid hitting the cache, we first set the icon to s_altIconUrl (ibm.com),
// then back to s_iconUrl (kde.org) (to avoid messing up the favicons for the user ;)
void FavIconTest::testSetIconForURL()
{
    if ( !checkNetworkAccess() )
        QSKIP( "no network access", SkipAll );

    QSignalSpy spy( &m_favIconModule, SIGNAL( iconChanged(bool,QString,QString) ) );
    QVERIFY( spy.isValid() );
    QCOMPARE( spy.count(), 0 );

    // The call to connect() triggers qdbus initialization stuff, while QSignalSpy doesn't...
    connect(&m_favIconModule, SIGNAL( iconChanged(bool,QString,QString) ), &m_eventLoop, SLOT(quit()));

    m_favIconModule.setIconForUrl( QString( s_hostUrl ), QString( s_altIconUrl ) );

    qDebug( "called first setIconForUrl, waiting" );
    if ( spy.count() < 1 ) {
        m_eventLoop.exec( QEventLoop::ExcludeUserInputEvents );
    }

    QCOMPARE( spy.count(), 1 );
    QCOMPARE( spy[0][0].toBool(), false );
    QCOMPARE( spy[0][1].toString(), QString( s_hostUrl ) );
    QCOMPARE( spy[0][2].toString(), QString( "favicons/www.ibm.com" ) );

    m_favIconModule.setIconForUrl( QString( s_hostUrl ), QString( s_iconUrl ) );

    qDebug( "called setIconForUrl again, waiting" );
    if ( spy.count() < 2 ) {
        m_eventLoop.exec( QEventLoop::ExcludeUserInputEvents );
    }

    QCOMPARE( spy.count(), 2 );
    QCOMPARE( spy[1][0].toBool(), false );
    QCOMPARE( spy[1][1].toString(), QString( s_hostUrl ) );
    QCOMPARE( spy[1][2].toString(), QString( "favicons/www.google.com" ) );

    disconnect(&m_favIconModule, SIGNAL( iconChanged(bool,QString,QString) ), &m_eventLoop, SLOT(quit()));
}

void FavIconTest::testIconForURL()
{
    QString icon = KMimeType::favIconForUrl( KUrl( s_hostUrl ) );
    if ( icon.isEmpty() && !checkNetworkAccess() )
        QSKIP( "no network access", SkipAll );

    QCOMPARE( icon, QString( "favicons/www.google.com" ) );
}

#if 0
// downloadHostIcon does nothing if the icon is available already, so how to test this?
// We could delete the icon first, but given that we have a different KDEHOME than the kded module,
// that's not really easy...
void FavIconTest::testDownloadHostIcon()
{
    if ( !checkNetworkAccess() )
        QSKIP( "no network access", SkipAll );

    QSignalSpy spy( &m_favIconModule, SIGNAL( iconChanged(bool,QString,QString) ) );
    QVERIFY( spy.isValid() );
    QCOMPARE( spy.count(), 0 );

    qDebug( "called downloadHostIcon, waiting" );
    m_favIconModule.downloadHostIcon( QString( s_hostUrl ) );
    waitForSignal();

    QCOMPARE( spy.count(), 1 );
    QCOMPARE( mgr.m_isHost, true );
    QCOMPARE( mgr.m_hostOrURL, KUrl( s_hostUrl ).host() );
    qDebug( "icon: %s", qPrintable( mgr.m_iconName ) );
}
#endif
