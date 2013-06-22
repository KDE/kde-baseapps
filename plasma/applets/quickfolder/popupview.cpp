/*
 *   Copyright © 2009 Fredrik Höglund <fredrik@kde.org>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public License
 *   along with this library; see the file COPYING.LIB.  If not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#include "popupview.h"
#include "dialogshadows_p.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsWidget>
#include <QItemSelectionModel>
#include <QStyle>

#include <KAction>
#include <KBookmarkManager>
#include <KDesktopFile>
#include <KDirModel>
#include <kfileitemactions.h>
#include <KFileItemDelegate>
#include <kfileitemlistproperties.h>
#include <kfilepreviewgenerator.h>
#include <knewmenu.h>
#include <KWindowSystem>
#include <KMenu>

#include <kio/fileundomanager.h>
#include <kio/paste.h>

#include <konqmimedata.h>
#include <konq_operations.h>
#include <konq_popupmenu.h>

#include "dirlister.h"
#include "folderviewadapter.h"
#include "iconview.h"
#include "proxymodel.h"

#include <Plasma/Applet>
#include <Plasma/BusyWidget>
#include <Plasma/FrameSvg>
#include <Plasma/Theme>
#include <Plasma/WindowEffects>


QTime PopupView::s_lastOpenClose;

PopupView::PopupView(const QModelIndex &index, const QPoint &pos,
                     const bool &showPreview, const QStringList &previewPlugins,
                     const IconView *parentView)
    : QWidget(0, Qt::X11BypassWindowManagerHint),
      m_view(0),
      m_parentView(parentView),
      m_busyWidget(0),
      m_iconView(0),
      m_parentViewModel(0),
      m_dirModel(0),
      m_model(0),
      m_actionCollection(this),
      m_newMenu(0),
      m_itemActions(0),
      m_showingMenu(false),
      m_showPreview(showPreview),
      m_delayedClose(false),
      m_previewPlugins(previewPlugins)
{
    setAttribute(Qt::WA_TranslucentBackground);
#ifdef Q_WS_X11
    if (KWindowSystem::compositingActive()) {
        setAttribute(Qt::WA_NoSystemBackground, false);
    }
#endif

#ifdef Q_WS_WIN
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
#endif

    KWindowSystem::setState(effectiveWinId(), NET::SkipTaskbar | NET::SkipPager);

    setAcceptDrops(true);

    QPalette pal = palette();
    pal.setColor(backgroundRole(), Qt::transparent);
    pal.setColor(QPalette::Text, Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
    setPalette(pal);

    m_parentViewModel = static_cast<const ProxyModel*>(index.model());

    KFileItem item = m_parentViewModel->itemForIndex(index);
    if (item.isDesktopFile()) {
        KDesktopFile file(item.localPath());
        m_url = file.readUrl();
    } else {
        m_url = item.targetUrl();
    }

    m_background = new Plasma::FrameSvg(this);
    m_background->setImagePath("dialogs/background");

    int left   = m_background->marginSize(Plasma::LeftMargin);
    int top    = m_background->marginSize(Plasma::TopMargin);
    int right  = m_background->marginSize(Plasma::RightMargin);
    int bottom = m_background->marginSize(Plasma::BottomMargin);

    setContentsMargins(left, top, right, bottom);

    resize(parentView->sizeForRowsColumns(2, 3) + QSize(left + right, top + bottom));

    const QRect available = QApplication::desktop()->availableGeometry(pos);
    QPoint pt = pos;

    if (pt.x() + width() > available.right()) {
        pt.rx() -= width();
    }
    if (pt.x() < available.left()) {
        pt.rx() = available.left();
    }

    if (pt.y() + height() > available.bottom()) {
        pt.ry() -= height();
    }
    if (pt.y() < available.top()) {
        pt.ry() = available.top();
    }

    Plasma::WindowEffects::overrideShadow(winId(), true);

    move(pt);
    show();

    QTimer::singleShot(10, this, SLOT(init()));
    s_lastOpenClose.restart();
}

PopupView::~PopupView()
{
    delete m_newMenu;
    s_lastOpenClose.restart();
}

void PopupView::delayedHide()
{
    if (!m_iconView || !m_iconView->dragInProgress()) {
        m_hideTimer.start(400, this);
    }
}

bool PopupView::dragInProgress()
{
    return m_iconView && m_iconView->dragInProgress();
}

void PopupView::init()
{
    if (m_model) {
        return;
    }

    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->viewport()->setAutoFillBackground(false);
    m_view->setGeometry(contentsRect());
    m_view->show();

    DirLister *lister = new DirLister(this);
    lister->setDelayedMimeTypes(true);
    lister->setAutoErrorHandlingEnabled(false, 0);
    lister->openUrl(m_url);

    m_dirModel = new KDirModel(this);
    m_dirModel->setDropsAllowed(KDirModel::DropOnDirectory | KDirModel::DropOnLocalExecutable);
    m_dirModel->setDirLister(lister);

    m_model = new ProxyModel(this);
    m_model->setSourceModel(m_dirModel);
    m_model->setSortLocaleAware(m_parentViewModel->isSortLocaleAware());
    m_model->setParseDesktopFiles(m_parentViewModel->parseDesktopFiles());
    m_model->setFilterMode(m_parentViewModel->ProxyModel::NoFilter);
    m_model->setDynamicSortFilter(true);

    if (!m_parentViewModel->dynamicSortFilter()) {
        m_model->setSortDirectoriesFirst(true);
        m_model->sort(int(KDirModel::Name), Qt::AscendingOrder);
    } else {
        m_model->setSortDirectoriesFirst(m_parentViewModel->sortDirectoriesFirst());
        m_model->sort(m_parentViewModel->sortColumn(), m_parentViewModel->sortOrder());
    }

    m_delegate = new KFileItemDelegate(this);
    m_selectionModel = new QItemSelectionModel(m_model, this);

    m_iconView = new IconView(0);
    m_iconView->setModel(m_model);
    m_iconView->setItemDelegate(m_delegate);
    m_iconView->setSelectionModel(m_selectionModel);
    m_iconView->setFont(m_parentView->font());
    m_iconView->setPalette(palette());
    m_iconView->setDrawShadows(m_parentView->drawShadows());
    m_iconView->setIconSize(m_parentView->iconSize());
    m_iconView->setGridSize(m_parentView->gridSize());
    m_iconView->setTextLineCount(m_parentView->textLineCount());
    m_iconView->setWordWrap(m_parentView->wordWrap());
    m_iconView->setIconsMoveable(false);
    m_iconView->setClickToViewFolders(false);
    m_iconView->setShowSelectionMarker(m_parentView->showSelectionMarker());

    connect(m_iconView, SIGNAL(activated(QModelIndex)), SLOT(activated(QModelIndex)));
    connect(m_iconView, SIGNAL(contextMenuRequest(QWidget*,QPoint)), SLOT(contextMenuRequest(QWidget*,QPoint)));
    connect(m_iconView, SIGNAL(busy(bool)), SLOT(setBusy(bool)));
    connect(m_iconView, SIGNAL(popupViewClosed()), SLOT(maybeClose()));

    FolderViewAdapter *adapter = new FolderViewAdapter(m_iconView);
    m_previewGenerator = new KFilePreviewGenerator(adapter, m_model);
    m_previewGenerator->setPreviewShown(m_showPreview);
    m_previewGenerator->setEnabledPlugins(m_previewPlugins);

    m_iconView->setGeometry(contentsRect());
    m_iconView->show();

    m_scene->addItem(m_iconView);
    setBusy(true);
}

void PopupView::createActions()
{
    // Remove the Shift+Delete shortcut from the cut action, since it's used for deleting files
    KAction *cut = KStandardAction::cut(this, SLOT(cut()), this);
    KShortcut cutShortCut = cut->shortcut();
    cutShortCut.remove(Qt::SHIFT + Qt::Key_Delete);
    cut->setShortcut(cutShortCut);

    KAction *copy = KStandardAction::copy(this, SLOT(copy()), this);

    KIO::FileUndoManager *manager = KIO::FileUndoManager::self();

    KAction *undo = KStandardAction::undo(manager, SLOT(undo()), this);
    connect(manager, SIGNAL(undoAvailable(bool)), undo, SLOT(setEnabled(bool)));
    connect(manager, SIGNAL(undoTextChanged(QString)), SLOT(undoTextChanged(QString)));
    undo->setEnabled(manager->undoAvailable());

    KAction *paste = KStandardAction::paste(this, SLOT(paste()), this);
    KAction *pasteTo = KStandardAction::paste(this, SLOT(pasteTo()), this);
    pasteTo->setEnabled(false); // Only enabled during popupMenu()

    QString actionText = KIO::pasteActionText();
    if (!actionText.isEmpty()) {
        paste->setText(actionText);
    } else {
        paste->setEnabled(false);
    }

    KAction *rename = new KAction(KIcon("edit-rename"), i18n("&Rename"), this);
    rename->setShortcut(Qt::Key_F2);
    connect(rename, SIGNAL(triggered()), SLOT(renameSelectedIcon()));

    KAction *trash = new KAction(KIcon("user-trash"), i18n("&Move to Trash"), this);
    trash->setShortcut(Qt::Key_Delete);
    connect(trash, SIGNAL(triggered(Qt::MouseButtons,Qt::KeyboardModifiers)),
            SLOT(moveToTrash(Qt::MouseButtons,Qt::KeyboardModifiers)));

    KAction *emptyTrash = new KAction(KIcon("trash-empty"), i18n("&Empty Trash Bin"), this);
    KConfig trashConfig("trashrc", KConfig::SimpleConfig);
    emptyTrash->setEnabled(!trashConfig.group("Status").readEntry("Empty", true));
    connect(emptyTrash, SIGNAL(triggered()), SLOT(emptyTrashBin()));

    KAction *del = new KAction(i18n("&Delete"), this);
    del->setIcon(KIcon("edit-delete"));
    del->setShortcut(Qt::SHIFT + Qt::Key_Delete);
    connect(del, SIGNAL(triggered()), SLOT(deleteSelectedIcons()));

    // Create the new menu
    m_newMenu = new KNewFileMenu(&m_actionCollection, "new_menu", this);
    connect(m_newMenu->menu(), SIGNAL(aboutToShow()), this, SLOT(aboutToShowCreateNew()));

    m_actionCollection.addAction("undo", undo);
    m_actionCollection.addAction("cut", cut);
    m_actionCollection.addAction("copy", copy);
    m_actionCollection.addAction("paste", paste);
    m_actionCollection.addAction("pasteto", pasteTo);
    m_actionCollection.addAction("rename", rename);
    m_actionCollection.addAction("trash", trash);
    m_actionCollection.addAction("del", del);
    m_actionCollection.addAction("empty_trash", emptyTrash);
}

void PopupView::contextMenuRequest(QWidget *widget, const QPoint& screenPos)
{
    showContextMenu(widget, screenPos, m_selectionModel->selectedIndexes());
}

void PopupView::showContextMenu(QWidget *widget, const QPoint &screenPos, const QList<QModelIndex> &indexes)
{
    Q_UNUSED(widget)
    // contextMenuRequest is only called from the icon view, which is created in init()
    // which mean m_model should always be initialized
    Q_ASSERT(m_model);

    if (indexes.isEmpty()) {
        return;
    }

    if (m_actionCollection.isEmpty()) {
        createActions();
    }

    KFileItemList items;
    bool hasRemoteFiles = false;
    bool isTrashLink = false;

    foreach (const QModelIndex &index, m_selectionModel->selectedIndexes()) {
        KFileItem item = m_model->itemForIndex(index);
        if (!item.isNull()) {
            hasRemoteFiles |= item.localPath().isEmpty();
            items.append(item);
        }
    }

    // Check if we're showing the menu for the trash link
    if (items.count() == 1 && items.at(0).isDesktopFile()) {
        KDesktopFile file(items.at(0).localPath());
        if (file.readType() == "Link" && file.readUrl() == "trash:/") {
            isTrashLink = true;
        }
    }

    QAction *pasteTo = m_actionCollection.action("pasteto");
    if (pasteTo) {
        if (QAction *paste = m_actionCollection.action("paste")) {
            pasteTo->setEnabled(paste->isEnabled());
            pasteTo->setText(paste->text());
        }
    }

    QList<QAction*> editActions;
    editActions.append(m_actionCollection.action("rename"));

    KConfigGroup configGroup(KGlobal::config(), "KDE");
    bool showDeleteCommand = configGroup.readEntry("ShowDeleteCommand", false);

    // Don't add the "Move to Trash" action if we're showing the menu for the trash link
    if (!isTrashLink) {
        if (!hasRemoteFiles) {
            editActions.append(m_actionCollection.action("trash"));
        } else {
            showDeleteCommand = true;
        }
    }
    if (showDeleteCommand) {
        editActions.append(m_actionCollection.action("del"));
    }

    KParts::BrowserExtension::ActionGroupMap actionGroups;
    actionGroups.insert("editactions", editActions);

    KParts::BrowserExtension::PopupFlags flags = KParts::BrowserExtension::ShowProperties;
    flags |= KParts::BrowserExtension::ShowUrlOperations;

    // m_newMenu can be NULL here but KonqPopupMenu does handle this.
    KonqPopupMenu *contextMenu = new KonqPopupMenu(items, m_url, m_actionCollection, m_newMenu,
                                                   KonqPopupMenu::ShowNewWindow, flags,
                                                   QApplication::desktop(),
                                                   KBookmarkManager::userBookmarksManager(),
                                                   actionGroups);

    connect(contextMenu->fileItemActions(), SIGNAL(openWithDialogAboutToBeShown()), this, SLOT(openWithDialogAboutToShow()));

    m_showingMenu = true;
    contextMenu->exec(screenPos);
    delete contextMenu;
    m_showingMenu = false;

    if (pasteTo) {
        pasteTo->setEnabled(false);
    }

    if (m_delayedClose) {
        m_delayedClose = false;
        closeThisAndParentPopup();
    }
}

KUrl::List PopupView::selectedUrls() const
{
    Q_ASSERT(m_model);

    KUrl::List urls;
    foreach (const QModelIndex &index, m_selectionModel->selectedIndexes())
    {
        KFileItem item = m_model->itemForIndex(index);
        // Prefer the local URL if there is one, since we can't trash remote URL's
        const QString path = item.localPath();
        if (!path.isEmpty()) {
            urls.append(path);
        } else {
            urls.append(item.url());
        }
    }
    return urls;
}

void PopupView::cut()
{
    QMimeData *mimeData = m_model->mimeData(m_selectionModel->selectedIndexes());
    KonqMimeData::addIsCutSelection(mimeData, true);
    QApplication::clipboard()->setMimeData(mimeData);
}

void PopupView::copy()
{
    QMimeData *mimeData = m_model->mimeData(m_selectionModel->selectedIndexes());
    QApplication::clipboard()->setMimeData(mimeData);
}

void PopupView::paste()
{
    KonqOperations::doPaste(QApplication::desktop(), m_url);
}

void PopupView::pasteTo()
{
    KUrl::List urls = selectedUrls();
    Q_ASSERT(urls.count() == 1);
    KonqOperations::doPaste(QApplication::desktop(), urls.first());
}

void PopupView::moveToTrash(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(buttons)
    if (!m_iconView->renameInProgress()) {
        KonqOperations::Operation op = (modifiers & Qt::ShiftModifier) ?
                KonqOperations::DEL : KonqOperations::TRASH;

        KonqOperations::del(QApplication::desktop(), op, selectedUrls());
    }
}

void PopupView::deleteSelectedIcons()
{
    if (!m_iconView->renameInProgress()) {
        KonqOperations::del(QApplication::desktop(), KonqOperations::DEL, selectedUrls());
    }
}

void PopupView::renameSelectedIcon()
{
    activateWindow();
    m_iconView->renameSelectedIcon();
}

void PopupView::activated(const QModelIndex &index)
{
    const KFileItem item = m_model->itemForIndex(index);
    item.run();

    closeThisAndParentPopup();
}

void PopupView::openWithDialogAboutToShow()
{
    m_delayedClose = true;
    hideThisAndParentPopup();
}

void PopupView::setBusy(bool busy)
{
    m_busy = busy;
    if (busy && !m_busyWidget) {
        QTimer::singleShot(100, this, SLOT(createBusyWidgetIfNeeded()));
    } else {
        delete m_busyWidget;
        m_busyWidget = 0;
    }
}

void PopupView::createBusyWidgetIfNeeded()
{
    if (m_busy && !m_busyWidget) {
        const int size = qMin(width(), height()) * .3;
        m_busyWidget = new Plasma::BusyWidget;
        m_busyWidget->setGeometry(QStyle::alignedRect(layoutDirection(), Qt::AlignCenter, QSize(size, size), contentsRect()));
        m_scene->addItem(m_busyWidget);
    }
}

void PopupView::emptyTrashBin()
{
    KonqOperations::emptyTrash(QApplication::desktop());
}

void PopupView::undoTextChanged(const QString &text)
{
    if (QAction *action = m_actionCollection.action("undo")) {
        action->setText(text);
    }
}

void PopupView::aboutToShowCreateNew()
{
    if (m_newMenu) {
        m_newMenu->checkUpToDate();
        m_newMenu->setPopupFiles(m_url);
    }
}

void PopupView::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)

    m_background->resizeFrame(rect().size());

    if (m_view) {
        m_view->setGeometry(contentsRect());
    }

    if (KWindowSystem::compositingActive()) {
        Plasma::WindowEffects::enableBlurBehind(winId(), true, m_background->mask());
    } else {
        setMask(m_background->mask());
    }
}

void PopupView::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)

    DialogShadows::self()->addWindow(this);
}

void PopupView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    m_background->paintFrame(&p);
}

void PopupView::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_model) {
        init();
    }

    if (m_actionCollection.isEmpty()) {
        createActions();
    }

    KFileItem rootItem = m_model->itemForIndex(QModelIndex());
    //The root item is invalid (non-existent)
    if (rootItem.isNull()) {
        return;
    }
    
    QMenu menu;
    menu.addAction(m_actionCollection.action("new_menu"));
    menu.addSeparator();
    menu.addAction(m_actionCollection.action("undo"));
    menu.addAction(m_actionCollection.action("paste"));
    menu.addSeparator();

    // Add an action for opening the folder in the preferred application.
    if (!m_itemActions) {
        // Create a new KFileItem to prevent the target URL in the root item
        // from being used. In this case we want the configured URL instead.
        KFileItem item(rootItem.mode(), rootItem.permissions(), m_url);

        KFileItemListProperties itemList(KFileItemList() << item);

        m_itemActions = new KFileItemActions(this);
        m_itemActions->setItemListProperties(itemList);
    }
    menu.addAction(m_itemActions->preferredOpenWithAction(QString()));

    if (m_url.protocol() == "trash") {
        menu.addAction(m_actionCollection.action("empty_trash"));
    }

    m_showingMenu = true;
    menu.exec(event->globalPos());
    m_showingMenu = false;
}

// This function calls a given method in the parent PopupView, and returns true
// if successful or false otherwise.
bool PopupView::callOnParent(const char *method)
{
    // Since the scene is a child of the popup view, we can get to the parent view easily
    PopupView *parentView = qobject_cast<PopupView*>(m_parentView->scene()->parent());

    if (parentView) {
        // We use a delayed call to give enter and leave events time be delivered
        QMetaObject::invokeMethod(parentView, method, Qt::QueuedConnection);
        return true;
    }

    return false;
}

void PopupView::maybeClose()
{
    if (!underMouse() && !m_showingMenu &&
        (!m_iconView || (!m_iconView->isUnderMouse() && !m_iconView->dragInProgress())) &&
        !callOnParent("maybeClose") && !m_hideTimer.isActive()) {
        m_hideTimer.start(400, this);
    }
}

void PopupView::closeThisAndParentPopup() {
    hide();
    deleteLater();
    callOnParent("closeThisAndParentPopup");
}

void PopupView::hideThisAndParentPopup()
{
    hide();
    callOnParent("hideThisAndParentPopup");
}

void PopupView::cancelHideTimer()
{
    m_hideTimer.stop();

    // Propagate the call down the chain of popups
    callOnParent("cancelHideTimer");
}

void PopupView::enterEvent(QEvent *event)
{
    Q_UNUSED(event)

    // Make sure that any hide timer down the popup chain is stopped
    cancelHideTimer();
}

void PopupView::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)

    // The popups are normally closed by the icon views that created them
    // in response to hover events, but when the cursor leaves a popup and
    // enters a widget that isn't an icon view in the popup chain, that
    // mechanism doesn't work.
    //
    // To make sure that the popups are closed when this happens, we call
    // maybeClose() which checks if the popup is under the mouse cursor,
    // and if it isn't it calls maybeClose() in the next popup in the chain
    // and so on.  If no popups in the chain is under the mouse cursor,
    // the root popup will start the hide timer which will close the whole 
    // chain when it fires. 
    if (!m_iconView || !m_iconView->popupVisible()) {
        maybeClose();
    }
}

void PopupView::dragEnterEvent(QDragEnterEvent *event)
{
    m_hideTimer.stop();
    callOnParent("cancelHideTimer");

    // If the popup is open during a drag and drop operation,
    // assume that we accept the mimetype.
    event->setAccepted(true);
}

void PopupView::dragLeaveEvent(QDragLeaveEvent *event)
{
    if (!m_iconView || !m_iconView->popupVisible()) {
        maybeClose();
    }

    // If the popup is open during a drag and drop operation,
    // assume that we accept the mimetype.
    event->setAccepted(true);
}

void PopupView::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_hideTimer.timerId()) {
        m_hideTimer.stop();
        emit requestClose();
    }
}

#include "popupview.moc"

