/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
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

#include "asyncfiletester.h"
#include "proxymodel.h"

#include <KDesktopFile>
#include <KFileItem>
#include <KProtocolInfo>
#include <KUrl>
#include <KIO/StatJob>
#include <KIO/Scheduler>


AsyncFileTester::AsyncFileTester(const QModelIndex &index, QObject *object, const char *member)
    : index(index), object(object), member(member)
{
}

void AsyncFileTester::checkIfFolder(const QModelIndex &index, QObject *object, const char *method)
{
    if (!index.isValid()) {
        callResultMethod(object, method, index, false);
        return;
    }

    KFileItem item = static_cast<const ProxyModel*>(index.model())->itemForIndex(index);
    KUrl url = item.targetUrl();
    
    if (item.isDir()) {
        callResultMethod(object, method, index, true);
        return;
    }
    
    if (item.isDesktopFile()) {
        // Check if the desktop file is a link to a local folder
        KDesktopFile file(url.path());
        if (file.readType() == "Link") {
            url = file.readUrl();
            if (url.isLocalFile()) {
                KFileItem destItem(KFileItem::Unknown, KFileItem::Unknown, url);
                callResultMethod(object, method, index, destItem.isDir());
                return;
            }
            
            if (KProtocolInfo::protocolClass(url.protocol()) == QString(":local")) {
                AsyncFileTester *tester = new AsyncFileTester(index, object, method);
                tester->delayedFolderCheck(url);
                return;
            }
        }
    }
    callResultMethod(object, method, index, false);
}

void AsyncFileTester::delayedFolderCheck(const KUrl &url)
{
    KIO::StatJob *job = KIO::stat(url, KIO::HideProgressInfo);
    job->setSide(KIO::StatJob::SourceSide); // We will only read the file
    connect(job, SIGNAL(result(KJob*)), SLOT(statResult(KJob*)));
}

void AsyncFileTester::callResultMethod(QObject *object, const char *member, const QModelIndex &index, bool result)
{
    QMetaObject::invokeMethod(object, member, Q_ARG(QModelIndex, index),
                              Q_ARG(bool, result));
}

void AsyncFileTester::statResult(KJob *job)
{
    if (object && !job->error()) {
        KIO::StatJob *statJob = static_cast<KIO::StatJob*>(job);
        callResultMethod(object.data(), member, index, statJob->statResult().isDir());
    }
    deleteLater();
}

#include "asyncfiletester.moc"

