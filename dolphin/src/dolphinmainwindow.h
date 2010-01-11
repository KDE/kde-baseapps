/***************************************************************************
 *   Copyright (C) 2006 by Peter Penz <peter.penz@gmx.at>                  *
 *   Copyright (C) 2006 by Stefan Monov <logixoul@gmail.com>               *
 *   Copyright (C) 2006 by Cvetoslav Ludmiloff <ludmiloff@gmail.com>       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#ifndef DOLPHIN_MAINWINDOW_H
#define DOLPHIN_MAINWINDOW_H

#include "dolphinview.h"
#include "panels/panel.h"

#include <config-nepomuk.h>

#include <kfileitemdelegate.h>
#include <kio/fileundomanager.h>
#include <ksortablelist.h>
#include <kxmlguiwindow.h>
#include <kactionmenu.h>

#include <QtCore/QList>

typedef KIO::FileUndoManager::CommandType CommandType;

class KAction;
class DolphinViewActionHandler;
class DolphinApplication;
class DolphinSearchBox;
class DolphinSearchOptionsConfigurator;
class DolphinSettingsDialog;
class DolphinViewContainer;
class DolphinRemoteEncoding;
class KNewMenu;
class KTabBar;
class KUrl;
class QSplitter;

/**
 * @short Main window for Dolphin.
 *
 * Handles the menus, toolbars and Dolphin views.
 */
class DolphinMainWindow: public KXmlGuiWindow
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.dolphin.MainWindow")
    Q_PROPERTY(int id READ getId SCRIPTABLE true)
    friend class DolphinApplication;

public:
    virtual ~DolphinMainWindow();

    /**
     * Returns the currently active view.
     * All menu actions are applied to this view. When
     * having a split view setup, the nonactive view
     * is usually shown in darker colors.
     */
    DolphinViewContainer* activeViewContainer() const;

    /**
     * Opens each directory \p in a separate tab. If the "split view"
     * option is enabled, 2 directories are collected within one tab.
     */
    void openDirectories(const QList<KUrl>& dirs);
    
    /**
     * Opens the directory which contains the files \p files
     * and selects all files (implements the --select option
     * of Dolphin).
     */
    void openFiles(const QList<KUrl>& files);

    /**
     * Returns true, if the main window contains two instances
     * of a view container. The active view constainer can be
     * accessed by DolphinMainWindow::activeViewContainer().
     */
    bool isSplit() const;

    /**
     * If the main window contains two instances of a view container
     * (DolphinMainWindow::isSplit() returns true), then the
     * two views get toggled (the right view is on the left, the left
     * view on the right).
     */
    void toggleViews();

    /** Renames the item represented by \a oldUrl to \a newUrl. */
    void rename(const KUrl& oldUrl, const KUrl& newUrl);

    /**
     * Refreshes the views of the main window by recreating them dependent from
     * the given Dolphin settings.
     */
    void refreshViews();

    /**
     * Returns the 'Create New...' sub menu which also can be shared
     * with other menus (e. g. a context menu).
     */
    KNewMenu* newMenu() const;

    /**
     * Returns the 'Show Menubar' action which can be shared with
     * other menus (e. g. a context menu).
     */
    KAction* showMenuBarAction() const;

public slots:
    /**
     * Pastes the clipboard data into the currently selected folder
     * of the active view. If not exactly one folder is selected,
     * no pasting is done at all.
     */
    void pasteIntoFolder();

    /**
     * Returns the main window ID used through DBus.
     */
    int getId() const;

    /**
     * Implementation of the MainWindowAdaptor/QDBusAbstractAdaptor interface.
     * Inform all affected dolphin components (panels, views) of an URL
     * change.
     */
    void changeUrl(const KUrl& url);

    /** Stores all settings and quits Dolphin. */
    void quit();

signals:
    /**
     * Is sent if the selection of the currently active view has
     * been changed.
     */
    void selectionChanged(const KFileItemList& selection);

    /**
     * Is sent if the url of the currently active view has
     * been changed.
     */
    void urlChanged(const KUrl& url);

    /**
     * Is emitted if information of an item is requested to be shown e. g. in the panel.
     * If item is null, no item information request is pending.
     */
    void requestItemInfo(const KFileItem& item);

protected:
    /** @see QWidget::showEvent() */
    virtual void showEvent(QShowEvent* event);

    /** @see QMainWindow::closeEvent() */
    virtual void closeEvent(QCloseEvent* event);

    /** @see KMainWindow::saveProperties() */
    virtual void saveProperties(KConfigGroup& group);

    /** @see KMainWindow::readProperties() */
    virtual void readProperties(const KConfigGroup& group);

private slots:
    void clearStatusBar();

    /** Updates the 'Create New...' sub menu. */
    void updateNewMenu();

    void createDirectory();

    /** Shows the error message in the status bar of the active view. */
    void showErrorMessage(const QString& message);

    /**
     * Updates the state of the 'Undo' menu action dependent
     * from the parameter \a available.
     */
    void slotUndoAvailable(bool available);

    /** Invoked when an action in the recent tabs menu is clicked. */
    void restoreClosedTab(QAction* action);

    /** Sets the text of the 'Undo' menu action to \a text. */
    void slotUndoTextChanged(const QString& text);

    /** Performs the current undo operation. */
    void undo();

    /**
     * Copies all selected items to the clipboard and marks
     * the items as cut.
     */
    void cut();

    /** Copies all selected items to the clipboard. */
    void copy();

    /** Pastes the clipboard data to the active view. */
    void paste();

    /**
     * Updates the text of the paste action dependent from
     * the number of items which are in the clipboard.
     */
    void updatePasteAction();

    /** Selects all items from the active view. */
    void selectAll();

    /**
     * Inverts the selection of all items of the active view:
     * Selected items get nonselected and nonselected items get
     * selected.
     */
    void invertSelection();

    /**
     * Switches between one and two views:
     * If one view is visible, it will get split into two views.
     * If already two views are visible, the nonactivated view gets closed.
     */
    void toggleSplitView();

    /** Reloads the current active view. */
    void reloadView();

    /** Stops the loading process for the current active view. */
    void stopLoading();

    /**
     * Toggles between showing and hiding of the filter bar
     */
    void toggleFilterBarVisibility(bool show);

    /**
     * Toggles between edit and browse mode of the navigation bar.
     */
    void toggleEditLocation();

    /**
     * Switches to the edit mode of the navigation bar and selects
     * the whole URL, so that it can be replaced by the user. If the edit mode is
     * already active, it is assured that the navigation bar get focused.
     */
    void replaceLocation();

    /** Goes back on step of the URL history. */
    void goBack();

    /** Goes forward one step of the URL history. */
    void goForward();

    /** Goes up one hierarchy of the current URL. */
    void goUp();

    /**
     * Open the previous URL in the URL history in a new tab
     * if the middle mouse button is clicked.
     */
    void goBack(Qt::MouseButtons buttons);

    /**
     * Open the next URL in the URL history in a new tab
     * if the middle mouse button is clicked.
     */
    void goForward(Qt::MouseButtons buttons);

    /**
     * Open the URL one hierarchy above the current URL in a new tab
     * if the middle mouse button is clicked.
     */
    void goUp(Qt::MouseButtons buttons);

    /** Goes to the home URL. */
    void goHome();

    /** Opens Kompare for 2 selected files. */
    void compareFiles();

    /**
     * Hides the menu bar if it is visible, makes the menu bar
     * visible if it is hidden.
     */
    void toggleShowMenuBar();

    /** Opens a terminal window for the current location. */
    void openTerminal();

    /** Opens the settings dialog for Dolphin. */
    void editSettings();

    /** Updates the state of the 'Show Full Location' action. */
    void slotEditableStateChanged(bool editable);

    /**
     * Updates the state of the 'Edit' menu actions and emits
     * the signal selectionChanged().
     */
    void slotSelectionChanged(const KFileItemList& selection);

    /** Enables changing of tabs via mouse wheel. */
    void slotWheelMoved(int wheelDelta);

    /** Emits the signal requestItemInfo(). */
    void slotRequestItemInfo(const KFileItem&);

    /**
     * Updates the state of the 'Back' and 'Forward' menu
     * actions corresponding to the current history.
     */
    void updateHistory();

    /** Updates the state of the 'Show filter bar' menu action. */
    void updateFilterBarAction(bool show);

    /** Open a new main window. */
    void openNewMainWindow();

    /** Opens a new view with the current URL that is part of a tab. */
    void openNewTab();

    /**
     * Opens a new tab showing the URL \a url.
     */
    void openNewTab(const KUrl& url);

    void activateNextTab();

    void activatePrevTab();

    /**
     * Opens the selected folder in a new tab.
     */
    void openInNewTab();

    /**
     * Opens the selected folder in a new window.
     */
    void openInNewWindow();

    /** Toggles the active view if two views are shown within the main window. */
    void toggleActiveView();

    /**
     * Indicates in the statusbar that the execution of the command \a command
     * has been finished.
     */
    void showCommand(CommandType command);

    /**
     * Activates the tab with the index \a index, which means that the current view
     * is replaced by the view of the given tab.
     */
    void setActiveTab(int index);

    /** Closes the currently active tab. */
    void closeTab();

    /**
     * Closes the tab with the index \index and activates the tab with index - 1.
     */
    void closeTab(int index);


    /**
     * Opens a context menu for the tab with the index \a index
     * on the position \a pos.
     */
    void openTabContextMenu(int index, const QPoint& pos);

    /**
     * Handles a click on a places item: if the middle mouse button is
     * clicked, a new tab is opened for \a url, otherwise the current
     * view is replaced by \a url.
     */
    void handlePlacesClick(const KUrl& url, Qt::MouseButtons buttons);

    /**
     * Is connected to the KTabBar signal testCanDecode() and adjusts
     * the output parameter \a accept.
     */
    void slotTestCanDecode(const QDragMoveEvent* event, bool& accept);

    /**
     * Is connected with the Dolphin search box and the search configurator
     * and triggers a Nepomuk search.
     */
    void searchItems();

    /**
     * Is connected to the QTabBar signal tabMoved(int from, int to).
     * Reorders the list of tabs after a tab was moved in the tab bar
     * and sets m_tabIndex to the new index of the current tab.
     */
    void slotTabMoved(int from, int to);

    /**
     * Is connected to the searchbox signal 'requestSearchOptions' and
     * takes care to show the search options.
     */
    void showSearchOptions();

    /**
     * If the URL can be listed open it in the current view, otherwise
     * run it through KRun.
     */
    void handleUrl(const KUrl& url);

private:
    DolphinMainWindow(int id);
    void init();

    /**
     * Activates the given view, which means that
     * all menu actions are applied to this view. When
     * having a split view setup, the nonactive view
     * is usually shown in darker colors.
     */
    void setActiveViewContainer(DolphinViewContainer* view);

    void setupActions();
    void setupDockWidgets();
    void updateEditActions();
    void updateViewActions();
    void updateGoActions();

    /**
     * Adds the tab[\a index] to the closed tab menu's list of actions.
     */
    void rememberClosedTab(int index);

    /**
     * Connects the signals from the created DolphinView with
     * the DolphinViewContainer \a container with the corresponding slots of
     * the DolphinMainWindow. This method must be invoked each
     * time a DolphinView has been created.
     */
    void connectViewSignals(DolphinViewContainer* container);

    /**
     * Updates the text of the split action:
     * If two views are shown, the text is set to "Split",
     * otherwise the text is set to "Join". The icon
     * is updated to match with the text and the currently active view.
     */
    void updateSplitAction();

    /** Returns the name of the tab for the URL \a url. */
    QString tabName(const KUrl& url) const;

    bool isKompareInstalled() const;

    void createSecondaryView(int tabIndex);

    /**
     * Helper method for saveProperties() and readProperties(): Returns
     * the property string for a tab with the index \a tabIndex and
     * the property \a property.
     */
    QString tabProperty(const QString& property, int tabIndex) const;

    /**
     * Sets the window caption to url.fileName() if this is non-empty,
     * "/" if the URL is "file:///", and url.protocol() otherwise.
     */
    void setUrlAsCaption(const KUrl& url);

    QString squeezedText(const QString& text) const;

private:
    /**
     * Implements a custom error handling for the undo manager. This
     * assures that all errors are shown in the status bar of Dolphin
     * instead as modal error dialog with an OK button.
     */
    class UndoUiInterface : public KIO::FileUndoManager::UiInterface
    {
    public:
        UndoUiInterface();
        virtual ~UndoUiInterface();
        virtual void jobError(KIO::Job* job);
    };

    KNewMenu* m_newMenu;
    KActionMenu* m_recentTabsMenu;
    KAction* m_showMenuBar;
    KTabBar* m_tabBar;
    DolphinViewContainer* m_activeViewContainer;
    QVBoxLayout* m_centralWidgetLayout;
    DolphinSearchBox* m_searchBox;
    DolphinSearchOptionsConfigurator* m_searchOptionsConfigurator;
    int m_id;

    struct ViewTab
    {
        ViewTab() : isPrimaryViewActive(true), primaryView(0), secondaryView(0), splitter(0) {}
        bool isPrimaryViewActive;
        DolphinViewContainer* primaryView;
        DolphinViewContainer* secondaryView;
        QSplitter* splitter;
    };

    int m_tabIndex;
    QList<ViewTab> m_viewTab;

    DolphinViewActionHandler* m_actionHandler;
    DolphinRemoteEncoding* m_remoteEncoding;
    QPointer<DolphinSettingsDialog> m_settingsDialog;
};

inline DolphinViewContainer* DolphinMainWindow::activeViewContainer() const
{
    return m_activeViewContainer;
}

inline bool DolphinMainWindow::isSplit() const
{
    return m_viewTab[m_tabIndex].secondaryView != 0;
}

inline KNewMenu* DolphinMainWindow::newMenu() const
{
    return m_newMenu;
}

inline KAction* DolphinMainWindow::showMenuBarAction() const
{
    return m_showMenuBar;
}

inline int DolphinMainWindow::getId() const
{
    return m_id;
}

#endif // DOLPHIN_MAINWINDOW_H

