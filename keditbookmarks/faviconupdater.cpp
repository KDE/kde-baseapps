// -*- indent-tabs-mode:nil -*-
// vim: set ts=4 sts=4 sw=4 et:
/* This file is part of the KDE project
   Copyright (C) 2003 Alexander Kellett <lypanov@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License version 2 or at your option version 3 as published by
   the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "faviconupdater.h"

#include "bookmarkiterator.h"
#include "toplevel.h"

#include <QDebug>
#include <klocalizedstring.h>

#include <kio/job.h>
#include <KIO/FavIconRequestJob>

#include <kparts/part.h>
#include <kparts/browserextension.h>
#include <kmimetypetrader.h>

FavIconUpdater::FavIconUpdater(QObject *parent)
    : QObject(parent)
{
    m_part = 0;
    m_webGrabber = 0;
    m_browserIface = 0;
}

void FavIconUpdater::downloadIcon(const KBookmark &bk)
{
    m_bk = bk;
    const QUrl url = bk.url();
    const QString favicon = KIO::favIconForUrl(url);
    if (!favicon.isEmpty()) {
        //qDebug() << "got favicon" << favicon;
        m_bk.setIcon(favicon);
        KEBApp::self()->notifyCommandExecuted();
        // //qDebug() << "emit done(true)";
        emit done(true, QString());

    } else {
        //qDebug() << "no favicon found";
        webupdate = false;
        KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url, KIO::Reload);
        connect(job, &KIO::FavIconRequestJob::result, this, &FavIconUpdater::slotResult);
    }
}

FavIconUpdater::~FavIconUpdater()
{
    delete m_browserIface;
    delete m_webGrabber;
    delete m_part;
}

void FavIconUpdater::downloadIconUsingWebBrowser(const KBookmark &bk, const QString& currentError)
{
    //qDebug();
    m_bk = bk;
    webupdate = true;

    if (!m_part) {
        QString partLoadingError;
        KParts::ReadOnlyPart *part
            = KMimeTypeTrader::createPartInstanceFromQuery<KParts::ReadOnlyPart>("text/html", 0, this, QString(), QVariantList(), &partLoadingError);
        if (!part) {
            emit done(false, i18n("%1; no HTML component found (%2)", currentError, partLoadingError));
            return;
        }

        part->setProperty("pluginsEnabled", QVariant(false));
        part->setProperty("javaScriptEnabled", QVariant(false));
        part->setProperty("javaEnabled", QVariant(false));
        part->setProperty("autoloadImages", QVariant(false));

        KParts::BrowserExtension *ext = KParts::BrowserExtension::childObject(part);
        Q_ASSERT(ext);

        // TODO: what is this useful for?
        m_browserIface = new KParts::BrowserInterface(this);
        ext->setBrowserInterface(m_browserIface);

        connect(ext, SIGNAL(setIconUrl(QUrl)),
                this, SLOT(setIconUrl(QUrl)));

        m_part = part;
    }

    // The part isn't created by the webgrabber so that we can create the part
    // only once.
    delete m_webGrabber;
    m_webGrabber = new FavIconWebGrabber(m_part, bk.url());
    connect(m_webGrabber, SIGNAL(done(bool,QString)), this, SIGNAL(done(bool,QString)));
}

// khtml callback
void FavIconUpdater::setIconUrl(const QUrl &iconURL)
{
    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(m_bk.url());
    job->setIconUrl(iconURL);
    connect(job, &KIO::FavIconRequestJob::result, this, &FavIconUpdater::slotResult);

    delete m_webGrabber;
    m_webGrabber = 0;
}

void FavIconUpdater::slotResult(KJob *job)
{
    KIO::FavIconRequestJob *requestJob = static_cast<KIO::FavIconRequestJob *>(job);
    if (job->error()) {
        if (!webupdate) {
            qDebug() << "favicon job failed, going to downloadIconUsingWebBrowser";
            // no icon found, try webupdater
            downloadIconUsingWebBrowser(m_bk, job->errorString());
        } else {
            qDebug() << "favicon job failed, emit done";
            // already tried webupdater
            emit done(false, job->errorString());
        }
        return;
    }
    m_bk.setIcon(requestJob->iconFile());
    emit done(true, QString());
}

/* -------------------------- */

FavIconWebGrabber::FavIconWebGrabber(KParts::ReadOnlyPart *part, const QUrl &url)
    : m_part(part), m_url(url) {

    //FIXME only connect to result?
//  connect(part, SIGNAL(result(KIO::Job*job)),
//          this, SLOT(slotCompleted()));
    connect(part, SIGNAL(canceled(QString)),
            this, SLOT(slotCanceled(QString)));
    connect(part, SIGNAL(completed(bool)),
            this, SLOT(slotCompleted()));

    // the use of KIO rather than directly using KHTML is to allow silently abort on error
    // TODO: an alternative would be to derive from KHTMLPart and reimplement showError(KJob*).

    //qDebug() << "starting KIO::get() on" << m_url;
    KIO::Job *job = KIO::get(m_url, KIO::NoReload, KIO::HideProgressInfo);
    job->addMetaData( QString("cookies"), QString("none") );
    job->addMetaData( QString("errorPage"), QString("false") );
    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(slotFinished(KJob*)));
    connect(job, SIGNAL(mimetype(KIO::Job*,QString)),
            this, SLOT(slotMimetype(KIO::Job*,QString)));
}

void FavIconWebGrabber::slotMimetype(KIO::Job *job, const QString &type)
{
    Q_ASSERT(!job->error()); // can't be set already, surely?

    KIO::SimpleJob *sjob = static_cast<KIO::SimpleJob *>(job);
    m_url = sjob->url(); // allow for redirection
    sjob->putOnHold();

    // QString typeLocal = typeUncopied; // local copy
    qDebug() << "slotMimetype " << type << "calling openUrl on" << m_url;
    // TODO - what to do if typeLocal is not text/html ??

    m_part->openUrl(m_url);
}

void FavIconWebGrabber::slotFinished(KJob *job)
{
    if (job->error()) {
        qDebug() << job->errorString();
        emit done(false, job->errorString());
        return;
    }
    // On success mimetype was emitted, so no need to do anything.
}

void FavIconWebGrabber::slotCompleted()
{
    qDebug();
    emit done(true, QString());
}

void FavIconWebGrabber::slotCanceled(const QString& errorString)
{
    //qDebug() << errorString;
    emit done(false, errorString);
}


