/**
 * kcookiespolicies.cpp - Cookies configuration
 *
 * Original Authors
 * Copyright (c) Waldo Bastian <bastian@kde.org>
 * Copyright (c) 1999 David Faure <faure@kde.org>
 *
 * Re-written by:
 * Copyright (c) 2000- Dawit Alemayehu <adawit@kde.org>
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

// Own
#include "kcookiespolicies.h"

// Qt
#include <QtGui/QLayout>
#include <QtGui/QCheckBox>
#include <QtGui/QPushButton>
#include <QtGui/QRadioButton>
#include <QtGui/QToolButton>
#include <QtGui/QBoxLayout>
#include <QtDBus/QtDBus>

// KDE
#include <kiconloader.h>
#include <kmessagebox.h>
#include <k3listview.h>
#include <k3listviewsearchline.h>
#include <klocale.h>
#include <kconfig.h>
#include <kurl.h>
#include <kdebug.h>

#include "ksaveioconfig.h"


KCookiesPolicies::KCookiesPolicies(const KComponentData &componentData, QWidget *parent)
                 :KCModule(componentData, parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);

    dlg = new KCookiesPolicyDlgUI (this);
    dlg->lvDomainPolicy->header()->setStretchEnabled(true, 0);
    dlg->lvDomainPolicy->setColumnWidthMode(0, K3ListView::Manual);
    dlg->lvDomainPolicy->setColumnWidthMode(1, K3ListView::Maximum);
    dlg->kListViewSearchLine->setListView(dlg->lvDomainPolicy);
    QList<int> columns;
    columns.append(0);
    dlg->kListViewSearchLine->setSearchColumns(columns);

    mainLayout->addWidget(dlg);

    // Connect the main swicth :) Enable/disable cookie support
    connect( dlg->cbEnableCookies, SIGNAL( toggled(bool) ),
             SLOT( cookiesEnabled(bool) ) );
    connect( dlg->cbEnableCookies, SIGNAL( toggled(bool) ),
             SLOT( configChanged() ) );

    // Connect the preference check boxes...
    connect ( dlg->cbRejectCrossDomainCookies, SIGNAL(toggled(bool)),
              SLOT(configChanged()));
    connect ( dlg->cbAutoAcceptSessionCookies, SIGNAL(toggled(bool)),
              SLOT(configChanged()));
    connect ( dlg->cbIgnoreCookieExpirationDate, SIGNAL(toggled(bool)),
              SLOT(configChanged()));

    connect ( dlg->cbAutoAcceptSessionCookies, SIGNAL(toggled(bool)),
              SLOT(autoAcceptSessionCookies(bool)));
    connect ( dlg->cbIgnoreCookieExpirationDate, SIGNAL(toggled(bool)),
              SLOT(ignoreCookieExpirationDate(bool)));

    connect ( dlg->rbPolicyAsk, SIGNAL(toggled(bool)),
              SLOT(configChanged()));
    connect ( dlg->rbPolicyAccept, SIGNAL(toggled(bool)),
              SLOT(configChanged()));
    connect ( dlg->rbPolicyReject, SIGNAL(toggled(bool)),
              SLOT(configChanged()));
    // Connect signals from the domain specific policy listview.
    connect( dlg->lvDomainPolicy, SIGNAL(selectionChanged()),
             SLOT(selectionChanged()) );
    connect( dlg->lvDomainPolicy, SIGNAL(doubleClicked (Q3ListViewItem *)),
             SLOT(changePressed() ) );
    connect( dlg->lvDomainPolicy, SIGNAL(returnPressed ( Q3ListViewItem * )),
             SLOT(changePressed() ) );

    // Connect the buttons...
    connect( dlg->pbNew, SIGNAL(clicked()), SLOT( addPressed() ) );
    connect( dlg->pbChange, SIGNAL( clicked() ), SLOT( changePressed() ) );
    connect( dlg->pbDelete, SIGNAL( clicked() ), SLOT( deletePressed() ) );
    connect( dlg->pbDeleteAll, SIGNAL( clicked() ), SLOT( deleteAllPressed() ) );

    load();
}

KCookiesPolicies::~KCookiesPolicies()
{
}

void KCookiesPolicies::configChanged ()
{
  //kDebug() << "KCookiesPolicies::configChanged...";
  emit changed((d_configChanged=true));
}

void KCookiesPolicies::cookiesEnabled( bool enable )
{
  dlg->bgDefault->setEnabled( enable );
  dlg->bgPreferences->setEnabled ( enable );
  dlg->gbDomainSpecific->setEnabled( enable );

  if (enable)
  {
    ignoreCookieExpirationDate ( enable );
    autoAcceptSessionCookies ( enable );
  }
}

void KCookiesPolicies::ignoreCookieExpirationDate ( bool enable )
{
  bool isAutoAcceptChecked = dlg->cbAutoAcceptSessionCookies->isChecked();
  enable = (enable && isAutoAcceptChecked);

  dlg->bgDefault->setEnabled( !enable );
  dlg->gbDomainSpecific->setEnabled( !enable );
}

void KCookiesPolicies::autoAcceptSessionCookies ( bool enable )
{
  bool isIgnoreExpirationChecked = dlg->cbIgnoreCookieExpirationDate->isChecked();
  enable = (enable && isIgnoreExpirationChecked);

  dlg->bgDefault->setEnabled( !enable );
  dlg->gbDomainSpecific->setEnabled( !enable );
}

void KCookiesPolicies::addNewPolicy(const QString& domain)
{
  PolicyDlg pdlg (i18n("New Cookie Policy"), this);
  pdlg.setEnableHostEdit (true, domain);

  if (dlg->rbPolicyAccept->isChecked())
    pdlg.setPolicy(KCookieAdvice::Reject);
  else
    pdlg.setPolicy(KCookieAdvice::Accept);

  if (pdlg.exec() && !pdlg.domain().isEmpty())
  {
    QString domain = KUrl::fromAce(pdlg.domain().toLatin1());
    int advice = pdlg.advice();

    if ( !handleDuplicate(domain, advice) )
    {
      const char* strAdvice = KCookieAdvice::adviceToStr(advice);
      Q3ListViewItem* index = new Q3ListViewItem (dlg->lvDomainPolicy,
                                                domain, i18n(strAdvice));
      m_pDomainPolicy.insert (index, strAdvice);
      configChanged();
    }
  }
}


void KCookiesPolicies::addPressed()
{
  addNewPolicy (QString());
}

void KCookiesPolicies::changePressed()
{
  Q3ListViewItem* index = dlg->lvDomainPolicy->currentItem();

  if (!index)
    return;

  QString oldDomain = index->text(0);

  PolicyDlg pdlg (i18n("Change Cookie Policy"), this);
  pdlg.setPolicy (KCookieAdvice::strToAdvice(m_pDomainPolicy[index]));
  pdlg.setEnableHostEdit (true, oldDomain);

  if( pdlg.exec() && !pdlg.domain().isEmpty())
  {
    QString newDomain = KUrl::fromAce(pdlg.domain().toLatin1());
    int advice = pdlg.advice();
    if (newDomain == oldDomain || !handleDuplicate(newDomain, advice))
    {
      m_pDomainPolicy[index] = KCookieAdvice::adviceToStr(advice);
      index->setText(0, newDomain);
      index->setText(1, i18n(m_pDomainPolicy[index]) );
      configChanged();
    }
  }
}

bool KCookiesPolicies::handleDuplicate( const QString& domain, int advice )
{
  Q3ListViewItem* item = dlg->lvDomainPolicy->firstChild();
  while ( item != 0 )
  {
    if ( item->text(0) == domain )
    {
      QString msg = i18n("<qt>A policy already exists for"
                         "<center><b>%1</b></center>"
                         "Do you want to replace it?</qt>", domain);
      int res = KMessageBox::warningContinueCancel(this, msg,
                                          i18n("Duplicate Policy"),
                                          KGuiItem(i18n("Replace")));
      if ( res == KMessageBox::Continue )
      {
        m_pDomainPolicy[item]= KCookieAdvice::adviceToStr(advice);
        item->setText(0, domain);
        item->setText(1, i18n(m_pDomainPolicy[item]));
        configChanged();
        return true;
      }
      else
        return true;  // User Cancelled!!
    }
    item = item->nextSibling();
  }
  return false;
}

void KCookiesPolicies::deletePressed()
{
  Q3ListViewItem* nextItem = 0L;
  Q3ListViewItem* item = dlg->lvDomainPolicy->firstChild ();

  while (item != 0L)
  {
    if (dlg->lvDomainPolicy->isSelected (item))
    {
      nextItem = item->itemBelow();
      if ( !nextItem )
        nextItem = item->itemAbove();

      delete item;
      item = nextItem;
    }
    else
    {
      item = item->itemBelow();
    }
  }

  if (nextItem)
    dlg->lvDomainPolicy->setSelected (nextItem, true);

  updateButtons();
  configChanged();
}

void KCookiesPolicies::deleteAllPressed()
{
  m_pDomainPolicy.clear();
  dlg->lvDomainPolicy->clear();
  updateButtons();
  configChanged();
}

void KCookiesPolicies::updateButtons()
{
  bool hasItems = dlg->lvDomainPolicy->childCount() > 0;

  dlg->pbChange->setEnabled ((hasItems && d_itemsSelected == 1));
  dlg->pbDelete->setEnabled ((hasItems && d_itemsSelected > 0));
  dlg->pbDeleteAll->setEnabled ( hasItems );
}

void KCookiesPolicies::updateDomainList(const QStringList &domainConfig)
{
  dlg->lvDomainPolicy->clear();

  QStringList::ConstIterator it = domainConfig.begin();
  for (; it != domainConfig.end(); ++it)
  {
    QString domain;
    KCookieAdvice::Value advice = KCookieAdvice::Dunno;

    splitDomainAdvice(*it, domain, advice);

    if (!domain.isEmpty())
    {
        Q3ListViewItem* index = new Q3ListViewItem( dlg->lvDomainPolicy, KUrl::fromAce(domain.toLatin1()),
                                                  i18n(KCookieAdvice::adviceToStr(advice)) );
        m_pDomainPolicy[index] = KCookieAdvice::adviceToStr(advice);
    }
  }
}

void KCookiesPolicies::selectionChanged ()
{
  Q3ListViewItem* item = dlg->lvDomainPolicy->firstChild ();

  d_itemsSelected = 0;

  while (item != 0L)
  {
    if (dlg->lvDomainPolicy->isSelected (item))
      d_itemsSelected++;
    item = item->nextSibling ();
  }

  updateButtons ();
}

void KCookiesPolicies::load()
{
  d_itemsSelected = 0;
  d_configChanged = false;

  KConfig cfg ("kcookiejarrc");
  KConfigGroup group = cfg.group ("Cookie Policy");

  bool enableCookies = group.readEntry("Cookies", true);
  dlg->cbEnableCookies->setChecked (enableCookies);
  cookiesEnabled( enableCookies );

  KCookieAdvice::Value advice = KCookieAdvice::strToAdvice (group.readEntry(
                                               "CookieGlobalAdvice", "Ask"));
  switch (advice)
  {
    case KCookieAdvice::Accept:
      dlg->rbPolicyAccept->setChecked (true);
      break;
    case KCookieAdvice::Reject:
      dlg->rbPolicyReject->setChecked (true);
      break;
    case KCookieAdvice::Ask:
    case KCookieAdvice::Dunno:
    default:
      dlg->rbPolicyAsk->setChecked (true);
  }

  bool enable = group.readEntry("RejectCrossDomainCookies", true);
  dlg->cbRejectCrossDomainCookies->setChecked (enable);

  bool sessionCookies = group.readEntry("AcceptSessionCookies", true);
  dlg->cbAutoAcceptSessionCookies->setChecked (sessionCookies);
  bool cookieExpiration = group.readEntry("IgnoreExpirationDate", false);
  dlg->cbIgnoreCookieExpirationDate->setChecked (cookieExpiration);
  updateDomainList(group.readEntry("CookieDomainAdvice", QStringList()));

  if (enableCookies)
  {
    ignoreCookieExpirationDate( cookieExpiration );
    autoAcceptSessionCookies( sessionCookies );
    updateButtons();
  }
}

void KCookiesPolicies::save()
{
  // If nothing changed, ignore the save request.
  if (!d_configChanged)
    return;

  KConfig cfg ( "kcookiejarrc" );
  KConfigGroup group = cfg.group( "Cookie Policy" );

  bool state = dlg->cbEnableCookies->isChecked();
  group.writeEntry( "Cookies", state );
  state = dlg->cbRejectCrossDomainCookies->isChecked();
  group.writeEntry( "RejectCrossDomainCookies", state );
  state = dlg->cbAutoAcceptSessionCookies->isChecked();
  group.writeEntry( "AcceptSessionCookies", state );
  state = dlg->cbIgnoreCookieExpirationDate->isChecked();
  group.writeEntry( "IgnoreExpirationDate", state );

  QString advice;
  if (dlg->rbPolicyAccept->isChecked())
      advice = KCookieAdvice::adviceToStr(KCookieAdvice::Accept);
  else if (dlg->rbPolicyReject->isChecked())
      advice = KCookieAdvice::adviceToStr(KCookieAdvice::Reject);
  else
      advice = KCookieAdvice::adviceToStr(KCookieAdvice::Ask);

  group.writeEntry("CookieGlobalAdvice", advice);

  QStringList domainConfig;
  Q3ListViewItem *at = dlg->lvDomainPolicy->firstChild();

  while( at )
  {
    domainConfig.append(QString::fromLatin1("%1:%2").arg(QString(KUrl::toAce(at->text(0)))).arg(m_pDomainPolicy[at]));
    at = at->nextSibling();
  }

  group.writeEntry("CookieDomainAdvice", domainConfig);
  group.sync();

  // Update the cookiejar...
  if (!dlg->cbEnableCookies->isChecked())
  {
      QDBusInterface kded("org.kde.kded", "/modules/kcookiejar", "org.kde.KCookieServer", QDBusConnection::sessionBus());
      kded.call( "shutdown" );
  }
  else
  {
       QDBusInterface kded("org.kde.kded", "/modules/kcookiejar", "org.kde.KCookieServer", QDBusConnection::sessionBus());
       QDBusReply<void> reply = kded.call( "reloadPolicy" );
    if (!reply.isValid())
      KMessageBox::sorry(0, i18n("Unable to communicate with the cookie handler service.\n"
                                 "Any changes you made will not take effect until the service "
                                 "is restarted."));
  }

  // Force running io-slave to reload configurations...
  KSaveIOConfig::updateRunningIOSlaves (this);
  emit changed( false );
}


void KCookiesPolicies::defaults()
{
  dlg->cbEnableCookies->setChecked( true );
  dlg->rbPolicyAsk->setChecked( true );
  dlg->rbPolicyAccept->setChecked( false );
  dlg->rbPolicyReject->setChecked( false );
  dlg->cbRejectCrossDomainCookies->setChecked( true );
  dlg->cbAutoAcceptSessionCookies->setChecked( true );
  dlg->cbIgnoreCookieExpirationDate->setChecked( false );
  dlg->lvDomainPolicy->clear();

  cookiesEnabled( dlg->cbEnableCookies->isChecked() );
  updateButtons();
}

void KCookiesPolicies::splitDomainAdvice (const QString& cfg, QString &domain,
                                          KCookieAdvice::Value &advice)
{
  int sepPos = cfg.lastIndexOf(':');

  // Ignore any policy that does not contain a domain...
  if ( sepPos <= 0 )
    return;

  domain = cfg.left(sepPos);
  advice = KCookieAdvice::strToAdvice( cfg.mid( sepPos+1 ) );
}

QString KCookiesPolicies::quickHelp() const
{
  return i18n("<p><h1>Cookies</h1> Cookies contain information that Konqueror"
              " (or any other KDE application using the HTTP protocol) stores"
              " on your computer from a remote Internet server. This means"
              " that a web server can store information about you and your"
              " browsing activities on your machine for later use. You might"
              " consider this an invasion of privacy.</p><p>However, cookies are"
              " useful in certain situations. For example, they are often used"
              " by Internet shops, so you can 'put things into a shopping"
              " basket'. Some sites require you have a browser that supports"
              " cookies.</p><p>Because most people want a compromise between privacy"
              " and the benefits cookies offer, KDE offers you the ability to"
              " customize the way it handles cookies. You might, for example"
              " want to set KDE's default policy to ask you whenever a server"
              " wants to set a cookie or simply reject or accept everything."
              " For example, you might choose to accept all cookies from your"
              " favorite shopping web site. For this all you have to do is"
              " either browse to that particular site and when you are presented"
              " with the cookie dialog box, click on <i> This domain </i> under"
              " the 'apply to' tab and choose accept or simply specify the name"
              " of the site in the <i> Domain Specific Policy </i> tab and set"
              " it to accept. This enables you to receive cookies from trusted"
              " web sites without being asked every time KDE receives a cookie.</p>"
             );
}

#include "kcookiespolicies.moc"
