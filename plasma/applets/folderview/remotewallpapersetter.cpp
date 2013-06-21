/*
 *   Copyright © 2008, 2009 Fredrik Höglund <fredrik@kde.org>
 *   Copyright © 2008 Rafael Fernández López <ereslibre@kde.org>
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

#include "remotewallpapersetter.h"

#include <KTemporaryFile>
#include <KStandardDirs>
#include <kio/job.h>
#include <kio/copyjob.h>

#include "folderview.h"

RemoteWallpaperSetter::RemoteWallpaperSetter(const KUrl &url, FolderView *containment)
    : QObject(containment)
{
    const QString suffix = QFileInfo(url.fileName()).suffix();

    KTemporaryFile file;
    file.setPrefix(KGlobal::dirs()->saveLocation("wallpaper"));
    file.setSuffix(QString(".") + suffix);
    file.setAutoRemove(false);

    if (file.open()) {
        KIO::FileCopyJob *job = KIO::file_copy(url, KUrl::fromPath(file.fileName()), -1, KIO::Overwrite);
        connect(job, SIGNAL(result(KJob*)), SLOT(result(KJob*)));
    } else {
        deleteLater();
    }
}

void RemoteWallpaperSetter::result(KJob *job)
{
    if (!job->error()) {
        FolderView *containment = static_cast<FolderView*>(parent());
        KIO::FileCopyJob *copyJob = static_cast<KIO::FileCopyJob*>(job);
        containment->setWallpaper(copyJob->destUrl());
    }
    deleteLater();
}

#include "remotewallpapersetter.moc"
