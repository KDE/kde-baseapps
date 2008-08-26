/*  This file is part of the KDE project

    Copyright (C) 2002-2003 Konqueror Developers <konq-e@kde.org>
    Copyright (C) 2002-2003 Douglas Hanley <douglash@caltech.edu>

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA  02110-1301, USA.
*/

#ifndef KONQTABS_H
#define KONQTABS_H

#include "konqframe.h"
#include "konqframecontainer.h"

#include <ktabwidget.h>
#include <QtGui/QKeyEvent>
#include <QtCore/QList>

class QMenu;
class QToolButton;

class KonqView;
class KonqViewManager;
class KonqFrameContainerBase;
class KonqFrameContainer;
class KonqTabsStyle;
class KConfig;

class KonqFrameTabs : public KTabWidget, public KonqFrameContainerBase
{
  Q_OBJECT
  friend class KonqFrame; //for emitting ctrlTabPressed() only, aleXXX

public:
  KonqFrameTabs(QWidget* parent, KonqFrameContainerBase* parentContainer,
		KonqViewManager* viewManager);
  virtual ~KonqFrameTabs();

    virtual bool accept( KonqFrameVisitor* visitor );

  virtual void saveConfig( KConfigGroup& config, const QString &prefix, const KonqFrameBase::Options &options,
			   KonqFrameBase* docContainer, int id = 0, int depth = 0 );
  virtual void copyHistory( KonqFrameBase *other );

  const QList<KonqFrameBase*>& childFrameList() const { return m_childFrameList; }

  virtual void setTitle( const QString &title, QWidget* sender );
  virtual void setTabIcon( const KUrl &url, QWidget* sender );

  virtual QWidget* asQWidget() { return this; }
  virtual QByteArray frameType() { return QByteArray("Tabs"); }

  void activateChild();

    /**
     * Insert a new frame into the container.
     */
    void insertChildFrame(KonqFrameBase * frame, int index = -1);

    /**
     * Call this before deleting one of our children.
     */
    void childFrameRemoved( KonqFrameBase * frame );

    virtual void replaceChildFrame(KonqFrameBase* oldFrame, KonqFrameBase* newFrame);

  void moveTabBackward(int index);
  void moveTabForward(int index);

  void setLoading(KonqFrameBase* frame, bool loading);
  
  /**
   * Returns the tab that contains (directly or indirectly) the frame @p frame,
   * or 0 if the frame is not in the tab widget.
   */
  KonqFrameBase* tabContaining(KonqFrameBase* frame) const;

public Q_SLOTS:
  void slotCurrentChanged( QWidget* newPage );
  void setAlwaysTabbedMode( bool );

Q_SIGNALS:
  void ctrlTabPressed();
  void removeTabPopup();
    void openUrl(KonqView* view, const KUrl& url);

private:
  void refreshSubPopupMenuTab();
    void updateTabBarVisibility();
    void initPopupMenu();
   /**
    * Returns the index position of the tab where the frame @p frame is, assuming that
    * it's the active frame in that tab,
    * or -1 if the frame is not in the tab widget or it's not active.
    */
    int tabWhereActive(KonqFrameBase* frame) const;

private Q_SLOTS:
  void slotContextMenu( const QPoint& );
  void slotContextMenu( QWidget*, const QPoint& );
  void slotCloseRequest( QWidget* );
  void slotMovedTab( int, int );
  void slotMouseMiddleClick();
  void slotMouseMiddleClick( QWidget* );

  void slotTestCanDecode(const QDragMoveEvent *e, bool &accept /* result */);
  void slotReceivedDropEvent( QDropEvent* );
  void slotInitiateDrag( QWidget * );
  void slotReceivedDropEvent( QWidget *, QDropEvent * );
  void slotSubPopupMenuTabActivated( int );

private:
  QList<KonqFrameBase*> m_childFrameList;

  KonqViewManager* m_pViewManager;
  QMenu* m_pPopupMenu;
  QMenu* m_pSubPopupMenuTab;
  QToolButton* m_rightWidget;
  QToolButton* m_leftWidget;
  bool m_permanentCloseButtons;
  bool m_alwaysTabBar;
  bool m_MouseMiddleClickClosesTab;
  int m_closeOtherTabsId;
  KonqTabsStyle *m_konqTabsStyle;

  friend class KonqTabsStyle;
};

#endif // KONQTABS_H
