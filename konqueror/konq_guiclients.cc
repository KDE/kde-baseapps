/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Simon Hausmann <hausmann@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <kaction.h>
#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmenubar.h>
#include <konq_view.h>
#include <konq_factory.h>
#include <konq_frame.h>
#include <konq_guiclients.h>
#include <konq_mainwindow.h>
#include <konq_viewmgr.h>

PopupMenuGUIClient::PopupMenuGUIClient( KonqMainWindow *mainWindow, const KTrader::OfferList &embeddingServices )
{
  m_mainWindow = mainWindow;

  m_doc = QDomDocument( "kpartgui" );
  QDomElement root = m_doc.createElement( "kpartgui" );
  root.setAttribute( "name", "konqueror" );
  m_doc.appendChild( root );

  QDomElement menu = m_doc.createElement( "Menu" );
  root.appendChild( menu );
  menu.setAttribute( "name", "popupmenu" );

  if ( !mainWindow->menuBar()->isVisible() )
  {
    QDomElement showMenuBarElement = m_doc.createElement( "action" );
    showMenuBarElement.setAttribute( "name", "showmenubar" );
    menu.appendChild( showMenuBarElement );

    menu.appendChild( m_doc.createElement( "separator" ) );
  }

  if ( mainWindow->fullScreenMode() )
  {
    QDomElement stopFullScreenElement = m_doc.createElement( "action" );
    stopFullScreenElement.setAttribute( "name", "fullscreen" );
    menu.appendChild( stopFullScreenElement );

    menu.appendChild( m_doc.createElement( "separator" ) );
  }

  QString currentServiceName = mainWindow->currentView()->service()->name();

  KTrader::OfferList::ConstIterator it = embeddingServices.begin();
  KTrader::OfferList::ConstIterator end = embeddingServices.end();

  QVariant builtin;
  if ( embeddingServices.count() == 1 )
  {
    KService::Ptr service = *embeddingServices.begin();
    builtin = service->property( "X-KDE-BrowserView-HideFromMenus" );
    if ( ( !builtin.isValid() || !builtin.toBool() ) &&
	 service->name() != currentServiceName )
      addEmbeddingService( menu, 0, i18n( "Preview in %1" ).arg( service->comment() ), service );
  }
  else if ( embeddingServices.count() > 1 )
  {
    int idx = 0;
    QDomElement subMenu = m_doc.createElement( "menu" );
    menu.appendChild( subMenu );
    QDomElement text = m_doc.createElement( "text" );
    subMenu.appendChild( text );
    text.appendChild( m_doc.createTextNode( i18n( "Preview in" ) ) );
    subMenu.setAttribute( "group", "preview" );

    bool inserted = false;

    for (; it != end; ++it )
    {
      builtin = (*it)->property( "X-KDE-BrowserView-HideFromMenus" );
      if ( ( !builtin.isValid() || !builtin.toBool() ) &&
       (*it)->name() != currentServiceName )
      {
        addEmbeddingService( subMenu, idx++, (*it)->comment(), *it );
	inserted = true;
      }
    }

    if ( !inserted ) // oops, if empty then remove the menu :-]
      menu.removeChild( menu.namedItem( "menu" ) );
  }

  setDOMDocument( m_doc );
}

PopupMenuGUIClient::~PopupMenuGUIClient()
{
}

KAction *PopupMenuGUIClient::action( const QDomElement &element ) const
{
  KAction *res = KXMLGUIClient::action( element );

  if ( !res )
    res = m_mainWindow->action( element );

  return res;
}

void PopupMenuGUIClient::addEmbeddingService( QDomElement &menu, int idx, const QString &name, const KService::Ptr &service )
{
  QDomElement action = m_doc.createElement( "action" );
  menu.appendChild( action );

  QCString actName;
  actName.setNum( idx );

  action.setAttribute( "name", QString::number( idx ) );

  action.setAttribute( "group", "preview" );

  (void)new KAction( name, service->pixmap( KIcon::Small ), 0,
		     m_mainWindow, SLOT( slotOpenEmbedded() ), actionCollection(), actName );
}

ToggleViewGUIClient::ToggleViewGUIClient( KonqMainWindow *mainWindow )
: QObject( mainWindow )
{
  m_mainWindow = mainWindow;
  m_actions.setAutoDelete( true );

  KTrader::OfferList offers = KTrader::self()->query( "Browser/View" );
  KTrader::OfferList::Iterator it = offers.begin();
  while ( it != offers.end() )
  {
    QVariant prop = (*it)->property( "X-KDE-BrowserView-Toggable" );
    QVariant orientation = (*it)->property( "X-KDE-BrowserView-ToggableView-Orientation" );

    if ( !prop.isValid() || !prop.toBool() ||
	 !orientation.isValid() || orientation.toString().isEmpty() )
    {
      offers.remove( it );
      it = offers.begin();
    }
    else
      ++it;
  }

  m_empty = ( offers.count() == 0 );

  if ( m_empty )
    return;

  KTrader::OfferList::ConstIterator cIt = offers.begin();
  KTrader::OfferList::ConstIterator cEnd = offers.end();
  for (; cIt != cEnd; ++cIt )
  {
    QString description = i18n( "Show %1" ).arg( (*cIt)->comment() );
    QString name = (*cIt)->name();
    //kdDebug(1202) << "ToggleViewGUIClient: name=" << name << endl;
    KToggleAction *action = new KToggleAction( description, 0, 0, name.latin1() );

    // HACK
    if ( (*cIt)->icon() != "unknown" )
      action->setIcon( (*cIt)->icon() );

    connect( action, SIGNAL( toggled( bool ) ),
	     this, SLOT( slotToggleView( bool ) ) );

    m_actions.insert( name, action );

    QVariant orientation = (*cIt)->property( "X-KDE-BrowserView-ToggableView-Orientation" );
    bool horizontal = orientation.toString().lower() == "horizontal";
    m_mapOrientation.insert( name, horizontal );
  }

  connect( m_mainWindow, SIGNAL( viewAdded( KonqView * ) ),
	   this, SLOT( slotViewAdded( KonqView * ) ) );
  connect( m_mainWindow, SIGNAL( viewRemoved( KonqView * ) ),
	   this, SLOT( slotViewRemoved( KonqView * ) ) );
}

ToggleViewGUIClient::~ToggleViewGUIClient()
{
}

QList<KAction> ToggleViewGUIClient::actions() const
{
  QList<KAction> res;

  QDictIterator<KAction> it( m_actions );
  for (; it.current(); ++it )
    res.append( it.current() );

  return res;
}

void ToggleViewGUIClient::slotToggleView( bool toggle )
{
  QString serviceName = QString::fromLatin1( sender()->name() );

  bool horizontal = m_mapOrientation[ serviceName ];

  KonqViewManager *viewManager = m_mainWindow->viewManager();

  KonqFrameContainer *mainContainer = viewManager->mainContainer();

  if ( toggle )
  {
    // This should be probably merged with KonqViewManager::splitWindow

    KonqFrameBase *splitFrame = mainContainer ? mainContainer->firstChild() : 0L;

    KonqFrameContainer *newContainer;

    KonqView *childView = viewManager->split( splitFrame, horizontal ? Qt::Vertical : Qt::Horizontal,
                                                   QString::fromLatin1( "Browser/View" ), serviceName, &newContainer );

    if ( !horizontal )
    {
      if (!splitFrame)
        kdWarning(1202) << "No split frame !" << endl;
      else
      {
        //kdDebug(1202) << "Swapping" << endl;
        newContainer->moveToLast( splitFrame->widget() );
        newContainer->swapChildren();
      }
    }

    QValueList<int> newSplitterSizes;

    if ( horizontal )
      newSplitterSizes << 100 << 30;
    else
      newSplitterSizes << 30 << 100;

    newContainer->setSizes( newSplitterSizes );

    if ( m_mainWindow->currentView() )
    {
        childView->setLocationBarURL( m_mainWindow->currentView()->url().prettyURL() ); // default one in case it doesn't set it
        childView->openURL( m_mainWindow->currentView()->url() );
    }

    // If not passive, set as active :)
    if (!childView->passiveMode())
      //viewManager->setActivePart( view );
      childView->part()->widget()->setFocus();

  }
  else
  {
    QList<KonqView> viewList;

    mainContainer->listViews( &viewList );

    QListIterator<KonqView> it( viewList );
    for (; it.current(); ++it )
      if ( it.current()->service()->name() == serviceName )
        // takes care of choosing the new active view
        viewManager->removeView( it.current() );
  }

}
void ToggleViewGUIClient::saveConfig( bool add, const QString &serviceName )
{
  // The current approach is : save this setting as soon as it is changed
  // (This obeys to "no 'Save settings' menu item approach in the Style Guide")
  // I'm on the safe side, this way: whoever doesn't agree has to discuss
  // with the style guide authors, not with me ;-)     (David)
  KConfig *config = KonqFactory::instance()->config();
  KConfigGroupSaver cgs( config, "MainView Settings" );
  QStringList toggableViewsShown = config->readListEntry( "ToggableViewsShown" );
  if (add)
  {
      if ( !toggableViewsShown.contains( serviceName ) )
          toggableViewsShown.append(serviceName);
  }
  else
      toggableViewsShown.remove(serviceName);
  config->writeEntry( "ToggableViewsShown", toggableViewsShown );
}

void ToggleViewGUIClient::slotViewAdded( KonqView *view )
{
  QString name = view->service()->name();

  KAction *action = m_actions[ name ];

  if ( action )
  {
    static_cast<KToggleAction *>( action )->setChecked( true );
    saveConfig( true, name );
  }
}

void ToggleViewGUIClient::slotViewRemoved( KonqView *view )
{
  QString name = view->service()->name();

  KAction *action = m_actions[ name ];

  if ( action )
  {
    static_cast<KToggleAction *>( action )->setChecked( false );
    saveConfig( false, name );
  }
}

#include "konq_guiclients.moc"
