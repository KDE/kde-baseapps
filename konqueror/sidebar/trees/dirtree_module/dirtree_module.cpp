/* This file is part of the KDE project
   Copyright (C) 2000 David Faure <faure@kde.org>
                 2002 Michael Brade <brade@kde.org>
   Copyright (C) 2003 Waldo Bastian <bastian@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "dirtree_module.h"
#include "dirtree_item.h"

#include <kconfiggroup.h>
#include <kdebug.h>
#include <kprotocolmanager.h>
#include <kdesktopfile.h>
#include <kmessagebox.h>
#include <kiconloader.h>
#include <kdirlister.h>
#include <klocale.h>


KonqSidebarDirTreeModule::KonqSidebarDirTreeModule( KonqSidebarTree * parentTree , bool showHidden)
    : KonqSidebarTreeModule( parentTree, showHidden ), m_dirLister(0L), m_topLevelItem(0L)
{
    // SLOW! Get the KConfigGroup from the plugin.
    KConfig config("konqsidebartngrc");
    KConfigGroup generalGroup( &config, "General" );
    m_showArchivesAsFolders = generalGroup.readEntry( "ShowArchivesAsFolders", true );
}

KonqSidebarDirTreeModule::~KonqSidebarDirTreeModule()
{
    // KDirLister may still emit canceled while being deleted.
    if (m_dirLister)
    {
       disconnect( m_dirLister, SIGNAL(canceled(KUrl)),
                   this, SLOT(slotListingStopped(KUrl)) );
       delete m_dirLister;
    }
}

KUrl::List KonqSidebarDirTreeModule::selectedUrls()
{
    KUrl::List lst;
    KonqSidebarDirTreeItem *selection = static_cast<KonqSidebarDirTreeItem *>( m_pTree->selectedItem() );
    if( !selection )
    {
        kError() << "KonqSidebarDirTreeModule::selectedUrls: no selection!" << endl;
        return lst;
    }
    lst.append(selection->fileItem().url());
    return lst;
}

void KonqSidebarDirTreeModule::addTopLevelItem( KonqSidebarTreeTopLevelItem * item )
{
    if(m_topLevelItem) // We can handle only one at a time !
        kError() << "KonqSidebarDirTreeModule::addTopLevelItem: Impossible, we can have only one toplevel item !" << endl;

    KDesktopFile cfg( item->path() );
    KConfigGroup desktopGroup = cfg.desktopGroup();

    KUrl targetURL;
    targetURL.setPath(item->path());

    if ( cfg.hasLinkType() )
    {
        targetURL = cfg.readUrl();
        // some services might want to make their URL configurable in kcontrol
        QString configured = desktopGroup.readPathEntry("X-KDE-ConfiguredURL", QString());
        if (!configured.isEmpty()) {
            QStringList list = configured.split( ':');
            KConfig config(list[0]);
            KConfigGroup urlGroup(&config, list[1] != "noGroup" ? list[1] : "General") ;
            QString conf_url = urlGroup.readEntry(list[2], QString());
            if (!conf_url.isEmpty()) {
                targetURL = conf_url;
            }
        }
    }
    else if ( cfg.hasDeviceType() )
    {
        // Determine the mountpoint
        QString mp = desktopGroup.readPathEntry("MountPoint", QString());
        if ( mp.isEmpty() )
            return;

        targetURL.setPath(mp);
    }
    else
        return;

    bool bListable = KProtocolManager::supportsListing( targetURL );
    //kDebug(1201) << targetURL.prettyUrl() << " listable : " << bListable;

    if ( !bListable )
    {
        item->setExpandable( false );
        item->setListable( false );
    }

    item->setExternalURL( targetURL );
    addSubDir( item );

    m_topLevelItem = item;
}

void KonqSidebarDirTreeModule::openTopLevelItem( KonqSidebarTreeTopLevelItem * item )
{
    if ( !item->childCount() && item->isListable() )
        openSubFolder( item );
}

void KonqSidebarDirTreeModule::addSubDir( KonqSidebarTreeItem *item )
{
    QString id = item->externalURL().url( KUrl::RemoveTrailingSlash );
    kDebug(1201) << this << " KonqSidebarDirTreeModule::addSubDir " << id;
    m_dictSubDirs.insert(id, item );

    KonqSidebarDirTreeItem *ditem = dynamic_cast<KonqSidebarDirTreeItem*>(item);
    if (ditem)
        m_ptrdictSubDirs.insert(ditem->fileItem(), item);
}

// Remove <key, item> from dict, taking into account that there maybe
// other items with the same key.
static void remove(Q3Dict<KonqSidebarTreeItem> &dict, const QString &key, KonqSidebarTreeItem *item)
{
    Q3PtrList<KonqSidebarTreeItem> *otherItems = 0;
    while(true) {
        KonqSidebarTreeItem *takeItem = dict.take(key);
        if (!takeItem || (takeItem == item))
        {
            if (!otherItems)
                return;

            // Insert the otherItems back in
            for(KonqSidebarTreeItem *otherItem; (otherItem = otherItems->take(0));)
            {
                dict.insert(key, otherItem);
            }
            delete otherItems;
            return;
        }
        // Not the item we are looking for
        if (!otherItems)
            otherItems = new Q3PtrList<KonqSidebarTreeItem>();

        otherItems->prepend(takeItem);
    }
}

// Looks up key in dict and returns it in item, if there are multiple items
// with the same key, additional items are returned in itemList which should
// be deleted by the caller.
static void lookupItems(Q3Dict<KonqSidebarTreeItem> &dict, const QString &key, KonqSidebarTreeItem *&item, Q3PtrList<KonqSidebarTreeItem> *&itemList)
{
    itemList = 0;
    item = dict.take(key);
    if (!item)
        return;

    while(true)
    {
        KonqSidebarTreeItem *takeItem = dict.take(key);
        if (!takeItem)
        {
            //
            // Insert itemList back in
            if (itemList)
            {
                for(KonqSidebarTreeItem *otherItem = itemList->first(); otherItem; otherItem = itemList->next())
                    dict.insert(key, otherItem);
            }
            dict.insert(key, item);
            return;
        }
        if (!itemList)
            itemList = new Q3PtrList<KonqSidebarTreeItem>();

        itemList->prepend(takeItem);
    }
}

// Remove <key, item> from dict, taking into account that there maybe
// other items with the same key.
static void remove(QHash<KFileItem, KonqSidebarTreeItem*> &dict, const KFileItem &key, KonqSidebarTreeItem *item)
{
    Q3PtrList<KonqSidebarTreeItem> *otherItems = 0;
    while(true) {
        KonqSidebarTreeItem *takeItem = dict.take(key);
        if (!takeItem || (takeItem == item))
        {
            if (!otherItems)
                return;

            // Insert the otherItems back in
            for(KonqSidebarTreeItem *otherItem; (otherItem = otherItems->take(0));)
            {
                dict.insert(key, otherItem);
            }
            delete otherItems;
            return;
        }
        // Not the item we are looking for
        if (!otherItems)
            otherItems = new Q3PtrList<KonqSidebarTreeItem>();

        otherItems->prepend(takeItem);
    }
}

// Looks up key in dict and returns it in item, if there are multiple items
// with the same key, additional items are returned in itemList which should
// be deleted by the caller.
static void lookupItems(QHash<KFileItem, KonqSidebarTreeItem*> &dict, const KFileItem &key, KonqSidebarTreeItem *&item, Q3PtrList<KonqSidebarTreeItem> *&itemList)
{
    itemList = 0;
    item = dict.take(key);
    if (!item)
        return;

    while(true)
    {
        KonqSidebarTreeItem *takeItem = dict.take(key);
        if (!takeItem)
        {
            //
            // Insert itemList back in
            if (itemList)
            {
                for(KonqSidebarTreeItem *otherItem = itemList->first(); otherItem; otherItem = itemList->next())
                    dict.insert(key, otherItem);
            }
            dict.insert(key, item);
            return;
        }
        if (!itemList)
            itemList = new Q3PtrList<KonqSidebarTreeItem>();

        itemList->prepend(takeItem);
    }
}


void KonqSidebarDirTreeModule::removeSubDir( KonqSidebarTreeItem *item, bool childrenOnly )
{
    kDebug(1201) << this << " KonqSidebarDirTreeModule::removeSubDir item=" << item;
    if ( item->firstChild() )
    {
        KonqSidebarTreeItem * it = static_cast<KonqSidebarTreeItem *>(item->firstChild());
        KonqSidebarTreeItem * next = 0L;
        while ( it ) {
            next = static_cast<KonqSidebarTreeItem *>(it->nextSibling());
            removeSubDir( it );
            delete it;
            it = next;
        }
    }

    if ( !childrenOnly )
    {
        QString id = item->externalURL().url( KUrl::RemoveTrailingSlash );
        remove(m_dictSubDirs, id, item);
        while (!(item->alias.isEmpty()))
        {
            remove(m_dictSubDirs, item->alias.front(), item);
            item->alias.pop_front();
        }

        KonqSidebarDirTreeItem *ditem = dynamic_cast<KonqSidebarDirTreeItem*>(item);
        if (ditem)
           remove(m_ptrdictSubDirs, ditem->fileItem(), item);
    }
}


void KonqSidebarDirTreeModule::openSubFolder( KonqSidebarTreeItem *item )
{
    kDebug(1201) << this << " openSubFolder( " << item->externalURL().prettyUrl() << " )";

    if ( !m_dirLister ) // created on demand
    {
        m_dirLister = new KDirLister();
        //m_dirLister->setDelayedMimeTypes( true ); // this was set, but it's wrong, without a KMimeTypeResolver...
        //m_dirLister->setDirOnlyMode( true );
//	QStringList mimetypes;
//	mimetypes<<QString("inode/directory");
//	m_dirLister->setMimeFilter(mimetypes);

        connect( m_dirLister, SIGNAL(newItems(KFileItemList)),
                 this, SLOT(slotNewItems(KFileItemList)) );
        connect( m_dirLister, SIGNAL(refreshItems(QList<QPair<KFileItem,KFileItem> >)),
                 this, SLOT(slotRefreshItems(QList<QPair<KFileItem,KFileItem> >)) );
        connect( m_dirLister, SIGNAL(deleteItem(KFileItem)),
                 this, SLOT(slotDeleteItem(KFileItem)) );
        connect( m_dirLister, SIGNAL(completed(KUrl)),
                 this, SLOT(slotListingStopped(KUrl)) );
        connect( m_dirLister, SIGNAL(canceled(KUrl)),
                 this, SLOT(slotListingStopped(KUrl)) );
        connect( m_dirLister, SIGNAL(redirection(KUrl,KUrl)),
                 this, SLOT(slotRedirection(KUrl,KUrl)) );
    }


    if ( !item->isTopLevelItem() &&
         static_cast<KonqSidebarDirTreeItem *>(item)->hasStandardIcon() )
    {
        int size = KIconLoader::global()->currentSize( KIconLoader::Small );
        QPixmap pix = DesktopIcon( "folder-open", size );
        m_pTree->startAnimation( item, "kde", 6, &pix );
    }
    else
        m_pTree->startAnimation( item );

    listDirectory( item );
}

void KonqSidebarDirTreeModule::listDirectory( KonqSidebarTreeItem *item )
{
    // This causes a reparsing, but gets rid of the trailing slash
    QString strUrl = item->externalURL().url( KUrl::RemoveTrailingSlash );
    KUrl url( strUrl );

    Q3PtrList<KonqSidebarTreeItem> *itemList;
    KonqSidebarTreeItem * openItem;
    lookupItems(m_dictSubDirs, strUrl, openItem, itemList);

    while(openItem)
    {
        if (openItem->childCount())
            break;

        openItem = itemList ? itemList->take(0) : 0;
    }
    delete itemList;

    if (openItem)
    {
       // We have this directory listed already, just copy the entries as we
       // can't use the dirlister, it would invalidate the old entries
       int size = KIconLoader::global()->currentSize( KIconLoader::Small );
       KonqSidebarTreeItem * parentItem = item;
       KonqSidebarDirTreeItem *oldItem = static_cast<KonqSidebarDirTreeItem *> (openItem->firstChild());
       while(oldItem)
       {
          const KFileItem fileItem = oldItem->fileItem();
          if (! fileItem.isDir() )
          {
              if ( !fileItem.url().isLocalFile() )
                  continue;
	      KMimeType::Ptr ptr = fileItem.determineMimeType();
              if ( ptr && (ptr->is("inode/directory") || m_showArchivesAsFolders)
                       && ((!ptr->property("X-KDE-LocalProtocol").toString().isEmpty())) ) {
		kDebug()<<"Something not really a directory";
	      } else {
//	              kError() << "Item " << fileItem->url().prettyUrl() << " is not a directory!" << endl;
                  continue;
	      }
          }

          KonqSidebarDirTreeItem *dirTreeItem = new KonqSidebarDirTreeItem( parentItem, m_topLevelItem, fileItem );
          dirTreeItem->setPixmap( 0, fileItem.pixmap( size ) );
          dirTreeItem->setText( 0, KIO::decodeFileName( fileItem.name() ) );

          oldItem = static_cast<KonqSidebarDirTreeItem *> (oldItem->nextSibling());
       }
       m_pTree->stopAnimation( item );

       return;
    }

    m_dirLister->setShowingDotFiles( showHidden());

    if (tree()->isOpeningFirstChild()) m_dirLister->setAutoErrorHandlingEnabled(false,0);
	else m_dirLister->setAutoErrorHandlingEnabled(true,tree());

    m_dirLister->openUrl( url, KDirLister::Keep );
}

void KonqSidebarDirTreeModule::slotNewItems( const KFileItemList& entries )
{
    kDebug(1201) << this << " KonqSidebarDirTreeModule::slotNewItems " << entries.count();

    Q_ASSERT(entries.count());
    const KFileItem firstItem = entries.first();

    // Find parent item - it's the same for all the items
    KUrl dir( firstItem.url().url( KUrl::RemoveTrailingSlash ) );
    dir.setFileName( "" );
    kDebug(1201) << this << " KonqSidebarDirTreeModule::slotNewItems dir=" << dir.url( KUrl::RemoveTrailingSlash );

    Q3PtrList<KonqSidebarTreeItem> *parentItemList;
    KonqSidebarTreeItem * parentItem;
    lookupItems(m_dictSubDirs, dir.url( KUrl::RemoveTrailingSlash ), parentItem, parentItemList);

    if ( !parentItem )   // hack for dnssd://domain/type/service listed in dnssd:/type/ dir
    {
    	dir.setHost( QString() );
	lookupItems( m_dictSubDirs, dir.url( KUrl::RemoveTrailingSlash ), parentItem, parentItemList );
    }

    if( !parentItem )
    {
        KMessageBox::error( tree(), i18n("Cannot find parent item %1 in the tree. Internal error.", dir.url( KUrl::RemoveTrailingSlash ) ) );
       	return;
    }

    kDebug()<<"number of additional parent items:"<< (parentItemList?parentItemList->count():0);
    int size = KIconLoader::global()->currentSize( KIconLoader::Small );
    do
    {
    	kDebug()<<"Parent Item URL:"<<parentItem->externalURL();
        KFileItemList::const_iterator kit = entries.begin();
        const KFileItemList::const_iterator kend = entries.end();
        for (; kit != kend; ++kit )
        {
            const KFileItem fileItem = *kit;

            if (! fileItem.isDir() )
            {
              if ( !fileItem.url().isLocalFile() )
                  continue;
	      KMimeType::Ptr ptr = fileItem.determineMimeType();

	      if ( ptr && (ptr->is("inode/directory") || m_showArchivesAsFolders)
                       && ((!ptr->property("X-KDE-LocalProtocol").toString().isEmpty())) ) {
		kDebug()<<"Something really a directory";
	      } else {
                  //kError() << "Item " << fileItem->url().prettyUrl() << " is not a directory!" << endl;
                  continue;
	      }
            }

            KonqSidebarDirTreeItem *dirTreeItem = new KonqSidebarDirTreeItem( parentItem, m_topLevelItem, fileItem );
            dirTreeItem->setPixmap( 0, fileItem.pixmap( size ) );
            dirTreeItem->setText( 0, KIO::decodeFileName( fileItem.name() ) );
        }

    } while ((parentItem = parentItemList ? parentItemList->take(0) : 0));
    delete parentItemList;
}

void KonqSidebarDirTreeModule::slotRefreshItems( const QList<QPair<KFileItem, KFileItem> >&entries )
{
    int size = KIconLoader::global()->currentSize( KIconLoader::Small );

    kDebug(1201) << "# of items to refresh:" << entries.count();

    for ( int i = 0; i < entries.count(); ++i )
    {
        const KFileItem fileItem (entries.at(i).second);
        const KFileItem oldFileItem(entries.at(i).first);

        Q3PtrList<KonqSidebarTreeItem> *itemList;
        KonqSidebarTreeItem * item;
        lookupItems(m_ptrdictSubDirs, oldFileItem, item, itemList);

        if (!item)
        {
            kWarning(1201) << "can't find old entry for " << oldFileItem.url().url( KUrl::RemoveTrailingSlash ) ;
            continue;
        }

        do
        {
            if ( item->isTopLevelItem() ) // we only have dirs and one toplevel item in the dict
            {
                kWarning(1201) << "entry for " << oldFileItem.url().url( KUrl::RemoveTrailingSlash ) << "matches against toplevel." ;
                break;
            }

            KonqSidebarDirTreeItem * dirTreeItem = static_cast<KonqSidebarDirTreeItem *>(item);
            // Item renamed ?
            if ( dirTreeItem->id != fileItem.url().url( KUrl::RemoveTrailingSlash ) )
            {
                kDebug(1201) << "renaming" << oldFileItem << "->" << fileItem;
                // We need to update the URL in m_dictSubDirs, and to get rid of the child items, so remove and add.
                // Then remove + delete
                removeSubDir( dirTreeItem, true /*children only*/ );
                remove(m_dictSubDirs, dirTreeItem->id, dirTreeItem);
                remove(m_ptrdictSubDirs, oldFileItem, dirTreeItem);

                dirTreeItem->reset(); // Reset id
                dirTreeItem->setPixmap( 0, fileItem.pixmap( size ) );
                dirTreeItem->setText( 0, KIO::decodeFileName( fileItem.name() ) );

                // Make sure the item doesn't get inserted twice!
                // dirTreeItem->id points to the new name
                remove(m_dictSubDirs, dirTreeItem->id, dirTreeItem);
                remove(m_ptrdictSubDirs, fileItem, dirTreeItem);
                m_dictSubDirs.insert(dirTreeItem->id, dirTreeItem);
                m_ptrdictSubDirs.insert(fileItem, dirTreeItem);
            }
            else
            {
                dirTreeItem->setPixmap( 0, fileItem.pixmap( size ) );
                dirTreeItem->setText( 0, KIO::decodeFileName( fileItem.name() ) );
            }

        } while ((item = itemList ? itemList->take(0) : 0));
        delete itemList;
    }
}

void KonqSidebarDirTreeModule::slotDeleteItem( const KFileItem &fileItem )
{
    kDebug(1201) << "KonqSidebarDirTreeModule::slotDeleteItem( " << fileItem.url().url( KUrl::RemoveTrailingSlash ) << " )";

    // All items are in m_ptrdictSubDirs, so look it up fast
    Q3PtrList<KonqSidebarTreeItem> *itemList;
    KonqSidebarTreeItem * item;
    lookupItems(m_dictSubDirs, fileItem.url().url( KUrl::RemoveTrailingSlash ), item, itemList);
    while(item)
    {
        removeSubDir( item );
        delete item;

        item = itemList ? itemList->take(0) : 0;
    }
    delete itemList;
}

void KonqSidebarDirTreeModule::slotRedirection( const KUrl & oldUrl, const KUrl & newUrl )
{
    kDebug(1201) << newUrl;

    QString oldUrlStr = oldUrl.url( KUrl::RemoveTrailingSlash );
    QString newUrlStr = newUrl.url( KUrl::RemoveTrailingSlash );

    Q3PtrList<KonqSidebarTreeItem> *itemList;
    KonqSidebarTreeItem * item;
    lookupItems(m_dictSubDirs, oldUrlStr, item, itemList);

    if (!item)
    {
        kWarning(1201) << "NOT FOUND   oldUrl=" << oldUrlStr ;
        return;
    }

    do
    {
	if (item->alias.contains(newUrlStr)) continue;
	kDebug()<<"Redirectiong element";
        // We need to update the URL in m_dictSubDirs
        m_dictSubDirs.insert( newUrlStr, item );
        item->alias << newUrlStr;

        kDebug(1201) << "Updating url of " << item << " to " << newUrlStr;

    } while ((item = itemList ? itemList->take(0) : 0));
    delete itemList;
}

void KonqSidebarDirTreeModule::slotListingStopped( const KUrl & url )
{
    //kDebug(1201) << url;

    Q3PtrList<KonqSidebarTreeItem> *itemList;
    KonqSidebarTreeItem * item;
    lookupItems(m_dictSubDirs, url.url( KUrl::RemoveTrailingSlash ), item, itemList);

    while(item)
    {
        if ( item->childCount() == 0 )
        {
            item->setExpandable( false );
            item->repaint();
        }
        m_pTree->stopAnimation( item );

        item = itemList ? itemList->take(0) : 0;
    }
    delete itemList;

    //kDebug(1201) << "m_selectAfterOpening " << m_selectAfterOpening.prettyUrl();
    if ( !m_selectAfterOpening.isEmpty() && url.isParentOf( m_selectAfterOpening ) )
    {
        KUrl theURL( m_selectAfterOpening );
        m_selectAfterOpening = KUrl();
        followURL( theURL );
    }
}

void KonqSidebarDirTreeModule::followURL( const KUrl & url )
{
    // Check if we already know this URL
    KonqSidebarTreeItem * item = m_dictSubDirs[ url.url( KUrl::RemoveTrailingSlash ) ];
    if (item) // found it  -> ensure visible, select, return.
    {
        m_pTree->ensureItemVisible( item );
        m_pTree->setSelected( item, true );
        return;
    }

    KUrl uParent( url );
    KonqSidebarTreeItem * parentItem = 0L;
    // Go up to the first known parent
    do
    {
        uParent = uParent.upUrl();
        parentItem = m_dictSubDirs[ uParent.url( KUrl::RemoveTrailingSlash ) ];
    } while ( !parentItem && !uParent.path().isEmpty() && uParent.path() != "/" );

    // Not found !?!
    if (!parentItem)
    {
        kDebug() << "No parent found for url " << url.prettyUrl();
        return;
    }
    kDebug() << "Found parent " << uParent.prettyUrl();

    // That's the parent directory we found. Open if not open...
    if ( !parentItem->isOpen() )
    {
        parentItem->setOpen( true );
        if ( parentItem->childCount() && m_dictSubDirs[ url.url( KUrl::RemoveTrailingSlash ) ] )
        {
            // Immediate opening, if the dir was already listed
            followURL( url ); // equivalent to a goto-beginning-of-method
        } else
        {
            m_selectAfterOpening = url;
            //kDebug() << "KonqSidebarDirTreeModule::followURL: m_selectAfterOpening=" << m_selectAfterOpening.url();
        }
    }
}


extern "C"
{
        KDE_EXPORT KonqSidebarTreeModule *create_konq_sidebartree_dirtree(KonqSidebarTree* par,const bool showHidden)
	{
		return new KonqSidebarDirTreeModule(par,showHidden);
	}
}



#include "dirtree_module.moc"
