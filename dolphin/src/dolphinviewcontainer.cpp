/***************************************************************************
 *   Copyright (C) 2007 by Peter Penz <peter.penz@gmx.at>                  *
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

#include "dolphinviewcontainer.h"
#include <kprotocolmanager.h>

#include <QtGui/QApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QItemSelection>
#include <QtGui/QBoxLayout>
#include <QtCore/QTimer>
#include <QtGui/QScrollBar>

#include <kdesktopfile.h>
#include <kfileitemdelegate.h>
#include <kfileplacesmodel.h>
#include <kglobalsettings.h>
#include <klocale.h>
#include <kiconeffect.h>
#include <kio/netaccess.h>
#include <kio/previewjob.h>
#include <kmenu.h>
#include <knewmenu.h>
#include <konqmimedata.h>
#include <konq_operations.h>
#include <kshell.h>
#include <kurl.h>
#include <kurlcombobox.h>
#include <kurlnavigator.h>
#include <krun.h>

#include "dolphin_generalsettings.h"
#include "dolphinmainwindow.h"
#include "filterbar/filterbar.h"
#include "search/dolphinsearchbox.h"
#include "settings/dolphinsettings.h"
#include "statusbar/dolphinstatusbar.h"
#include "views/dolphincolumnview.h"
#include "views/dolphindetailsview.h"
#include "views/dolphindirlister.h"
#include "views/dolphinsortfilterproxymodel.h"
#include "views/draganddrophelper.h"
#include "views/dolphiniconsview.h"
#include "views/dolphinmodel.h"
#include "views/dolphinviewcontroller.h"
#include "views/viewmodecontroller.h"
#include "views/viewproperties.h"

DolphinViewContainer::DolphinViewContainer(const KUrl& url, QWidget* parent) :
    QWidget(parent),
    m_topLayout(0),
    m_urlNavigator(0),
    m_searchBox(0),
    m_view(0),
    m_filterBar(0),
    m_statusBar(0),
    m_statusBarTimer(0),
    m_statusBarTimestamp()
{
    hide();

    m_topLayout = new QVBoxLayout(this);
    m_topLayout->setSpacing(0);
    m_topLayout->setMargin(0);

    m_urlNavigator = new KUrlNavigator(DolphinSettings::instance().placesModel(), url, this);
    connect(m_urlNavigator, SIGNAL(urlsDropped(const KUrl&, QDropEvent*)),
            this, SLOT(dropUrls(const KUrl&, QDropEvent*)));
    connect(m_urlNavigator, SIGNAL(activated()),
            this, SLOT(activate()));
    connect(m_urlNavigator->editor(), SIGNAL(completionModeChanged(KGlobalSettings::Completion)),
            this, SLOT(saveUrlCompletionMode(KGlobalSettings::Completion)));

    const GeneralSettings* settings = DolphinSettings::instance().generalSettings();
    m_urlNavigator->setUrlEditable(settings->editableUrl());
    m_urlNavigator->setShowFullPath(settings->showFullPath());
    m_urlNavigator->setHomeUrl(KUrl(settings->homeUrl()));
    KUrlComboBox* editor = m_urlNavigator->editor();
    editor->setCompletionMode(KGlobalSettings::Completion(settings->urlCompletionMode()));

    m_searchBox = new DolphinSearchBox(this);
    m_searchBox->hide();
    connect(m_searchBox, SIGNAL(closeRequest()), this, SLOT(closeSearchBox()));
    connect(m_searchBox, SIGNAL(search(QString)), this, SLOT(startSearching(QString)));
    connect(m_searchBox, SIGNAL(returnPressed(QString)), this, SLOT(requestFocus()));

    DolphinDirLister* dirLister = new DolphinDirLister();
    dirLister->setAutoUpdate(true);
    dirLister->setMainWindow(window());
    dirLister->setDelayedMimeTypes(true);

    DolphinModel* dolphinModel = new DolphinModel(this);
    dolphinModel->setDirLister(dirLister);  // dolphinModel takes ownership of dirLister
    dolphinModel->setDropsAllowed(DolphinModel::DropOnDirectory);

    DolphinSortFilterProxyModel* proxyModel = new DolphinSortFilterProxyModel(this);
    proxyModel->setSourceModel(dolphinModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    // TODO: In the case of the column view the directory lister changes. Let the DolphinView
    // inform the container about this information for KDE SC 4.7
    connect(dirLister, SIGNAL(clear()),
            this, SLOT(delayedStatusBarUpdate()));
    connect(dirLister, SIGNAL(percent(int)),
            this, SLOT(updateProgress(int)));
    connect(dirLister, SIGNAL(itemsDeleted(const KFileItemList&)),
            this, SLOT(delayedStatusBarUpdate()));
    connect(dirLister, SIGNAL(newItems(KFileItemList)),
            this, SLOT(delayedStatusBarUpdate()));
    connect(dirLister, SIGNAL(infoMessage(const QString&)),
            this, SLOT(showInfoMessage(const QString&)));
    connect(dirLister, SIGNAL(errorMessage(const QString&)),
            this, SLOT(showErrorMessage(const QString&)));
    connect(dirLister, SIGNAL(urlIsFileError(const KUrl&)),
            this, SLOT(openFile(const KUrl&)));

    m_view = new DolphinView(this, url, proxyModel);
    connect(m_view, SIGNAL(urlChanged(const KUrl&)),
            m_urlNavigator, SLOT(setUrl(const KUrl&)));
    connect(m_view, SIGNAL(requestItemInfo(KFileItem)),
            this, SLOT(showItemInfo(KFileItem)));
    connect(m_view, SIGNAL(errorMessage(const QString&)),
            this, SLOT(showErrorMessage(const QString&)));
    connect(m_view, SIGNAL(infoMessage(const QString&)),
            this, SLOT(showInfoMessage(const QString&)));
    connect(m_view, SIGNAL(operationCompletedMessage(const QString&)),
            this, SLOT(showOperationCompletedMessage(const QString&)));
    connect(m_view, SIGNAL(itemTriggered(KFileItem)),
            this, SLOT(slotItemTriggered(KFileItem)));
    connect(m_view, SIGNAL(redirection(KUrl, KUrl)),
            this, SLOT(redirect(KUrl, KUrl)));
    connect(m_view, SIGNAL(selectionChanged(const KFileItemList&)),
            this, SLOT(delayedStatusBarUpdate()));
    connect(m_view, SIGNAL(startedPathLoading(KUrl)),
            this, SLOT(slotStartedPathLoading()));
    connect(m_view, SIGNAL(finishedPathLoading(KUrl)),
            this, SLOT(slotFinishedPathLoading()));
    connect(m_view, SIGNAL(writeStateChanged(bool)),
            this, SIGNAL(writeStateChanged(bool)));

    connect(m_urlNavigator, SIGNAL(urlChanged(const KUrl&)),
            this, SLOT(slotUrlNavigatorLocationChanged(const KUrl&)));
    connect(m_urlNavigator, SIGNAL(urlAboutToBeChanged(const KUrl&)),
            this, SLOT(saveViewState()));
    connect(m_urlNavigator, SIGNAL(historyChanged()),
            this, SLOT(slotHistoryChanged()));

    // initialize status bar
    m_statusBar = new DolphinStatusBar(this, m_view);
    connect(m_statusBar, SIGNAL(stopPressed()), this, SLOT(stopLoading()));

    m_statusBarTimer = new QTimer(this);
    m_statusBarTimer->setSingleShot(true);
    m_statusBarTimer->setInterval(300);
    connect(m_statusBarTimer, SIGNAL(timeout()),
            this, SLOT(updateStatusBar()));

    KIO::FileUndoManager* undoManager = KIO::FileUndoManager::self();
    connect(undoManager, SIGNAL(jobRecordingFinished(CommandType)),
            this, SLOT(delayedStatusBarUpdate()));

    // initialize filter bar
    m_filterBar = new FilterBar(this);
    m_filterBar->setVisible(settings->filterBar());
    connect(m_filterBar, SIGNAL(filterChanged(const QString&)),
            this, SLOT(setNameFilter(const QString&)));
    connect(m_filterBar, SIGNAL(closeRequest()),
            this, SLOT(closeFilterBar()));
    connect(m_view, SIGNAL(urlChanged(const KUrl&)),
            m_filterBar, SLOT(clear()));

    m_topLayout->addWidget(m_urlNavigator);
    m_topLayout->addWidget(m_searchBox);
    m_topLayout->addWidget(m_view);
    m_topLayout->addWidget(m_filterBar);
    m_topLayout->addWidget(m_statusBar);

    setSearchModeEnabled(isSearchUrl(url));
}

DolphinViewContainer::~DolphinViewContainer()
{
}

KUrl DolphinViewContainer::url() const
{
    return m_view->url();
}

void DolphinViewContainer::setActive(bool active)
{
    m_urlNavigator->setActive(active);
    m_view->setActive(active);
}

bool DolphinViewContainer::isActive() const
{
    Q_ASSERT(m_view->isActive() == m_urlNavigator->isActive());
    return m_view->isActive();
}

void DolphinViewContainer::refresh()
{
    GeneralSettings* settings = DolphinSettings::instance().generalSettings();
    if (settings->modifiedStartupSettings()) {
        // The startup settings should only get applied if they have been
        // modified by the user. Otherwise keep the (possibly) different current
        // settings of the URL navigator and the filterbar.
        m_urlNavigator->setUrlEditable(settings->editableUrl());
        m_urlNavigator->setShowFullPath(settings->showFullPath());
        setFilterBarVisible(settings->filterBar());
    }

    m_view->refresh();
    m_statusBar->refresh();
}

bool DolphinViewContainer::isFilterBarVisible() const
{
    return m_filterBar->isVisible();
}

void DolphinViewContainer::setSearchModeEnabled(bool enabled)
{
    if (enabled == isSearchModeEnabled()) {
        if (enabled && !m_searchBox->hasFocus()) {
            m_searchBox->setFocus();
            m_searchBox->selectAll();
        }
        return;
    }

    m_searchBox->setVisible(enabled);
    m_urlNavigator->setVisible(!enabled);

    if (enabled) {
        m_searchBox->clearText();

        // Remember the most recent non-search URL as search path
        // of the search-box, so that it can be restored
        // when switching back to the URL navigator.
        KUrl url = m_urlNavigator->locationUrl();

        int index = m_urlNavigator->historyIndex();
        const int historySize = m_urlNavigator->historySize();
        while (isSearchUrl(url) && (index < historySize)) {
            ++index;
            url = m_urlNavigator->locationUrl(index);
        }

        if (!isSearchUrl(url)) {
            m_searchBox->setSearchPath(url);
        }
    } else {
        // Restore the URL for the URL navigator. If Dolphin has been
        // started with a search-URL, the home URL is used as fallback.
        const KUrl url = m_searchBox->searchPath();
        if (url.isValid() && !url.isEmpty()) {
            if (isSearchUrl(url)) {
                m_urlNavigator->goHome();
            } else {
                m_urlNavigator->setLocationUrl(url);
            }
        }
    }

    emit searchModeChanged(enabled);
}

bool DolphinViewContainer::isSearchModeEnabled() const
{
    return m_searchBox->isVisible();
}

void DolphinViewContainer::setUrl(const KUrl& newUrl)
{
    if (newUrl != m_urlNavigator->locationUrl()) {
        m_urlNavigator->setLocationUrl(newUrl);
    }
}

void DolphinViewContainer::setFilterBarVisible(bool visible)
{
    Q_ASSERT(m_filterBar != 0);
    if (visible) {
        m_filterBar->show();
        m_filterBar->setFocus();
        m_filterBar->selectAll();
    } else {
        closeFilterBar();
    }
}

void DolphinViewContainer::delayedStatusBarUpdate()
{
    if (m_statusBarTimer->isActive() && (m_statusBarTimestamp.elapsed() > 2000)) {
        // No update of the statusbar has been done during the last 2 seconds,
        // although an update has been requested. Trigger an immediate update.
        m_statusBarTimer->stop();
        updateStatusBar();
    } else {
        // Invoke updateStatusBar() with a small delay. This assures that
        // when a lot of delayedStatusBarUpdates() are done in a short time,
        // no bottleneck is given.
        m_statusBarTimer->start();
    }
}

void DolphinViewContainer::updateStatusBar()
{
    m_statusBarTimestamp.start();

    // As the item count information is less important
    // in comparison with other messages, it should only
    // be shown if:
    // - the status bar is empty or
    // - shows already the item count information or
    // - shows only a not very important information
    const QString newMessage = m_view->statusBarText();
    const QString currentMessage = m_statusBar->message();
    const bool updateStatusBarMsg = currentMessage.isEmpty()
                                    || (currentMessage == m_statusBar->defaultText())
                                    || (m_statusBar->type() == DolphinStatusBar::Information);

    m_statusBar->setDefaultText(newMessage);

    if (updateStatusBarMsg) {
        m_statusBar->setMessage(newMessage, DolphinStatusBar::Default);
    }
}

void DolphinViewContainer::updateProgress(int percent)
{
    if (m_statusBar->progressText().isEmpty()) {
        m_statusBar->setProgressText(i18nc("@info:progress", "Loading folder..."));
    }
    m_statusBar->setProgress(percent);
}

void DolphinViewContainer::slotStartedPathLoading()
{
    if (isSearchUrl(url())) {
        // Search KIO-slaves usually don't provide any progress information. Give
        // a hint to the user that a searching is done:
        updateStatusBar();
        m_statusBar->setProgressText(i18nc("@info", "Searching..."));
        m_statusBar->setProgress(-1);
    } else {
        // Trigger an undetermined progress indication. The progress
        // information in percent will be triggered by the percent() signal
        // of the directory lister later.
        updateProgress(-1);
    }
}

void DolphinViewContainer::slotFinishedPathLoading()
{
    if (!m_statusBar->progressText().isEmpty()) {
        m_statusBar->setProgressText(QString());
        m_statusBar->setProgress(100);
    }

    if (isSearchUrl(url()) && (m_view->items().count() == 0)) {
        // The dir lister has been completed on a Nepomuk-URI and no items have been found. Instead
        // of showing the default status bar information ("0 items") a more helpful information is given:
        m_statusBar->setMessage(i18nc("@info:status", "No items found."), DolphinStatusBar::Information);
    } else {
        updateStatusBar();
    }
}

void DolphinViewContainer::showItemInfo(const KFileItem& item)
{
    if (item.isNull()) {
        // Only clear the status bar if unimportant messages are shown.
        // This prevents that information- or error-messages get hidden
        // by moving the mouse above the viewport or when closing the
        // context menu.
        if (m_statusBar->type() == DolphinStatusBar::Default) {
            m_statusBar->clear();
        }
    } else {
        m_statusBar->setMessage(item.getStatusBarInfo(), DolphinStatusBar::Default);
    }
}

void DolphinViewContainer::showInfoMessage(const QString& msg)
{
    m_statusBar->setMessage(msg, DolphinStatusBar::Information);
}

void DolphinViewContainer::showErrorMessage(const QString& msg)
{
    m_statusBar->setMessage(msg, DolphinStatusBar::Error);
}

void DolphinViewContainer::showOperationCompletedMessage(const QString& msg)
{
    m_statusBar->setMessage(msg, DolphinStatusBar::OperationCompleted);
}

void DolphinViewContainer::closeFilterBar()
{
    m_filterBar->hide();
    m_filterBar->clear();
    m_view->setFocus();
    emit showFilterBarChanged(false);
}

void DolphinViewContainer::setNameFilter(const QString& nameFilter)
{
    m_view->setNameFilter(nameFilter);
    delayedStatusBarUpdate();
}

void DolphinViewContainer::activate()
{
    setActive(true);
}

void DolphinViewContainer::saveViewState()
{
    QByteArray locationState;
    QDataStream stream(&locationState, QIODevice::WriteOnly);
    m_view->saveState(stream);
    m_urlNavigator->saveLocationState(locationState);
}

void DolphinViewContainer::slotUrlNavigatorLocationChanged(const KUrl& url)
{
    if (KProtocolManager::supportsListing(url)) {
        setSearchModeEnabled(isSearchUrl(url));

        m_view->setUrl(url);
        if (isActive() && !isSearchUrl(url)) {
            // When an URL has been entered, the view should get the focus.
            // The focus must be requested asynchronously, as changing the URL might create
            // a new view widget.
            QTimer::singleShot(0, this, SLOT(requestFocus()));
        }
    } else if (KProtocolManager::isSourceProtocol(url)) {
        QString app = "konqueror";
        if (url.protocol().startsWith(QLatin1String("http"))) {
            showErrorMessage(i18nc("@info:status",
                                   "Dolphin does not support web pages, the web browser has been launched"));
            const KConfigGroup config(KSharedConfig::openConfig("kdeglobals"), "General");
            const QString browser = config.readEntry("BrowserApplication");
            if (!browser.isEmpty()) {
                app = browser;
                if (app.startsWith('!')) {
                    // a literal command has been configured, remove the '!' prefix
                    app = app.mid(1);
                }
            }
        } else {
            showErrorMessage(i18nc("@info:status",
                                   "Protocol not supported by Dolphin, Konqueror has been launched"));
        }

        const QString secureUrl = KShell::quoteArg(url.pathOrUrl());
        const QString command = app + ' ' + secureUrl;
        KRun::runCommand(command, app, app, this);
    } else {
        showErrorMessage(i18nc("@info:status", "Invalid protocol"));
    }
}

void DolphinViewContainer::dropUrls(const KUrl& destination, QDropEvent* event)
{
    DragAndDropHelper::instance().dropUrls(KFileItem(), destination, event, this);
}

void DolphinViewContainer::redirect(const KUrl& oldUrl, const KUrl& newUrl)
{
    Q_UNUSED(oldUrl);
    const bool block = m_urlNavigator->signalsBlocked();
    m_urlNavigator->blockSignals(true);

    // Assure that the location state is reset for redirection URLs. This
    // allows to skip redirection URLs when going back or forward in the
    // URL history.
    m_urlNavigator->saveLocationState(QByteArray());
    m_urlNavigator->setLocationUrl(newUrl);
    setSearchModeEnabled(isSearchUrl(newUrl));

    m_urlNavigator->blockSignals(block);
}

void DolphinViewContainer::requestFocus()
{
    m_view->setFocus();
}

void DolphinViewContainer::saveUrlCompletionMode(KGlobalSettings::Completion completion)
{
    DolphinSettings& settings = DolphinSettings::instance();
    settings.generalSettings()->setUrlCompletionMode(completion);
    settings.save();
}

void DolphinViewContainer::slotHistoryChanged()
{
    QByteArray locationState = m_urlNavigator->locationState();

    if (!locationState.isEmpty()) {
        QDataStream stream(&locationState, QIODevice::ReadOnly);
        m_view->restoreState(stream);
    }
}

void DolphinViewContainer::startSearching(const QString &text)
{
    Q_UNUSED(text);
    const KUrl url = m_searchBox->urlForSearching();
    if (url.isValid() && !url.isEmpty()) {
        m_urlNavigator->setLocationUrl(url);
    }
}

void DolphinViewContainer::closeSearchBox()
{
    setSearchModeEnabled(false);
}

void DolphinViewContainer::stopLoading()
{
    m_view->stopLoading();
    m_statusBar->setProgress(100);
}

bool DolphinViewContainer::isSearchUrl(const KUrl& url) const
{
    const QString protocol = url.protocol();
    return protocol.contains("search") || (protocol == QLatin1String("nepomuk"));
}

void DolphinViewContainer::slotItemTriggered(const KFileItem& item)
{
    KUrl url = item.targetUrl();

    if (item.isDir()) {
        m_view->setUrl(url);
        return;
    }

    const GeneralSettings* settings = DolphinSettings::instance().generalSettings();
    const bool browseThroughArchives = settings->browseThroughArchives();
    if (browseThroughArchives && item.isFile() && url.isLocalFile()) {
        // Generic mechanism for redirecting to tar:/<path>/ when clicking on a tar file,
        // zip:/<path>/ when clicking on a zip file, etc.
        // The .protocol file specifies the mimetype that the kioslave handles.
        // Note that we don't use mimetype inheritance since we don't want to
        // open OpenDocument files as zip folders...
        const QString protocol = KProtocolManager::protocolForArchiveMimetype(item.mimetype());
        if (!protocol.isEmpty()) {
            url.setProtocol(protocol);
            m_view->setUrl(url);
            return;
        }
    }

    if (item.mimetype() == "application/x-desktop") {
        // redirect to the url in Type=Link desktop files
        KDesktopFile desktopFile(url.toLocalFile());
        if (desktopFile.hasLinkType()) {
            url = desktopFile.readUrl();
            m_view->setUrl(url);
            return;
        }
    }

    item.run();
}

void DolphinViewContainer::openFile(const KUrl& url)
{
    const KFileItem item(KFileItem::Unknown, KFileItem::Unknown, url);
    slotItemTriggered(item);
}

#include "dolphinviewcontainer.moc"
