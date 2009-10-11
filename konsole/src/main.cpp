/*
    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "Application.h"
#include "MainWindow.h"
#include <KDebug>

// Unix
#include <unistd.h>

// X11
#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#endif

// KDE
#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocale>
#include <KWindowSystem>

#define KONSOLE_VERSION "2.3.3"

using namespace Konsole;

#ifdef Q_WS_X11
void getDisplayInformation(Display*& display , Visual*& visual , Colormap& colormap);
#endif

// fills the KAboutData structure with information about contributors to 
// Konsole
void fillAboutData(KAboutData& aboutData);
void fillCommandLineOptions(KCmdLineOptions& options);
bool useTransparency();     // returns true if transparency should be enabled
bool forceNewProcess();     // returns true if new instance should use a new
                            // process (instead of re-using an existing one)
void restoreSession(Application& app);

// ***
// Entry point into the Konsole terminal application.  
// ***
extern "C" int KDE_EXPORT kdemain(int argc,char** argv)
{
    KAboutData about(   "konsole", 0,
                        ki18n("Konsole"),
                        KONSOLE_VERSION,
                        ki18n("Terminal emulator"),
                        KAboutData::License_GPL_V2
                    );
    fillAboutData(about);

    KCmdLineArgs::init(argc,argv,&about);
    KCmdLineOptions options;
    fillCommandLineOptions(options);
    KCmdLineArgs::addCmdLineOptions(options);
    KUniqueApplication::addCmdLineOptions();

    KUniqueApplication::StartFlags startFlags;
    if (forceNewProcess())
        startFlags = KUniqueApplication::NonUniqueInstance;

    // create a new application instance if there are no running Konsole instances,
    // otherwise inform the existing Konsole process and exit
    if ( !KUniqueApplication::start(startFlags) )
    {
        exit(0);
    }
#ifdef Q_WS_X11 
    if ( useTransparency() ) 
    {
        Display* display = 0;
        Visual* visual = 0;
        Colormap colormap = 0;

        getDisplayInformation(display,visual,colormap);

        Application app(display,(Qt::HANDLE)visual,(Qt::HANDLE)colormap);
        restoreSession(app);
        return app.exec();
    }
    else
#endif 
    {
        Application app;
        restoreSession(app);
        return app.exec();
    }   
}
bool forceNewProcess()
{
    // when starting Konsole from a terminal, a new process must be used 
    // so that the current environment is propagated into the shells of the new
    // Konsole and any debug output or warnings from Konsole are written to
    // the current terminal
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    return isatty(1) && !args->isSet("new-tab");
}
bool useTransparency()
{
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    bool compositingAvailable = KWindowSystem::compositingActive() ||
                                args->isSet("force-transparency");
    return compositingAvailable && args->isSet("transparency");
}
void fillCommandLineOptions(KCmdLineOptions& options)
{
    options.add("profile <file>", ki18n("Name of profile to use for new Konsole instance"));
    options.add("list-profiles", ki18n("List the available profiles"));
    // TODO - Update this when F12 is no longer hard coded
    options.add("background-mode", ki18n("Start Konsole in the background"
                                    " and bring to the front when the F12"
                                    " key is pressed"));
    options.add("new-tab",ki18n("Create a new tab in an existing window rather than creating a new window"));
    options.add("workdir <dir>",   ki18n("Set the initial working directory of the new tab "
                                           "or window to 'dir'"));
    options.add("notransparency",ki18n("Disable transparent backgrounds, even if the system supports them."));
    options.add("force-transparency",ki18n("Try to enable transparency, even if the system does not appear to support it."));
    options.add("hold");
    options.add("noclose",ki18n("Do not close the initial session automatically when it ends."));
    // TODO - Document this option more clearly
    options.add("p <property=value>",ki18n("Change the value of a profile property."));
    options.add("!e <cmd>",ki18n("Command to execute"));
    options.add("+[args]",ki18n("Arguments passed to command"));
}

void fillAboutData(KAboutData& aboutData)
{
  aboutData.addAuthor(ki18n("Robert Knight"),ki18n("Maintainer"), "robertknight@gmail.com");
  aboutData.addAuthor(ki18n("Lars Doelle"),ki18n("Author"), "lars.doelle@on-line.de");
  aboutData.addCredit(ki18n("Kurt V. Hindenburg"),
    ki18n("Bug fixes and general improvements"), 
    "kurt.hindenburg@gmail.com");
  aboutData.addCredit(ki18n("Waldo Bastian"),
    ki18n("Bug fixes and general improvements"),
    "bastian@kde.org");
  aboutData.addCredit(ki18n("Stephan Binner"),
    ki18n("Bug fixes and general improvements"),
    "binner@kde.org");
  aboutData.addCredit(ki18n("Chris Machemer"),
    ki18n("Bug fixes"),
    "machey@ceinetworks.com");
  aboutData.addCredit(ki18n("Stephan Kulow"),
    ki18n("Solaris support and history"),
    "coolo@kde.org");
  aboutData.addCredit(ki18n("Alexander Neundorf"),
    ki18n("Bug fixes and improved startup performance"),
    "neundorf@kde.org");
  aboutData.addCredit(ki18n("Peter Silva"),
    ki18n("Marking improvements"),
    "Peter.A.Silva@gmail.com");
  aboutData.addCredit(ki18n("Lotzi Boloni"),
    ki18n("Embedded Konsole\n"
    "Toolbar and session names"),
    "boloni@cs.purdue.edu");
  aboutData.addCredit(ki18n("David Faure"),
    ki18n("Embedded Konsole\n"
    "General improvements"),
    "faure@kde.org");
  aboutData.addCredit(ki18n("Antonio Larrosa"),
    ki18n("Visual effects"),
    "larrosa@kde.org");
  aboutData.addCredit(ki18n("Matthias Ettrich"),
    ki18n("Code from the kvt project\n"
    "General improvements"),
    "ettrich@kde.org");
  aboutData.addCredit(ki18n("Warwick Allison"),
    ki18n("Schema and text selection improvements"),
    "warwick@troll.no");
  aboutData.addCredit(ki18n("Dan Pilone"),
    ki18n("SGI port"),
    "pilone@slac.com");
  aboutData.addCredit(ki18n("Kevin Street"),
    ki18n("FreeBSD port"),
    "street@iname.com");
  aboutData.addCredit(ki18n("Sven Fischer"),
    ki18n("Bug fixes"),
    "herpes@kawo2.renditionwth-aachen.de");
  aboutData.addCredit(ki18n("Dale M. Flaven"),
    ki18n("Bug fixes"),
    "dflaven@netport.com");
  aboutData.addCredit(ki18n("Martin Jones"),
    ki18n("Bug fixes"),
    "mjones@powerup.com.au");
  aboutData.addCredit(ki18n("Lars Knoll"),
    ki18n("Bug fixes"),
    "knoll@mpi-hd.mpg.de");
  aboutData.addCredit(ki18n("Thanks to many others.\n"));
  aboutData.setProgramIconName("utilities-terminal");
}

// code taken from the Qt 4 graphics dojo examples
// at http://labs.trolltech.com 
#ifdef Q_WS_X11
void getDisplayInformation(Display*& display , Visual*& visual , Colormap& colormap)
{
    display = XOpenDisplay(0); // open default display
    if (!display) {
        kWarning("Cannot connect to the X server");
        exit(1);
    }

    int screen = DefaultScreen(display);
    int eventBase, errorBase;

    if (XRenderQueryExtension(display, &eventBase, &errorBase)) {
        int nvi;
        XVisualInfo templ;
        templ.screen  = screen;
        templ.depth = 32;
        templ.c_class = TrueColor;
        XVisualInfo *xvi = XGetVisualInfo(display, VisualScreenMask |
                                          VisualDepthMask |
                                          VisualClassMask, &templ, &nvi);
    
        for (int i = 0; i < nvi; ++i) {
            XRenderPictFormat* format = XRenderFindVisualFormat(display,
                                                                xvi[i].visual);
            if (format->type == PictTypeDirect && format->direct.alphaMask) {
                visual = xvi[i].visual;
                colormap = XCreateColormap(display, RootWindow(display, screen),
                                           visual, AllocNone);

                // found ARGB visual
                break;
            }
        }
    }
}
#endif

void restoreSession(Application& app)
{
    if (app.isSessionRestored())
    {
        int n = 1;
        while (KMainWindow::canBeRestored(n))
            app.newMainWindow()->restore(n++);
    }
}

