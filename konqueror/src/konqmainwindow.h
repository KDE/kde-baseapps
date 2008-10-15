/*
   This file is part of the KDE project
   Copyright (C) 1998, 1999 Simon Hausmann <hausmann@kde.org>
   Copyright (C) 2000-2004 David Faure <faure@kde.org>
   Copyright (C) 2007 Eduardo Robles Elvira <edulix@gmail.com>
   Copyright (C) 2007 Daniel García Moreno <danigm@gmail.com>

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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KONQMAINWINDOW_H
#define KONQMAINWINDOW_H

#include "konqprivate_export.h"

#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QList>

#include <kfileitem.h>

#include "konqopenurlrequest.h"
#include <kparts/mainwindow.h>
#include <kglobalsettings.h>
#include <kservice.h>
#include "konqcombo.h"
#include "konqframe.h"
#include "konqframecontainer.h"

class KUrlCompletion;
class QLabel;
class KLocalizedString;
class KToggleFullScreenAction;
class KonqUndoManager;
class QFile;
class QAction;
class QPixmap;
class KAction;
class KActionCollection;
class KActionMenu;
class KBookmarkGroup;
class KBookmarkMenu;
class KBookmarkActionMenu;
class KCMultiDialog;
class KNewMenu;
class KToggleAction;
class KonqBidiHistoryAction;
class KBookmarkBar;
class KonqView;
class KonqFrameContainerBase;
class KonqFrameContainer;
class KToolBarPopupAction;
class KAnimatedButton;
class KonqViewManager;
class ToggleViewGUIClient;
class KonqMainWindowIface;
class KonqDirPart;
class KonqRun;
class KConfigGroup;
class KUrlRequester;
class KBookmarkManager;
struct HistoryEntry;

namespace KParts {
    class BrowserExtension;
    class BrowserHostExtension;
    class ReadOnlyPart;
    class OpenUrlArguments;
    struct BrowserArguments;
}

class KonqExtendedBookmarkOwner;


class KONQ_TESTS_EXPORT KonqMainWindow : public KParts::MainWindow, public KonqFrameContainerBase
{
  Q_OBJECT
  Q_PROPERTY( int viewCount READ viewCount )
  Q_PROPERTY( int activeViewsCount READ activeViewsCount )
  Q_PROPERTY( int linkableViewsCount READ linkableViewsCount )
  Q_PROPERTY( QString locationBarURL READ locationBarURL )
  Q_PROPERTY( bool fullScreenMode READ fullScreenMode )
  Q_PROPERTY( QString currentTitle READ currentTitle )
  Q_PROPERTY( QString currentURL READ currentURL )
  Q_PROPERTY( bool isHTMLAllowed READ isHTMLAllowed )
  Q_PROPERTY( QString currentProfile READ currentProfile )
public:
  enum ComboAction { ComboClear, ComboAdd, ComboRemove };
  enum PageSecurity { NotCrypted, Encrypted, Mixed };

    // TODO remove xmluiFile argument, this solution can't work for session management,
    // see readProperties.

    explicit KonqMainWindow(const KUrl &initialURL = KUrl(),
                            const QString& xmluiFile = "konqueror.rc");
    ~KonqMainWindow();

    /**
     * Filters the URL and calls the main openUrl method.
     */
    void openFilteredUrl(const QString& url, const KonqOpenURLRequest& req);

    /**
     * Convenience overload for openFilteredUrl(url, req)
     */
    void openFilteredUrl(const QString& url, bool inNewTab = false, bool tempFile = false);

public Q_SLOTS:
  /**
   * The main openUrl method.
   */
  void openUrl(KonqView * view, const KUrl & url,
               const QString &serviceType = QString(),
               const KonqOpenURLRequest & req = KonqOpenURLRequest::null,
               bool trustedSource = false); // trustedSource should be part of KonqOpenURLRequest, probably

public:
    /**
     * Called by openUrl when it knows the mime type (either directly,
     * or using KonqRun).
     * \param mimeType the mimetype of the URL to open. Always set.
     * \param url the URL to open.
     * \param childView the view in which to open the URL. Can be 0, in which
     * case a new tab (or the very first view) will be created.
     */
    bool openView(QString mimeType, const KUrl& url, KonqView *childView,
                  const KonqOpenURLRequest & req = KonqOpenURLRequest::null);


  void abortLoading();

    void openMultiURL( const KUrl::List& url );

    /// Returns the view manager for this window.
    KonqViewManager *viewManager() const { return m_pViewManager; }

    /// KXMLGUIBuilder methods, reimplemented for delayed bookmark-toolbar initialization
    virtual QWidget *createContainer( QWidget *parent, int index, const QDomElement &element, QAction* &containerAction );
    virtual void removeContainer( QWidget *container, QWidget *parent, QDomElement &element, QAction* containerAction );

    /// KMainWindow methods, for session management
    virtual void saveProperties( KConfigGroup& config );
    virtual void readProperties( const KConfigGroup& config );

  void setInitialFrameName( const QString &name );

  void reparseConfiguration();

    /// Called by KonqViewManager
    void insertChildView(KonqView *childView);
    /// Called by KonqViewManager
    void removeChildView(KonqView *childView);

  KonqView *childView( KParts::ReadOnlyPart *view );
  KonqView *childView( KParts::ReadOnlyPart *callingPart, const QString &name, KParts::BrowserHostExtension *&hostExtension, KParts::ReadOnlyPart **part );

  // Total number of views
  int viewCount() const { return m_mapViews.count(); }

    // Number of views not in "passive" mode
    int activeViewsCount() const;

    // Number of views not in "passive" mode and not locked
    int activeViewsNotLockedCount() const;

    // Number of views that can be linked, i.e. not with "follow active view" behavior
    int linkableViewsCount() const;

    // Number of main views (non-toggle non-passive views)
    int mainViewsCount() const;

    // Return true if we are showing a view that supports this mimeType.
    bool hasViewWithMimeType(const QString& mimeType) const;

  typedef QMap<KParts::ReadOnlyPart *, KonqView *> MapViews;

  const MapViews & viewMap() const { return m_mapViews; }

  KonqView *currentView() const;

  /** URL of current part, or URLs of selected items for directory views */
  KUrl::List currentURLs() const;

  // Only valid if there are one or two views
  KonqView * otherView( KonqView * view ) const;

  /// Overloaded of KMainWindow
  virtual void setCaption( const QString &caption );
  /// Overloaded of KMainWindow -- should never be called, or if it is, we ignore "modified" anyway
  virtual void setCaption( const QString &caption, bool modified ) {
      Q_UNUSED(modified);
      return setCaption(caption);
  }

  /**
   * Change URL displayed in the location bar
   */
  void setLocationBarURL( const QString &url );
  /**
   * Overload for convenience
   */
  void setLocationBarURL( const KUrl &url );
  /**
   * Return URL displayed in the location bar - for KonqViewManager
   */
  QString locationBarURL() const;
  void focusLocationBar();

  /**
   * Set page security related to current view
   */
  void setPageSecurity( PageSecurity );

  void enableAllActions( bool enable );

  void disableActionsNoView();

  void updateToolBarActions( bool pendingActions = false );
  void updateOpenWithActions();
  void updateViewActions();

  bool sidebarVisible() const;

  void setShowHTML( bool b );

    void showHTML( KonqView * view, bool b, bool _activateView );

    bool fullScreenMode() const;

  /**
   * @return the "link view" action, for checking/unchecking from KonqView
   */
  KToggleAction * linkViewAction()const { return m_paLinkView; }

  void enableAction( const char * name, bool enabled );
  void setActionText( const char * name, const QString& text );

  /**
   * The default settings "allow HTML" - the one used when creating a new view
   * Might not match the current view !
   */
  bool isHTMLAllowed() const { return m_bHTMLAllowed; }

  static QList<KonqMainWindow*> *mainWindowList() { return s_lstViews; }

  // public for konq_guiclients
  void viewCountChanged();

    /**
     * For the view manager: we are loading the profile from this config file,
     * so we should save mainwindow settings into that file from now on
     */
    void setProfileConfig(const KConfigGroup& cfg);
    void currentProfileChanged();

  // operates on all combos of all mainwindows of this instance
  // up to now adds an entry or clears all entries
  static void comboAction( int action, const QString& url,
			   const QString& senderId );

#ifndef NDEBUG
  void dumpViewList();
#endif

    // KonqFrameContainerBase implementation BEGIN

    virtual bool accept( KonqFrameVisitor* visitor );

    /**
     * Insert a new frame as the mainwindow's child
     */
    virtual void insertChildFrame(KonqFrameBase * frame, int index = -1 );
    /**
     * Call this before deleting one of our children.
     */
    virtual void childFrameRemoved( KonqFrameBase * frame );

  void saveConfig( KConfigGroup& config, const QString &prefix, const KonqFrameBase::Options &options, KonqFrameBase* docContainer, int id = 0, int depth = 0 );

  void copyHistory( KonqFrameBase *other );

  void setTitle( const QString &title , QWidget* sender);
  void setTabIcon( const KUrl &url, QWidget* sender );

  QWidget* asQWidget();

  QByteArray frameType();

  KonqFrameBase* childFrame()const;

  void setActiveChild( KonqFrameBase* activeChild );

  // KonqFrameContainerBase implementation END

  KonqFrameBase* workingTab()const { return m_pWorkingTab; }
  void setWorkingTab( KonqFrameBase* tab ) { m_pWorkingTab = tab; }

  static bool isMimeTypeAssociatedWithSelf( const QString &mimeType );
  static bool isMimeTypeAssociatedWithSelf( const QString &mimeType, const KService::Ptr &offer );

    bool refuseExecutingKonqueror(const QString& mimeType);

  void resetWindow();

    // TODO: move to a KonqPreloadHandler class
  static void setPreloadedFlag( bool preloaded );
  static bool isPreloaded() { return s_preloaded; }
  static void setPreloadedWindow( KonqMainWindow* );
  static KonqMainWindow* preloadedWindow() { return s_preloadedWindow; }

  QString currentTitle() const;
    // Not used by konqueror itself; only exists for the Q_PROPERTY,
    // which I guess is used by scripts and plugins...
  QString currentURL() const;
  QString currentProfile() const;
    void applyWindowSizeFromProfile(const KConfigGroup& profileGroup);

  void updateHistoryActions();

Q_SIGNALS:
  void viewAdded( KonqView *view );
  void viewRemoved( KonqView *view );
  void popupItemsDisturbed();

public Q_SLOTS:
    void updateViewModeActions();

    void slotInternalViewModeChanged();

    void slotCtrlTabPressed();

    void slotPopupMenu( const QPoint &global, const KFileItemList &items, const KParts::OpenUrlArguments &args, const KParts::BrowserArguments& browserArgs, KParts::BrowserExtension::PopupFlags flags, const KParts::BrowserExtension::ActionGroupMap& );
    void slotPopupMenu( const QPoint &global, const KUrl &url, mode_t mode, const KParts::OpenUrlArguments &args, const KParts::BrowserArguments& browserArgs, KParts::BrowserExtension::PopupFlags f, const KParts::BrowserExtension::ActionGroupMap& );

    /**
     * __NEEEEVER__ call this method directly. It relies on sender() (the part)
     */
    void slotOpenURLRequest( const KUrl &url, const KParts::OpenUrlArguments& args, const KParts::BrowserArguments &browserArgs );

  void openUrlRequestHelper( KonqView *childView, const KUrl &url, const KParts::OpenUrlArguments& args, const KParts::BrowserArguments &browserArgs );

  void slotCreateNewWindow( const KUrl &url, const KParts::OpenUrlArguments& args, const KParts::BrowserArguments &browserArgs,
                            const KParts::WindowArgs &windowArgs = KParts::WindowArgs(),
                            KParts::ReadOnlyPart **part = 0 );

  void slotNewWindow();
  void slotDuplicateWindow();
  void slotSendURL();
  void slotSendFile();
  void slotCopyFiles();
  void slotMoveFiles();
  void slotOpenLocation();
  void slotOpenFile();

  // View menu
  void slotViewModeTriggered(QAction* action);
  void slotShowHTML();
  void slotLockView();
  void slotLinkView();
  void slotReload( KonqView* view = 0L );
  void slotStop();

  // Go menu
  void slotUp();
  void slotUp(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
  void slotUpDelayed();
  void slotBack();
  void slotBack(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
  void slotForward();
  void slotForward(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
  void slotHome();
  void slotHome(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
  void slotGoHistory();

  void slotAddClosedUrl(KonqFrameBase *tab);

  void slotConfigure();
  void slotConfigureDone();
  void slotConfigureToolbars();
  void slotConfigureExtensions();
  void slotConfigureSpellChecking();
  void slotNewToolbarConfig();

  void slotUndoAvailable( bool avail );

  void slotPartChanged( KonqView *childView, KParts::ReadOnlyPart *oldPart, KParts::ReadOnlyPart *newPart );

  void slotRunFinished();
  void slotClearLocationBar();

  // reimplement from KParts::MainWindow
  virtual void slotSetStatusBarText( const QString &text );

  // public for KonqViewManager
  void slotPartActivated( KParts::Part *part );

  virtual void setIcon( const QPixmap& );
  void slotGoHistoryActivated( int steps );
  void slotGoHistoryActivated( int steps, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers );

  void slotAddTab();
  void slotSplitViewHorizontal();
  void slotSplitViewVertical();
  void slotRemoveOtherTabs();
  void slotRemoveOtherTabsPopupDelayed();

private Q_SLOTS:
  void slotViewCompleted( KonqView * view );

  void slotURLEntered(const QString &text, Qt::KeyboardModifiers);

  void slotLocationLabelActivated();

  void slotDuplicateTab();
  void slotDuplicateTabPopup();

  void slotBreakOffTab();
  void slotBreakOffTabPopup();
  void slotBreakOffTabPopupDelayed();

  void slotPopupNewWindow();
  void slotPopupThisWindow();
  void slotPopupNewTab();
  void slotPopupPasteTo();
  void slotRemoveView();

  void slotRemoveOtherTabsPopup();

  void slotReloadPopup();
  void slotReloadAllTabs();
  void slotRemoveTab();
  void slotRemoveTabPopup();
  void slotRemoveTabPopupDelayed();

  void slotActivateNextTab();
  void slotActivatePrevTab();
  void slotActivateTab();

  void slotDumpDebugInfo();

  void slotSaveViewProfile();

    void slotOpenEmbedded(KService::Ptr);

  // Connected to KSycoca
  void slotDatabaseChanged();

  // Connected to KApp
  void slotReconfigure();

  void slotForceSaveMainWindowSettings();

  void slotOpenWith();

#if 0
  void slotGoMenuAboutToShow();
#endif
  void slotUpAboutToShow();
  void slotBackAboutToShow();
  void slotForwardAboutToShow();

  void slotClosedItemsListAboutToShow();
  void updateClosedItemsAction();


  void slotSessionsListAboutToShow();
  void saveCurrentSession();
  void manageSessions();
  void slotSessionActivated(QAction* action);

  void slotUpActivated(QAction* action);
  void slotBackActivated(QAction* action);
  void slotForwardActivated(QAction* action);
  void slotGoHistoryDelayed();

  void slotCompletionModeChanged( KGlobalSettings::Completion );
  void slotMakeCompletion( const QString& );
  void slotSubstringcompletion( const QString& );
  void slotRotation( KCompletionBase::KeyBindingType );
  void slotMatch( const QString& );
  void slotClearHistory();
  void slotClearComboHistory();

  void slotClipboardDataChanged();
  void slotCheckComboSelection();

  void slotShowMenuBar();

  void slotOpenURL( const KUrl& );

#if 0
  void slotToolFind();
  void slotFindOpen( KonqDirPart * dirPart );
  void slotFindClosed( KonqDirPart * dirPart );
#endif

  void slotIconsChanged();

  virtual bool event( QEvent* );

  void slotMoveTabLeft();
  void slotMoveTabRight();

  void slotAddWebSideBar(const KUrl& url, const QString& name);

  void slotUpdateFullScreen( bool set ); // do not call directly

protected:
  virtual bool eventFilter(QObject*obj,QEvent *ev);

  /**
   * Reimplemented for internal reasons. The API is not affected.
   */
  virtual void showEvent(QShowEvent *event);

  bool makeViewsFollow( const KUrl & url,
                        const KParts::OpenUrlArguments& args,
                        const KParts::BrowserArguments &browserArgs, const QString & serviceType,
                        KonqView * senderView );

  void applyKonqMainWindowSettings();

  void viewsChanged();

  void updateLocalPropsActions();

  virtual void closeEvent( QCloseEvent * );
  virtual bool queryExit();

  bool askForTarget(const KLocalizedString& text, KUrl& url);

private Q_SLOTS:
  void slotUndoTextChanged(const QString& newText);

  void slotRequesterClicked( KUrlRequester * );
  void slotIntro();
  void slotItemsRemoved(const KFileItemList &);
  /**
   * Loads the url displayed currently in the lineedit of the locationbar, by
   * emulating a enter key press event.
   */
  void goURL();

  void bookmarksIntoCompletion();

  void initBookmarkBar();

  void showPageSecurity();

private:
  QString detectNameFilter( KUrl & url );

  /**
   * takes care of hiding the bookmarkbar and calling setChecked( false ) on the
   * corresponding action
   */
  void updateBookmarkBar();

  /**
   * Adds all children of @p group to the static completion object
   */
  static void bookmarksIntoCompletion( const KBookmarkGroup& group );

  /**
   * Returns all matches of the url-history for @p s. If there are no direct
   * matches, it will try completing with http:// prepended, and if there's
   * still no match, then http://www. Due to that, this is only usable for
   * popupcompletion and not for manual or auto-completion.
   */
  static QStringList historyPopupCompletionItems( const QString& s = QString());

  void startAnimation();
  void stopAnimation();

  void setUpEnabled( const KUrl &url );

  void checkDisableClearButton();
  void initCombo();
  void initActions();

  void popupNewTab(bool infront, bool openAfterCurrentPage);
  void addClosedWindowToUndoList();
  /**
   * Tries to find a index.html (.kde.html) file in the specified directory
   */
  static QString findIndexFile( const QString &directory );

  void connectExtension( KParts::BrowserExtension *ext );
  void disconnectExtension( KParts::BrowserExtension *ext );

  void plugViewModeActions();
  void unplugViewModeActions();

  bool stayPreloaded();
  bool checkPreloadResourceUsage();

  /**
   * Manage how many instances of this class are out there.
   */
  void incInstancesCount();
  void decInstancesCount();

  QObject* lastFrame( KonqView *view );

    // Maximum height of the animated logo qtoolbutton (m_paAnimatedLogo)
    int maxThrobberHeight();
    void setAnimatedLogoSize();

private: // members
  KonqUndoManager* m_undoManager;

  KNewMenu * m_pMenuNew;

  KAction *m_paPrint;

  KBookmarkActionMenu *m_pamBookmarks;

  KToolBarPopupAction *m_paUp;
  KToolBarPopupAction *m_paBack;
  KToolBarPopupAction *m_paForward;
  /// Action for the trash that contains closed tabs/windows
  KToolBarPopupAction *m_paClosedItems;
  KActionMenu *m_paSessions;
  KAction *m_paHome;

  KAction *m_paSaveViewProfile;

  KAction *m_paSplitViewHor;
  KAction *m_paSplitViewVer;
  KAction *m_paAddTab;
  KAction *m_paDuplicateTab;
  KAction *m_paBreakOffTab;
  KAction *m_paRemoveView;
  KAction *m_paRemoveTab;
  KAction *m_paRemoveOtherTabs;
  KAction *m_paActivateNextTab;
  KAction *m_paActivatePrevTab;

  KAction *m_paSaveRemoveViewProfile;
  KActionMenu *m_pamLoadViewProfile;

  KToggleAction *m_paLockView;
  KToggleAction *m_paLinkView;
  KAction *m_paReload;
  KAction *m_paReloadAllTabs;
  KAction *m_paUndo;
  KAction *m_paCut;
  KAction *m_paCopy;
  KAction *m_paPaste;
  KAction *m_paStop;

  KAction *m_paCopyFiles;
  KAction *m_paMoveFiles;

  KAction *m_paMoveTabLeft;
  KAction *m_paMoveTabRight;

  KAction *m_paConfigureExtensions;
  KAction *m_paConfigureSpellChecking;

  KAnimatedButton *m_paAnimatedLogo;

  KBookmarkBar *m_paBookmarkBar;

#if 0
  KToggleAction * m_paFindFiles;
#endif
  KToggleAction *m_ptaUseHTML;

  KToggleAction *m_paShowMenuBar;

  KToggleFullScreenAction *m_ptaFullScreen;

  bool m_bLocationBarConnected:1;
  bool m_bURLEnterLock:1;
  // Global settings
  bool m_bHTMLAllowed:1;
  // Set in constructor, used in slotRunFinished
  bool m_bNeedApplyKonqMainWindowSettings:1;
  bool m_urlCompletionStarted:1;
  bool m_prevMenuBarVisible:1;

  int m_goBuffer;
  Qt::MouseButtons m_goMouseState;
  Qt::KeyboardModifiers m_goKeyboardState;

  MapViews m_mapViews;

  QPointer<KonqView> m_currentView;

  KBookmarkMenu* m_pBookmarkMenu;
  KonqExtendedBookmarkOwner *m_pBookmarksOwner;
  KActionCollection* m_bookmarksActionCollection;
  bool m_bookmarkBarInitialized;

  KonqViewManager *m_pViewManager;
  KonqFrameBase* m_pChildFrame;

  KonqFrameBase* m_pWorkingTab;

    // Store a number of things when opening a popup, they are needed
    // in the slots connected to the popup's actions.
    // TODO: a struct with new/delete to save a bit of memory?
    QString m_popupMimeType;
    KUrl m_popupUrl;
    KFileItemList m_popupItems;
    KParts::OpenUrlArguments m_popupUrlArgs;
    KParts::BrowserArguments m_popupUrlBrowserArgs;

  KCMultiDialog* m_configureDialog;

  QLabel* m_locationLabel;
  QPointer<KonqCombo> m_combo;
  static KConfig *s_comboConfig;
  KUrlCompletion *m_pURLCompletion;
  // just a reference to KonqHistoryManager's completionObject
  static KCompletion *s_pCompletion;

  ToggleViewGUIClient *m_toggleViewGUIClient;

  QString m_initialFrameName;

  QList<QAction *> m_openWithActions;
  KActionMenu *m_openWithMenu;
  KActionMenu *m_viewModeMenu;
  QActionGroup* m_viewModesGroup;
  QActionGroup* m_closedItemsGroup;
  QActionGroup* m_sessionsGroup;

  static QList<KonqMainWindow*> *s_lstViews;

  QString m_currentDir; // stores current dir for relative URLs whenever applicable

    // TODO: move to a KonqPreloadHandler class
  static bool s_preloaded;
  static KonqMainWindow* s_preloadedWindow;

public:

  static QFile *s_crashlog_file;
};

#endif // KONQMAINWINDOW_H
