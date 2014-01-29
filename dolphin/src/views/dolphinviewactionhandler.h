/***************************************************************************
 *   Copyright (C) 2008 by David Faure <faure@kde.org>                     *
 *   Copyright (C) 2012 by Peter Penz <peter.penz19@gmail.com>             *
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


#ifndef DOLPHINVIEWACTIONHANDLER_H
#define DOLPHINVIEWACTIONHANDLER_H

#include "libdolphin_export.h"
#include <KActionMenu>
#include <KSelectAction>
#include "views/dolphinview.h"
#include <QObject>

class KToggleAction;
class QAction;
class QActionGroup;
class DolphinView;
class KActionCollection;

/**
 * @short Handles all actions for DolphinView
 *
 * The action handler owns all the actions and slots related to DolphinView,
 * but can the view that is acts upon can be switched to another one
 * (this is used in the case of split views).
 *
 * The purpose of this class is also to share this code between DolphinMainWindow
 * and DolphinPart.
 *
 * @see DolphinView
 * @see DolphinMainWindow
 * @see DolphinPart
 */
class LIBDOLPHINPRIVATE_EXPORT DolphinViewActionHandler : public QObject
{
    Q_OBJECT

public:
    explicit DolphinViewActionHandler(KActionCollection* collection, QObject* parent);

    /**
     * Sets the view that this action handler should work on.
     */
    void setCurrentView(DolphinView* view);

    /**
     * Returns the view that this action handler should work on.
     */
    DolphinView* currentView();

    /**
     * Returns the name of the action for the current viewmode
     */
    QString currentViewModeActionName() const;

    /**
     * Returns m_actionCollection
     */
    KActionCollection* actionCollection();

public Q_SLOTS:
    /**
     * Update all actions in the 'View' menu, i.e. those that depend on the
     * settings in the current view.
     */
    void updateViewActions();

Q_SIGNALS:
    /**
     * Emitted by DolphinViewActionHandler when the user triggered an action.
     * This is only used for clearining the statusbar in DolphinMainWindow.
     */
    void actionBeingHandled();

    /**
     * Emitted if the user requested creating a new directory by the F10 key.
     * The receiver of the signal (DolphinMainWindow or DolphinPart) invokes
     * the method createDirectory of their KNewFileMenu instance.
     */
    void createDirectory();

private Q_SLOTS:
    /**
     * Emitted when the user requested a change of view mode
     */
    void slotViewModeActionTriggered(QAction*);

    /**
     * Let the user input a name for the selected item(s) and trigger
     * a renaming afterwards.
     */
    void slotRename();

    /**
     * Moves the selected items of the active view to the trash.
     * This methods adds "shift means del" handling.
     */
    void slotTrashActivated(Qt::MouseButtons, Qt::KeyboardModifiers);

    /**
     * Deletes the selected items of the active view.
     */
    void slotDeleteItems();

    /**
     * Switches between showing a preview of the file content and showing the icon.
     */
    void togglePreview(bool);

    /** Updates the state of the 'Show preview' menu action. */
    void slotPreviewsShownChanged(bool shown);

    /** Increases the size of the current set view mode. */
    void zoomIn();

    /** Decreases the size of the current set view mode. */
    void zoomOut();

    /** Switches between an ascending and descending sorting order. */
    void toggleSortOrder();

    /** Switches between a separate sorting and a mixed sorting of files and folders. */
    void toggleSortFoldersFirst();

    /**
     * Updates the state of the 'Sort Ascending/Descending' action.
     */
    void slotSortOrderChanged(Qt::SortOrder order);

    /**
     * Updates the state of the 'Sort Folders First' action.
     */
    void slotSortFoldersFirstChanged(bool foldersFirst);

    /**
     * Updates the state of the 'Sort by' actions.
     */
    void slotSortRoleChanged(const QByteArray& role);

    /**
     * Updates the state of the 'Zoom In' and 'Zoom Out' actions.
     */
    void slotZoomLevelChanged(int current, int previous);

    /**
     * Switches on or off the displaying of additional information
     * as specified by \a action.
     */
    void toggleVisibleRole(QAction* action);

    /**
     * Changes the sorting of the current view.
     */
    void slotSortTriggered(QAction*);

    /**
     * Updates the state of the 'Additional Information' actions.
     */
    void slotVisibleRolesChanged(const QList<QByteArray>& current,
                                 const QList<QByteArray>& previous);

    /**
     * Switches between sorting by groups or not.
     */
    void toggleGroupedSorting(bool);

    /**
     * Updates the state of the 'Categorized sorting' menu action.
     */
    void slotGroupedSortingChanged(bool sortCategorized);

    /**
     * Switches between showing and hiding of hidden marked files
     */
    void toggleShowHiddenFiles(bool);

    /**
     * Updates the state of the 'Show hidden files' menu action.
     */
    void slotHiddenFilesShownChanged(bool shown);

    /**
     * Updates the state of the 'Create Folder...' action.
     */
    void slotWriteStateChanged(bool isFolderWritable);

    /**
     * Opens the view properties dialog, which allows to modify the properties
     * of the currently active view.
     */
    void slotAdjustViewProperties();

    /**
     * Connected to the "properties" action.
     * Opens the properties dialog for the selected items of the
     * active view. The properties dialog shows information
     * like name, size and permissions.
     */
    void slotProperties();

private:
    /**
     * Create all the actions.
     * This is called only once (by the constructor)
     */
    void createActions();

    /**
     * Creates an action-group out of all roles from KFileItemModel.
     * Dependent on the group-prefix either a radiobutton-group is
     * created for sorting (prefix is "sort_by_") or a checkbox-group
     * is created for additional information (prefix is "show_").
     * The changes of actions are reported to slotSortTriggered() or
     * toggleAdditionalInfo().
     */
    QActionGroup* createFileItemRolesActionGroup(const QString& groupPrefix);

    /**
     * Returns the "switch to icons mode" action.
     * Helper method for createActions();
     */
    KToggleAction* iconsModeAction();

    /**
     * Returns the "switch to compact mode" action.
     * Helper method for createActions();
     */
    KToggleAction* compactModeAction();

    /**
     * Returns the "switch to details mode" action.
     * Helper method for createActions();
     */
    KToggleAction* detailsModeAction();

    KActionCollection* m_actionCollection;
    DolphinView* m_currentView;

    QHash<QByteArray, KToggleAction*> m_sortByActions;
    QHash<QByteArray, KToggleAction*> m_visibleRoles;
};

#endif /* DOLPHINVIEWACTIONHANDLER_H */
