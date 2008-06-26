/*
 * This file is part of the KDE project.
 *
 * Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "knetworkreply.h"

#include <KDebug>
#include <kio/job.h>

KNetworkReply::KNetworkReply(const QNetworkRequest &request, KIO::Job *kioJob, QObject *parent)
    : QNetworkReply(parent),
    m_kioJob(kioJob)
{
    setRequest(request);
    setOpenMode(QIODevice::ReadOnly);
}

void KNetworkReply::abort()
{
//     m_kioJob->kill();
//     m_kioJob->deleteLater();
}

qint64 KNetworkReply::bytesAvailable() const
{
    return QNetworkReply::bytesAvailable() + m_data.length();
}

qint64 KNetworkReply::readData(char *data, qint64 maxSize)
{
    kDebug();
    qint64 length = qMin(qint64(m_data.length()), maxSize);
    if (length) {
        qMemCopy(data, m_data.constData(), length);
        m_data.remove(0, length);
    }

    return length;
}

void KNetworkReply::setContentType(KIO::Job *kioJob, const QString &contentType)
{
    Q_UNUSED(kioJob);
    setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    emit metaDataChanged();
}

void KNetworkReply::appendData(const QByteArray &data)
{
    kDebug();
    m_data += data;
    emit readyRead();
}

#include "knetworkreply.moc"
