/*
   kmanualproxydlg.cpp - Proxy configuration dialog

   Copyright (C) 2001-2004 Dawit Alemayehu <adawit@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License (GPL) version 2 as published by the Free Software
   Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

// Own
#include "kmanualproxydlg.h"

// Qt
#include <QtGui/QCheckBox>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QPushButton>

// KDE
#include <kdebug.h>
#include <kiconloader.h>
#include <kicontheme.h>
#include <kinputdialog.h>
#include <kio/ioslave_defaults.h>
#include <klineedit.h>
#include <klistwidget.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <knuminput.h>
#include <kurifilter.h>



KManualProxyDlg::KManualProxyDlg( QWidget* parent, const char* name )
                :KProxyDialogBase( parent, name, true,
                                   i18n("Manual Proxy Configuration") )
{
    mDlg = new ManualProxyDlgUI (this);
    setMainWidget(mDlg);
    mDlg->pbCopyDown->setIcon( KIcon("go-down") );
    QSizePolicy sizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    sizePolicy.setHeightForWidth( mDlg->pbCopyDown->sizePolicy().hasHeightForWidth() );
    mDlg->pbCopyDown->setSizePolicy( sizePolicy );

    init();
}

void KManualProxyDlg::init()
{
    mDlg->sbHttp->setRange( 0, MAX_PORT_VALUE );
    mDlg->sbHttps->setRange( 0, MAX_PORT_VALUE );
    mDlg->sbFtp->setRange( 0, MAX_PORT_VALUE );

    connect( mDlg->pbNew, SIGNAL( clicked() ), SLOT( newPressed() ) );
    connect( mDlg->pbChange, SIGNAL( clicked() ), SLOT( changePressed() ) );
    connect( mDlg->pbDelete, SIGNAL( clicked() ), SLOT( deletePressed() ) );
    connect( mDlg->pbDeleteAll, SIGNAL( clicked() ), SLOT( deleteAllPressed() ) );

    connect( mDlg->lbExceptions, SIGNAL(itemSelectionChanged()), SLOT(updateButtons()) );
    connect( mDlg->lbExceptions, SIGNAL(itemDoubleClicked (QListWidgetItem *)), SLOT(changePressed()));

    connect( mDlg->cbSameProxy, SIGNAL( toggled(bool) ), SLOT( sameProxy(bool) ) );
    connect( mDlg->pbCopyDown, SIGNAL( clicked() ), SLOT( copyDown() ) );

    connect( mDlg->leHttp, SIGNAL(textChanged(const QString&)), SLOT(textChanged(const QString&)) );
    connect( mDlg->sbHttp, SIGNAL(valueChanged(int)), SLOT(valueChanged (int)) );

    connect( this, SIGNAL(okClicked()), this, SLOT(slotOk()));
}

void KManualProxyDlg::setProxyData( const KProxyData &data )
{
    KUrl url;

    // Set the HTTP proxy...
    if (!isValidURL(data.proxyList["http"], &url))
        mDlg->sbHttp->setValue( DEFAULT_PROXY_PORT );
    else
    {
        int port = url.port();
        if ( port <= 0 )
            port = DEFAULT_PROXY_PORT;

        url.setPort( -1 );
        mDlg->leHttp->setText( url.url() );
        mDlg->sbHttp->setValue( port );
    }

    bool useSameProxy = (!mDlg->leHttp->text().isEmpty () &&
                         data.proxyList["http"] == data.proxyList["https"] &&
                         data.proxyList["http"] == data.proxyList["ftp"]);

    mDlg->cbSameProxy->setChecked ( useSameProxy );

    if ( useSameProxy )
    {
      mDlg->leHttps->setText ( mDlg->leHttp->text() );
      mDlg->leFtp->setText ( mDlg->leHttp->text() );
      mDlg->sbHttps->setValue( mDlg->sbHttp->value() );
      mDlg->sbFtp->setValue( mDlg->sbHttp->value() );

      sameProxy ( true );
    }
    else
    {
      // Set the HTTPS proxy...
      if( !isValidURL( data.proxyList["https"], &url ) )
          mDlg->sbHttps->setValue( DEFAULT_PROXY_PORT );
      else
      {
          int port = url.port();
          if ( port <= 0 )
              port = DEFAULT_PROXY_PORT;

          url.setPort( -1 );
          mDlg->leHttps->setText( url.url() );
          mDlg->sbHttps->setValue( port );
      }

      // Set the FTP proxy...
      if( !isValidURL( data.proxyList["ftp"], &url ) )
          mDlg->sbFtp->setValue( DEFAULT_PROXY_PORT );
      else
      {
          int port = url.port();
          if ( port <= 0 )
              port = DEFAULT_PROXY_PORT;

          url.setPort( -1 );
          mDlg->leFtp->setText( url.url() );
          mDlg->sbFtp->setValue( port );
      }
    }

    QStringList::ConstIterator it = data.noProxyFor.begin();
    for( ; it != data.noProxyFor.end(); ++it )
    {
      // "no_proxy" is a keyword used by the environment variable
      // based configuration. We ignore it here as it is not applicable...
      if ((*it).toLower() != "no_proxy" && !(*it).isEmpty())
      {
        // Validate the NOPROXYFOR entries and use only hostnames if the entry is
        // a valid or legitimate URL. NOTE: needed to catch manual manipulation
        // of the proxy config files...
        if( isValidURL( *it ) || ((*it).length() >= 3 && (*it).startsWith('.')) )
          mDlg->lbExceptions->addItem( *it );
      }
    }

    mDlg->cbReverseProxy->setChecked( data.useReverseProxy );
}

const KProxyData KManualProxyDlg::data() const
{
    KProxyData data;

    if (!m_bHasValidData)
      return data;

    data.proxyList["http"] = urlFromInput( mDlg->leHttp, mDlg->sbHttp );

    if ( mDlg->cbSameProxy->isChecked () )
    {
        data.proxyList["https"] = data.proxyList["http"];
        data.proxyList["ftp"] = data.proxyList["http"];
    }
    else
    {
        data.proxyList["https"] = urlFromInput( mDlg->leHttps, mDlg->sbHttps );
        data.proxyList["ftp"] = urlFromInput( mDlg->leFtp, mDlg->sbFtp );
    }

    if ( mDlg->lbExceptions->count() )
    {
        for ( int rowIndex = 0 ; rowIndex < mDlg->lbExceptions->count() ; rowIndex++ ) {
            data.noProxyFor << mDlg->lbExceptions->item(rowIndex)->text();
        }
    }

    data.type = KProtocolManager::ManualProxy;
    data.useReverseProxy = mDlg->cbReverseProxy->isChecked();

    return data;
}

void KManualProxyDlg::sameProxy( bool enable )
{
    mDlg->leHttps->setEnabled (!enable );
    mDlg->leFtp->setEnabled (!enable );
    mDlg->sbHttps->setEnabled (!enable );
    mDlg->sbFtp->setEnabled (!enable );
    mDlg->pbCopyDown->setEnabled( !enable );

    if (enable)
    {
      mOldFtpText = mDlg->leFtp->text();
      mOldHttpsText = mDlg->leHttps->text();

      mOldFtpPort = mDlg->sbFtp->value();
      mOldHttpsPort = mDlg->sbHttps->value();

      int port = mDlg->sbHttp->value();
      QString text = mDlg->leHttp->text();

      mDlg->leFtp->setText (text);
      mDlg->leHttps->setText (text);

      mDlg->sbFtp->setValue (port);
      mDlg->sbHttps->setValue (port);

      if (mDlg->lbHttps->font().bold())
        setHighLight( mDlg->lbHttps, false );

      if (mDlg->lbFtp->font().bold())
        setHighLight( mDlg->lbFtp, false );
    }
    else
    {
      mDlg->leFtp->setText (mOldFtpText);
      mDlg->leHttps->setText (mOldHttpsText);

      mDlg->sbFtp->setValue (mOldFtpPort);
      mDlg->sbHttps->setValue (mOldHttpsPort);
    }
}

bool KManualProxyDlg::validate()
{
    KUrl filteredURL;
    unsigned short count = 0;

    if ( isValidURL( mDlg->leHttp->text(), &filteredURL ) )
    {
        mDlg->leHttp->setText( filteredURL.url() );
        count++;
    }
    else
        setHighLight( mDlg->lbHttp, true );

    if ( !mDlg->cbSameProxy->isChecked () )
    {
        if ( isValidURL( mDlg->leHttps->text(), &filteredURL ) )
        {
            mDlg->leHttps->setText( filteredURL.url() );
            count++;
        }
        else
            setHighLight( mDlg->lbHttps, true );

        if ( isValidURL( mDlg->leFtp->text(), &filteredURL ) )
        {
            mDlg->leFtp->setText( filteredURL.url() );
            count++;
        }
        else
            setHighLight( mDlg->lbFtp, true );
    }

    if ( count == 0 )
    {
        showErrorMsg( i18n("Invalid Proxy Setting"),
                      i18n("One or more of the specified proxy settings are "
                           "invalid. The incorrect entries are highlighted.") );
    }

    return (count > 0);
}

void KManualProxyDlg::textChanged(const QString& text)
{
    if (!mDlg->cbSameProxy->isChecked())
        return;

    mDlg->leFtp->setText( text );
    mDlg->leHttps->setText( text );
}

void KManualProxyDlg::valueChanged(int value)
{
    if (!mDlg->cbSameProxy->isChecked())
        return;

    mDlg->sbFtp->setValue (value);
    mDlg->sbHttps->setValue (value);
 }

void KManualProxyDlg::copyDown()
{
    int action = -1;

    if ( !mDlg->leHttp->text().isEmpty() )
        action += 4;
    else if ( !mDlg->leHttps->text().isEmpty() )
        action += 3;

    switch ( action )
    {
      case 3:
        mDlg->leHttps->setText( mDlg->leHttp->text() );
        mDlg->sbHttps->setValue( mDlg->sbHttp->value() );
        mDlg->leFtp->setText( mDlg->leHttp->text() );
        mDlg->sbFtp->setValue( mDlg->sbHttp->value() );
        break;
      case 2:
        mDlg->leFtp->setText( mDlg->leHttps->text() );
        mDlg->sbFtp->setValue( mDlg->sbHttps->value() );
        break;
      case 1:
      case 0:
      default:
        break;
    }
}

void KManualProxyDlg::slotOk()
{
    if ( m_bHasValidData || validate() )
      m_bHasValidData = true;
}

bool KManualProxyDlg::handleDuplicate( const QString& site )
{
    for ( int rowIndex = 0 ; rowIndex < mDlg->lbExceptions->count() ; rowIndex++ )
    {
        QListWidgetItem* item = mDlg->lbExceptions->item(rowIndex);

        if ( item->text().lastIndexOf( site ) != -1 &&
             item != mDlg->lbExceptions->currentItem() )
        {
            QString msg = i18n("You entered a duplicate address. "
                               "Please try again.");
            QString details = i18n("<qt><center><b>%1</b></center> "
                                   "is already in the list.</qt>", site);
            KMessageBox::detailedError( this, msg, details, i18n("Duplicate Entry") );
            return true;
        }
    }
    return false;
}

void KManualProxyDlg::newPressed()
{
  QString result;
  if( getException(result, i18n("New Exception")) && !handleDuplicate(result) )
    mDlg->lbExceptions->addItem( result );
}

void KManualProxyDlg::changePressed()
{
  QString result;
  if( getException( result, i18n("Change Exception"),
                    mDlg->lbExceptions->currentItem()->text() ) &&
      !handleDuplicate( result ) ) 
      mDlg->lbExceptions->currentItem()->setText(result); 
}

void KManualProxyDlg::deletePressed()
{
    delete mDlg->lbExceptions->takeItem( mDlg->lbExceptions->currentRow() );
    if(mDlg->lbExceptions->currentItem())
       mDlg->lbExceptions->currentItem()->setSelected(true); 
    updateButtons();
}

void KManualProxyDlg::deleteAllPressed()
{
    mDlg->lbExceptions->clear();
    updateButtons();
}

void KManualProxyDlg::updateButtons()
{
    bool hasItems = mDlg->lbExceptions->count() > 0;
    bool itemSelected = (hasItems && mDlg->lbExceptions->currentItem()!=0);

    mDlg->pbDeleteAll->setEnabled( hasItems );
    mDlg->pbDelete->setEnabled( itemSelected );
    mDlg->pbChange->setEnabled( itemSelected );
}

QString KManualProxyDlg::urlFromInput(const KLineEdit* edit,
                                      const KIntSpinBox* spin) const
{
  if (!edit)
    return QString();

  KUrl u( edit->text() );

  if (spin)
    u.setPort( spin->value() );

  return u.url();
}

bool KManualProxyDlg::isValidURL( const QString& _url, KUrl* result ) const
{
    KUrl url (_url);

    QStringList filters;
    filters << "kshorturifilter" << "localdomainurifilter";

    // If the typed URL is malformed, and the filters cannot filter it
    // then it must be an invalid entry.
    if( !(url.isValid() && KUriFilter::self()->filterUri(url, filters) &&
        url.hasHost()) )
      return false;

    QString host (url.host());

    // We only check for a relevant subset of characters that are
    // not allowed in <authority> component of a URL.
    if ( host.contains ('*') || host.contains (' ') || host.contains ('?') )
      return false;

    if ( result )
      *result = url;

    return true;
}

void KManualProxyDlg::showErrorMsg( const QString& caption,
                                    const QString& message )
{
  QString cap( caption );
  QString msg( message );

  if ( cap.isEmpty() )
    cap = i18n("Invalid Entry");

  if ( msg.isEmpty() )
    msg = i18n("The address you have entered is not valid.");

  QString details = i18n("<qt>Make sure none of the addresses or URLs you "
                          "specified contain invalid or wildcard characters "
                          "such as spaces, asterisks (*), or question marks(?).<br /><br />"
                          "<u>Examples of VALID entries:</u><br />"
                          "<code>http://mycompany.com, 192.168.10.1, "
                          "mycompany.com, localhost, http://localhost</code><br /><br />"
                          "<u>Examples of INVALID entries:</u><br />"
                          "<code>http://my company.com, http:/mycompany,com "
                          "file:/localhost</code></qt>");

  KMessageBox::detailedError( this, msg, details, cap );
}

bool KManualProxyDlg::getException ( QString& result,
                                     const QString& caption,
                                     const QString& value )
{
    QString label;

    // Specify the appropriate message...
    if ( mDlg->cbReverseProxy->isChecked() )
      label = i18n("Enter the URL or address that should use the above proxy "
                   "settings:");
    else
      label = i18n("Enter the address or URL that should be excluded from "
                   "using the above proxy settings:");

    QString whatsThis = i18n("<qt>Enter a valid address or URL.<br /><br />"
                            "<b><u>NOTE:</u></b> Wildcard matching such as "
                            "<code>*.kde.org</code> is not supported. If you want "
                            "to match any host in the <code>.kde.org</code> domain, "
                            "e.g. <code>printing.kde.org</code>, then simply enter "
                            "<code>.kde.org</code>.</qt>");

    bool ok;
    result = KInputDialog::getText( caption, label, value, &ok, this,
                                    0, QString(), whatsThis );

    // If the user pressed cancel, do nothing...
    if (!ok)
      return false;

    // If the typed URL is malformed, and the filters cannot filter it
    // then it must be an invalid entry,
    if( isValidURL(result) || (result.length() >= 3 && result.startsWith('.')))	
      return true;

    showErrorMsg();
    return false;
}

#include "kmanualproxydlg.moc"
