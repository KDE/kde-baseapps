/*
 *  This file is part of the KDE project
 *  Copyright (C) 1998-2005 David Faure <faure@kde.org>
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
 **/

#ifndef __konq_view_h__
#define __konq_view_h__

#include "konqmainwindow.h" // hmm, please move PageSecurity out of konq_mainwindow...
#include "konqfactory.h"
#include "konqframe.h"

#include <kservice.h>

#include <QtCore/QList>

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QPointer>
#include <QtCore/QEvent>

class KonqRun;
class KonqFrame;
class KonqBrowserInterface;
namespace KParts
{
  class BrowserExtension;
  class StatusBarExtension;
}

struct HistoryEntry
{
  void loadItem( const KConfigGroup& config, const QString &prefix, const KonqFrameBase::Options &options);
  void saveConfig( KConfigGroup& config, const QString &prefix, const KonqFrameBase::Options &options);

  KUrl url;
  QString locationBarURL; // can be different from url when showing a index.html
  QString title;
  QByteArray buffer;
  QString strServiceType;
  QString strServiceName;
  QByteArray postData;
  QString postContentType;
  bool doPost;
  QString pageReferrer;
  KonqMainWindow::PageSecurity pageSecurity;
  bool reload; // This is used when History entry is restored from a config file
};

/* This class represents a child of the main view. The main view maintains
 * the list of children. A KonqView contains a Browser::View and
 * handles it. It's more or less the backend structure for the views.
 * The widget handling stuff is done by the KonqFrame.
 */
class KONQ_TESTS_EXPORT KonqView : public QObject
{
  Q_OBJECT
public:

  /**
   * Create a konqueror view
   * @param viewFactory the factory to be used to create the part
   * @param viewFrame the frame where to create the view
   * @param mainWindow is the main window :-)
   * @param service the service implementing the part
   * @param partServiceOffers list of part offers found by the factory
   * @param appServiceOffers list of app offers found by the factory
   * @param serviceType the serviceType implemented by the part
   * @param passiveMode whether to initially make the view passive
   */
  KonqView( KonqViewFactory &viewFactory,
            KonqFrame* viewFrame,
            KonqMainWindow * mainWindow,
            const KService::Ptr &service,
            const KService::List &partServiceOffers,
            const KService::List &appServiceOffers,
            const QString &serviceType,
            bool passiveMode);

  ~KonqView();

  /**
   * Displays another URL, but without changing the view mode (caller has to
   * ensure that the call makes sense)
   * @param url the URL to open
   * @param locationBarURL the URL to set in the location bar (see @ref setLocationBarURL)
   * @param nameFilter e.g. *.cpp
   * @param tempFile whether to delete the file after use
   */
  void openUrl( const KUrl &url,
                const QString & locationBarURL,
                const QString &nameFilter = QString(),
                bool tempFile = false );

    /**
     * Change the part inside this view if necessary.
     * Contract: the caller should call stop() first.
     *
     * @param mimeType the mime type we want to show
     * @param serviceName allows to enforce a particular service to be chosen,
     *        @see KonqFactory.
     * @param forceAutoEmbed
     */
    bool changePart(const QString &mimeType,
                    const QString &serviceName = QString(),
                    bool forceAutoEmbed = false);

    /**
     * Ensures that this view's part supports the given @p mimeType,
     * otherwise calls changePart.
     */
    bool ensureViewSupports(const QString& mimeType,
                            bool forceAutoEmbed);

  /**
   * Call this to prevent next openUrl() call from changing history lists
   * Used when the same URL is reloaded (for instance with another view mode)
   *
   * Calling with lock=false is a hack reserved to the "find" feature.
   */
  void lockHistory( bool lock = true ) { m_bLockHistory = lock; }

  /**
   * @return true if view can go back
   */
  bool canGoBack() const { return m_lstHistoryIndex > 0; }

  /**
   * @return true if view can go forward
   */
  bool canGoForward() const { return m_lstHistoryIndex != m_lstHistory.count()-1; }

  /**
   * @return the position in the history
   */
  int historyIndex() const { return m_lstHistoryIndex; }

  int historyLength() { return m_lstHistory.count(); }

  /**
   * Move in history. +1 is "forward", -1 is "back", you can guess the rest.
   */
  void go( int steps );

  /**
   * Helper function for go() and KonqViewManager
   */
  void restoreHistory();

  void setHistoryIndex(int index) { m_lstHistoryIndex = index; }

  /**
   * @return the history of this view
   */
  const QList<HistoryEntry*> & history() { return m_lstHistory; }

  /**
   * @return the HistoryEntry at postion @p pos
   */
  const HistoryEntry* historyAt(int pos);

  /**
   *
   */
  HistoryEntry* currentHistoryEntry() const { return m_lstHistory.value( m_lstHistoryIndex ); }

  /**
   * Creates a deep copy of the @p other view's history buffers.
   */
  void copyHistory( KonqView *other );

  /**
   * Set the KonqRun instance that is running something for this view
   * The main window uses this to store the KonqRun for each child view.
   */
  void setRun( KonqRun * run  );

  KonqRun *run() const { return m_pRun; }

  /**
   * Stop loading
   */
  void stop();

  /**
   * Retrieve view's URL
   */
  KUrl url() const;

  KUrl upUrl() const;

  /**
   * Get view's location bar URL, i.e. the one that the view signals
   * It can be different from url(), for instance if we display a index.html
   */
  QString locationBarURL() const { return m_sLocationBarURL; }

  /**
   * Get the URL that was typed to get the current URL.
   */
  QString typedUrl() const { return m_sTypedURL; }
  /**
   * Set the URL that was typed to get the current URL.
   */
  void setTypedURL( const QString & u ) { m_sTypedURL = u; }

    /**
     * Returns the name filter, if any, like *.txt
     */
    QString nameFilter() const;

  /**
   * Return the security state of page in view
   */
  KonqMainWindow::PageSecurity pageSecurity() const { return m_pageSecurity; }

  /**
   * @return the part embedded into this view
   */
  KParts::ReadOnlyPart *part() const { return m_pPart; }

  /**
   * see KonqViewManager::removePart
   */
  void partDeleted() { m_pPart = 0L; }

    /**
     * Return true if the loading in the view was aborted due to an error
     * or to user cancellation
     */
    bool aborted() const { return m_bAborted; }

  KParts::BrowserExtension *browserExtension() const;

  KParts::StatusBarExtension *statusBarExtension() const;

  /**
   * @return a pointer to the KonqFrame which the view lives in
   */
  KonqFrame* frame() const { return m_pKonqFrame; }

  /**
   * @return the servicetype this view is currently displaying
   * This is usually a mimetype, but it can be "Browser/View"
   * for toggle views like the sidebar or the embedded konsole.
   */
  QString serviceType() const { return m_serviceType; }

  /**
   * @return the servicetypes this view is capable to display
   */
  QStringList serviceTypes() const { return m_service->serviceTypes(); }

  bool supportsMimeType( const QString &mimeType ) const;

    // True if showing a directory
    bool showsDirectory() const;

  // True if "Use index.html" is set (->the view doesn't necessarily show HTML!)
  bool allowHTML() const { return m_bAllowHTML; }
  void setAllowHTML( bool allow ) { m_bAllowHTML = allow; }

  // True if currently loading
  bool isLoading() const { return m_bLoading; }
  void setLoading( bool loading, bool hasPending = false );

  // True if "locked to current location" (and their view mode, in fact)
  bool isLockedLocation() const { return m_bLockedLocation; }
  void setLockedLocation( bool b );

  // True if can't be made active (e.g. dirtree).
  bool isPassiveMode() const { return m_bPassiveMode; }
  void setPassiveMode( bool mode );

  // True if is hierarchical view
  bool isHierarchicalView() const { return m_bHierarchicalView; }
  void setHierarchicalView( bool mode );

  // True if 'link' symbol set
  bool isLinkedView() const { return m_bLinkedView; }
  void setLinkedView( bool mode );

  // True if toggle view
  void setToggleView( bool b ) { m_bToggleView = b; }
  bool isToggleView() const { return m_bToggleView; }

  // True if it always follows the active view
  void setFollowActive(bool b) { m_bFollowActive = b; }
  bool isFollowActive() { return m_bFollowActive; }

  // True if locked to current view mode
  // Toggle views and passive views are locked to their view mode.
  bool isLockedViewMode() const { return m_bToggleView || m_bPassiveMode; }

  // True if "builtin" (see X-KDE-BrowserView-Built-Into)
  //bool isBuiltinView() const { return m_bBuiltinView; }

    /**
     * The current viewmode used by this view -- only meaningful
     * when the part implements several view modes internally, like DolphinPart.
     */
    QString internalViewMode() const;
    /**
     * Switch the internal view mode in this view -- only meaningful
     * when the part implements several view modes internally, like DolphinPart.
     */
    void setInternalViewMode(const QString& viewMode);

  KService::Ptr service() { return m_service; }

  QString caption() const { return m_caption; }

  KService::List partServiceOffers() { return m_partServiceOffers; }
  KService::List appServiceOffers() { return m_appServiceOffers; }

  KonqMainWindow *mainWindow() const { return m_pMainWindow; }

  // return true if the method was found, false if the execution failed
  bool callExtensionMethod( const char *methodName );
  bool callExtensionBoolMethod( const char *methodName, bool value );
  bool callExtensionURLMethod( const char *methodName, const KUrl& value );

  void setViewName( const QString &name );
  QString viewName() const;

  // True to enable the context popup menu
  void enablePopupMenu( bool b );
  bool isPopupMenuEnabled() const { return m_bPopupMenuEnabled; }

  KFileItemList selectedItems() const { return m_selectedItems; }

  void reparseConfiguration();

  void disableScrolling();

  QStringList frameNames() const;

  QString dbusObjectPath();
  QString partObjectPath();

  void goHistory( int steps );

  // Set the KGlobal active componentData(the one used by KBugReport)
  void setActiveComponent();

  // Called before reloading this view. Sets args.reload to true, and offers to repost form data.
  // Returns false in case the reload must be canceled.
  bool prepareReload( KParts::OpenUrlArguments& args, KParts::BrowserArguments& browserArgs, bool softReload );

  // overload for the QString version
  void setLocationBarURL( const KUrl& locationBarURL );

  /**
   * Saves config in a KConfigGroup
   */
  void saveConfig( KConfigGroup& config, const QString &prefix, const KonqFrameBase::Options &options);
  void loadHistoryConfig( const KConfigGroup& config, const QString &prefix);

  static QStringList childFrameNames( KParts::ReadOnlyPart *part );

  static KParts::BrowserHostExtension *hostExtension( KParts::ReadOnlyPart *part, const QString &name );

Q_SIGNALS:

  /**
   * Signal the main window that the embedded part changed (e.g. because of changePart)
   */
  void sigPartChanged( KonqView *childView, KParts::ReadOnlyPart *oldPart, KParts::ReadOnlyPart *newPart );

  /**
   * Emitted in slotCompleted
   */
  void viewCompleted( KonqView * view );

public Q_SLOTS:
  /**
   * Store location-bar URL in the child view
   * and updates the main view if this view is the current one
   * May be different from url e.g. if using "allowHTML".
   */
  void setLocationBarURL( const QString & locationBarURL );
  /**
   * get an icon for the URL from the BrowserExtension
   */
  void setIconURL( const KUrl &iconURL );

  void setTabIcon( const KUrl &url );

  void setCaption( const QString & caption );

  void setPageSecurity( int );

  // connected to the KROP's KIO::Job
  // but also to KonqRun's job
  void slotInfoMessage( KJob *, const QString &msg );

private Q_SLOTS:
  // connected to the KROP's KIO::Job
  void slotStarted( KIO::Job * job );
  void slotCompleted();
  void slotCompleted( bool );
  void slotCanceled( const QString & errMsg );
  void slotPercent( KJob *, unsigned long percent );
  void slotSpeed( KJob *, unsigned long bytesPerSecond );

  /**
   * Connected to the BrowserExtension
   */
  void slotSelectionInfo( const KFileItemList &items );
  void slotMouseOverInfo( const KFileItem& item );
  void slotOpenURLNotify();
  void slotEnableAction( const char * name, bool enabled );
  void slotSetActionText( const char* name, const QString& text );
  void slotMoveTopLevelWidget( int x, int y );
  void slotResizeTopLevelWidget( int w, int h );
  void slotRequestFocus(KParts::ReadOnlyPart*);

private:
  /**
   * Replace the current view with a new view, created by @p viewFactory.
   */
  void switchView( KonqViewFactory &viewFactory );

  /**
   * Connects the internal part to the main window.
   * Do this after creating it and before inserting it.
   */
  void connectPart();

  /**
   * Creates a new entry in the history.
   */
  void createHistoryEntry();

  /**
   * Appends a entry in the history.
   */
  void appendHistoryEntry(HistoryEntry * historyEntry);

  /**
   * Updates the current entry in the history.
   * @param saveLocationBarURL whether to save the location bar URL as part of it
   * (not done in openUrl, to be able to revert if aborting)
   */
  void updateHistoryEntry(bool saveLocationBarURL);

  void aboutToOpenURL( const KUrl &url, const KParts::OpenUrlArguments &args = KParts::OpenUrlArguments() );

  void setPartMimeType();

  void finishedWithCurrentURL();

  virtual bool eventFilter( QObject *obj, QEvent *e );

////////////////// private members ///////////////

  KParts::ReadOnlyPart *m_pPart;

  QString m_sLocationBarURL;
  QString m_sTypedURL;
  KonqMainWindow::PageSecurity m_pageSecurity;
  KFileItemList m_selectedItems;

  /**
   * The full history (back + current + forward)
   */
  QList<HistoryEntry*> m_lstHistory;
  /**
   * The current position in the history
   */
  int m_lstHistoryIndex;

  /**
   * The post data that _resulted_ in this page.
   * e.g. when submitting a form, and the result is an image, this data will be
   * set (and saved/restored) when the image is being viewed. Necessary for reload.
   */
  QByteArray m_postData;
  QString m_postContentType;
  bool m_doPost;

  /**
   * The referrer that was used to obtain this page.
   */
  QString m_pageReferrer;

  KonqMainWindow *m_pMainWindow;
  KonqRun *m_pRun;
  KonqFrame *m_pKonqFrame;

  uint m_bAllowHTML:1;
  uint m_bLoading:1;
  uint m_bLockedLocation:1;
  uint m_bPassiveMode:1;
  uint m_bLinkedView:1;
  uint m_bToggleView:1;
  uint m_bLockHistory:1;
  uint m_bAborted:1;
  uint m_bGotIconURL:1;
  uint m_bPopupMenuEnabled:1;
  uint m_bFollowActive:1;
  uint m_bPendingRedirection:1;
  uint m_bBuiltinView:1;
  uint m_bURLDropHandling:1;
  uint m_bHierarchicalView:1;
  uint m_bDisableScrolling:1;
  KService::List m_partServiceOffers;
  KService::List m_appServiceOffers;
  KService::Ptr m_service;
  QString m_serviceType;
  QString m_caption;
  QString m_tempFile;
  QString m_dbusObjectPath;
  KonqBrowserInterface *m_browserIface;
  int m_randID;
};

#endif
