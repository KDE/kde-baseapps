#include "konq_aboutpage.h"

#include <QtCore/QTextCodec>
#include <QApplication>
#include <QtCore/QDir>

#include <kaboutdata.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kdebug.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kpluginfactory.h>
#include <ksavefile.h>
#include <kstandarddirs.h>
#include <ktoolinvocation.h>

K_PLUGIN_FACTORY(KonqAboutPageFactory, registerPlugin<KonqAboutPage>();)
K_EXPORT_PLUGIN(KonqAboutPageFactory("konqaboutpage"))

K_GLOBAL_STATIC(KonqAboutPageSingleton, s_staticData)

KonqAboutPageSingleton::KonqAboutPageSingleton()
{
}

KonqAboutPageSingleton::~KonqAboutPageSingleton()
{
}

QString KonqAboutPageSingleton::loadFile( const QString& file )
{
    QString res;
    if ( file.isEmpty() )
	return res;

    QFile f( file );

    if ( !f.open( QIODevice::ReadOnly ) )
	return res;

    QTextStream t( &f );

    res = t.readAll();

    // otherwise all embedded objects are referenced as about:/...
    QString basehref = QLatin1String("<BASE HREF=\"file:") +
		       file.left( file.lastIndexOf( '/' )) +
		       QLatin1String("/\">\n");
    res.replace("<head>", "<head>\n\t" + basehref, Qt::CaseInsensitive);
    return res;
}

QString KonqAboutPageSingleton::launch()
{
    if (!m_launch_html.isEmpty())
        return m_launch_html;

  QString res = loadFile( KStandardDirs::locate( "data", "konqueror/about/launch.html" ));
  if ( res.isEmpty() )
    return res;

  KIconLoader *iconloader = KIconLoader::global();
  int iconSize = iconloader->currentSize(KIconLoader::Desktop);
  QString home_icon_path = iconloader->iconPath("go-home", KIconLoader::Desktop );
  QString remote_icon_path = iconloader->iconPath("folder-remote", KIconLoader::Desktop );
  QString wastebin_icon_path = iconloader->iconPath("user-trash-full", KIconLoader::Desktop );
  QString applications_icon_path = iconloader->iconPath("start-here-kde", KIconLoader::Desktop );
  QString home_folder = QDir::homePath();
  QString continue_icon_path = iconloader->iconPath(QApplication::isRightToLeft() ? "go-previous" : "go-next", KIconLoader::Small );

  res = res.arg( KStandardDirs::locate( "data", "kdeui/about/kde_infopage.css" ) );
  if ( qApp->layoutDirection() == Qt::RightToLeft )
    res = res.arg( "@import \"%1\";" ).arg( KStandardDirs::locate( "data", "kdeui/about/kde_infopage_rtl.css" ) );
  else
    res = res.arg( "" );

  res = res.arg( i18n("Be free.") )
      .arg( i18n( "Konqueror" ) )
      .arg( i18n("Be free.") )
      .arg( i18n("Konqueror is a web browser, file manager and universal document viewer.") )
      .arg( i18n( "Starting Points" ) )
      .arg( i18n( "Introduction" ) )
      .arg( i18n( "Tips" ) )
      .arg( i18n( "Specifications" ) )
      .arg( home_folder )
      .arg( home_icon_path )
      .arg(iconSize).arg(iconSize)
      .arg( home_folder )
      .arg( i18n( "Home Folder" ) )
      .arg( i18n( "Your personal files" ) )
      .arg( wastebin_icon_path )
      .arg(iconSize).arg(iconSize)
      .arg( i18n( "Trash" ) )
      .arg( i18n( "Browse and restore the trash" ) )
      .arg( remote_icon_path )
      .arg(iconSize).arg(iconSize)
      .arg( i18n( "Network Folders" ) )
      .arg( i18n( "Shared files and folders" ) )
      .arg( applications_icon_path )
      .arg(iconSize).arg(iconSize)
      .arg( i18n( "Applications" ) )
      .arg( i18n( "Installed programs" ) )
      .arg( continue_icon_path )
      .arg( KIconLoader::SizeSmall ).arg( KIconLoader::SizeSmall )
      .arg( i18n( "Next: An Introduction to Konqueror" ) )
      ;
  i18n("Search the Web");//i18n for possible future use

  m_launch_html = res;
  return res;
}

QString KonqAboutPageSingleton::intro()
{
    if (!m_intro_html.isEmpty())
        return m_intro_html;

    QString res = loadFile( KStandardDirs::locate( "data", "konqueror/about/intro.html" ));
    if ( res.isEmpty() )
	return res;

    KIconLoader *iconloader = KIconLoader::global();
    QString back_icon_path = iconloader->iconPath(QApplication::isRightToLeft() ? "go-next" : "go-previous", KIconLoader::Small );
    QString gohome_icon_path = iconloader->iconPath("go-home", KIconLoader::Small );
    QString continue_icon_path = iconloader->iconPath(QApplication::isRightToLeft() ? "go-previous" : "go-next", KIconLoader::Small );

    res = res.arg( KStandardDirs::locate( "data", "kdeui/about/kde_infopage.css" ) );
    if ( qApp->layoutDirection() == Qt::RightToLeft )
	res = res.arg( "@import \"%1\";" ).arg( KStandardDirs::locate( "data", "kdeui/about/kde_infopage_rtl.css" ) );
    else
	res = res.arg( "" );

    res = res.arg( i18n("Be free.") )
	.arg( i18n( "Konqueror" ) )
	.arg( i18n( "Be free.") )
	.arg( i18n( "Konqueror is your file manager, web browser and universal document viewer.") )
	.arg( i18n( "Starting Points" ) )
	.arg( i18n( "Introduction" ) )
          .arg( i18n( "Tips" ) )
          .arg( i18n( "Specifications" ) )
          .arg( i18n( "Konqueror makes working with and managing your files easy. You can browse "
                      "both local and networked folders while enjoying advanced features "
                      "such as the powerful sidebar and file previews."
		      ) )
          .arg( i18n( "Konqueror is also a full featured and easy to use web browser which you "
                      "can use to explore the Internet. "
                      "Enter the address (e.g. <a href=\"http://www.kde.org\">http://www.kde.org</a>) "
                      "of a web page you would like to visit in the location bar and press Enter, "
                      "or choose an entry from the Bookmarks menu.") )
          .arg( i18n( "To return to the previous "
		      "location, press the back button <img width='16' height='16' src=\"%1\"></img> "
                      "in the toolbar. ",  back_icon_path ) )
          .arg( i18n( "To quickly go to your Home folder press the "
                      " home button <img width='16' height='16' src=\"%1\"></img>." , gohome_icon_path) )
          .arg( i18n( "For more detailed documentation on Konqueror click <a href=\"%1\">here</a>." ,
                      QString("exec:/khelpcenter")) )
          .arg( i18n( "<em>Tuning Tip:</em> If you want the Konqueror web browser to start faster,"
			" you can turn off this information screen by clicking <a href=\"%1\">here</a>. You can re-enable it"
			" by choosing the Help -> Konqueror Introduction menu option, and then pressing "
			"Settings -> Save View Profile \"Web Browsing\".", QString("config:/disable_overview")) )
	  .arg( "<img width='16' height='16' src=\"%1\">" ).arg( continue_icon_path )
	  .arg( i18n( "Next: Tips &amp; Tricks" ) )
	;


    m_intro_html = res;
    return res;
}

QString KonqAboutPageSingleton::specs()
{
    if (!m_specs_html.isEmpty())
        return m_specs_html;

    KIconLoader *iconloader = KIconLoader::global();
    QString res = loadFile( KStandardDirs::locate( "data", "konqueror/about/specs.html" ));
    QString continue_icon_path = iconloader->iconPath(QApplication::isRightToLeft() ? "go-previous" : "go-next", KIconLoader::Small );
    if ( res.isEmpty() )
	return res;

    res = res.arg( KStandardDirs::locate( "data", "kdeui/about/kde_infopage.css" ) );
    if ( qApp->layoutDirection() == Qt::RightToLeft )
	res = res.arg( "@import \"%1\";" ).arg( KStandardDirs::locate( "data", "kdeui/about/kde_infopage_rtl.css" ) );
    else
	res = res.arg( "" );

    res = res.arg( i18n("Be free.") )
	.arg( i18n( "Konqueror" ) )
	.arg( i18n("Be free.") )
	.arg( i18n("Konqueror is your file manager, web browser and universal document viewer.") )
	.arg( i18n( "Starting Points" ) )
	.arg( i18n( "Introduction" ) )
	.arg( i18n( "Tips" ) )
	.arg( i18n( "Specifications" ) )
          .arg( i18n("Specifications") )
          .arg( i18n("Konqueror is designed to embrace and support Internet standards. "
		     "The aim is to fully implement the officially sanctioned standards "
		     "from organizations such as the W3 and OASIS, while also adding "
		     "extra support for other common usability features that arise as "
		     "de facto standards across the Internet. Along with this support, "
		     "for such functions as favicons, Internet Keywords, and <A HREF=\"%1\">XBEL bookmarks</A>, "
                     "Konqueror also implements:", QString("http://pyxml.sourceforge.net/topics/xbel/")) )
          .arg( i18n("Web Browsing") )
          .arg( i18n("Supported standards") )
          .arg( i18n("Additional requirements*") )
          .arg( i18n("<A HREF=\"%1\">DOM</A> (Level 1, partially Level 2) based "
                     "<A HREF=\"%2\">HTML 4.01</A>", QString("http://www.w3.org/DOM"), QString("http://www.w3.org/TR/html4/")) )
          .arg( i18n("built-in") )
          .arg( i18n("<A HREF=\"%1\">Cascading Style Sheets</A> (CSS 1, partially CSS 2)", QString("http://www.w3.org/Style/CSS/")) )
          .arg( i18n("built-in") )
          .arg( i18n("<A HREF=\"%1\">ECMA-262</A> Edition 3 (roughly equals JavaScript 1.5)", QString("http://www.ecma.ch/ecma1/STAND/ECMA-262.HTM")) )
          .arg( i18n("JavaScript disabled (globally). Enable JavaScript <A HREF=\"%1\">here</A>.", QString("exec:/kcmshell4 khtml_java_js")) )
          .arg( i18n("JavaScript enabled (globally). Configure JavaScript <A HREF=\\\"%1\\\">here</A>.", QString("exec:/kcmshell4 khtml_java_js")) ) // leave the double backslashes here, they are necessary for javascript !
          .arg( i18n("Secure <A HREF=\"%1\">Java</A><SUP>&reg;</SUP> support", QString("http://java.sun.com")) )
          .arg( i18n("JDK 1.2.0 (Java 2) compatible VM (<A HREF=\"%1\">Blackdown</A>, <A HREF=\"%2\">IBM</A> or <A HREF=\"%3\">Sun</A>)",
                       QString("http://www.blackdown.org"), QString("http://www.ibm.com"), QString("http://java.sun.com")) )
          .arg( i18n("Enable Java (globally) <A HREF=\"%1\">here</A>.", QString("exec:/kcmshell4 khtml_java_js")) ) // TODO Maybe test if Java is enabled ?
          .arg( i18n("Netscape Communicator<SUP>&reg;</SUP> <A HREF=\"%4\">plugins</A> (for viewing <A HREF=\"%1\">Flash<SUP>&reg;</SUP></A>, <A HREF=\"%2\">Real<SUP>&reg;</SUP></A>Audio, <A HREF=\"%3\">Real<SUP>&reg;</SUP></A>Video, etc.)",
                       QString("http://www.macromedia.com/shockwave/download/index.cgi?P1_Prod_Version=ShockwaveFlash"),
                       QString("http://www.real.com"), QString("http://www.real.com"),
                       QString("about:plugins")) )
          .arg( i18n("built-in") )
          .arg( i18n("Secure Sockets Layer") )
          .arg( i18n("(TLS/SSL v2/3) for secure communications up to 168bit") )
          .arg( i18n("OpenSSL") )
          .arg( i18n("Bidirectional 16bit unicode support") )
          .arg( i18n("built-in") )
          .arg( i18n("AutoCompletion for forms") )
          .arg( i18n("built-in") )
          .arg( i18n("G E N E R A L") )
          .arg( i18n("Feature") )
          .arg( i18n("Details") )
          .arg( i18n("Image formats") )
          .arg( i18n("PNG<br />MNG<br />JPG<br />GIF") )
          .arg( i18n("Transfer protocols") )
          .arg( i18n("HTTP 1.1 (including gzip/bzip2 compression)") )
          .arg( i18n("FTP") )
          .arg( i18n("and <A HREF=\"%1\">many more...</A>", QString("exec:/kcmshell4 ioslaveinfo")) )
          .arg( i18n("URL-Completion") )
          .arg( i18n("Manual"))
	  .arg( i18n("Popup"))
	  .arg( i18n("(Short-) Automatic"))
	  .arg( "<img width='16' height='16' src=\"%1\">" ).arg( continue_icon_path )
	  .arg( i18n("<a href=\"%1\">Return to Starting Points</a>", QString("launch.html")) )

          ;

    m_specs_html = res;
    return res;
}

QString KonqAboutPageSingleton::tips()
{
    if (!m_tips_html.isEmpty())
        return m_tips_html;

    QString res = loadFile( KStandardDirs::locate( "data", "konqueror/about/tips.html" ));
    if ( res.isEmpty() )
	return res;

    KIconLoader *iconloader = KIconLoader::global();
    QString viewmag_icon_path =
	    iconloader->iconPath("zoom-in", KIconLoader::Small );
    QString history_icon_path =
	    iconloader->iconPath("view-history", KIconLoader::Small );
    QString openterm_icon_path =
	    iconloader->iconPath("utilities-terminal", KIconLoader::Small );
    QString locationbar_erase_rtl_icon_path =
	    iconloader->iconPath("edit-clear-locationbar-rtl", KIconLoader::Small );
    QString locationbar_erase_icon_path =
	    iconloader->iconPath("edit-clear-locationbar-ltr", KIconLoader::Small );
    QString window_fullscreen_icon_path =
	    iconloader->iconPath("view-fullscreen", KIconLoader::Small );
    QString view_left_right_icon_path =
	    iconloader->iconPath("view-split-left-right", KIconLoader::Small );
    QString continue_icon_path = iconloader->iconPath(QApplication::isRightToLeft() ? "go-previous" : "go-next", KIconLoader::Small );

    res = res.arg( KStandardDirs::locate( "data", "kdeui/about/kde_infopage.css" ) );
    if ( qApp->layoutDirection() == Qt::RightToLeft )
	res = res.arg( "@import \"%1\";" ).arg( KStandardDirs::locate( "data", "kdeui/about/kde_infopage_rtl.css" ) );
    else
	res = res.arg( "" );

    res = res.arg( i18n("Be free.") )
	.arg( i18n( "Konqueror" ) )
	.arg( i18n("Be free.") )
	.arg( i18n("Konqueror is your file manager, web browser and universal document viewer.") )
	.arg( i18n( "Starting Points" ) )
	.arg( i18n( "Introduction" ) )
	.arg( i18n( "Tips" ) )
	.arg( i18n( "Specifications" ) )
	.arg( i18n( "Tips &amp; Tricks" ) )
	  .arg( i18n( "Use Internet-Keywords and Web-Shortcuts: by typing \"gg: KDE\" one can search the Internet, "
		      "using Google, for the search phrase \"KDE\". There are a lot of "
		      "Web-Shortcuts predefined to make searching for software or looking "
		      "up certain words in an encyclopedia a breeze. You can even "
                      "<a href=\"%1\">create your own</a> Web-Shortcuts." , QString("exec:/kcmshell4 ebrowsing")) )
	  .arg( i18n( "Use the magnifier button <img width='16' height='16' src=\"%1\"></img> in the"
		      " toolbar to increase the font size on your web page.", viewmag_icon_path) )
	  .arg( i18n( "When you want to paste a new address into the Location toolbar you might want to "
		      "clear the current entry by pressing the black arrow with the white cross "
		      "<img width='16' height='16' src=\"%1\"></img> in the toolbar.",
               QApplication::isRightToLeft() ? locationbar_erase_rtl_icon_path : locationbar_erase_icon_path))
	  .arg( i18n( "To create a link on your desktop pointing to the current page, "
		      "simply drag the \"Location\" label that is to the left of the Location toolbar, drop it on to "
		      "the desktop, and choose \"Link\"." ) )
	  .arg( i18n( "You can also find <img width='16' height='16' src=\"%1\" /> \"Full-Screen Mode\" "
		      "in the Settings menu. This feature is very useful for \"Talk\" "
		      "sessions.", window_fullscreen_icon_path) )
	  .arg( i18n( "Divide et impera (lat. \"Divide and conquer\") - by splitting a window "
		      "into two parts (e.g. Window -> <img width='16' height='16' src=\"%1\" /> Split View "
		      "Left/Right) you can make Konqueror appear the way you like. You"
		      " can even load some example view-profiles (e.g. Midnight Commander)"
		      ", or create your own ones." , view_left_right_icon_path))
	  .arg( i18n( "Use the <a href=\"%1\">user-agent</a> feature if the website you are visiting "
                      "asks you to use a different browser "
		      "(and do not forget to send a complaint to the webmaster!)" , QString("exec:/kcmshell4 useragent")) )
	  .arg( i18n( "The <img width='16' height='16' src=\"%1\"></img> History in your SideBar ensures "
		      "that you can keep track of the pages you have visited recently.", history_icon_path) )
	  .arg( i18n( "Use a caching <a href=\"%1\">proxy</a> to speed up your"
		      " Internet connection.", QString("exec:/kcmshell4 proxy")) )
	  .arg( i18n( "Advanced users will appreciate the Konsole which you can embed into "
		      "Konqueror (Window -> <img width='16' height='16' SRC=\"%1\"></img> Show "
 		      "Terminal Emulator).", openterm_icon_path))
	  .arg( i18n( "<img width='16' height='16' src=\"%1\"></img>" ,  continue_icon_path ) )
	  .arg( i18n( "Next: Specifications" ) )
          ;


    m_tips_html = res;
    return res;
}


QString KonqAboutPageSingleton::plugins()
{
    if (!m_plugins_html.isEmpty())
        return m_plugins_html;

    QString res = loadFile( KStandardDirs::locate( "data", qApp->layoutDirection() == Qt::RightToLeft ? "konqueror/about/plugins_rtl.html" : "konqueror/about/plugins.html" ))
                  .arg(i18n("Installed Plugins"))
                  .arg(i18n("<td>Plugin</td><td>Description</td><td>File</td><td>Types</td>"))
                  .arg(i18n("Installed"))
                  .arg(i18n("<td>Mime Type</td><td>Description</td><td>Suffixes</td><td>Plugin</td>"));
    if ( res.isEmpty() )
	return res;

    m_plugins_html = res;
    return res;
}


KonqAboutPage::KonqAboutPage(QWidget *parentWidget, QObject *parent, const QVariantList& args)
    : KHTMLPart( parentWidget, parent, BrowserViewGUI )
{
    Q_UNUSED(args)
    QTextCodec* codec = KGlobal::locale()->codecForEncoding();
    if (codec)
      setEncoding(codec->name(), true);
    else
      setEncoding("iso-8859-1", true);
#if 0
    // about:blah isn't a kioslave -> disable View source
    QAction * act = actionCollection()->action("viewDocumentSource");
    if ( act )
      act->setEnabled( false );
#endif
}

KonqAboutPage::~KonqAboutPage()
{
}

bool KonqAboutPage::openUrl(const KUrl &u)
{
    emit started(0);
    if (u.url() == "about:plugins")
        serve(s_staticData->plugins(), "plugins");
    else
        serve(s_staticData->launch(), "konqueror");
    emit completed();
    return true;
}

bool KonqAboutPage::openFile()
{
    return true;
}

void KonqAboutPage::saveState( QDataStream &stream )
{
    stream << m_htmlDoc;
    stream << m_what;
}

void KonqAboutPage::restoreState( QDataStream &stream )
{
    stream >> m_htmlDoc;
    stream >> m_what;
    serve( m_htmlDoc, m_what );
}

void KonqAboutPage::serve( const QString& html, const QString& what )
{
    m_what = what;
    begin( KUrl( QString("about:%1").arg(what) ) );
    write( html );
    end();
    m_htmlDoc = html;
}

bool KonqAboutPage::urlSelected( const QString &url, int button, int state, const QString &target,
                                 const KParts::OpenUrlArguments& args,
                                 const KParts::BrowserArguments& browserArgs )
{
    KUrl u( url );
    if ( u.protocol() == "exec" )
    {
        QStringList execArgs = url.mid( 6 ).split(QChar( ' ' ), QString::SkipEmptyParts );
        QString executable = execArgs.first();
        execArgs.erase( execArgs.begin() );
        KToolInvocation::kdeinitExec( executable, execArgs );
        return true;
    }

    if ( url == QLatin1String("launch.html") )
    {
        emit browserExtension()->openUrlNotify();
	serve(s_staticData->launch(), "konqueror");
        return true;
    }
    else if ( url == QLatin1String("intro.html") )
    {
        emit browserExtension()->openUrlNotify();
        serve(s_staticData->intro(), "konqueror");
        return true;
    }
    else if ( url == QLatin1String("specs.html") )
    {
        emit browserExtension()->openUrlNotify();
	serve(s_staticData->specs(), "konqueror");
        return true;
    }
    else if ( url == QLatin1String("tips.html") )
    {
        emit browserExtension()->openUrlNotify();
        serve(s_staticData->tips(), "konqueror");
        return true;
    }

    else if ( url == QLatin1String("config:/disable_overview") )
    {
	if ( KMessageBox::questionYesNo( widget(),
					 i18n("Do you want to disable showing "
					      "the introduction in the webbrowsing profile?"),
					 i18n("Faster Startup?"),KGuiItem(i18n("Disable")),KGuiItem(i18n("Keep")) )
	     == KMessageBox::Yes )
	{
	    QString profile = KStandardDirs::locateLocal("data", "konqueror/profiles/webbrowsing");
	    KSaveFile file( profile );
	    if ( file.open() ) {
		QTextStream stream(&file);
		stream << "[Profile]\n"
                    "Name=Web-Browser";
	    }
	}
	return true;
    }

    return KHTMLPart::urlSelected( url, button, state, target, args, browserArgs );
}

#include "konq_aboutpage.moc"
