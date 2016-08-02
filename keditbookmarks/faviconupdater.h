/* This file is part of the KDE project
   Copyright (C) 2003 Alexander Kellett <lypanov@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#ifndef FAVICONUPDATER_H
#define FAVICONUPDATER_H

#include <kbookmark.h>

#include <kparts/part.h>
#include <kparts/browserinterface.h>
#include <KParts/ReadOnlyPart>
#include <QUrl>
#include <KIO/Job>

class FavIconWebGrabber : public QObject
{
    Q_OBJECT
public:
    FavIconWebGrabber(KParts::ReadOnlyPart *part, const QUrl &url);
    ~FavIconWebGrabber() {}

Q_SIGNALS:
    void done(bool succeeded, const QString& errorString);

private Q_SLOTS:
    void slotMimetype(KIO::Job *job, const QString &_type);
    void slotFinished(KJob *job);
    void slotCanceled(const QString& errorString);
    void slotCompleted();

private:
    KParts::ReadOnlyPart *m_part;
    QUrl m_url;
};

class FavIconBrowserInterface;

class FavIconUpdater : public QObject
{
    Q_OBJECT

public:
    FavIconUpdater(QObject *parent);
    ~FavIconUpdater();
    void downloadIcon(const KBookmark &bk);
    void downloadIconUsingWebBrowser(const KBookmark &bk, const QString& currentError);

private Q_SLOTS:
    void setIconUrl(const QUrl &iconURL);
    void slotResult(KJob *job);

Q_SIGNALS:
    void done(bool succeeded, const QString& error);

private:
    KParts::ReadOnlyPart *m_part;
    KParts::BrowserInterface *m_browserIface;
    FavIconWebGrabber *m_webGrabber;
    KBookmark m_bk;
    bool webupdate;
};

#endif

