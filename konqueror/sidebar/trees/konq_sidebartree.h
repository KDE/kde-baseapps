/* This file is part of the KDE project
   Copyright (C) 2000 David Faure <faure@kde.org>
                 2000 Carsten Pfeiffer <pfeiffer@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef konq_tree_h
#define konq_tree_h

#include <klistview.h>
#include "konq_sidebartreetoplevelitem.h"
#include "konqsidebar_tree.h"
#include <kdirnotify.h>
#include <qmap.h>
#include <qpoint.h>
#include <qstrlist.h>
#include <qtooltip.h>
class KonqSidebarTreeModule;
class KonqSidebarTreeItem;

class QTimer;

class KonqSidebarTree_Internal;

#define VIRT_Link 0
#define VIRT_Folder 1 // A directory which is parsed for .desktop files

typedef KonqSidebarTreeModule*(*getModule)(KonqSidebarTree*, const bool); 

typedef struct DirTreeConfigData_
{
  KURL dir;
  int type;
  QString relDir;
} DirTreeConfigData;


class KonqSidebarTreeToolTip : public QToolTip
{
public:
    KonqSidebarTreeToolTip( QListView *view ) : QToolTip( view ), m_view( view ) {}

protected:
    virtual void maybeTip( const QPoint & );

private:
    QListView *m_view;
};

/**
 * The multi-purpose tree (listview)
 * It parses its configuration (desktop files), each one corresponding to
 * a toplevel item, and creates the modules that will handle the contents
 * of those items.
 */
class KonqSidebarTree : public KListView, public KDirNotify
{
    Q_OBJECT
public:
    KonqSidebarTree( KonqSidebar_Tree *parent, QWidget *parentWidget, int virt, const QString& path );
    virtual ~KonqSidebarTree();

    void followURL( const KURL &url );

    /**
     * @return the current (i.e. selected) item
     */
    KonqSidebarTreeItem * currentItem() const;

    void startAnimation( KonqSidebarTreeItem * item, const char * iconBaseName = "kde", uint iconCount = 6, const QPixmap * originalPixmap = 0L );
    void stopAnimation( KonqSidebarTreeItem * item );

    // Reimplemented from KDirNotify
    void FilesAdded( const KURL & dir );
    void FilesRemoved( const KURL::List & urls );
    void FilesChanged( const KURL::List & urls );

    KonqSidebarPlugin * part() { return m_part; }

    void lockScrolling( bool lock ) { m_scrollingLocked = lock; }

    bool isOpeningFirstChild() const { return m_bOpeningFirstChild; }
 
    void enableActions( bool copy, bool cut, bool paste,
                        bool trash, bool del, bool shred,
                        bool rename = false );

    void itemDestructed( KonqSidebarTreeItem *item );

public slots:
    virtual void setContentsPos( int x, int y );

protected:
    virtual void contentsDragEnterEvent( QDragEnterEvent *e );
    virtual void contentsDragMoveEvent( QDragMoveEvent *e );
    virtual void contentsDragLeaveEvent( QDragLeaveEvent *e );
    virtual void contentsDropEvent( QDropEvent *ev );

    virtual void contentsMousePressEvent( QMouseEvent *e );
    virtual void contentsMouseMoveEvent( QMouseEvent *e );
    virtual void contentsMouseReleaseEvent( QMouseEvent *e );

    virtual void leaveEvent( QEvent * );

private slots:
    void slotDoubleClicked( QListViewItem *item );
    void slotExecuted( QListViewItem *item );
    void slotMouseButtonPressed(int _button, QListViewItem* _item, const QPoint&, int col);
    void slotMouseButtonClicked(int _button, QListViewItem* _item, const QPoint&, int col);
    void slotSelectionChanged();

    void slotAnimation();

    void slotAutoOpenFolder();

    void rescanConfiguration();

    void slotOnItem( QListViewItem * );
    void slotItemRenamed(QListViewItem*, const QString &, int);

private:
    void clearTree();
    void scanDir( KonqSidebarTreeItem *parent, const QString &path, bool isRoot = false );
    void loadTopLevelGroup( KonqSidebarTreeItem *parent, const QString &path );
    void loadTopLevelItem( KonqSidebarTreeItem *parent, const QString &filename );

    void loadModuleFactories();

    QPtrList<KonqSidebarTreeTopLevelItem> m_topLevelItems;

    QPtrList<KonqSidebarTreeModule> m_lstModules;

    KonqSidebarPlugin  *m_part;

    struct AnimationInfo
    {
        AnimationInfo( const char * _iconBaseName, uint _iconCount, const QPixmap & _originalPixmap )
            : iconBaseName(_iconBaseName), iconCount(_iconCount), iconNumber(1), originalPixmap(_originalPixmap) {}
        AnimationInfo() : iconCount(0) {}
        QCString iconBaseName;
        uint iconCount;
        uint iconNumber;
        QPixmap originalPixmap;
    };
    typedef QMap<KonqSidebarTreeItem *, AnimationInfo> MapCurrentOpeningFolders;
    MapCurrentOpeningFolders m_mapCurrentOpeningFolders;

    QTimer *m_animationTimer;

    QPoint m_dragPos;
    bool m_bDrag;

    QListViewItem *m_currentBeforeDropItem; // The item that was current before the drag-enter event happened
    QListViewItem *m_dropItem; // The item we are moving the mouse over (during a drag)
    QStrList m_lstDropFormats;

    QTimer *m_autoOpenTimer;

    // The base URL for our configuration directory
    //KURL m_dirtreeDir;
    DirTreeConfigData m_dirtreeDir;

    KonqSidebarTreeToolTip m_toolTip;
    bool m_scrollingLocked;

    QMap<QString,getModule> plugins;

    private:
    bool m_bOpeningFirstChild;

    KonqSidebarTree_Internal *d;

#undef signals
#define signals public
signals:
#undef signals
#define signals protected
    void openURLRequest( const KURL &url, const KParts::URLArgs &args = KParts::URLArgs() );
    void createNewWindow( const KURL &url, const KParts::URLArgs &args = KParts::URLArgs() );
    void popupMenu( const QPoint &global, const KURL &url,
         const QString &mimeType, mode_t mode = (mode_t)-1 );
    void popupMenu( const QPoint &global, const KFileItemList &items );
    void enableAction( const char * name, bool enabled );
};

#endif
