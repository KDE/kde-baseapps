/***************************************************************************
 *   Copyright (C) 2006 by Peter Penz <peter.penz19@gmail.com>             *
 *   Copyright (C) 2006 by Stefan Monov <logixoul@gmail.com>               *
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

#include "dolphinapplication.h"

#include "dolphinmainwindow.h"

#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocale>
#include <kmainwindow.h>
#include <KDebug>

extern "C"
KDE_EXPORT int kdemain(int argc, char **argv)
{
    KAboutData about("dolphin", 0,
                     ki18nc("@title", "Dolphin"),
                     "15.04.0",
                     ki18nc("@title", "File Manager"),
                     KAboutData::License_GPL,
                     ki18nc("@info:credit", "(C) 2006-2015 Peter Penz, Frank Reininghaus, and Emmanuel Pescosta"));
    about.setHomepage("http://dolphin.kde.org");
    about.addAuthor(ki18nc("@info:credit", "Emmanuel Pescosta"),
                    ki18nc("@info:credit", "Maintainer (since 2014) and developer"),
                    "emmanuelpescosta099@gmail.com");
    about.addAuthor(ki18nc("@info:credit", "Frank Reininghaus"),
                    ki18nc("@info:credit", "Maintainer (2012-2014) and developer"),
                    "frank78ac@googlemail.com");
    about.addAuthor(ki18nc("@info:credit", "Peter Penz"),
                    ki18nc("@info:credit", "Maintainer and developer (2006-2012)"),
                    "peter.penz19@gmail.com");
    about.addAuthor(ki18nc("@info:credit", "Sebastian Trüg"),
                    ki18nc("@info:credit", "Developer"),
                    "trueg@kde.org"),
    about.addAuthor(ki18nc("@info:credit", "David Faure"),
                    ki18nc("@info:credit", "Developer"),
                    "faure@kde.org");
    about.addAuthor(ki18nc("@info:credit", "Aaron J. Seigo"),
                    ki18nc("@info:credit", "Developer"),
                    "aseigo@kde.org");
    about.addAuthor(ki18nc("@info:credit", "Rafael Fernández López"),
                    ki18nc("@info:credit", "Developer"),
                    "ereslibre@kde.org");
    about.addAuthor(ki18nc("@info:credit", "Kevin Ottens"),
                    ki18nc("@info:credit", "Developer"),
                    "ervin@kde.org");
    about.addAuthor(ki18nc("@info:credit", "Holger Freyther"),
                    ki18nc("@info:credit", "Developer"),
                    "freyther@gmx.net");
    about.addAuthor(ki18nc("@info:credit", "Max Blazejak"),
                    ki18nc("@info:credit", "Developer"),
                    "m43ksrocks@gmail.com");
    about.addAuthor(ki18nc("@info:credit", "Michael Austin"),
                    ki18nc("@info:credit", "Documentation"),
                    "tuxedup@users.sourceforge.net");
    // the .desktop file is not taken into account when launching manually, so
    // set the icon precautionally:
    about.setProgramIconName("system-file-manager");

    KCmdLineArgs::init(argc, argv, &about);

    KCmdLineOptions options;

    options.add("select", ki18nc("@info:shell", "The files and directories passed as arguments "
                                                "will be selected."));
    options.add("split", ki18nc("@info:shell", "Dolphin will get started with a split view."));
    options.add("+[Url]", ki18nc("@info:shell", "Document to open"));
    KCmdLineArgs::addCmdLineOptions(options);

    {
        DolphinApplication app;
        if (app.isSessionRestored()) {
            app.restoreSession();
        }
        app.exec(); // krazy:exclude=crashy
    }

    return 0;
}
