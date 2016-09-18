// -*- indent-tabs-mode:nil -*-
// vim: set ts=4 sts=4 sw=4 et:
/* This file is part of the KDE project
   Copyright (C) 2000 David Faure <faure@kde.org>
   Copyright (C) 2002-2003 Alexander Kellett <lypanov@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License version 2 or at your option version 3 as published by
   the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "globalbookmarkmanager.h"
#include "toplevel.h"
#include "importers.h"
#include "kbookmarkmodel/commandhistory.h"

#include <QApplication>
#include <QDebug>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include <KAboutData>
#include <kdelibs4configmigrator.h>

#include <kmessagebox.h>
#include <kwindowsystem.h>
#include <unistd.h>

#include <kbookmarkmanager.h>
#include <kbookmarkexporter.h>
#include <toplevel_interface.h>
#include <QStandardPaths>

// TODO - make this register() or something like that and move dialog into main
static bool askUser(const QString& filename, bool &readonly) {

    QString requestedName("keditbookmarks");
    QString interfaceName = "org.kde.keditbookmarks";
    QString appId = interfaceName + '-' +QString().setNum(getpid());

    QDBusConnection dbus = QDBusConnection::sessionBus();
    QDBusReply<QStringList> reply = dbus.interface()->registeredServiceNames();
    if ( !reply.isValid() )
        return true;
    const QStringList allServices = reply;
    for ( QStringList::const_iterator it = allServices.begin(), end = allServices.end() ; it != end ; ++it ) {
        const QString service = *it;
        if ( service.startsWith( interfaceName ) && service != appId ) {
            org::kde::keditbookmarks keditbookmarks(service,"/keditbookmarks", dbus);
            QDBusReply<QString> bookmarks = keditbookmarks.bookmarkFilename();
            QString name;
            if( bookmarks.isValid())
                name = bookmarks;
            if( name == filename)
            {
                int ret = KMessageBox::warningYesNo(0,
                i18n("Another instance of %1 is already running. Do you really "
                "want to open another instance or continue work in the same instance?\n"
                "Please note that, unfortunately, duplicate views are read-only.", QGuiApplication::applicationDisplayName()),
                i18nc("@title:window", "Warning"),
                KGuiItem(i18n("Run Another")),    /* yes */
                KGuiItem(i18n("Continue in Same")) /*  no */);
                if (ret == KMessageBox::No) {
                    QDBusInterface keditinterface(service, "/keditbookmarks/MainWindow_1");
                    //TODO fix me
                    QDBusReply<qlonglong> value = keditinterface.call(QDBus::NoBlock, "winId");
                    qlonglong id = 0;
                    if( value.isValid())
                        id = value;
                    ////qDebug()<<" id !!!!!!!!!!!!!!!!!!! :"<<id;
                    KWindowSystem::activateWindow((WId)id);
                    return false;
                } else if (ret == KMessageBox::Yes) {
                    readonly = true;
                }
            }
        }
    }
    return true;
}


int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Kdelibs4ConfigMigrator migrate(QStringLiteral("keditbookmarks"));
    migrate.setConfigFiles(QStringList() << QStringLiteral("keditbookmarksrc"));
    migrate.setUiFiles(QStringList() << QStringLiteral("keditbookmarksuirc"));
    migrate.migrate();

    KLocalizedString::setApplicationDomain("keditbookmarks");

    KAboutData aboutData(QStringLiteral("keditbookmarks"),
                         i18n("Bookmark Editor"),
                         QStringLiteral("5.0"),
                         i18n("Bookmark Organizer and Editor"),
                         KAboutLicense::GPL,
                         i18n("Copyright 2000-2007, KDE developers") );
    aboutData.addAuthor(i18n("David Faure"), i18n("Initial author"), "faure@kde.org");
    aboutData.addAuthor(i18n("Alexander Kellett"), i18n("Author"), "lypanov@kde.org");
    KAboutData::setApplicationData(aboutData);

    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("bookmarks-organize")));

    QCommandLineParser parser;
    parser.setApplicationDescription(aboutData.shortDescription());

    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("importmoz"), i18n("Import bookmarks from a file in Mozilla format"), QLatin1String("filename")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("importns"), i18n("Import bookmarks from a file in Netscape (4.x and earlier) format"), QLatin1String("filename")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("importie"), i18n("Import bookmarks from a file in Internet Explorer's Favorites format"), QLatin1String("filename")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("importopera"), i18n("Import bookmarks from a file in Opera format"), QLatin1String("filename")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("importkde3"), i18n("Import bookmarks from a file in KDE2 format"), QLatin1String("filename")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("importgaleon"), i18n("Import bookmarks from a file in Galeon format"), QLatin1String("filename")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("exportmoz"), i18n("Export bookmarks to a file in Mozilla format"), QLatin1String("filename")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("exportns"), i18n("Export bookmarks to a file in Netscape (4.x and earlier) format"), QLatin1String("filename")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("exporthtml"), i18n("Export bookmarks to a file in a printable HTML format"), QLatin1String("filename")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("exportie"), i18n("Export bookmarks to a file in Internet Explorer's Favorites format"), QLatin1String("filename")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("exportopera"), i18n("Export bookmarks to a file in Opera format"), QLatin1String("filename")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("address"), i18n("Open at the given position in the bookmarks file"), QLatin1String("address")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("customcaption"), i18n("Set the user-readable caption, for example \"Konsole\""), QLatin1String("caption")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("nobrowser"), i18n("Hide all browser related functions")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("dbusObjectName"), i18n("A unique name that represents this bookmark collection, usually the kinstance name.\n"
                                 "This should be \"konqueror\" for the Konqueror bookmarks, \"kfile\" for KFileDialog bookmarks, etc.\n"
                                 "The final D-Bus object path is /KBookmarkManager/<dbusObjectName>"), QLatin1String("name")));
    parser.addPositionalArgument(QLatin1String("[file]"), i18n("File to edit"));

    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    const bool isGui = !(parser.isSet("exportmoz") || parser.isSet("exportns") || parser.isSet("exporthtml")
                || parser.isSet("exportie") || parser.isSet("exportopera")
                || parser.isSet("importmoz") || parser.isSet("importns")
                || parser.isSet("importie") || parser.isSet("importopera")
                || parser.isSet("importkde3") || parser.isSet("importgaleon"));

    const bool browser = !parser.isSet("nobrowser");

    // enable high dpi support
    app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    const bool gotFilenameArg = (parser.positionalArguments().count() == 1);

    QString filename = gotFilenameArg
        ? parser.positionalArguments().at(0)
        : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/konqueror/bookmarks.xml");

    if (!isGui) {
        GlobalBookmarkManager::self()->createManager(filename, QString(), new CommandHistory());
        GlobalBookmarkManager::ExportType exportType = GlobalBookmarkManager::MozillaExport; // uumm.. can i just set it to -1 ?
        int got = 0;
        const char *arg, *arg2 = 0, *importType = 0;
        if (arg = "exportmoz",  parser.isSet(arg)) { exportType = GlobalBookmarkManager::MozillaExport;  arg2 = arg; got++; }
        if (arg = "exportns",   parser.isSet(arg)) { exportType = GlobalBookmarkManager::NetscapeExport; arg2 = arg; got++; }
        if (arg = "exporthtml", parser.isSet(arg)) { exportType = GlobalBookmarkManager::HTMLExport;     arg2 = arg; got++; }
        if (arg = "exportie",   parser.isSet(arg)) { exportType = GlobalBookmarkManager::IEExport;       arg2 = arg; got++; }
        if (arg = "exportopera", parser.isSet(arg)) { exportType = GlobalBookmarkManager::OperaExport;    arg2 = arg; got++; }
        if (arg = "importmoz",  parser.isSet(arg)) { importType = "Moz";   arg2 = arg; got++; }
        if (arg = "importns",   parser.isSet(arg)) { importType = "NS";    arg2 = arg; got++; }
        if (arg = "importie",   parser.isSet(arg)) { importType = "IE";    arg2 = arg; got++; }
        if (arg = "importopera", parser.isSet(arg)) { importType = "Opera"; arg2 = arg; got++; }
        if (arg = "importgaleon", parser.isSet(arg)) { importType = "Galeon"; arg2 = arg; got++; }
        if (arg = "importkde3", parser.isSet(arg)) { importType = "KDE2"; arg2 = arg; got++; }
        if (!importType && arg2) {
            Q_ASSERT(arg2);
            // TODO - maybe an xbel export???
            if (got > 1) { // got == 0 isn't possible as !isGui is dependant on "export.*"
                qWarning() << i18n("You may only specify a single --export option.");
                return 1;
            }
            QString path = parser.value(arg2);
            GlobalBookmarkManager::self()->doExport(exportType, path);
        } else if (importType) {
            if (got > 1) { // got == 0 isn't possible as !isGui is dependant on "import.*"
                qWarning() << i18n("You may only specify a single --import option.");
                return 1;
            }
            QString path = parser.value(arg2);
            KBookmarkModel* model = GlobalBookmarkManager::self()->model();
            ImportCommand *importer = ImportCommand::importerFactory(model, importType);
            importer->import(path, true);
            importer->redo();
            GlobalBookmarkManager::self()->managerSave();
            GlobalBookmarkManager::self()->notifyManagers();
        }
        return 0; // error flag on exit?, 1?
    }

    QString address = parser.isSet("address")
        ? parser.value("address")
        : QString("/0");

    QString caption = parser.isSet("customcaption")
        ? parser.value("customcaption")
        : QString();

    QString dbusObjectName;
    if(parser.isSet("dbusObjectName"))
    {
        dbusObjectName = parser.value("dbusObjectName");
    }
    else
    {
        if(gotFilenameArg)
          dbusObjectName = QString();
        else
          dbusObjectName = "konqueror";
    }

    bool readonly = false; // passed by ref

    if (askUser((gotFilenameArg ? filename : QString()), readonly)) {
        KEBApp *toplevel = new KEBApp(filename, readonly, address, browser, caption, dbusObjectName);
        toplevel->setAttribute(Qt::WA_DeleteOnClose);
        toplevel->show();
        return app.exec();
    }

    return 0;
}
