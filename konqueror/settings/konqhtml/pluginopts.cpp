/*
 * Copyright (C) 2002-2003 Leo Savernik
 *    per-domain settings
 * Copyright (C) 2001, Daniel Naber
 *    based on javaopts.cpp
 * Copyright (C) 2000 Stefan Schimanski <1Stein@gmx.de>
 *    Netscape parts
 *
 */

// Own
#include "pluginopts.h"

// std
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Qt
#include <QtCore/QRegExp>
#include <QtCore/QTextStream>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QSlider>
#include <QtGui/QTreeWidget>

// KDE
#include <kprocess.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kurlrequester.h>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KProgressDialog>

// Local
#include "htmlopts.h"
#include "ui_nsconfigwidget.h"
#include "policydlg.h"

// == class PluginPolicies =====

PluginPolicies::PluginPolicies(KSharedConfig::Ptr config, const QString &group, bool global,
  		const QString &domain) :
	Policies(config,group,global,domain,"plugins.","EnablePlugins") {
}

PluginPolicies::~PluginPolicies() {
}

// == class KPluginOptions =====

K_PLUGIN_FACTORY_DECLARATION(KcmKonqHtmlFactory)

KPluginOptions::KPluginOptions( QWidget *parent, const QVariantList& )
    : KCModule( KcmKonqHtmlFactory::componentData(), parent ),
      m_pConfig( KSharedConfig::openConfig("konquerorrc", KConfig::NoGlobals) ),
      m_groupname( "Java/JavaScript Settings" ),
      global_policies(m_pConfig,m_groupname,true)
{
    QVBoxLayout* toplevel = new QVBoxLayout( this );

    QTabWidget* topleveltab = new QTabWidget( this );
    toplevel->addWidget( topleveltab );

    QWidget* globalGB = new QWidget( topleveltab );
    topleveltab->addTab( globalGB, i18n( "Global Settings" ) );

    /**************************************************************************
     ******************** Global Settings *************************************
     *************************************************************************/
    enablePluginsGloballyCB = new QCheckBox( i18n( "&Enable plugins globally" ), globalGB );
    enableHTTPOnly = new QCheckBox( i18n( "Only allow &HTTP and HTTPS URLs for plugins" ), globalGB );
    enableUserDemand = new QCheckBox( i18n( "&Load plugins on demand only" ), globalGB );
    priorityLabel = new QLabel(i18n("CPU priority for plugins: %1", QString()), globalGB);
    //priority = new QSlider(5, 100, 5, 100, Qt::Horizontal, globalGB);
    priority = new QSlider(Qt::Horizontal, globalGB);
    priority->setMinimum(5);
    priority->setMaximum(100);
    priority->setPageStep(5);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(enablePluginsGloballyCB);
    vbox->addWidget(enableHTTPOnly);
    vbox->addWidget(enableUserDemand);
    vbox->addWidget(priorityLabel);
    vbox->addWidget(priority);

    connect( enablePluginsGloballyCB, SIGNAL( clicked() ), this, SLOT( slotChanged() ) );
    connect( enablePluginsGloballyCB, SIGNAL( clicked() ), this, SLOT( slotTogglePluginsEnabled() ) );
    connect( enableHTTPOnly, SIGNAL( clicked() ), this, SLOT( slotChanged() ) );
    connect( enableUserDemand, SIGNAL( clicked() ), this, SLOT( slotChanged() ) );
    connect( priority, SIGNAL( valueChanged(int) ), this, SLOT( slotChanged() ) );
    connect( priority, SIGNAL( valueChanged(int) ), this, SLOT( updatePLabel(int) ) );

    QFrame *hrule = new QFrame(globalGB);
    hrule->setFrameStyle(QFrame::HLine | QFrame::Sunken);
    hrule->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Fixed);

    /**************************************************************************
     ********************* Domain-specific Settings ***************************
     *************************************************************************/
    QPushButton *domainSpecPB = new QPushButton(i18n("Domain-Specific Settin&gs"),
    						globalGB);
    domainSpecPB->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    connect(domainSpecPB,SIGNAL(clicked()),SLOT(slotShowDomainDlg()));

    vbox->addWidget(hrule);
    vbox->addWidget(domainSpecPB);

    globalGB->setLayout(vbox);

    vbox->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));

    domainSpecificDlg = new KDialog( this );
    domainSpecificDlg->setCaption( i18n("Domain-Specific Policies") );
    domainSpecificDlg->setButtons( KDialog::Close );
    domainSpecificDlg->setDefaultButton( KDialog::Close );
    domainSpecificDlg->setObjectName( "domainSpecificDlg" );
    domainSpecificDlg->setModal( true );

    domainSpecific = new PluginDomainListView(m_pConfig,m_groupname,this,domainSpecificDlg);
    domainSpecific->setMinimumSize(320,200);
    connect(domainSpecific,SIGNAL(changed(bool)),SLOT(slotChanged()));

    domainSpecificDlg->setMainWidget(domainSpecific);

    /**************************************************************************
     ********************** WhatsThis? items **********************************
     *************************************************************************/
    enablePluginsGloballyCB->setWhatsThis( i18n("Enables the execution of plugins "
          "that can be contained in HTML pages, e.g. Macromedia Flash. "
          "Note that, as with any browser, enabling active contents can be a security problem.") );

    QString wtstr = i18n("<p>This box contains the domains and hosts you have set "
                         "a specific plugin policy for. This policy will be used "
                         "instead of the default policy for enabling or disabling plugins on pages sent by these "
                         "domains or hosts.</p><p>Select a policy and use the controls on "
                         "the right to modify it.</p>");
    domainSpecific->listView()->setWhatsThis( wtstr );
    domainSpecific->importButton()->setWhatsThis( i18n("Click this button to choose the file that contains "
                                          "the plugin policies. These policies will be merged "
                                          "with the existing ones. Duplicate entries are ignored.") );
    domainSpecific->exportButton()->setWhatsThis( i18n("Click this button to save the plugin policy to a zipped "
                                          "file. The file, named <b>plugin_policy.tgz</b>, will be "
                                          "saved to a location of your choice." ) );
    domainSpecific->setWhatsThis( i18n("Here you can set specific plugin policies for any particular "
                                            "host or domain. To add a new policy, simply click the <i>New...</i> "
                                            "button and supply the necessary information requested by the "
                                            "dialog box. To change an existing policy, click on the <i>Change...</i> "
                                            "button and choose the new policy from the policy dialog box. Clicking "
                                            "on the <i>Delete</i> button will remove the selected policy causing the default "
                                            "policy setting to be used for that domain.") );
#if 0
                                            "The <i>Import</i> and <i>Export</i> "
                                            "button allows you to easily share your policies with other people by allowing "
                                            "you to save and retrieve them from a zipped file.") );
#endif

/*****************************************************************************/

    QWidget* pluginsSettingsContainer = new QWidget( topleveltab );
    topleveltab->addTab( pluginsSettingsContainer, i18n( "Plugins" ) );

    // create Designer made widget
    m_widget.setupUi( pluginsSettingsContainer );
    pluginsSettingsContainer->setObjectName( "configwidget" );
    m_widget.dirEdit->setMode(KFile::ExistingOnly | KFile::LocalOnly | KFile::Directory);

    // setup widgets
    connect( m_widget.scanButton, SIGNAL(clicked()), SLOT(scan()) );

    m_changed = false;

    dirInit();
    pluginInit();

}

void KPluginOptions::updatePLabel(int p) {
    QString level;
    p = (100 - p)/5;
    if (p > 15) {
            level = i18nc("lowest priority", "lowest");
    } else if (p > 11) {
            level = i18nc("low priority", "low");
    } else if (p > 7) {
            level = i18nc("medium priority", "medium");
    } else if (p > 3) {
            level = i18nc("high priority", "high");
    } else {
            level = i18nc("highest priority", "highest");
    }

    priorityLabel->setText(i18n("CPU priority for plugins: %1", level));
}


void KPluginOptions::load()
{
    // *** load ***
    global_policies.load();
    bool bPluginGlobal = global_policies.isFeatureEnabled();

    // *** apply to GUI ***
    enablePluginsGloballyCB->setChecked( bPluginGlobal );

    domainSpecific->initialize(m_pConfig->group(m_groupname).readEntry("PluginDomains", QStringList() ));

/****************************************************************************/

  KSharedConfig::Ptr config = KSharedConfig::openConfig("kcmnspluginrc");
  KConfigGroup cg(config, "Misc");

  m_widget.dirEdit->setUrl(KUrl());
  m_widget.dirEdit->setEnabled( false );
  m_widget.dirRemove->setEnabled( false );
  m_widget.dirUp->setEnabled( false );
  m_widget.dirDown->setEnabled( false );
  enableHTTPOnly->setChecked( cg.readEntry("HTTP URLs Only", false) );
  enableUserDemand->setChecked( cg.readEntry("demandLoad", false) );
  priority->setValue(100 - qBound(0, cg.readEntry("Nice Level", 0), 19) * 5);
  updatePLabel(priority->value());

  dirLoad( config );
  pluginLoad( config );

  change( false );
}

void KPluginOptions::defaults()
{
    global_policies.defaults();
    enablePluginsGloballyCB->setChecked( global_policies.isFeatureEnabled() );
    enableHTTPOnly->setChecked(false);
    enableUserDemand->setChecked(false);
    priority->setValue(100);

/*****************************************************************************/

    KSharedConfig::Ptr config = KSharedConfig::openConfig( QString(), KConfig::NoGlobals );

    m_widget.dirEdit->setUrl(KUrl());
    m_widget.dirEdit->setEnabled( false );
    m_widget.dirRemove->setEnabled( false );

    dirLoad( config, true );
    pluginLoad( config );

    change();
}

void KPluginOptions::save()
{
    global_policies.save();

    domainSpecific->save(m_groupname,"PluginDomains");

    m_pConfig->sync();	// I need a sync here, otherwise "apply" won't work
    			// instantly
    // Send signal to all konqueror instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KonqMain", "org.kde.Konqueror.Main", "reparseConfiguration");
    QDBusConnection::sessionBus().send(message);

/*****************************************************************************/

    KSharedConfig::Ptr config = KSharedConfig::openConfig("kcmnspluginrc");

    dirSave( config );
    pluginSave( config );

    KConfigGroup cg(config, "Misc");
    cg.writeEntry( "HTTP URLs Only", enableHTTPOnly->isChecked() );
    cg.writeEntry( "demandLoad", enableUserDemand->isChecked() );
    cg.writeEntry("Nice Level", (int)(100 - priority->value()) / 5);
    cg.sync();

    change( false );
}

QString KPluginOptions::quickHelp() const
{
      return i18n("<h1>Konqueror Plugins</h1> The Konqueror web browser can use Netscape"
        " plugins to show special content, just like the Navigator does. Please note that"
        " the way you have to install Netscape plugins may depend on your distribution. A typical"
        " place to install them is, for example, '/opt/netscape/plugins'.");
}

void KPluginOptions::slotChanged()
{
    emit changed(true);
}

void KPluginOptions::slotTogglePluginsEnabled() {
  global_policies.setFeatureEnabled(enablePluginsGloballyCB->isChecked());
}

void KPluginOptions::slotShowDomainDlg() {
  domainSpecificDlg->show();
}

/***********************************************************************************/

void KPluginOptions::scan()
{
    m_widget.scanButton->setEnabled(false);
    if ( m_changed ) {
        int ret = KMessageBox::warningYesNoCancel( this,
                                                    i18n("Do you want to apply your changes "
                                                         "before the scan? Otherwise the "
                                                         "changes will be lost."), QString(), KStandardGuiItem::save(), KStandardGuiItem::discard() );
        if ( ret==KMessageBox::Cancel ) {
            m_widget.scanButton->setEnabled(true);
            return;
        }
        if ( ret==KMessageBox::Yes )
             save();
    }

    nspluginscan = new KProcess(this);
    nspluginscan->setOutputChannelMode(KProcess::SeparateChannels);
    QString scanExe = KGlobal::dirs()->findExe("nspluginscan");
    if (scanExe.isEmpty()) {
        kDebug() << "can't find nspluginviewer";

        KMessageBox::sorry ( this,
                             i18n("The nspluginscan executable cannot be found. "
                                  "Netscape plugins will not be scanned.") );
        m_widget.scanButton->setEnabled(true);
        return;
    }

    // find nspluginscan executable
    m_progress = new KProgressDialog( this, QString(), i18n("Scanning for plugins") );
    m_progress->progressBar()->setValue( 5 );

    // start nspluginscan
    *nspluginscan << scanExe << "--verbose";
    kDebug() << "Running nspluginscan";
    connect(nspluginscan, SIGNAL(readyReadStandardOutput()),
            this, SLOT(progress()));
    connect(nspluginscan, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(scanDone()));
    connect(m_progress, SIGNAL(cancelClicked()), this, SLOT(scanDone()));

    nspluginscan->start();
}

void KPluginOptions::progress()
{
    // we do not know if the output array ends in the middle of an utf-8 sequence
    m_output += nspluginscan->readAllStandardOutput();
    QString line;
    int pos;
    while ((pos = m_output.indexOf('\n')) != -1) {
        line = QString::fromLocal8Bit(m_output, pos + 1);
        m_output.remove(0, pos + 1);
    }
    m_progress->progressBar()->setValue(line.trimmed().toInt());
}

void KPluginOptions::scanDone()
{
    // update dialog
    if (m_progress) {
        m_progress->progressBar()->setValue(100);
        load();
        m_progress->deleteLater();
        m_progress = 0;
    }
    m_widget.scanButton->setEnabled(true);
}

/***********************************************************************************/


void KPluginOptions::dirInit()
{
    m_widget.dirEdit->setWindowTitle(i18n("Select Plugin Scan Folder"));
    connect( m_widget.dirNew, SIGNAL(clicked()), SLOT(dirNew()));
    connect( m_widget.dirRemove, SIGNAL(clicked()), SLOT(dirRemove()));
    connect( m_widget.dirUp, SIGNAL(clicked()), SLOT(dirUp()));
    connect( m_widget.dirDown, SIGNAL(clicked()), SLOT(dirDown()) );
    connect( m_widget.dirEdit,
             SIGNAL(textChanged(const QString&)),
             SLOT(dirEdited(const QString &)) );

    connect( m_widget.dirList,
             SIGNAL(executed(QListWidgetItem*)),
             SLOT(dirSelect(QListWidgetItem*)) );

    connect( m_widget.dirList,
             SIGNAL(itemChanged(QListWidgetItem*)),
             SLOT(dirSelect(QListWidgetItem*)) );
}


void KPluginOptions::dirLoad( KSharedConfig::Ptr config, bool useDefault )
{
    QStringList paths;

    // read search paths

    KConfigGroup cg(config, "Misc");
    if ( cg.hasKey( "scanPaths" ) && !useDefault )
        paths = cg.readEntry( "scanPaths" , QStringList() );
    else {//keep sync with kdebase/apps/nsplugins
        paths.append("$HOME/.mozilla/plugins");
        paths.append("$HOME/.netscape/plugins");
	paths.append("/usr/lib/firefox/plugins");
        paths.append("/usr/lib64/browser-plugins");
        paths.append("/usr/lib/browser-plugins");
        paths.append("/usr/local/netscape/plugins");
        paths.append("/opt/mozilla/plugins");
	paths.append("/opt/mozilla/lib/plugins");
        paths.append("/opt/netscape/plugins");
        paths.append("/opt/netscape/communicator/plugins");
        paths.append("/usr/lib/netscape/plugins");
        paths.append("/usr/lib/netscape/plugins-libc5");
        paths.append("/usr/lib/netscape/plugins-libc6");
        paths.append("/usr/lib/mozilla/plugins");
	paths.append("/usr/lib64/netscape/plugins");
	paths.append("/usr/lib64/mozilla/plugins");
        paths.append("$MOZILLA_HOME/plugins");
    }

    // fill list
    m_widget.dirList->clear();
    m_widget.dirList->addItems( paths );

}


void KPluginOptions::dirSave( KSharedConfig::Ptr config )
{
    // create stringlist
    QStringList paths;
    
    for ( int rowIndex = 0 ; rowIndex < m_widget.dirList->count() ; rowIndex++ ) {
        if ( !m_widget.dirList->item(rowIndex)->text().isEmpty() )
            paths << m_widget.dirList->item(rowIndex)->text();
    }

    // write entry
    KConfigGroup cg(config, "Misc");
    cg.writeEntry( "scanPaths", paths );
}


void KPluginOptions::dirSelect( QListWidgetItem *item )
{
    m_widget.dirEdit->setEnabled( item!=0 );
    m_widget.dirRemove->setEnabled( item!=0 );

    int cur = m_widget.dirList->currentRow();
    m_widget.dirDown->setEnabled( item!=0 && cur<m_widget.dirList->count()-1 );
    m_widget.dirUp->setEnabled( item!=0 && cur>0 );
    m_widget.dirEdit->setUrl( item!=0 ? item->text() : QString() );
 }


void KPluginOptions::dirNew()
{
    m_widget.dirList->insertItem( 0 , QString() );
    m_widget.dirList->setCurrentRow( 0 );
    dirSelect( m_widget.dirList->currentItem() );
    m_widget.dirEdit->setUrl(QString());
    m_widget.dirEdit->setFocus();
    change();
}


void KPluginOptions::dirRemove()
{
    m_widget.dirEdit->setUrl(QString());
    delete m_widget.dirList->currentItem();
    m_widget.dirRemove->setEnabled( false );
    m_widget.dirUp->setEnabled( false );
    m_widget.dirDown->setEnabled( false );
    m_widget.dirEdit->setEnabled( false );
    change();
}


void KPluginOptions::dirUp()
{
    int cur = m_widget.dirList->currentRow();
    if ( cur>0 ) {
        QString txt = m_widget.dirList->item(cur-1)->text();
        delete m_widget.dirList->takeItem( cur-1 );
        m_widget.dirList->insertItem( cur , txt );

        m_widget.dirUp->setEnabled( cur-1>0 );
        m_widget.dirDown->setEnabled( true );
        change();
    }
}


void KPluginOptions::dirDown()
{
    int cur = m_widget.dirList->currentRow();
    if ( cur < m_widget.dirList->count()-1 ) {
        QString txt = m_widget.dirList->item(cur+1)->text();
        delete m_widget.dirList->takeItem( cur+1 );
        m_widget.dirList->insertItem( cur , txt );

        m_widget.dirUp->setEnabled( true );
        m_widget.dirDown->setEnabled( cur+1<m_widget.dirList->count()-1 );
        change();
    }
}


void KPluginOptions::dirEdited(const QString &txt )
{
    if ( m_widget.dirList->currentItem()->text() != txt ) {
        m_widget.dirList->blockSignals(true);
        m_widget.dirList->currentItem()->setText(txt);
        m_widget.dirList->blockSignals(false);
        change();
    }
}


/***********************************************************************************/


void KPluginOptions::pluginInit()
{
}


void KPluginOptions::pluginLoad( KSharedConfig::Ptr /*config*/ )
{
    m_widget.pluginList->setRootIsDecorated(false);
    m_widget.pluginList->setColumnWidth( 0, 200 );
    kDebug() << "-> KPluginOptions::fillPluginList";
    m_widget.pluginList->clear();
    QRegExp version(";version=[^:]*:");

    // open the cache file
    QFile cachef( KStandardDirs::locate("data", "nsplugins/cache") );
    if ( !cachef.exists() || !cachef.open(QIODevice::ReadOnly) ) {
        kDebug() << "Could not load plugin cache file!";
        return;
    }

    QTextStream cache(&cachef);

    // root object
    QTreeWidgetItem *root = new QTreeWidgetItem( m_widget.pluginList, QStringList() << i18n("Netscape Plugins") );
    root->setFlags( Qt::ItemIsEnabled );
    root->setExpanded( true );
    root->setIcon(0, KIcon("netscape"));

    // read in cache
    QString line, plugin;
    QTreeWidgetItem *next = 0;
    QTreeWidgetItem *lastMIME = 0;
    while ( !cache.atEnd() ) {

        line = cache.readLine();
        //kDebug() << line;
        if (line.isEmpty() || (line.left(1) == "#"))
            continue;

        if (line.left(1) == "[") {

            plugin = line.mid(1,line.length()-2);
            //kDebug() << "plugin=" << plugin;

            // add plugin root item
            next = new QTreeWidgetItem( root, QStringList() << i18n("Plugin") << plugin );
            next->setFlags( Qt::ItemIsEnabled );

            lastMIME = 0;

            continue;
        }

        const QStringList desc = line.split(':');
        // avoid crash on broken lines
        if (desc.size()<2)
            continue;

        QString mime = desc[0].trimmed();
        QString name;
        QString suffixes;
        if (desc.count() > 2)
            name = desc[2];
        if (desc.count() > 1)
            suffixes = desc[1];

        if (!mime.isEmpty() && next) {
            //kDebug() << "mime=" << mime << " desc=" << name << " suffix=" << suffixes;
            lastMIME = new QTreeWidgetItem( next, QStringList() << i18n("MIME type") << mime );
            lastMIME->setFlags( Qt::ItemIsEnabled );

            QTreeWidgetItem *last = new QTreeWidgetItem( lastMIME, QStringList() << i18n("Description") << name );
            last->setFlags( Qt::ItemIsEnabled );

            last = new QTreeWidgetItem( lastMIME, QStringList() << i18n("Suffixes") << suffixes );
            last->setFlags( Qt::ItemIsEnabled );
        }
    }

    //kDebug() << "<- KPluginOptions::fillPluginList";
}


void KPluginOptions::pluginSave( KSharedConfig::Ptr /*config*/ )
{

}

// == class PluginDomainDialog =====

PluginDomainDialog::PluginDomainDialog(QWidget *parent) :
	QWidget(parent)
{
  setObjectName("PluginDomainDialog");
  setWindowTitle(i18n("Domain-Specific Policies"));

  thisLayout = new QVBoxLayout(this);
  thisLayout->addSpacing(6);
  QFrame *hrule = new QFrame(this);
  hrule->setFrameStyle(QFrame::HLine | QFrame::Sunken);
  thisLayout->addWidget(hrule);
  thisLayout->addSpacing(6);

  QBoxLayout *hl = new QHBoxLayout(this);
  hl->setSpacing(6);
  hl->setMargin(0);
  hl->addStretch(10);

  QPushButton *closePB = new KPushButton(KStandardGuiItem::close(),this);
  connect(closePB,SIGNAL(clicked()),SLOT(slotClose()));
  hl->addWidget(closePB);
  thisLayout->addLayout(hl);
}

PluginDomainDialog::~PluginDomainDialog() {
}

void PluginDomainDialog::setMainWidget(QWidget *widget) {
  thisLayout->insertWidget(0,widget);
}

void PluginDomainDialog::slotClose() {
  hide();
}

// == class PluginDomainListView =====

PluginDomainListView::PluginDomainListView(KSharedConfig::Ptr config,const QString &group,
	KPluginOptions *options,QWidget *parent)
	: DomainListView(config,i18n( "Doma&in-Specific" ), parent),
	group(group), options(options) {
}

PluginDomainListView::~PluginDomainListView() {
}

void PluginDomainListView::setupPolicyDlg(PushButton trigger,PolicyDialog &pDlg,
		Policies *pol) {
  QString caption;
  switch (trigger) {
    case AddButton:
      caption = i18n( "New Plugin Policy" );
      pol->setFeatureEnabled(!options->enablePluginsGloballyCB->isChecked());
      break;
    case ChangeButton: caption = i18n( "Change Plugin Policy" ); break;
    default: ; // inhibit gcc warning
  }/*end switch*/
  pDlg.setWindowTitle(caption);
  pDlg.setFeatureEnabledLabel(i18n("&Plugin policy:"));
  pDlg.setFeatureEnabledWhatsThis(i18n("Select a plugin policy for "
                                    "the above host or domain."));
  pDlg.refresh();
}

PluginPolicies *PluginDomainListView::createPolicies() {
  return new PluginPolicies(config,group,false);
}

PluginPolicies *PluginDomainListView::copyPolicies(Policies *pol) {
  return new PluginPolicies(*static_cast<PluginPolicies *>(pol));
}

#include "pluginopts.moc"
