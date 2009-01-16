/***************************************************************************
 *   Copyright (C) 2006 by Peter Penz <peter.penz@gmx.at>                  *
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

#include "treeviewsidebarpage.h"

#include "dolphinmodel.h"
#include "dolphinsortfilterproxymodel.h"
#include "dolphinview.h"
#include "dolphinsettings.h"
#include "dolphin_folderspanelsettings.h"
#include "dolphin_generalsettings.h"
#include "draganddrophelper.h"
#include "folderexpander.h"
#include "renamedialog.h"
#include "sidebartreeview.h"
#include "treeviewcontextmenu.h"

#include <kfileplacesmodel.h>
#include <kdirlister.h>
#include <kfileitem.h>
#include <konq_operations.h>

#include <QApplication>
#include <QItemSelection>
#include <QTreeView>
#include <QBoxLayout>
#include <QModelIndex>
#include <QScrollBar>
#include <QTimer>

TreeViewSidebarPage::TreeViewSidebarPage(QWidget* parent) :
    SidebarPage(parent),
    m_setLeafVisible(false),
    m_mouseButtons(Qt::NoButton),
    m_dirLister(0),
    m_dolphinModel(0),
    m_proxyModel(0),
    m_treeView(0),
    m_leafDir()
{
    setLayoutDirection(Qt::LeftToRight);
}

TreeViewSidebarPage::~TreeViewSidebarPage()
{
    FoldersPanelSettings::self()->writeConfig();

    delete m_proxyModel;
    m_proxyModel = 0;
    delete m_dolphinModel;
    m_dolphinModel = 0;
    m_dirLister = 0; // deleted by m_dolphinModel
}

QSize TreeViewSidebarPage::sizeHint() const
{
    return QSize(200, 400);
}

void TreeViewSidebarPage::setShowHiddenFiles(bool show)
{
    FoldersPanelSettings::setShowHiddenFiles(show);
    if (m_dirLister != 0) {
        m_dirLister->setShowingDotFiles(show);
        m_dirLister->openUrl(m_dirLister->url(), KDirLister::Reload);
    }
}

bool TreeViewSidebarPage::showHiddenFiles() const
{
    return FoldersPanelSettings::showHiddenFiles();
}

void TreeViewSidebarPage::rename(const KFileItem& item)
{
    if (DolphinSettings::instance().generalSettings()->renameInline()) {
        const QModelIndex dirIndex = m_dolphinModel->indexForItem(item);
        const QModelIndex proxyIndex = m_proxyModel->mapFromSource(dirIndex);
        m_treeView->edit(proxyIndex);
    } else {
        KFileItemList items;
        items.append(item);
        RenameDialog dialog(this, items);
        if (dialog.exec() == QDialog::Accepted) {
            const QString& newName = dialog.newName();
            if (!newName.isEmpty()) {
                KUrl newUrl = item.url();
                newUrl.setFileName(newName);
                KonqOperations::rename(this, item.url(), newUrl);
            }
        }
    }
}

void TreeViewSidebarPage::setUrl(const KUrl& url)
{
    if (!url.isValid() || (url == SidebarPage::url())) {
        return;
    }

    SidebarPage::setUrl(url);
    if (m_dirLister != 0) {
        m_setLeafVisible = true;
        loadTree(url);
    }
}

void TreeViewSidebarPage::showEvent(QShowEvent* event)
{
    if (event->spontaneous()) {
        SidebarPage::showEvent(event);
        return;
    }

    if (m_dirLister == 0) {
        // Postpone the creating of the dir lister to the first show event.
        // This assures that no performance and memory overhead is given when the TreeView is not
        // used at all (see TreeViewSidebarPage::setUrl()).
        m_dirLister = new KDirLister();
        m_dirLister->setDirOnlyMode(true);
        m_dirLister->setAutoUpdate(true);
        m_dirLister->setMainWindow(window());
        m_dirLister->setDelayedMimeTypes(true);
        m_dirLister->setAutoErrorHandlingEnabled(false, this);
        m_dirLister->setShowingDotFiles(FoldersPanelSettings::showHiddenFiles());

        Q_ASSERT(m_dolphinModel == 0);
        m_dolphinModel = new DolphinModel(this);
        m_dolphinModel->setDirLister(m_dirLister);
        m_dolphinModel->setDropsAllowed(DolphinModel::DropOnDirectory);
        connect(m_dolphinModel, SIGNAL(expand(const QModelIndex&)),
                this, SLOT(expandToDir(const QModelIndex&)));

        Q_ASSERT(m_proxyModel == 0);
        m_proxyModel = new DolphinSortFilterProxyModel(this);
        m_proxyModel->setSourceModel(m_dolphinModel);

        Q_ASSERT(m_treeView == 0);
        m_treeView = new SidebarTreeView(this);
        m_treeView->setModel(m_proxyModel);
        m_proxyModel->setSorting(DolphinView::SortByName);
        m_proxyModel->setSortOrder(Qt::AscendingOrder);

        new FolderExpander(m_treeView, m_proxyModel);

        connect(m_treeView, SIGNAL(activated(const QModelIndex&)),
                this, SLOT(updateActiveView(const QModelIndex&)));
        connect(m_treeView, SIGNAL(urlsDropped(const QModelIndex&, QDropEvent*)),
                this, SLOT(dropUrls(const QModelIndex&, QDropEvent*)));
        connect(m_treeView, SIGNAL(pressed(const QModelIndex&)),
                this, SLOT(updateMouseButtons()));

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setMargin(0);
        layout->addWidget(m_treeView);
    }

    loadTree(url());
    SidebarPage::showEvent(event);
}

void TreeViewSidebarPage::contextMenuEvent(QContextMenuEvent* event)
{
    SidebarPage::contextMenuEvent(event);

    KFileItem item;
    const QModelIndex index = m_treeView->indexAt(event->pos());
    if (index.isValid()) {
        const QModelIndex dolphinModelIndex = m_proxyModel->mapToSource(index);
        item = m_dolphinModel->itemForIndex(dolphinModelIndex);
        emit changeSelection(KFileItemList());
    }

    TreeViewContextMenu contextMenu(this, item);
    contextMenu.open();
}

void TreeViewSidebarPage::updateActiveView(const QModelIndex& index)
{
    const QModelIndex dirIndex = m_proxyModel->mapToSource(index);
    const KFileItem item = m_dolphinModel->itemForIndex(dirIndex);
    if (!item.isNull()) {
        emit changeUrl(item.url(), m_mouseButtons);
    }
}

void TreeViewSidebarPage::dropUrls(const QModelIndex& index, QDropEvent* event)
{
    if (index.isValid()) {
        const QModelIndex dirIndex = m_proxyModel->mapToSource(index);
        KFileItem item = m_dolphinModel->itemForIndex(dirIndex);
        Q_ASSERT(!item.isNull());
        if (item.isDir()) {
            DragAndDropHelper::instance().dropUrls(item, item.url(), event, this);
        }
    }
}

void TreeViewSidebarPage::expandToDir(const QModelIndex& index)
{
    m_treeView->setExpanded(index, true);
    selectLeafDirectory();
    m_treeView->resizeColumnToContents(DolphinModel::Name);
}

void TreeViewSidebarPage::scrollToLeaf()
{
    const QModelIndex dirIndex = m_dolphinModel->indexForUrl(m_leafDir);
    const QModelIndex proxyIndex = m_proxyModel->mapFromSource(dirIndex);
    if (proxyIndex.isValid()) {
        m_treeView->scrollTo(proxyIndex);
    }
}

void TreeViewSidebarPage::updateMouseButtons()
{
    m_mouseButtons = QApplication::mouseButtons();
}

void TreeViewSidebarPage::loadTree(const KUrl& url)
{
    Q_ASSERT(m_dirLister != 0);
    m_leafDir = url;

    KUrl baseUrl;
    if (url.isLocalFile()) {
        // use the root directory as base for local URLs (#150941)
        baseUrl = QDir::rootPath();
    } else {
        // clear the path for non-local URLs and use it as base
        baseUrl = url;
        baseUrl.setPath(QString('/'));
    }

    if (m_dirLister->url() != baseUrl) {
        m_dirLister->stop();
        m_dirLister->openUrl(baseUrl, KDirLister::Reload);
    }
    m_dolphinModel->expandToUrl(m_leafDir);
}

void TreeViewSidebarPage::selectLeafDirectory()
{
    const QModelIndex dirIndex = m_dolphinModel->indexForUrl(m_leafDir);
    const QModelIndex proxyIndex = m_proxyModel->mapFromSource(dirIndex);
    if (!proxyIndex.isValid()) {
        return;
    }

    if (m_setLeafVisible) {
        // Invoke m_treeView->scrollTo(proxyIndex) asynchronously by
        // scrollToLeaf(). This assures that the scrolling is done after
        // the horizontal scrollbar gets visible (otherwise the scrollbar
        // might hide the leaf).
        QTimer::singleShot(100, this, SLOT(scrollToLeaf()));
        m_setLeafVisible = false;
    }

    QItemSelectionModel* selModel = m_treeView->selectionModel();
    selModel->setCurrentIndex(proxyIndex, QItemSelectionModel::ClearAndSelect);
}

#include "treeviewsidebarpage.moc"
