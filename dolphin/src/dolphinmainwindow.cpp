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

#include "dolphinmainwindow.h"
#include "dolphinviewactionhandler.h"

#include <config-nepomuk.h>

#include "dolphinapplication.h"
#include "dolphinfileplacesview.h"
#include "dolphinnewmenu.h"
#include "dolphinsettings.h"
#include "dolphinsettingsdialog.h"
#include "dolphinstatusbar.h"
#include "dolphinviewcontainer.h"
#include "infosidebarpage.h"
#include "metadatawidget.h"
#include "mainwindowadaptor.h"
#include "treeviewsidebarpage.h"
#include "viewproperties.h"

#ifndef Q_OS_WIN
#include "terminalsidebarpage.h"
#endif

#include "dolphin_generalsettings.h"
#include "dolphin_iconsmodesettings.h"
#include "draganddrophelper.h"

#include <kaction.h>
#include <kactioncollection.h>
#include <kconfig.h>
#include <kdesktopfile.h>
#include <kdeversion.h>
#include <kfiledialog.h>
#include <kfileplacesmodel.h>
#include <kglobal.h>
#include <kicon.h>
#include <kiconloader.h>
#include <kio/netaccess.h>
#include <kinputdialog.h>
#include <klocale.h>
#include <kprotocolmanager.h>
#include <kmenu.h>
#include <kmenubar.h>
#include <kmessagebox.h>
#include <konq_fileitemcapabilities.h>
#include <konqmimedata.h>
#include <kprotocolinfo.h>
#include <krun.h>
#include <kshell.h>
#include <kstandarddirs.h>
#include <kstatusbar.h>
#include <kstandardaction.h>
#include <ktabbar.h>
#include <ktoggleaction.h>
#include <kurlnavigator.h>
#include <kurl.h>
#include <kurlcombobox.h>

#include <QDBusMessage>
#include <QKeyEvent>
#include <QClipboard>
#include <QLineEdit>
#include <QSplitter>
#include <QDockWidget>

DolphinMainWindow::DolphinMainWindow(int id) :
    KXmlGuiWindow(0),
    m_newMenu(0),
    m_showMenuBar(0),
    m_tabBar(0),
    m_activeViewContainer(0),
    m_centralWidgetLayout(0),
    m_id(id),
    m_tabIndex(0),
    m_viewTab(),
    m_actionHandler(0)
{
    setObjectName("Dolphin#");

    m_viewTab.append(ViewTab());

    new MainWindowAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QString("/dolphin/MainWindow%1").arg(m_id), this);

    KIO::FileUndoManager* undoManager = KIO::FileUndoManager::self();
    undoManager->setUiInterface(new UndoUiInterface());

    connect(undoManager, SIGNAL(undoAvailable(bool)),
            this, SLOT(slotUndoAvailable(bool)));
    connect(undoManager, SIGNAL(undoTextChanged(const QString&)),
            this, SLOT(slotUndoTextChanged(const QString&)));
    connect(undoManager, SIGNAL(jobRecordingStarted(CommandType)),
            this, SLOT(clearStatusBar()));
    connect(undoManager, SIGNAL(jobRecordingFinished(CommandType)),
            this, SLOT(showCommand(CommandType)));
    connect(DolphinSettings::instance().placesModel(), SIGNAL(errorMessage(const QString&)),
            this, SLOT(showErrorMessage(const QString&)));
    connect(&DragAndDropHelper::instance(), SIGNAL(errorMessage(const QString&)),
            this, SLOT(showErrorMessage(const QString&)));
}

DolphinMainWindow::~DolphinMainWindow()
{
    DolphinApplication::app()->removeMainWindow(this);
}

void DolphinMainWindow::toggleViews()
{
    if (m_viewTab[m_tabIndex].primaryView == 0) {
        return;
    }

    // move secondary view from the last position of the splitter
    // to the first position
    m_viewTab[m_tabIndex].splitter->insertWidget(0, m_viewTab[m_tabIndex].secondaryView);

    DolphinViewContainer* container = m_viewTab[m_tabIndex].primaryView;
    m_viewTab[m_tabIndex].primaryView = m_viewTab[m_tabIndex].secondaryView;
    m_viewTab[m_tabIndex].secondaryView = container;
}

void DolphinMainWindow::showCommand(CommandType command)
{
    DolphinStatusBar* statusBar = m_activeViewContainer->statusBar();
    switch (command) {
    case KIO::FileUndoManager::Copy:
        statusBar->setMessage(i18nc("@info:status", "Copy operation completed."),
                              DolphinStatusBar::OperationCompleted);
        break;
    case KIO::FileUndoManager::Move:
        statusBar->setMessage(i18nc("@info:status", "Move operation completed."),
                              DolphinStatusBar::OperationCompleted);
        break;
    case KIO::FileUndoManager::Link:
        statusBar->setMessage(i18nc("@info:status", "Link operation completed."),
                              DolphinStatusBar::OperationCompleted);
        break;
    case KIO::FileUndoManager::Trash:
        statusBar->setMessage(i18nc("@info:status", "Move to trash operation completed."),
                              DolphinStatusBar::OperationCompleted);
        break;
    case KIO::FileUndoManager::Rename:
        statusBar->setMessage(i18nc("@info:status", "Renaming operation completed."),
                              DolphinStatusBar::OperationCompleted);
        break;

    case KIO::FileUndoManager::Mkdir:
        statusBar->setMessage(i18nc("@info:status", "Created folder."),
                              DolphinStatusBar::OperationCompleted);
        break;

    default:
        break;
    }
}

void DolphinMainWindow::refreshViews()
{
    Q_ASSERT(m_viewTab[m_tabIndex].primaryView != 0);

    // remember the current active view, as because of
    // the refreshing the active view might change to
    // the secondary view
    DolphinViewContainer* activeViewContainer = m_activeViewContainer;

    const int tabCount = m_viewTab.count();
    for (int i = 0; i < tabCount; ++i) {
        m_viewTab[i].primaryView->refresh();
        if (m_viewTab[i].secondaryView != 0) {
            m_viewTab[i].secondaryView->refresh();
        }
    }

    setActiveViewContainer(activeViewContainer);
}

void DolphinMainWindow::pasteIntoFolder()
{
    m_activeViewContainer->view()->pasteIntoFolder();
}

void DolphinMainWindow::changeUrl(const KUrl& url)
{
    if (!KProtocolManager::supportsListing(url)) {
        // The URL navigator only checks for validity, not
        // if the URL can be listed. An error message is
        // shown due to DolphinViewContainer::restoreView().
        return;
    }

    DolphinViewContainer* view = activeViewContainer();
    if (view != 0) {
        view->setUrl(url);
        updateEditActions();
        updateViewActions();
        updateGoActions();
        setCaption(url.fileName());
        if (m_viewTab.count() > 1) {
            m_tabBar->setTabText(m_tabIndex, tabName(url));
        }
        emit urlChanged(url);
    }
}

void DolphinMainWindow::changeSelection(const KFileItemList& selection)
{
    activeViewContainer()->view()->changeSelection(selection);
}

void DolphinMainWindow::slotEditableStateChanged(bool editable)
{
    KToggleAction* editableLocationAction =
        static_cast<KToggleAction*>(actionCollection()->action("editable_location"));
    editableLocationAction->setChecked(editable);
}

void DolphinMainWindow::slotSelectionChanged(const KFileItemList& selection)
{
    updateEditActions();

    Q_ASSERT(m_viewTab[m_tabIndex].primaryView != 0);
    int selectedUrlsCount = m_viewTab[m_tabIndex].primaryView->view()->selectedItemsCount();
    if (m_viewTab[m_tabIndex].secondaryView != 0) {
        selectedUrlsCount += m_viewTab[m_tabIndex].secondaryView->view()->selectedItemsCount();
    }

    QAction* compareFilesAction = actionCollection()->action("compare_files");
    if (selectedUrlsCount == 2) {
        compareFilesAction->setEnabled(isKompareInstalled());
    } else {
        compareFilesAction->setEnabled(false);
    }

#if defined(QUICK_VIEW)
    const bool activeViewHasSelection = (activeViewContainer()->view()->selectedItemsCount() > 0);
    actionCollection()->action("quick_view")->setEnabled(activeViewHasSelection);
#endif

    m_activeViewContainer->updateStatusBar();

    emit selectionChanged(selection);
}

void DolphinMainWindow::slotRequestItemInfo(const KFileItem& item)
{
    emit requestItemInfo(item);
}

void DolphinMainWindow::updateHistory()
{
    const KUrlNavigator* urlNavigator = m_activeViewContainer->urlNavigator();
    const int index = urlNavigator->historyIndex();

    QAction* backAction = actionCollection()->action("go_back");
    if (backAction != 0) {
        backAction->setEnabled(index < urlNavigator->historySize() - 1);
    }

    QAction* forwardAction = actionCollection()->action("go_forward");
    if (forwardAction != 0) {
        forwardAction->setEnabled(index > 0);
    }
}

void DolphinMainWindow::updateFilterBarAction(bool show)
{
    QAction* showFilterBarAction = actionCollection()->action("show_filter_bar");
    showFilterBarAction->setChecked(show);
}

void DolphinMainWindow::openNewMainWindow()
{
    DolphinApplication::app()->createMainWindow()->show();
}

void DolphinMainWindow::openNewTab()
{
    openNewTab(m_activeViewContainer->url());
    m_tabBar->setCurrentIndex(m_viewTab.count() - 1);

    KUrlNavigator* navigator = m_activeViewContainer->urlNavigator();
    if (navigator->isUrlEditable()) {
        // if a new tab is opened and the URL is editable, assure that
        // the user can edit the URL without manually setting the focus
        navigator->setFocus();
    }
}

void DolphinMainWindow::openNewTab(const KUrl& url)
{
    if (m_viewTab.count() == 1) {
        // Only one view is open currently and hence no tab is shown at
        // all. Before creating a tab for 'url', provide a tab for the current URL.
        m_tabBar->addTab(KIcon("folder"), tabName(m_activeViewContainer->url()));
        m_tabBar->blockSignals(false);
    }

    m_tabBar->addTab(KIcon("folder"), tabName(url));

    ViewTab viewTab;
    viewTab.splitter = new QSplitter(this);
    viewTab.primaryView = new DolphinViewContainer(this, viewTab.splitter, url);
    viewTab.primaryView->setActive(false);
    connectViewSignals(viewTab.primaryView);
    viewTab.primaryView->view()->reload();

    m_viewTab.append(viewTab);

    actionCollection()->action("close_tab")->setEnabled(true);

    // provide a split view, if the startup settings are set this way
    const GeneralSettings* generalSettings = DolphinSettings::instance().generalSettings();
    if (generalSettings->splitView()) {
        const int tabIndex = m_viewTab.count() - 1;
        createSecondaryView(tabIndex);
        m_viewTab[tabIndex].secondaryView->setActive(true);
        m_viewTab[tabIndex].isPrimaryViewActive = false;
    }
}

void DolphinMainWindow::activateNextTab()
{
    if (m_viewTab.count() == 1 || m_tabBar->count() < 2) {
        return;
    }

    const int tabIndex = (m_tabBar->currentIndex() + 1) % m_tabBar->count();
    m_tabBar->setCurrentIndex(tabIndex);
}

void DolphinMainWindow::activatePrevTab()
{
    if (m_viewTab.count() == 1 || m_tabBar->count() < 2) {
        return;
    }

    int tabIndex = m_tabBar->currentIndex() - 1;
    if (tabIndex == -1) {
        tabIndex = m_tabBar->count() - 1;
    }
    m_tabBar->setCurrentIndex(tabIndex);
}

void DolphinMainWindow::openInNewTab()
{
    const KFileItemList list = m_activeViewContainer->view()->selectedItems();
    if ((list.count() == 1) && list[0].isDir()) {
        openNewTab(m_activeViewContainer->view()->selectedUrls()[0]);
    }
}

void DolphinMainWindow::openInNewWindow()
{
    const KFileItemList list = m_activeViewContainer->view()->selectedItems();
    if ((list.count() == 1) && list[0].isDir()) {
        DolphinMainWindow* window = DolphinApplication::app()->createMainWindow();
        window->changeUrl(m_activeViewContainer->view()->selectedUrls()[0]);
        window->show();
    }
}

void DolphinMainWindow::toggleActiveView()
{
    if (m_viewTab[m_tabIndex].secondaryView == 0) {
        // only one view is available
        return;
    }

    Q_ASSERT(m_activeViewContainer != 0);
    Q_ASSERT(m_viewTab[m_tabIndex].primaryView != 0);

    DolphinViewContainer* left  = m_viewTab[m_tabIndex].primaryView;
    DolphinViewContainer* right = m_viewTab[m_tabIndex].secondaryView;
    setActiveViewContainer(m_activeViewContainer == right ? left : right);
}

void DolphinMainWindow::closeEvent(QCloseEvent* event)
{
    DolphinSettings& settings = DolphinSettings::instance();
    GeneralSettings* generalSettings = settings.generalSettings();
    generalSettings->setFirstRun(false);

    settings.save();

    KXmlGuiWindow::closeEvent(event);
}

void DolphinMainWindow::saveProperties(KConfigGroup& group)
{
    // TODO: remember tabs
    DolphinViewContainer* cont = m_viewTab[m_tabIndex].primaryView;
    group.writeEntry("Primary Url", cont->url().url());
    group.writeEntry("Primary Editable Url", cont->isUrlEditable());

    cont = m_viewTab[m_tabIndex].secondaryView;
    if (cont != 0) {
        group.writeEntry("Secondary Url", cont->url().url());
        group.writeEntry("Secondary Editable Url", cont->isUrlEditable());
    }
}

void DolphinMainWindow::readProperties(const KConfigGroup& group)
{
    // TODO: read tabs
    DolphinViewContainer* cont = m_viewTab[m_tabIndex].primaryView;

    cont->setUrl(group.readEntry("Primary Url"));
    bool editable = group.readEntry("Primary Editable Url", false);
    cont->urlNavigator()->setUrlEditable(editable);

    cont = m_viewTab[m_tabIndex].secondaryView;
    const QString secondaryUrl = group.readEntry("Secondary Url");
    if (!secondaryUrl.isEmpty()) {
        if (cont == 0) {
            // a secondary view should be shown, but no one is available
            // currently -> create a new view
            toggleSplitView();
            cont = m_viewTab[m_tabIndex].secondaryView;
            Q_ASSERT(cont != 0);
        }

        cont->setUrl(secondaryUrl);
        bool editable = group.readEntry("Secondary Editable Url", false);
        cont->urlNavigator()->setUrlEditable(editable);
    } else if (cont != 0) {
        // no secondary view should be shown, but the default setting shows
        // one already -> close the view
        toggleSplitView();
    }
}

void DolphinMainWindow::updateNewMenu()
{
    m_newMenu->slotCheckUpToDate();
    m_newMenu->setPopupFiles(activeViewContainer()->url());
}

void DolphinMainWindow::quit()
{
    close();
}

void DolphinMainWindow::showErrorMessage(const QString& message)
{
    if (!message.isEmpty()) {
        DolphinStatusBar* statusBar = m_activeViewContainer->statusBar();
        statusBar->setMessage(message, DolphinStatusBar::Error);
    }
}

void DolphinMainWindow::slotUndoAvailable(bool available)
{
    QAction* undoAction = actionCollection()->action(KStandardAction::name(KStandardAction::Undo));
    if (undoAction != 0) {
        undoAction->setEnabled(available);
    }
}

void DolphinMainWindow::slotUndoTextChanged(const QString& text)
{
    QAction* undoAction = actionCollection()->action(KStandardAction::name(KStandardAction::Undo));
    if (undoAction != 0) {
        undoAction->setText(text);
    }
}

void DolphinMainWindow::undo()
{
    clearStatusBar();
    KIO::FileUndoManager::self()->uiInterface()->setParentWidget(this);
    KIO::FileUndoManager::self()->undo();
}

void DolphinMainWindow::cut()
{
    m_activeViewContainer->view()->cutSelectedItems();
}

void DolphinMainWindow::copy()
{
    m_activeViewContainer->view()->copySelectedItems();
}

void DolphinMainWindow::paste()
{
    m_activeViewContainer->view()->paste();
}

void DolphinMainWindow::updatePasteAction()
{
    QAction* pasteAction = actionCollection()->action(KStandardAction::name(KStandardAction::Paste));
    QPair<bool, QString> pasteInfo = m_activeViewContainer->view()->pasteInfo();
    pasteAction->setEnabled(pasteInfo.first);
    pasteAction->setText(pasteInfo.second);
}

void DolphinMainWindow::selectAll()
{
    clearStatusBar();

    // if the URL navigator is editable and focused, select the whole
    // URL instead of all items of the view

    KUrlNavigator* urlNavigator = m_activeViewContainer->urlNavigator();
    QLineEdit* lineEdit = urlNavigator->editor()->lineEdit();
    const bool selectUrl = urlNavigator->isUrlEditable() &&
                           lineEdit->hasFocus();
    if (selectUrl) {
        lineEdit->selectAll();
    } else {
        m_activeViewContainer->view()->selectAll();
    }
}

void DolphinMainWindow::invertSelection()
{
    clearStatusBar();
    m_activeViewContainer->view()->invertSelection();
}

void DolphinMainWindow::toggleSplitView()
{
    if (m_viewTab[m_tabIndex].secondaryView == 0) {
        createSecondaryView(m_tabIndex);
        setActiveViewContainer(m_viewTab[m_tabIndex].secondaryView);
    } else if (m_activeViewContainer == m_viewTab[m_tabIndex].secondaryView) {
        // remove secondary view
        m_viewTab[m_tabIndex].secondaryView->close();
        m_viewTab[m_tabIndex].secondaryView->deleteLater();
        m_viewTab[m_tabIndex].secondaryView = 0;

        setActiveViewContainer(m_viewTab[m_tabIndex].primaryView);
    } else {
        // The primary view is active and should be closed. Hence from a users point of view
        // the content of the secondary view should be moved to the primary view.
        // From an implementation point of view it is more efficient to close
        // the primary view and exchange the internal pointers afterwards.

        m_viewTab[m_tabIndex].primaryView->close();
        m_viewTab[m_tabIndex].primaryView->deleteLater();
        m_viewTab[m_tabIndex].primaryView = m_viewTab[m_tabIndex].secondaryView;
        m_viewTab[m_tabIndex].secondaryView = 0;

        setActiveViewContainer(m_viewTab[m_tabIndex].primaryView);
    }

    updateViewActions();
}

void DolphinMainWindow::reloadView()
{
    clearStatusBar();
    m_activeViewContainer->view()->reload();
}

void DolphinMainWindow::stopLoading()
{
}

void DolphinMainWindow::toggleFilterBarVisibility(bool show)
{
    m_activeViewContainer->showFilterBar(show);
}

void DolphinMainWindow::toggleEditLocation()
{
    clearStatusBar();

    QAction* action = actionCollection()->action("editable_location");
    KUrlNavigator* urlNavigator = m_activeViewContainer->urlNavigator();
    urlNavigator->setUrlEditable(action->isChecked());
}

void DolphinMainWindow::replaceLocation()
{
    KUrlNavigator* navigator = m_activeViewContainer->urlNavigator();
    navigator->setUrlEditable(true);
    navigator->setFocus();

    // select the whole text of the combo box editor
    QLineEdit* lineEdit = navigator->editor()->lineEdit();
    const QString text = lineEdit->text();
    lineEdit->setSelection(0, text.length());
}

void DolphinMainWindow::goBack()
{
    clearStatusBar();
    m_activeViewContainer->urlNavigator()->goBack();
}

void DolphinMainWindow::goForward()
{
    clearStatusBar();
    m_activeViewContainer->urlNavigator()->goForward();
}

void DolphinMainWindow::goUp()
{
    clearStatusBar();
    m_activeViewContainer->urlNavigator()->goUp();
}

void DolphinMainWindow::goHome()
{
    clearStatusBar();
    m_activeViewContainer->urlNavigator()->goHome();
}

void DolphinMainWindow::compareFiles()
{
    // The method is only invoked if exactly 2 files have
    // been selected. The selected files may be:
    // - both in the primary view
    // - both in the secondary view
    // - one in the primary view and the other in the secondary
    //   view
    Q_ASSERT(m_viewTab[m_tabIndex].primaryView != 0);

    KUrl urlA;
    KUrl urlB;
    KUrl::List urls = m_viewTab[m_tabIndex].primaryView->view()->selectedUrls();

    switch (urls.count()) {
    case 0: {
        Q_ASSERT(m_viewTab[m_tabIndex].secondaryView != 0);
        urls = m_viewTab[m_tabIndex].secondaryView->view()->selectedUrls();
        Q_ASSERT(urls.count() == 2);
        urlA = urls[0];
        urlB = urls[1];
        break;
    }

    case 1: {
        urlA = urls[0];
        Q_ASSERT(m_viewTab[m_tabIndex].secondaryView != 0);
        urls = m_viewTab[m_tabIndex].secondaryView->view()->selectedUrls();
        Q_ASSERT(urls.count() == 1);
        urlB = urls[0];
        break;
    }

    case 2: {
        urlA = urls[0];
        urlB = urls[1];
        break;
    }

    default: {
        // may not happen: compareFiles may only get invoked if 2
        // files are selected
        Q_ASSERT(false);
    }
    }

    QString command("kompare -c \"");
    command.append(urlA.pathOrUrl());
    command.append("\" \"");
    command.append(urlB.pathOrUrl());
    command.append('\"');
    KRun::runCommand(command, "Kompare", "kompare", this);
}

void DolphinMainWindow::quickView()
{
    const KUrl::List urls = activeViewContainer()->view()->selectedUrls();
    Q_ASSERT(urls.count() > 0);

    QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.plasma", "/Previewer", "", "openFile");
    foreach (const KUrl& url, urls) {
        msg.setArguments(QList<QVariant>() << url.prettyUrl());
        QDBusConnection::sessionBus().send(msg);
    }
}

void DolphinMainWindow::toggleShowMenuBar()
{
    const bool visible = menuBar()->isVisible();
    menuBar()->setVisible(!visible);
}

void DolphinMainWindow::editSettings()
{
    DolphinSettingsDialog* dialog = new DolphinSettingsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void DolphinMainWindow::setActiveTab(int index)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < m_viewTab.count());
    if (index == m_tabIndex) {
        return;
    }

    // hide current tab content
    ViewTab& hiddenTab = m_viewTab[m_tabIndex];
    hiddenTab.isPrimaryViewActive = hiddenTab.primaryView->isActive();
    hiddenTab.primaryView->setActive(false);
    if (hiddenTab.secondaryView != 0) {
        hiddenTab.secondaryView->setActive(false);
    }
    QSplitter* splitter = m_viewTab[m_tabIndex].splitter;
    splitter->hide();
    m_centralWidgetLayout->removeWidget(splitter);

    // show active tab content
    m_tabIndex = index;

    ViewTab& viewTab = m_viewTab[index];
    m_centralWidgetLayout->addWidget(viewTab.splitter);
    viewTab.primaryView->show();
    if (viewTab.secondaryView != 0) {
        viewTab.secondaryView->show();
    }
    viewTab.splitter->show();

    setActiveViewContainer(viewTab.isPrimaryViewActive ? viewTab.primaryView :
                                                         viewTab.secondaryView);
}

void DolphinMainWindow::closeTab()
{
    closeTab(m_tabBar->currentIndex());
}

void DolphinMainWindow::closeTab(int index)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < m_viewTab.count());
    if (m_viewTab.count() == 1) {
          // the last tab may never get closed
        return;
    }

    if (index == m_tabIndex) {
        // The tab that should be closed is the active tab. Activate the
        // previous tab before closing the tab.
        m_tabBar->setCurrentIndex((index > 0) ? index - 1 : 1);
    }

    // delete tab
    m_viewTab[index].primaryView->deleteLater();
    if (m_viewTab[index].secondaryView != 0) {
        m_viewTab[index].secondaryView->deleteLater();
    }
    m_viewTab[index].splitter->deleteLater();
    m_viewTab.erase(m_viewTab.begin() + index);

    m_tabBar->blockSignals(true);
    m_tabBar->removeTab(index);

    if (m_tabIndex > index) {
        m_tabIndex--;
        Q_ASSERT(m_tabIndex >= 0);
    }

    // if only one tab is left, also remove the tab entry so that
    // closing the last tab is not possible
    if (m_viewTab.count() == 1) {
        m_tabBar->removeTab(0);
        actionCollection()->action("close_tab")->setEnabled(false);
    } else {
        m_tabBar->blockSignals(false);
    }
}

void DolphinMainWindow::openTabContextMenu(int index, const QPoint& pos)
{
    KMenu menu(this);

    QAction* newTabAction = menu.addAction(KIcon("tab-new"), i18nc("@action:inmenu", "New Tab"));
    newTabAction->setShortcut(actionCollection()->action("new_tab")->shortcut());

    QAction* closeOtherTabsAction = menu.addAction(KIcon("tab-close-other"), i18nc("@action:inmenu", "Close Other Tabs"));

    QAction* closeTabAction = menu.addAction(KIcon("tab-close"), i18nc("@action:inmenu", "Close Tab"));
    closeTabAction->setShortcut(actionCollection()->action("close_tab")->shortcut());

    QAction* selectedAction = menu.exec(pos);
    if (selectedAction == newTabAction) {
        const ViewTab& tab = m_viewTab[index];
        Q_ASSERT(tab.primaryView != 0);
        const KUrl url = (tab.secondaryView != 0) && tab.secondaryView->isActive() ?
                         tab.secondaryView->url() : tab.primaryView->url();
        openNewTab(url);
        m_tabBar->setCurrentIndex(m_viewTab.count() - 1);
    } else if (selectedAction == closeOtherTabsAction) {
        const int count = m_tabBar->count();
        for (int i = 0; i < index; ++i) {
            closeTab(0);
        }
        for (int i = index + 1; i < count; ++i) {
            closeTab(1);
        }
    } else if (selectedAction == closeTabAction) {
        closeTab(index);
    }
}

void DolphinMainWindow::handlePlacesClick(const KUrl& url, Qt::MouseButtons buttons)
{
    if (buttons & Qt::MidButton) {
        openNewTab(url);
        m_tabBar->setCurrentIndex(m_viewTab.count() - 1);
    } else {
        changeUrl(url);
    }
}

void DolphinMainWindow::slotTestCanDecode(const QDragMoveEvent* event, bool& canDecode)
{
    canDecode = KUrl::List::canDecode(event->mimeData());
}

void DolphinMainWindow::init()
{
    DolphinSettings& settings = DolphinSettings::instance();

    // Check whether Dolphin runs the first time. If yes then
    // a proper default window size is given at the end of DolphinMainWindow::init().
    GeneralSettings* generalSettings = settings.generalSettings();
    const bool firstRun = generalSettings->firstRun();
    if (firstRun) {
        generalSettings->setViewPropsTimestamp(QDateTime::currentDateTime());
    }

    setAcceptDrops(true);

    m_viewTab[m_tabIndex].splitter = new QSplitter(this);

    setupActions();

    const KUrl& homeUrl = generalSettings->homeUrl();
    setCaption(homeUrl.fileName());
    m_actionHandler = new DolphinViewActionHandler(actionCollection(), this);
    connect(m_actionHandler, SIGNAL(actionBeingHandled()), SLOT(clearStatusBar()));
    ViewProperties props(homeUrl);
    m_viewTab[m_tabIndex].primaryView = new DolphinViewContainer(this,
                                                                 m_viewTab[m_tabIndex].splitter,
                                                                 homeUrl);

    m_activeViewContainer = m_viewTab[m_tabIndex].primaryView;
    connectViewSignals(m_activeViewContainer);
    DolphinView* view = m_activeViewContainer->view();
    view->reload();
    m_activeViewContainer->show();
    m_actionHandler->setCurrentView(view);

    m_tabBar = new KTabBar(this);
    m_tabBar->setCloseButtonEnabled(true);
    connect(m_tabBar, SIGNAL(currentChanged(int)),
            this, SLOT(setActiveTab(int)));
    connect(m_tabBar, SIGNAL(closeRequest(int)),
            this, SLOT(closeTab(int)));
    connect(m_tabBar, SIGNAL(contextMenu(int, const QPoint&)),
            this, SLOT(openTabContextMenu(int, const QPoint&)));
    connect(m_tabBar, SIGNAL(newTabRequest()),
            this, SLOT(openNewTab()));
    connect(m_tabBar, SIGNAL(testCanDecode(const QDragMoveEvent*, bool&)),
            this, SLOT(slotTestCanDecode(const QDragMoveEvent*, bool&)));
    m_tabBar->blockSignals(true);  // signals get unblocked after at least 2 tabs are open

    QWidget* centralWidget = new QWidget(this);
    m_centralWidgetLayout = new QVBoxLayout(centralWidget);
    m_centralWidgetLayout->setSpacing(0);
    m_centralWidgetLayout->setMargin(0);
    m_centralWidgetLayout->addWidget(m_tabBar);
    m_centralWidgetLayout->addWidget(m_viewTab[m_tabIndex].splitter);

    setCentralWidget(centralWidget);
    setupDockWidgets();
    emit urlChanged(homeUrl);

    setupGUI(Keys | Save | Create | ToolBar);

    stateChanged("new_file");

    QClipboard* clipboard = QApplication::clipboard();
    connect(clipboard, SIGNAL(dataChanged()),
            this, SLOT(updatePasteAction()));
    updatePasteAction();
    updateGoActions();

    if (generalSettings->splitView()) {
        toggleSplitView();
    }
    updateViewActions();

    QAction* showFilterBarAction = actionCollection()->action("show_filter_bar");
    showFilterBarAction->setChecked(generalSettings->filterBar());

    if (firstRun) {
        // assure a proper default size if Dolphin runs the first time
        resize(750, 500);
    }

    m_showMenuBar->setChecked(!menuBar()->isHidden());  // workaround for bug #171080
}

void DolphinMainWindow::setActiveViewContainer(DolphinViewContainer* viewContainer)
{
    Q_ASSERT(viewContainer != 0);
    Q_ASSERT((viewContainer == m_viewTab[m_tabIndex].primaryView) ||
             (viewContainer == m_viewTab[m_tabIndex].secondaryView));
    if (m_activeViewContainer == viewContainer) {
        return;
    }

    m_activeViewContainer->setActive(false);
    m_activeViewContainer = viewContainer;

    // Activating the view container might trigger a recursive setActiveViewContainer() call
    // inside DolphinMainWindow::toggleActiveView() when having a split view. Temporary
    // disconnect the activated() signal in this case:
    disconnect(m_activeViewContainer->view(), SIGNAL(activated()), this, SLOT(toggleActiveView()));
    m_activeViewContainer->setActive(true);
    connect(m_activeViewContainer->view(), SIGNAL(activated()), this, SLOT(toggleActiveView()));

    m_actionHandler->setCurrentView(viewContainer->view());

    updateHistory();
    updateEditActions();
    updateViewActions();
    updateGoActions();

    const KUrl& url = m_activeViewContainer->url();
    setCaption(url.fileName());
    if (m_viewTab.count() > 1) {
        m_tabBar->setTabText(m_tabIndex, tabName(url));
    }

    emit urlChanged(url);
}

void DolphinMainWindow::setupActions()
{
    // setup 'File' menu
    m_newMenu = new DolphinNewMenu(this, this);
    KMenu* menu = m_newMenu->menu();
    menu->setTitle(i18nc("@title:menu Create new folder, file, link, etc.", "Create New"));
    menu->setIcon(KIcon("document-new"));
    connect(menu, SIGNAL(aboutToShow()),
            this, SLOT(updateNewMenu()));

    KAction* newWindow = actionCollection()->addAction("new_window");
    newWindow->setIcon(KIcon("window-new"));
    newWindow->setText(i18nc("@action:inmenu File", "New &Window"));
    newWindow->setShortcut(Qt::CTRL | Qt::Key_N);
    connect(newWindow, SIGNAL(triggered()), this, SLOT(openNewMainWindow()));

    KAction* newTab = actionCollection()->addAction("new_tab");
    newTab->setIcon(KIcon("tab-new"));
    newTab->setText(i18nc("@action:inmenu File", "New Tab"));
    newTab->setShortcut(KShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_N, Qt::CTRL | Qt::Key_T));
    connect(newTab, SIGNAL(triggered()), this, SLOT(openNewTab()));

    QAction* closeTab = new QAction(KIcon("tab-close"), i18nc("@action:inmenu File", "Close Tab"), this);
    closeTab->setShortcut(Qt::CTRL | Qt::Key_W);
    closeTab->setEnabled(false);
    connect(closeTab, SIGNAL(triggered()), this, SLOT(closeTab()));
    actionCollection()->addAction("close_tab", closeTab);

    KStandardAction::quit(this, SLOT(quit()), actionCollection());

    // setup 'Edit' menu
    KStandardAction::undo(this,
                          SLOT(undo()),
                          actionCollection());

    // need to remove shift+del from cut action, else the shortcut for deletejob
    // doesn't work
    KAction* cut = KStandardAction::cut(this, SLOT(cut()), actionCollection());
    KShortcut cutShortcut = cut->shortcut();
    cutShortcut.remove(Qt::SHIFT + Qt::Key_Delete, KShortcut::KeepEmpty);
    cut->setShortcut(cutShortcut);
    KStandardAction::copy(this, SLOT(copy()), actionCollection());
    KStandardAction::paste(this, SLOT(paste()), actionCollection());

    KAction* selectAll = actionCollection()->addAction("select_all");
    selectAll->setText(i18nc("@action:inmenu Edit", "Select All"));
    selectAll->setShortcut(Qt::CTRL + Qt::Key_A);
    connect(selectAll, SIGNAL(triggered()), this, SLOT(selectAll()));

    KAction* invertSelection = actionCollection()->addAction("invert_selection");
    invertSelection->setText(i18nc("@action:inmenu Edit", "Invert Selection"));
    invertSelection->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_A);
    connect(invertSelection, SIGNAL(triggered()), this, SLOT(invertSelection()));

    // setup 'View' menu
    // (note that most of it is set up in DolphinViewActionHandler)

    KAction* split = actionCollection()->addAction("split_view");
    split->setShortcut(Qt::Key_F3);
    updateSplitAction();
    connect(split, SIGNAL(triggered()), this, SLOT(toggleSplitView()));

    KAction* reload = actionCollection()->addAction("reload");
    reload->setText(i18nc("@action:inmenu View", "Reload"));
    reload->setShortcut(Qt::Key_F5);
    reload->setIcon(KIcon("view-refresh"));
    connect(reload, SIGNAL(triggered()), this, SLOT(reloadView()));

    KAction* stop = actionCollection()->addAction("stop");
    stop->setText(i18nc("@action:inmenu View", "Stop"));
    stop->setIcon(KIcon("process-stop"));
    connect(stop, SIGNAL(triggered()), this, SLOT(stopLoading()));

    KToggleAction* showFullLocation = actionCollection()->add<KToggleAction>("editable_location");
    showFullLocation->setText(i18nc("@action:inmenu Navigation Bar", "Editable Location"));
    showFullLocation->setShortcut(Qt::CTRL | Qt::Key_L);
    connect(showFullLocation, SIGNAL(triggered()), this, SLOT(toggleEditLocation()));

    KAction* replaceLocation = actionCollection()->addAction("replace_location");
    replaceLocation->setText(i18nc("@action:inmenu Navigation Bar", "Replace Location"));
    replaceLocation->setShortcut(Qt::Key_F6);
    connect(replaceLocation, SIGNAL(triggered()), this, SLOT(replaceLocation()));

    // setup 'Go' menu
    KAction* backAction = KStandardAction::back(this, SLOT(goBack()), actionCollection());
    KShortcut backShortcut = backAction->shortcut();
    backShortcut.setAlternate(Qt::Key_Backspace);
    backAction->setShortcut(backShortcut);

    KStandardAction::forward(this, SLOT(goForward()), actionCollection());
    KStandardAction::up(this, SLOT(goUp()), actionCollection());
    KStandardAction::home(this, SLOT(goHome()), actionCollection());

    // setup 'Tools' menu
    KToggleAction* showFilterBar = actionCollection()->add<KToggleAction>("show_filter_bar");
    showFilterBar->setText(i18nc("@action:inmenu Tools", "Show Filter Bar"));
    showFilterBar->setShortcut(Qt::CTRL | Qt::Key_I);
    connect(showFilterBar, SIGNAL(triggered(bool)), this, SLOT(toggleFilterBarVisibility(bool)));

    KAction* compareFiles = actionCollection()->addAction("compare_files");
    compareFiles->setText(i18nc("@action:inmenu Tools", "Compare Files"));
    compareFiles->setIcon(KIcon("kompare"));
    compareFiles->setEnabled(false);
    connect(compareFiles, SIGNAL(triggered()), this, SLOT(compareFiles()));

    // disabled Quick View
#if defined(QUICK_VIEW)
    KAction* quickView = actionCollection()->addAction("quick_view");
    quickView->setText(i18nc("@action:inmenu Tools", "Quick View"));
    quickView->setIcon(KIcon("view-preview"));
    quickView->setShortcut(Qt::CTRL + Qt::Key_Return);
    quickView->setEnabled(false);
    connect(quickView, SIGNAL(triggered()), this, SLOT(quickView()));
#endif

    // setup 'Settings' menu
    m_showMenuBar = KStandardAction::showMenubar(this, SLOT(toggleShowMenuBar()), actionCollection());
    KStandardAction::preferences(this, SLOT(editSettings()), actionCollection());

    // not in menu actions
    KAction* activateNextTab = actionCollection()->addAction("activatenexttab");
    activateNextTab->setText(i18nc("@action:inmenu", "Activate Next Tab"));
    connect(activateNextTab, SIGNAL(triggered()), SLOT(activateNextTab()));
    activateNextTab->setShortcuts(QApplication::isRightToLeft() ? KStandardShortcut::tabPrev() :
                                                                  KStandardShortcut::tabNext());

    KAction* activatePrevTab = actionCollection()->addAction("activateprevtab");
    activatePrevTab->setText(i18nc("@action:inmenu", "Activate Previous Tab"));
    connect(activatePrevTab, SIGNAL(triggered()), SLOT(activatePrevTab()));
    activatePrevTab->setShortcuts(QApplication::isRightToLeft() ? KStandardShortcut::tabNext() :
                                                                  KStandardShortcut::tabPrev());

    // for context menu
    KAction* openInNewTab = actionCollection()->addAction("open_in_new_tab");
    openInNewTab->setText(i18nc("@action:inmenu", "Open in New Tab"));
    openInNewTab->setIcon(KIcon("tab-new"));
    connect(openInNewTab, SIGNAL(triggered()), this, SLOT(openInNewTab()));

    KAction* openInNewWindow = actionCollection()->addAction("open_in_new_window");
    openInNewWindow->setText(i18nc("@action:inmenu", "Open in New Window"));
    openInNewWindow->setIcon(KIcon("window-new"));
    connect(openInNewWindow, SIGNAL(triggered()), this, SLOT(openInNewWindow()));
}

void DolphinMainWindow::setupDockWidgets()
{
    // setup "Information"
    QDockWidget* infoDock = new QDockWidget(i18nc("@title:window", "Information"));
    infoDock->setObjectName("infoDock");
    infoDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    SidebarPage* infoWidget = new InfoSidebarPage(infoDock);
    infoDock->setWidget(infoWidget);

    infoDock->toggleViewAction()->setText(i18nc("@title:window", "Information"));
    infoDock->toggleViewAction()->setShortcut(Qt::Key_F11);
    actionCollection()->addAction("show_info_panel", infoDock->toggleViewAction());

    addDockWidget(Qt::RightDockWidgetArea, infoDock);
    connect(this, SIGNAL(urlChanged(KUrl)),
            infoWidget, SLOT(setUrl(KUrl)));
    connect(this, SIGNAL(selectionChanged(KFileItemList)),
            infoWidget, SLOT(setSelection(KFileItemList)));
    connect(this, SIGNAL(requestItemInfo(KFileItem)),
            infoWidget, SLOT(requestDelayedItemInfo(KFileItem)));

    // setup "Tree View"
    QDockWidget* treeViewDock = new QDockWidget(i18nc("@title:window", "Folders"));
    treeViewDock->setObjectName("treeViewDock");
    treeViewDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    TreeViewSidebarPage* treeWidget = new TreeViewSidebarPage(treeViewDock);
    treeViewDock->setWidget(treeWidget);

    treeViewDock->toggleViewAction()->setText(i18nc("@title:window", "Folders"));
    treeViewDock->toggleViewAction()->setShortcut(Qt::Key_F7);
    actionCollection()->addAction("show_folders_panel", treeViewDock->toggleViewAction());

    addDockWidget(Qt::LeftDockWidgetArea, treeViewDock);
    connect(this, SIGNAL(urlChanged(KUrl)),
            treeWidget, SLOT(setUrl(KUrl)));
    connect(treeWidget, SIGNAL(changeUrl(KUrl, Qt::MouseButtons)),
            this, SLOT(handlePlacesClick(KUrl, Qt::MouseButtons)));
    connect(treeWidget, SIGNAL(changeSelection(KFileItemList)),
            this, SLOT(changeSelection(KFileItemList)));

    // setup "Terminal"
#ifndef Q_OS_WIN
    QDockWidget* terminalDock = new QDockWidget(i18nc("@title:window Shell terminal", "Terminal"));
    terminalDock->setObjectName("terminalDock");
    terminalDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    SidebarPage* terminalWidget = new TerminalSidebarPage(terminalDock);
    terminalDock->setWidget(terminalWidget);

    connect(terminalWidget, SIGNAL(hideTerminalSidebarPage()), terminalDock, SLOT(hide()));

    terminalDock->toggleViewAction()->setText(i18nc("@title:window Shell terminal", "Terminal"));
    terminalDock->toggleViewAction()->setShortcut(Qt::Key_F4);
    actionCollection()->addAction("show_terminal_panel", terminalDock->toggleViewAction());

    addDockWidget(Qt::BottomDockWidgetArea, terminalDock);
    connect(this, SIGNAL(urlChanged(KUrl)),
            terminalWidget, SLOT(setUrl(KUrl)));
#endif

    const bool firstRun = DolphinSettings::instance().generalSettings()->firstRun();
    if (firstRun) {
        treeViewDock->hide();
#ifndef Q_OS_WIN
        terminalDock->hide();
#endif
    }

    QDockWidget* placesDock = new QDockWidget(i18nc("@title:window", "Places"));
    placesDock->setObjectName("placesDock");
    placesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    DolphinFilePlacesView* placesView = new DolphinFilePlacesView(placesDock);
    placesDock->setWidget(placesView);
    placesView->setModel(DolphinSettings::instance().placesModel());
    placesView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    placesDock->toggleViewAction()->setText(i18nc("@title:window", "Places"));
    placesDock->toggleViewAction()->setShortcut(Qt::Key_F9);
    actionCollection()->addAction("show_places_panel", placesDock->toggleViewAction());

    addDockWidget(Qt::LeftDockWidgetArea, placesDock);
    connect(placesView, SIGNAL(urlChanged(KUrl, Qt::MouseButtons)),
            this, SLOT(handlePlacesClick(KUrl, Qt::MouseButtons)));
    connect(this, SIGNAL(urlChanged(KUrl)),
            placesView, SLOT(setUrl(KUrl)));
}

void DolphinMainWindow::updateEditActions()
{
    const KFileItemList list = m_activeViewContainer->view()->selectedItems();
    if (list.isEmpty()) {
        stateChanged("has_no_selection");
    } else {
        stateChanged("has_selection");

        KActionCollection* col = actionCollection();
        QAction* renameAction      = col->action("rename");
        QAction* moveToTrashAction = col->action("move_to_trash");
        QAction* deleteAction      = col->action("delete");
        QAction* cutAction         = col->action(KStandardAction::name(KStandardAction::Cut));
        QAction* deleteWithTrashShortcut = col->action("delete_shortcut"); // see DolphinViewActionHandler

        KonqFileItemCapabilities capabilities(list);
        const bool enableMoveToTrash = capabilities.isLocal() && capabilities.supportsMoving();

        renameAction->setEnabled(capabilities.supportsMoving());
        moveToTrashAction->setEnabled(enableMoveToTrash);
        deleteAction->setEnabled(capabilities.supportsDeleting());
        deleteWithTrashShortcut->setEnabled(capabilities.supportsDeleting() && !enableMoveToTrash);
        cutAction->setEnabled(capabilities.supportsMoving());
    }
    updatePasteAction();
}

void DolphinMainWindow::updateViewActions()
{
    m_actionHandler->updateViewActions();

    QAction* showFilterBarAction = actionCollection()->action("show_filter_bar");
    showFilterBarAction->setChecked(m_activeViewContainer->isFilterBarVisible());

    updateSplitAction();

    QAction* editableLocactionAction = actionCollection()->action("editable_location");
    const KUrlNavigator* urlNavigator = m_activeViewContainer->urlNavigator();
    editableLocactionAction->setChecked(urlNavigator->isUrlEditable());
}

void DolphinMainWindow::updateGoActions()
{
    QAction* goUpAction = actionCollection()->action(KStandardAction::name(KStandardAction::Up));
    const KUrl& currentUrl = m_activeViewContainer->url();
    goUpAction->setEnabled(currentUrl.upUrl() != currentUrl);
}

void DolphinMainWindow::clearStatusBar()
{
    m_activeViewContainer->statusBar()->clear();
}

void DolphinMainWindow::connectViewSignals(DolphinViewContainer* container)
{
    connect(container, SIGNAL(showFilterBarChanged(bool)),
            this, SLOT(updateFilterBarAction(bool)));

    DolphinView* view = container->view();
    connect(view, SIGNAL(selectionChanged(KFileItemList)),
            this, SLOT(slotSelectionChanged(KFileItemList)));
    connect(view, SIGNAL(requestItemInfo(KFileItem)),
            this, SLOT(slotRequestItemInfo(KFileItem)));
    connect(view, SIGNAL(activated()),
            this, SLOT(toggleActiveView()));
    connect(view, SIGNAL(tabRequested(const KUrl&)),
            this, SLOT(openNewTab(const KUrl&)));

    const KUrlNavigator* navigator = container->urlNavigator();
    connect(navigator, SIGNAL(urlChanged(const KUrl&)),
            this, SLOT(changeUrl(const KUrl&)));
    connect(navigator, SIGNAL(historyChanged()),
            this, SLOT(updateHistory()));
    connect(navigator, SIGNAL(editableStateChanged(bool)),
            this, SLOT(slotEditableStateChanged(bool)));
}

void DolphinMainWindow::updateSplitAction()
{
    QAction* splitAction = actionCollection()->action("split_view");
    if (m_viewTab[m_tabIndex].secondaryView != 0) {
        if (m_activeViewContainer == m_viewTab[m_tabIndex].secondaryView) {
            splitAction->setText(i18nc("@action:intoolbar Close right view", "Close"));
            splitAction->setIcon(KIcon("view-right-close"));
        } else {
            splitAction->setText(i18nc("@action:intoolbar Close left view", "Close"));
            splitAction->setIcon(KIcon("view-left-close"));
        }
    } else {
        splitAction->setText(i18nc("@action:intoolbar Split view", "Split"));
        splitAction->setIcon(KIcon("view-right-new"));
    }
}

QString DolphinMainWindow::tabName(const KUrl& url) const
{
    QString name;
    if (url.equals(KUrl("file:///"))) {
        name = "/";
    } else {
        name = url.fileName();
        if (name.isEmpty()) {
            name = url.protocol();
        }
    }
    return name;
}

bool DolphinMainWindow::isKompareInstalled() const
{
    static bool initialized = false;
    static bool installed = false;
    if (!initialized) {
        // TODO: maybe replace this approach later by using a menu
        // plugin like kdiff3plugin.cpp
        installed = !KGlobal::dirs()->findExe("kompare").isEmpty();
        initialized = true;
    }
    return installed;
}

void DolphinMainWindow::createSecondaryView(int tabIndex)
{
    QSplitter* splitter = m_viewTab[tabIndex].splitter;
    const int newWidth = (m_viewTab[tabIndex].primaryView->width() - splitter->handleWidth()) / 2;

    const DolphinView* view = m_viewTab[tabIndex].primaryView->view();
    m_viewTab[tabIndex].secondaryView = new DolphinViewContainer(this, 0, view->rootUrl());
    splitter->addWidget(m_viewTab[tabIndex].secondaryView);
    splitter->setSizes(QList<int>() << newWidth << newWidth);
    connectViewSignals(m_viewTab[tabIndex].secondaryView);
    m_viewTab[tabIndex].secondaryView->view()->reload();
    m_viewTab[tabIndex].secondaryView->setActive(false);
    m_viewTab[tabIndex].secondaryView->show();
}

DolphinMainWindow::UndoUiInterface::UndoUiInterface() :
    KIO::FileUndoManager::UiInterface()
{
}

DolphinMainWindow::UndoUiInterface::~UndoUiInterface()
{
}

void DolphinMainWindow::UndoUiInterface::jobError(KIO::Job* job)
{
    DolphinMainWindow* mainWin= qobject_cast<DolphinMainWindow *>(parentWidget());
    if (mainWin) {
        DolphinStatusBar* statusBar = mainWin->activeViewContainer()->statusBar();
        statusBar->setMessage(job->errorString(), DolphinStatusBar::Error);
    } else {
        KIO::FileUndoManager::UiInterface::jobError(job);
    }
}

#include "dolphinmainwindow.moc"
