/*  This file is part of the KDE project
    Copyright (C) 1998, 1999 Michael Reiher <michael.reiher@gmx.de>
    Copyright 2007 David Faure <faure@kde.org>

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "konqframestatusbar.h"
#include <kdebug.h>
#include "konqframe.h"
#include "konqview.h"
#include <kiconloader.h>
#include <QCheckBox>
#include <QPainter>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <ksqueezedtextlabel.h>
#include <klocale.h>
#include <kactioncollection.h>

static QPixmap statusBarIcon(const char* name)
{
    return KIconLoader::global()->loadIcon(QLatin1String(name),
                                           KIconLoader::User,
                                           KIconLoader::SizeSmall);
}

/**
 * A CheckBox with a special paintEvent(). It looks like the
 * unchecked radiobutton in b2k style if unchecked and contains a little
 * anchor if checked.
 */
class KonqCheckBox : public QCheckBox
{
    //Q_OBJECT // for classname. not used, and needs a moc
public:
    explicit KonqCheckBox(QWidget *parent=0)
      : QCheckBox( parent ) {}
protected:
    void paintEvent( QPaintEvent * );

    QSize sizeHint() const
    {
        QSize size = connectPixmap().size();
        // Add some room around the pixmap. Makes it a bit easier to click and
        // ensure it does not look crowded with styles which draw a frame
        // around statusbar items.
        size.rwidth() += 4;
        return size;
    }

private:
    const QPixmap& connectPixmap() const
    {
        static QPixmap indicator_connect(statusBarIcon("indicator_connect"));
        return indicator_connect;
    }

    const QPixmap& noConnectPixmap() const
    {
        static QPixmap indicator_noconnect(statusBarIcon("indicator_noconnect"));
        return indicator_noconnect;
    }
};

#define DEFAULT_HEADER_HEIGHT 13

void KonqCheckBox::paintEvent( QPaintEvent * )
{
    QPainter p(this);

    const QPixmap &pixmap = (isChecked() || isDown()) ? connectPixmap() : noConnectPixmap();
    p.drawPixmap(
        (width() - pixmap.width()) / 2,
        (height() - pixmap.height()) / 2,
        pixmap);
}

KonqFrameStatusBar::KonqFrameStatusBar( KonqFrame *_parent )
  : KStatusBar( _parent ),
    m_pParentKonqFrame( _parent )
{
    setSizeGripEnabled( false );

    // TODO remove active view indicator and use a different bg color like dolphin does?
    // Works nicely for file management, but not so much with other parts...
    m_led = new QLabel( this );
    m_led->setAlignment( Qt::AlignCenter );
    m_led->setSizePolicy(QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed ));
    addWidget( m_led, 0 ); // led (active view indicator)
    m_led->hide();

    // TODO re-enable squeezing
    //m_pStatusLabel = new KSqueezedTextLabel( this );
    m_pStatusLabel = new KonqStatusBarMessageLabel( this );
    m_pStatusLabel->installEventFilter(this);
    addWidget( m_pStatusLabel, 1 /*stretch*/ ); // status label

    m_pLinkedViewCheckBox = new KonqCheckBox( this );
    m_pLinkedViewCheckBox->setObjectName( QLatin1String( "m_pLinkedViewCheckBox" ) );
    m_pLinkedViewCheckBox->setFocusPolicy(Qt::NoFocus);
    m_pLinkedViewCheckBox->setSizePolicy(QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Minimum ));
    m_pLinkedViewCheckBox->setWhatsThis( i18n("Checking this box on at least two views sets those views as 'linked'. "
                          "Then, when you change directories in one view, the other views "
                          "linked with it will automatically update to show the current directory. "
                          "This is especially useful with different types of views, such as a "
                          "directory tree with an icon view or detailed view, and possibly a "
                          "terminal emulator window." ) );
    addPermanentWidget( m_pLinkedViewCheckBox, 0 );
    connect( m_pLinkedViewCheckBox, SIGNAL(toggled(bool)),
            this, SIGNAL(linkedViewClicked(bool)) );

    m_progressBar = new QProgressBar( this );
    // Minimum width of QProgressBar::sizeHint depends on PM_ProgressBarChunkWidth which doesn't make sense;
    // we don't want chunking but we want a reasonably-sized progressbar :)
    m_progressBar->setMinimumWidth(150);
    m_progressBar->setMaximumHeight(fontMetrics().height());
    m_progressBar->hide();
    addPermanentWidget( m_progressBar, 0 );

    installEventFilter( this );
}

KonqFrameStatusBar::~KonqFrameStatusBar()
{
}

// I don't think this code _ever_ gets called!
// I don't want to remove it, though.  :-)
void KonqFrameStatusBar::mousePressEvent( QMouseEvent* event )
{
   QWidget::mousePressEvent( event );
   if ( !m_pParentKonqFrame->childView()->isPassiveMode() )
   {
      emit clicked();
      update();
   }

   //Blocks menu of custom status bar entries
   //if (event->button()==RightButton)
   //   splitFrameMenu();
}

void KonqFrameStatusBar::splitFrameMenu()
{
   KonqMainWindow * mw = m_pParentKonqFrame->childView()->mainWindow();

   // We have to ship the remove view action ourselves,
   // since this may not be the active view (passive view)
   KAction actRemoveView(KIcon("view-close"), i18n("Close View"), 0);
   actRemoveView.setObjectName( QLatin1String("removethisview" ));
   connect(&actRemoveView, SIGNAL(triggered(bool)), m_pParentKonqFrame, SLOT(slotRemoveView()));
   actRemoveView.setEnabled( mw->mainViewsCount() > 1 || m_pParentKonqFrame->childView()->isToggleView() || m_pParentKonqFrame->childView()->isPassiveMode() );

   // For the rest, we borrow them from the main window
   // ###### might be not right for passive views !
   KActionCollection *actionColl = mw->actionCollection();

   QMenu menu;

   menu.addAction( actionColl->action( "splitviewh" ) );
   menu.addAction( actionColl->action( "splitviewv" ) );
   menu.addSeparator();
   menu.addAction( actionColl->action( "lock" ) );
   menu.addAction( &actRemoveView );

   menu.exec(QCursor::pos());
}

bool KonqFrameStatusBar::eventFilter(QObject* o, QEvent *e)
{
   if (o == m_pStatusLabel && e->type()==QEvent::MouseButtonPress)
   {
      emit clicked();
      update();
      if ( static_cast<QMouseEvent *>(e)->button() == Qt::RightButton)
         splitFrameMenu();
      return true;
   }
   else if ( o == this && e->type() == QEvent::ApplicationPaletteChange )
   {
      //unsetPalette();
      setPalette(QPalette());
      updateActiveStatus();
      return true;
   }

   return KStatusBar::eventFilter(o, e);
}

void KonqFrameStatusBar::message( const QString &msg )
{
    // We don't use the message()/clear() mechanism of QStatusBar because
    // it really looks ugly (the label border goes away, the active-view indicator
    // is hidden...)
    QString saveMsg = m_savedMessage;
    slotDisplayStatusText( msg );
    m_savedMessage = saveMsg;
}

void KonqFrameStatusBar::slotDisplayStatusText(const QString& text)
{
    //kDebug() << text;
    m_pStatusLabel->setMessage(text, KonqStatusBarMessageLabel::Default);
    m_savedMessage = text;
}

// ### TODO: was also used in kde3 for the signals from kactioncollection...
void KonqFrameStatusBar::slotClear()
{
    slotDisplayStatusText( m_savedMessage );
}

void KonqFrameStatusBar::slotLoadingProgress( int percent )
{
    if (percent == -1 || percent == 100) { // hide on error and on success
        m_progressBar->hide();
    } else {
        m_progressBar->show();
    }

    m_progressBar->setValue(percent);
}

void KonqFrameStatusBar::slotSpeedProgress( int bytesPerSecond )
{
  QString sizeStr;

  if ( bytesPerSecond > 0 )
    sizeStr = i18n( "%1/s", KIO::convertSize( bytesPerSecond ) );
  else
    sizeStr = i18n( "Stalled" );

  slotDisplayStatusText( sizeStr ); // let's share the same label...
}

void KonqFrameStatusBar::slotConnectToNewView(KonqView *, KParts::ReadOnlyPart *,KParts::ReadOnlyPart *newOne)
{
   if (newOne)
      connect(newOne,SIGNAL(setStatusBarText(QString)),this,SLOT(slotDisplayStatusText(QString)));
   slotDisplayStatusText( QString() );
}

void KonqFrameStatusBar::showActiveViewIndicator( bool b )
{
    m_led->setVisible( b );
    updateActiveStatus();
}

void KonqFrameStatusBar::showLinkedViewIndicator( bool b )
{
    m_pLinkedViewCheckBox->setVisible( b );
}

void KonqFrameStatusBar::setLinkedView( bool b )
{
    m_pLinkedViewCheckBox->blockSignals( true );
    m_pLinkedViewCheckBox->setChecked( b );
    m_pLinkedViewCheckBox->blockSignals( false );
}

void KonqFrameStatusBar::updateActiveStatus()
{
    if ( m_led->isHidden() )
    {
        //unsetPalette();
        setPalette(QPalette());
        return;
    }

    bool hasFocus = m_pParentKonqFrame->isActivePart();

    const QColor midLight = palette().midlight().color();
    const QColor Mid = palette().mid().color();
    QPalette palette;
    palette.setColor(backgroundRole(), hasFocus ? midLight : Mid);
    setPalette(palette);

    static QPixmap indicator_viewactive(statusBarIcon("indicator_viewactive"));
    static QPixmap indicator_empty(statusBarIcon("indicator_empty"));
    m_led->setPixmap( hasFocus ? indicator_viewactive : indicator_empty );
}

void KonqFrameStatusBar::setMessage(const QString& msg, KonqStatusBarMessageLabel::Type type)
{
    m_pStatusLabel->setMessage(msg, type);
}

#include "konqframestatusbar.moc"
