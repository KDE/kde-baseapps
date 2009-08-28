/***************************************************************************
                               sidebar_widget.cpp
                             -------------------
    begin                : Sat June 2 16:25:27 CEST 2001
    copyright            : (C) 2001 Joseph Wenninger
    email                : jowenn@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// Own
#include "sidebar_widget.h"

// std
#include <limits.h>

// Qt
#include <QtCore/QDir>
#include <QtGui/QPushButton>
#include <QtGui/QLayout>
#include <QtGui/QSplitter>
#include <QtCore/QStringList>
#include <QtGui/QMenu>

// KDE
#include <klocale.h>
#include <kconfig.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kiconloader.h>
#include <kicondialog.h>
#include <kmessagebox.h>
#include <kinputdialog.h>
#include <konq_events.h>
#include <kfileitem.h>
#include <kio/netaccess.h>
#include <kmenu.h>
#include <kshell.h>
#include <kurlrequesterdialog.h>
#include <kfiledialog.h>
#include <kdesktopfile.h>
#include "konqsidebar.h"

// Local
#include <config-apps.h>



addBackEnd::addBackEnd(QWidget *parent,class QMenu *addmenu,bool universal,const QString &currentProfile, const char *name)
 : QObject(parent),
   m_parent(parent)
{
	setObjectName( name );
	m_universal=universal;
	m_currentProfile = currentProfile;
	menu = addmenu;
	connect(menu,SIGNAL(aboutToShow()),this,SLOT(aboutToShowAddMenu()));
	connect(menu,SIGNAL(triggered(QAction*)),this,SLOT(triggeredAddMenu(QAction*)));
}

void addBackEnd::aboutToShowAddMenu()
{
	if (!menu)
		return;
	KStandardDirs *dirs = KGlobal::dirs();
	const QStringList list = dirs->findAllResources("data","konqsidebartng/add/*.desktop",
						KStandardDirs::Recursive |
						KStandardDirs::NoDuplicates);
	menu->clear();
	int i = 0;

	for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it, i++ )
	{
		KDesktopFile confFile( *it );
		KConfigGroup desktopGroup = confFile.desktopGroup();
		if (!confFile.tryExec()) {
			i--;
			continue;
		}
		if (m_universal) {
			if (desktopGroup.readEntry("X-KDE-KonqSidebarUniversal").toUpper()!="TRUE") {
				i--;
				continue;
			}
		} else {
			if (desktopGroup.readEntry("X-KDE-KonqSidebarBrowser").toUpper()=="FALSE") {
				i--;
				continue;
			}
		}
		QString icon = confFile.readIcon();
		QStringList libs;
		libs << desktopGroup.readEntry("X-KDE-KonqSidebarAddModule") << desktopGroup.readEntry("X-KDE-KonqSidebarAddParam");
		if (!icon.isEmpty())
		{
			menu->addAction(QIcon(SmallIcon(icon)), confFile.readName())->setData(libs);
		} else {
			menu->addAction(confFile.readName())->setData(libs);
		}
	}

	menu->addSeparator();
	menu->addAction(i18n("Rollback to System Default"), this, SLOT(doRollBack()));
}


void addBackEnd::doRollBack()
{
	if (KMessageBox::warningContinueCancel(m_parent, i18n("<qt>This removes all your entries from the sidebar and adds the system default ones.<br /><b>This procedure is irreversible</b><br />Do you want to proceed?</qt>"))==KMessageBox::Continue)
	{
		KStandardDirs *dirs = KGlobal::dirs();
		QString loc=dirs->saveLocation("data","konqsidebartng/" + m_currentProfile + "/",true);
		QDir dir(loc);
		QStringList dirEntries = dir.entryList( QDir::Dirs | QDir::NoSymLinks );
		dirEntries.removeAll(".");
		dirEntries.removeAll("..");
		for ( QStringList::const_iterator it = dirEntries.constBegin(); it != dirEntries.constEnd(); ++it ) {
			if ((*it)!="add")
				 KIO::NetAccess::del(KUrl( loc+(*it) ), m_parent);
		}
		emit initialCopyNeeded();
	}
}


static QString findFileName(const QString* tmpl,bool universal, const QString &profile) {
	QString myFile, filename;
	KStandardDirs *dirs = KGlobal::dirs();
	QString tmp = *tmpl;

	if (universal) {
		dirs->saveLocation("data", "konqsidebartng/kicker_entries/", true);
		tmp.prepend("/konqsidebartng/kicker_entries/");
	} else {
		dirs->saveLocation("data", "konqsidebartng/" + profile + "/entries/", true);
		tmp.prepend("/konqsidebartng/" + profile + "/entries/");
	}
	filename = tmp.arg("");
	myFile = KStandardDirs::locateLocal("data", filename);

	if (QFile::exists(myFile)) {
		for (ulong l = 0; l < ULONG_MAX; l++) {
			filename = tmp.arg(l);
			myFile = KStandardDirs::locateLocal("data", filename);
			if (!QFile::exists(myFile)) {
				break;
			} else {
				myFile.clear();
			}
		}
	}

	return myFile;
}

void addBackEnd::triggeredAddMenu(QAction* action)
{
	kDebug() << action->text();

	if (!action->data().canConvert(QVariant::StringList))
		return;

	const QStringList libs = action->data().toStringList();

	KLibLoader *loader = KLibLoader::self();

        // try to load the library
	QString libname = libs[0];
	QString libparam = libs[1];
        KLibrary *lib = loader->library(libname);
        if (lib)
       	{
		// get the create_ function
		QString factory("add_");
		factory = factory+ libname;
                KLibrary::void_function_ptr add = lib->resolveFunction(QFile::encodeName(factory));

		if (add)
		{
			//call the add function
			bool (*func)(QString*, QString*, QMap<QString,QString> *);
			QMap<QString,QString> map;
			func = (bool (*)(QString*, QString*, QMap<QString,QString> *)) add;
			QString *tmp = new QString("");
			if (func(tmp,&libparam,&map))
			{
				QString myFile = findFileName(tmp,m_universal,m_currentProfile);

				if (!myFile.isEmpty())
				{
					kDebug() <<"trying to save to file: "<<myFile;
					KConfig _scf( myFile, KConfig::SimpleConfig );
					KConfigGroup scf(&_scf, "Desktop Entry");
					for (QMap<QString,QString>::ConstIterator it = map.constBegin(); it != map.constEnd(); ++it) {
						kDebug() <<"writing:"<<it.key()<<" / "<<it.value();
						scf.writePathEntry(it.key(), it.value());
					}
					scf.sync();
					emit updateNeeded();

				} else {
					kWarning() << "No unique filename found" ;
				}
			} else {
				kWarning() << "No new entry (error?)" ;
			}
			delete tmp;
		}
	} else {
		kWarning() << "libname:" << libname
			<< " doesn't specify a library!" << endl;
	}
}


/**************************************************************/
/*                      Sidebar_Widget                        */
/**************************************************************/

Sidebar_Widget::Sidebar_Widget(QWidget *parent, KParts::ReadOnlyPart *par, bool universalMode, const QString &currentProfile)
	:QWidget(parent),m_universalMode(universalMode),m_partParent(par),m_currentProfile(currentProfile)
{
	m_somethingVisible = false;
	m_initial = true;
	m_noUpdate = false;
	m_layout = 0;
	m_currentButton = 0;
	m_activeModule = 0;
	//m_userMovedSplitter = false;
        //kDebug() << "**** Sidebar_Widget:SidebarWidget()";
	if (universalMode)
	{
		m_relPath = "konqsidebartng/kicker_entries/";
	}
	else
	{
		m_relPath = "konqsidebartng/" + currentProfile + "/entries/";
	}
	m_path = KGlobal::dirs()->saveLocation("data", m_relPath, true);
	m_hasStoredUrl = false;
	m_latestViewed = -1;
	setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

#if 0 // Konqueror says: leave my splitter alone!
	QSplitter *splitterWidget = splitter();
	if (splitterWidget) {
            // ### this sets a stretch factor on the sidebar's sizepolicy, which makes it huge....
		splitterWidget->setResizeMode(parent, QSplitter::FollowSizeHint);
		splitterWidget->setOpaqueResize( false );
		connect(splitterWidget,SIGNAL(setRubberbandCalled()),SLOT(userMovedSplitter()));
	}
#endif

	m_area = new QSplitter(Qt::Vertical, this);
	m_area->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	m_area->setMinimumWidth(0);

   	m_buttonBar = new KMultiTabBar(KMultiTabBar::Left,this);

	m_menu = new QMenu(this);
	m_menu->setIcon(KIcon("configure"));
	m_menu->setTitle(i18n("Configure Navigation Panel"));

	QMenu *addMenu = m_menu->addMenu(i18n("Add New"));
	m_menu->addSeparator();
	m_multiViews = m_menu->addAction(i18n("Multiple Views"), this, SLOT(slotMultipleViews()));
        m_multiViews->setCheckable(true);
	m_showTabLeft = m_menu->addAction(i18n("Show Tabs Left"), this, SLOT(slotShowTabsLeft()));
	m_showConfigButton = m_menu->addAction(i18n("Show Configuration Button"), this, SLOT(slotShowConfigurationButton()));
        m_showConfigButton->setCheckable(true);
	if (!m_universalMode) {
		m_menu->addSeparator();
		m_menu->addAction(KIcon("window-close"), i18n("Close Navigation Panel"),
				par, SLOT(deleteLater()));
	}
        connect(m_menu, SIGNAL(aboutToShow()),
		this, SLOT(aboutToShowConfigMenu()));

	addBackEnd *ab = new addBackEnd(this, addMenu,universalMode,currentProfile,"Sidebar_Widget-addBackEnd");
	connect(ab, SIGNAL(updateNeeded()),
		this, SLOT(updateButtons()));
	connect(ab, SIGNAL(initialCopyNeeded()),
		this, SLOT(finishRollBack()));

	initialCopy();

	if (universalMode)
	{
		m_config = new KConfigGroup(KSharedConfig::openConfig("konqsidebartng_kicker.rc"), "");
	}
	else
	{
	    m_config = new KConfigGroup(KSharedConfig::openConfig("konqsidebartng.rc"),
					currentProfile);
	}
	m_configTimer.setSingleShot(true);
	connect(&m_configTimer, SIGNAL(timeout()),
		this, SLOT(saveConfig()));
        readConfig();
	m_somethingVisible = !m_openViews.isEmpty();
	doLayout();
	QTimer::singleShot(0,this,SLOT(createButtons()));
}

void Sidebar_Widget::addWebSideBar(const KUrl& url, const QString& /*name*/) {
	//kDebug() << "Web sidebar entry to be added: " << url.url()
	//	<< " [" << name << "]" << endl;

	// Look for existing ones with this URL
	KStandardDirs *dirs = KGlobal::dirs();
	QString list;
	dirs->saveLocation("data", m_relPath, true);
	list = KStandardDirs::locateLocal("data", m_relPath);

	// Go through list to see which ones exist.  Check them for the URL
	const QStringList files = QDir(list).entryList(QStringList() << "websidebarplugin*.desktop");
	for (QStringList::const_iterator it = files.constBegin(); it != files.constEnd(); ++it){
		KConfig _scf( list + *it, KConfig::SimpleConfig );
		KConfigGroup scf(&_scf, "Desktop Entry");
		if (scf.readPathEntry("URL", QString()) == url.url()) {
			// We already have this one!
			KMessageBox::information(this,
					i18n("This entry already exists."));
			return;
		}
	}

	QString tmpl = "websidebarplugin%1.desktop";
	QString myFile = findFileName(&tmpl,m_universalMode,m_currentProfile);

	if (!myFile.isEmpty()) {
		KConfig _scf( myFile, KConfig::SimpleConfig );
		KConfigGroup scf(&_scf, "Desktop Entry");
		scf.writeEntry("Type", "Link");
		scf.writePathEntry("URL", url.url());
		scf.writeEntry("Icon", "internet-web-browser");
		scf.writeEntry("Name", i18n("Web SideBar Plugin"));
		scf.writeEntry("Open", "true");
		scf.writeEntry("X-KDE-KonqSidebarModule", "konqsidebar_web");
		scf.sync();

		QTimer::singleShot(0,this,SLOT(updateButtons()));
	}
}


void Sidebar_Widget::finishRollBack()
{
	m_path = KGlobal::dirs()->saveLocation("data",m_relPath,true);
        initialCopy();
        QTimer::singleShot(0,this,SLOT(updateButtons()));
}


void Sidebar_Widget::saveConfig()
{
	m_config->writeEntry("SingleWidgetMode",m_singleWidgetMode);
	m_config->writeEntry("ShowExtraButtons",m_showExtraButtons);
	m_config->writeEntry("ShowTabsLeft", m_showTabsLeft);
	m_config->writeEntry("HideTabs", m_hideTabs);
	m_config->writeEntry("SavedWidth",m_savedWidth);
	m_config->sync();
}

void Sidebar_Widget::doLayout()
{
	if (m_layout)
		delete m_layout;

	m_layout = new QHBoxLayout(this);
        m_layout->setMargin( 0 );
        m_layout->setSpacing( 0 );
	if  (m_showTabsLeft)
	{
		m_layout->addWidget(m_buttonBar);
		m_layout->addWidget(m_area);
		m_buttonBar->setPosition(KMultiTabBar::Left);
	} else {
		m_layout->addWidget(m_area);
		m_layout->addWidget(m_buttonBar);
		m_buttonBar->setPosition(KMultiTabBar::Right);
	}
	m_layout->activate();
	if (m_hideTabs) m_buttonBar->hide();
		else m_buttonBar->show();
}


void Sidebar_Widget::aboutToShowConfigMenu()
{
        m_multiViews->setChecked(!m_singleWidgetMode);
        m_showTabLeft->setText(m_showTabsLeft ? i18n("Show Tabs Right") : i18n("Show Tabs Left"));
        m_showConfigButton->setChecked(m_showExtraButtons);
}


void Sidebar_Widget::initialCopy()
{
	kDebug()<<"Initial copy";
	QStringList dirtree_dirs;
	if (m_universalMode)
		dirtree_dirs = KGlobal::dirs()->findDirs("data","konqsidebartng/kicker_entries/");
	else
		dirtree_dirs = KGlobal::dirs()->findDirs("data","konqsidebartng/entries/");
	if (dirtree_dirs.last()==m_path)
		return; //oups;

	int nVersion=-1;
	KConfig lcfg(m_path+".version", KConfig::SimpleConfig);
	KConfigGroup generalGroup( &lcfg, "General" );
	int lVersion = generalGroup.readEntry("Version",0);


	for (QStringList::const_iterator ddit=dirtree_dirs.constBegin();ddit!=dirtree_dirs.constEnd();++ddit) {
		QString dirtree_dir=*ddit;
		if (dirtree_dir == m_path) continue;


		kDebug()<<"************************************ retrieving directory info:"<<dirtree_dir;

	        if ( !dirtree_dir.isEmpty() && dirtree_dir != m_path )
        	{
			KConfig gcfg(dirtree_dir+".version", KConfig::SimpleConfig);
			KConfigGroup dirGeneralGroup( &gcfg, "General" );
			int gversion = dirGeneralGroup.readEntry("Version", 1);
			nVersion=(nVersion>gversion)?nVersion:gversion;
			if (lVersion >= gversion)
				continue;

	 	        QDir dir(m_path);
    		        const QStringList entries = dir.entryList( QDir::Files );
                	QStringList dirEntries = dir.entryList( QDir::Dirs | QDir::NoSymLinks );
	                dirEntries.removeAll( "." );
        	        dirEntries.removeAll( ".." );

	                QDir globalDir( dirtree_dir );
        	        Q_ASSERT( globalDir.isReadable() );
	                // Only copy the entries that don't exist yet in the local dir
        	        const QStringList globalDirEntries = globalDir.entryList();
                	QStringList::ConstIterator eIt = globalDirEntries.constBegin();
	                QStringList::ConstIterator eEnd = globalDirEntries.end();
        	        for (; eIt != eEnd; ++eIt )
                	{
                		//kDebug(1201) << "KonqSidebarTree::scanDir dirtree_dir contains " << *eIt;
	                	if ( *eIt != "." && *eIt != ".." &&
					!entries.contains( *eIt ) &&
					!dirEntries.contains( *eIt ) )
				{ // we don't have that one yet -> copy it.
					QString cp("cp -R -- ");
					cp += KShell::quoteArg(dirtree_dir + *eIt);
					cp += ' ';
					cp += KShell::quoteArg(m_path);
					kDebug() << "SidebarWidget::intialCopy executing " << cp;
					::system( QFile::encodeName(cp) );
				}
			}
		}

			generalGroup.writeEntry("Version",(nVersion>lVersion)?nVersion:lVersion);
			lcfg.sync();

	}
}

void Sidebar_Widget::slotSetName( )
{
	// Set a name for this sidebar tab
	bool ok;

	// Pop up the dialog asking the user for name.
	const QString name = KInputDialog::getText(i18n("Set Name"), i18n("Enter the name:"),
		m_currentButton->displayName, &ok, this);

	if(ok)
	{
		// Write the name in the .desktop file of this side button.
		KConfig _ksc( m_path+m_currentButton->file, KConfig::SimpleConfig );
		KConfigGroup ksc(&_ksc, "Desktop Entry");
		ksc.writeEntry("Name", name, KConfigBase::Normal|KConfigBase::Localized);
		ksc.sync();

		// Update the buttons with a QTimer (why?)
		QTimer::singleShot(0,this,SLOT(updateButtons()));
	}
}

void Sidebar_Widget::slotSetURL( )
{
	KUrlRequesterDialog dlg( m_currentButton->URL, i18n("Enter a URL:"), this );
	dlg.fileDialog()->setMode( KFile::Directory );
	if (dlg.exec())
	{
		KConfig _ksc( m_path+m_currentButton->file, KConfig::SimpleConfig );
		KConfigGroup ksc(&_ksc, "Desktop Entry");
		if ( !dlg.selectedUrl().isValid())
		{
			KMessageBox::error(this, i18n("<qt><b>%1</b> does not exist</qt>", dlg.selectedUrl().url()));
		}
		else
		{
			QString newurl= dlg.selectedUrl().prettyUrl();
			//If we are going to set the name by 'set name', we don't set it here.
			//ksc.writeEntry("Name",newurl);
			ksc.writePathEntry("URL",newurl);
			ksc.sync();
			QTimer::singleShot(0,this,SLOT(updateButtons()));
		}
	}
}

void Sidebar_Widget::slotSetIcon( )
{
//	kicd.setStrictIconSize(true);
        QString iconname=KIconDialog::getIcon(KIconLoader::Small);
	kDebug()<<"New Icon Name:"<<iconname;
	if (!iconname.isEmpty())
	{
		KConfig _ksc( m_path+m_currentButton->file, KConfig::SimpleConfig );
		KConfigGroup ksc(&_ksc, "Desktop Entry");
		ksc.writeEntry("Icon",iconname);
		ksc.sync();
		QTimer::singleShot(0,this,SLOT(updateButtons()));
	}
}

void Sidebar_Widget::slotRemove()
{
	if (KMessageBox::warningContinueCancel(this,i18n("<qt>Do you really want to remove the <b>%1</b> tab?</qt>", m_currentButton->displayName),
		QString(),KStandardGuiItem::del())==KMessageBox::Continue)
	{
		QFile f(m_path+m_currentButton->file);
		if (!f.remove())
			qDebug("Error, file not deleted");
		QTimer::singleShot(0,this,SLOT(updateButtons()));
	}
}

void Sidebar_Widget::slotMultipleViews( )
{
	m_singleWidgetMode = !m_singleWidgetMode;
	if ((m_singleWidgetMode) && (m_visibleViews.count()>1))
	{
		int tmpViewID=m_latestViewed;
		for (int i=0; i<m_buttons.count(); i++) {
			ButtonInfo *button = m_buttons.at(i);
			if ((int) i != tmpViewID)
			{
				if (button->dock && button->dock->isVisibleTo(this))
					showHidePage(i);
			} 
		}
		m_latestViewed=tmpViewID;
	} 
	m_configTimer.start(400);
}

void Sidebar_Widget::slotShowTabsLeft( )
{
	m_showTabsLeft = ! m_showTabsLeft;
	doLayout();
	m_configTimer.start(400);
}

void Sidebar_Widget::slotShowConfigurationButton( )
{
	m_showExtraButtons = ! m_showExtraButtons;
	if(m_showExtraButtons)
	{
		m_buttonBar->button(-1)->show();
	}
	else
	{
		m_buttonBar->button(-1)->hide();

		KMessageBox::information(this,
		i18n("You have hidden the navigation panel configuration button. To make it visible again, click the right mouse button on any of the navigation panel buttons and select \"Show Configuration Button\"."));

	}
	m_configTimer.start(400);
}
void Sidebar_Widget::readConfig()
{
	m_singleWidgetMode = m_config->readEntry("SingleWidgetMode", true);
	m_showExtraButtons = m_config->readEntry("ShowExtraButtons", false);
	m_showTabsLeft = m_config->readEntry("ShowTabsLeft", true);
	m_hideTabs = m_config->readEntry("HideTabs", false);
	if (m_initial) {
		m_openViews = m_config->readEntry("OpenViews",QStringList());
		m_savedWidth = m_config->readEntry("SavedWidth",200);
		m_initial=false;
	}
}

void Sidebar_Widget::stdAction(const char *handlestd)
{
	ButtonInfo* mod = m_activeModule;

	if (!mod)
		return;
	if (!(mod->module))
		return;

	kDebug() << "Try calling >active< module's (" << mod->module->metaObject()->className() << ") slot " << handlestd;

	int id = mod->module->metaObject()->indexOfSlot( handlestd );
  	if ( id == -1 )
		return;
	kDebug() << "Action slot was found, it will be called now";
	QMetaObject::invokeMethod( mod->module, handlestd );
  	return;
}


void Sidebar_Widget::updateButtons()
{
	//PARSE ALL DESKTOP FILES
	m_openViews = m_visibleViews;

	if (m_buttons.count() > 0)
	{
		for (int i = 0; i < m_buttons.count(); i++)
		{
			ButtonInfo *button = m_buttons.at(i);
			if (button->dock)
			{
				m_noUpdate = true;
				if (button->dock->isVisibleTo(this)) {
					showHidePage(i);
				}

				delete button->module;
				delete button->dock;
			}
			m_buttonBar->removeTab(i);

		}
	}
        qDeleteAll(m_buttons);
	m_buttons.clear();

	readConfig();
	doLayout();
	createButtons();
}

void Sidebar_Widget::createButtons()
{
	if (!m_path.isEmpty())
	{
		kDebug()<<"m_path: "<<m_path;
		QDir dir(m_path);
		QStringList list=dir.entryList(QStringList() << "*.desktop");
		list.removeAll( "history.desktop" ); // #205521
		for (QStringList::const_iterator it=list.constBegin(); it!=list.constEnd(); ++it)
		{
			addButton(*it);
		}
	}

	if (!m_buttonBar->button(-1)) {
		m_buttonBar->appendButton(SmallIcon("configure"), -1, m_menu,
					i18n("Configure Sidebar"));
	}

	if (m_showExtraButtons) {
		m_buttonBar->button(-1)->show();
	} else {
		m_buttonBar->button(-1)->hide();
	}

	for (int i = 0; i < m_buttons.count(); i++)
	{
		ButtonInfo *button = m_buttons.at(i);
		if (m_openViews.contains(button->file))
		{
			m_buttonBar->setTab(i,true);
			m_noUpdate = true;
			showHidePage(i);
			if (m_singleWidgetMode) {
				break;
			}
		}
	}

	collapseExpandSidebar();
        m_noUpdate=false;
}

bool Sidebar_Widget::openUrl(const class KUrl &url)
{
	if (url.protocol()=="sidebar")
	{
		for (int i=0;i<m_buttons.count();i++)
			if (m_buttons.at(i)->file==url.path())
			{
				KMultiTabBarTab *tab = m_buttonBar->tab(i);
				if (!tab->isChecked())
					tab->animateClick();
				return true;
			}
		return false;
	}

	m_storedUrl=url;
	m_hasStoredUrl=true;
        bool ret = false;
	for (int i=0;i<m_buttons.count();i++)
	{
		ButtonInfo *button = m_buttons.at(i);
		if (button->dock)
		{
			if ((button->dock->isVisibleTo(this)) && (button->module))
			{
				ret = true;
				button->module->openUrl(url);
			}
		}
	}
	return ret;
}

bool Sidebar_Widget::addButton(const QString &desktoppath,int pos)
{
	int lastbtn = m_buttons.count();

  	KConfigGroup *confFile;

	kDebug() << "addButton:" << (m_path+desktoppath);

	confFile = new KConfigGroup(
	    KSharedConfig::openConfig(m_path+desktoppath, KConfig::SimpleConfig),
	    "Desktop Entry");

    	QString icon = confFile->readEntry("Icon");
	QString name = confFile->readEntry("Name");
	QString comment = confFile->readEntry("Comment");
	QString url = confFile->readPathEntry("URL",QString());
	QString lib = confFile->readEntry("X-KDE-KonqSidebarModule");

        delete confFile;

	if (pos == -1)
	{
	  	m_buttonBar->appendTab(SmallIcon(icon), lastbtn, name);
		ButtonInfo *bi = new ButtonInfo(desktoppath, ((KonqSidebar*)m_partParent),0, url, lib, name,
						icon, this);
		/*int id=*/m_buttons.insert(lastbtn, bi);
		KMultiTabBarTab *tab = m_buttonBar->tab(lastbtn);
		tab->installEventFilter(this);
		connect(tab,SIGNAL(clicked(int)),this,SLOT(showHidePage(int)));

		// Set Whats This help
		// This uses the comments in the .desktop files
		tab->setWhatsThis(comment);
	}

	return true;
}



bool Sidebar_Widget::eventFilter(QObject *obj, QEvent *ev)
{

	if (ev->type()==QEvent::MouseButtonPress && ((QMouseEvent *)ev)->button()==Qt::RightButton)
	{
		KMultiTabBarTab *bt=dynamic_cast<KMultiTabBarTab*>(obj);
		if (bt)
		{
			kDebug()<<"Request for popup";
			m_currentButton = 0;
			for (int i=0;i<m_buttons.count();i++)
			{
				if (bt==m_buttonBar->tab(i))
				{
					m_currentButton = m_buttons.at(i);
					break;
				}
			}

			if (m_currentButton)
			{
				KMenu *buttonPopup=new KMenu(this);
				buttonPopup->addTitle(SmallIcon(m_currentButton->iconName), m_currentButton->displayName);
				buttonPopup->addAction(KIcon("edit-rename"), i18n("Set Name..."), this, SLOT(slotSetName())); // Item to open a dialog to change the name of the sidebar item (by Pupeno)
				buttonPopup->addAction(KIcon("internet-web-browser"), i18n("Set URL..."), this, SLOT(slotSetURL()));
				buttonPopup->addAction(KIcon("preferences-desktop-icons"), i18n("Set Icon..."), this, SLOT(slotSetIcon()));
				buttonPopup->addSeparator();
				buttonPopup->addAction(KIcon("edit-delete"), i18n("Remove"), this, SLOT(slotRemove()));
				buttonPopup->addSeparator();
				buttonPopup->addMenu(m_menu);
				buttonPopup->setItemEnabled(2,!m_currentButton->URL.isEmpty());
				buttonPopup->exec(QCursor::pos());
				delete buttonPopup;
			}
			return true;

		}
	}
	return false;
}

void Sidebar_Widget::mousePressEvent(QMouseEvent *ev)
{
	if (ev->type()==QEvent::MouseButtonPress && ((QMouseEvent *)ev)->button()==Qt::RightButton)
		m_menu->exec(QCursor::pos());
}

KonqSidebarPlugin *Sidebar_Widget::loadModule(QWidget *par,QString &desktopName,const QString &lib_name,ButtonInfo* bi)
{
	KLibLoader *loader = KLibLoader::self();

	// try to load the library
      	KLibrary *lib = loader->library(lib_name);
	if (lib)
	{
		// get the create_ function
		QString factory("create_%1");
                KLibrary::void_function_ptr create = lib->resolveFunction(QFile::encodeName(factory.arg(lib_name)));

		if (create)
		{
			// create the module

			KonqSidebarPlugin* (*func)(const KComponentData &,QObject *, QWidget*, QString&, const char *);
			func = (KonqSidebarPlugin* (*)(const KComponentData &,QObject *, QWidget *, QString&, const char *)) create;
			QString fullPath(m_path+desktopName);
			return  (KonqSidebarPlugin*)func(getInstance(),bi,par,fullPath,0);
		}
	} else {
		kWarning() << "Module " << lib_name << " doesn't specify a library!" ;
	}
	return 0;
}

KParts::BrowserExtension *Sidebar_Widget::getExtension()
{
	return KParts::BrowserExtension::childObject(m_partParent);
}

bool Sidebar_Widget::createView( ButtonInfo *data)
{
	bool ret = true;
	KConfigGroup *confFile;
	confFile = new KConfigGroup(
	    KSharedConfig::openConfig(data->file, KConfig::SimpleConfig),
	    "Desktop Entry");

	data->dock = 0;
	data->module = loadModule(m_area, data->file,data->libName,data);

	if (data->module == 0)
	{
		ret = false;
	} else {
		data->dock = data->module->getWidget();
		connectModule(data->module);
		connect(this, SIGNAL(fileSelection(const KFileItemList&)),
			data->module, SLOT(openPreview(const KFileItemList&)));

		connect(this, SIGNAL(fileMouseOver(const KFileItem&)),
			data->module, SLOT(openPreviewOnMouseOver(const KFileItem&)));
	}

	delete confFile;
	return ret;
}

void Sidebar_Widget::showHidePage(int page)
{
	ButtonInfo *info = m_buttons.at(page);
	if (!info->dock)
	{
		if (m_buttonBar->isTabRaised(page))
		{
			//SingleWidgetMode
			if (m_singleWidgetMode)
			{
				if (m_latestViewed != -1)
				{
					m_noUpdate = true;
					showHidePage(m_latestViewed);
				}
			}

			if (!createView(info))
			{
				m_buttonBar->setTab(page,false);
				return;
			}

			m_buttonBar->setTab(page,true);

			connect(info->module,
				SIGNAL(setIcon(const QString&)),
				m_buttonBar->tab(page),
				SLOT(setIcon(const QString&)));

			connect(info->module,
				SIGNAL(setCaption(const QString&)),
				m_buttonBar->tab(page),
				SLOT(setText(const QString&)));
                        
                        m_area->addWidget(info->dock);
			info->dock->show();
                        m_area->show();
			if (m_hasStoredUrl)
				info->module->openUrl(m_storedUrl);
			m_visibleViews<<info->file;
			m_latestViewed=page;
		}
	} else {
		if ((!info->dock->isVisibleTo(this)) && (m_buttonBar->isTabRaised(page))) {
			//SingleWidgetMode
			if (m_singleWidgetMode) {
				if (m_latestViewed != -1) {
					m_noUpdate = true;
					showHidePage(m_latestViewed);
				}
			}

			info->dock->show();
                        m_area->show();
			m_latestViewed = page;
			if (m_hasStoredUrl)
				info->module->openUrl(m_storedUrl);
			m_visibleViews << info->file;
			m_buttonBar->setTab(page,true);
		} else {
			m_buttonBar->setTab(page,false);
			info->dock->hide();
			m_latestViewed = -1;
			m_visibleViews.removeAll(info->file);
                        if (m_visibleViews.empty()) m_area->hide();
		}
	}

	if (!m_noUpdate)
		collapseExpandSidebar();
	m_noUpdate = false;
}

void Sidebar_Widget::collapseExpandSidebar()
{
	if (!parentWidget())
		return; // Can happen during destruction

	if (m_visibleViews.count()==0)
	{
		m_somethingVisible = false;
		parentWidget()->setMaximumWidth(minimumSizeHint().width());
		updateGeometry();
		emit panelHasBeenExpanded(false);
	} else {
		m_somethingVisible = true;
		parentWidget()->setMaximumWidth(32767);
		updateGeometry();
		emit panelHasBeenExpanded(true);
	}
}

QSize Sidebar_Widget::sizeHint() const
{
        if (m_somethingVisible)
           return QSize(m_savedWidth,200);
        return minimumSizeHint();
}

const KComponentData &Sidebar_Widget::getInstance()
{
	return ((KonqSidebar*)m_partParent)->getInstance();
}

void Sidebar_Widget::submitFormRequest(const char *action,
					const QString& url,
					const QByteArray& formData,
					const QString& /*target*/,
					const QString& contentType,
					const QString& /*boundary*/ )
{
        KParts::OpenUrlArguments arguments;
        KParts::BrowserArguments browserArguments;
	browserArguments.setContentType("Content-Type: " + contentType);
	browserArguments.postData = formData;
	browserArguments.setDoPost(QByteArray(action).toLower() == "post");
	// boundary?
	emit getExtension()->openUrlRequest(KUrl( url ), arguments, browserArguments);
}

void Sidebar_Widget::openUrlRequest( const KUrl &url, const KParts::OpenUrlArguments& args, const KParts::BrowserArguments& browserArgs)
{
	getExtension()->openUrlRequest(url,args, browserArgs);
}

void Sidebar_Widget::createNewWindow( const KUrl &url, const KParts::OpenUrlArguments& args, const KParts::BrowserArguments& browserArgs,
	const KParts::WindowArgs &windowArgs, KParts::ReadOnlyPart **part )
{
	getExtension()->createNewWindow(url,args,browserArgs, windowArgs,part);
}

void Sidebar_Widget::enableAction( const char * name, bool enabled )
{
	if ((qstrcmp("ButtonInfo", sender()->parent()->metaObject()->className()) == 0))
	{
		ButtonInfo *btninfo = static_cast<ButtonInfo*>(sender()->parent());
		if (btninfo)
		{
			QString n(name);
			if (n == "copy")
				btninfo->copy = enabled;
			else if (n == "cut")
				btninfo->cut = enabled;
			else if (n == "paste")
				btninfo->paste = enabled;
			else if (n == "trash")
				btninfo->trash = enabled;
			else if (n == "del")
				btninfo->del = enabled;
			else if (n == "rename")
				btninfo->rename = enabled;
		}
	}
}


bool  Sidebar_Widget::doEnableActions()
{
	if ((qstrcmp("ButtonInfo", sender()->parent()->metaObject()->className()) != 0))
	{
		kDebug()<<"Couldn't set active module, aborting";
		return false;
	} else {
		m_activeModule=static_cast<ButtonInfo*>(sender()->parent());
		getExtension()->enableAction( "copy", m_activeModule->copy );
		getExtension()->enableAction( "cut", m_activeModule->cut );
		getExtension()->enableAction( "paste", m_activeModule->paste );
		getExtension()->enableAction( "trash", m_activeModule->trash );
		getExtension()->enableAction( "del", m_activeModule->del );
		getExtension()->enableAction( "rename", m_activeModule->rename );
		return true;
	}

}

void Sidebar_Widget::popupMenu( const QPoint &global, const KFileItemList &items )
{
	if (doEnableActions())
		getExtension()->popupMenu(global,items);
}

void Sidebar_Widget::popupMenu(
	const QPoint &global, const KUrl &url,
	const QString &mimeType, mode_t mode )
{
    if (doEnableActions()) {
        KParts::OpenUrlArguments args;
        args.setMimeType(mimeType);
        getExtension()->popupMenu(global,url,mode, args);
    }
}

void Sidebar_Widget::connectModule(QObject *mod)
{
	if (mod->metaObject()->indexOfSignal("started(KIO::Job*)") != -1) {
		connect(mod,SIGNAL(started(KIO::Job *)),this, SIGNAL(started(KIO::Job*)));
	}

	if (mod->metaObject()->indexOfSignal("completed()") != -1) {
		connect(mod,SIGNAL(completed()),this,SIGNAL(completed()));
	}

	if (mod->metaObject()->indexOfSignal("popupMenu(QPoint,KUrl,QString,mode_t)") != -1) {
		connect(mod,SIGNAL(popupMenu( const QPoint &, const KUrl &,
			const QString &, mode_t)),this,SLOT(popupMenu( const
			QPoint &, const KUrl&, const QString &, mode_t)));
	}

	if (mod->metaObject()->indexOfSignal("popupMenu(QPoint,KUrl,QString,mode_t)") != -1) {
		connect(mod,SIGNAL(popupMenu( const QPoint &,
			const KUrl &,const QString &, mode_t)),this,
			SLOT(popupMenu( const QPoint &,
			const KUrl &,const QString &, mode_t)));
	}

	if (mod->metaObject()->indexOfSignal("popupMenu(QPoint,KFileItemList)") != -1) {
		connect(mod,SIGNAL(popupMenu( const QPoint &, const KFileItemList & )),
			this,SLOT(popupMenu( const QPoint &, const KFileItemList & )));
	}

	if (mod->metaObject()->indexOfSignal("openUrlRequest(KUrl,KParts::OpenUrlArguments,KParts::BrowserArguments)") != -1) {
		connect(mod,SIGNAL(openUrlRequest( const KUrl &, const KParts::OpenUrlArguments&, const KParts::BrowserArguments&)),
			this,SLOT(openUrlRequest( const KUrl &, const KParts::OpenUrlArguments&, const KParts::BrowserArguments&)));
	}

	if (mod->metaObject()->indexOfSignal("submitFormRequest(const char*,QString,QByteArray,QString,QString,QString)") != -1) {
		connect(mod,
			SIGNAL(submitFormRequest(const char*,const QString&,const QByteArray&,const QString&,const QString&,const QString&)),
			this,
			SLOT(submitFormRequest(const char*,const QString&,const QByteArray&,const QString&,const QString&,const QString&)));
	}

	if (mod->metaObject()->indexOfSignal("enableAction(const char*,bool)") != -1) {
		connect(mod,SIGNAL(enableAction( const char *, bool)),
			this,SLOT(enableAction(const char *, bool)));
	}

	if (mod->metaObject()->indexOfSignal("createNewWindow(KUrl,KParts::OpenUrlArguments,KParts::BrowserArguments,KParts::WindowArgs,KParts::ReadOnlyPart**)") != -1) {
            connect(mod,SIGNAL(createNewWindow(KUrl,KParts::OpenUrlArguments,KParts::BrowserArguments,KParts::WindowArgs,KParts::ReadOnlyPart**)),
                    this,SLOT(createNewWindow(KUrl,KParts::OpenUrlArguments,KParts::BrowserArguments,KParts::WindowArgs,KParts::ReadOnlyPart**)));
	}
}



Sidebar_Widget::~Sidebar_Widget()
{
        m_config->writeEntry("OpenViews", m_visibleViews);
	if (m_configTimer.isActive())
		saveConfig();
	delete m_config;
        qDeleteAll(m_buttons);
        m_buttons.clear();
	m_noUpdate = true;
}

void Sidebar_Widget::customEvent(QEvent* ev)
{
	if (KonqFileSelectionEvent::test(ev))
	{
		emit fileSelection(static_cast<KonqFileSelectionEvent*>(ev)->selection());
	} else if (KonqFileMouseOverEvent::test(ev)) {
		emit fileMouseOver(static_cast<KonqFileMouseOverEvent*>(ev)->item());
	}
}

#if 0
void Sidebar_Widget::resizeEvent(QResizeEvent* ev)
{
	if (m_somethingVisible && m_userMovedSplitter)
	{
		int newWidth = width();
                QSplitter *split = splitter();
		if (split && (m_savedWidth != newWidth))
		{
			QList<int> sizes = split->sizes();
			if ((sizes.count() >= 2) && (sizes[1]))
			{
				m_savedWidth = newWidth;
				updateGeometry();
				m_configTimer.start(400);
			}
		}
	}
	m_userMovedSplitter = false;
	QWidget::resizeEvent(ev);
}

QSplitter *Sidebar_Widget::splitter() const
{
	if (m_universalMode) return 0;
	QObject *p = parent();
	if (!p) return 0;
	p = p->parent();
	return static_cast<QSplitter*>(p);
}

void Sidebar_Widget::userMovedSplitter()
{
	m_userMovedSplitter = true;
}
#endif

#include "sidebar_widget.moc"
