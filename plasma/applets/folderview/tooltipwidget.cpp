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

#include "tooltipwidget.h"
#include "abstractitemview.h"
#include "proxymodel.h"

#include <QApplication>
#include <QGraphicsSceneHoverEvent>
#include <QModelIndex>

#include <KDesktopFile>
#include <KDirModel>
#include <KLocale>
#include <KIO/PreviewJob>

#include <Plasma/ToolTipManager>

#include <cmath>


ToolTipWidget::ToolTipWidget(AbstractItemView *parent)
    : QGraphicsWidget(parent), m_view(parent), m_previewJob(0)
{
    Plasma::ToolTipManager::self()->registerWidget(this);
}

void ToolTipWidget::updateToolTip(const QModelIndex &index, const QRectF &rect)
{
    if (!index.isValid()) {
        // Send a fake hover leave event to the widget to trick the tooltip
        // manager into doing a delayed hide.
        QGraphicsSceneHoverEvent event(QEvent::GraphicsSceneHoverLeave);
        QApplication::sendEvent(this, &event);

        m_preview = QPixmap();
        m_item = KFileItem();
        m_index = QModelIndex();
        return;
    }

    setGeometry(rect);
    m_item = static_cast<ProxyModel*>(m_view->model())->itemForIndex(index);
    m_index = index;
    m_preview = QPixmap();

    // If a preview job is still running (from a previously hovered item),
    // wait 200 ms before starting a new one. This is done to throttle
    // the number of preview jobs that are started when the user moves
    // the cursor over the icon view.
    if (m_previewJob) {
        m_previewTimer.start(200, this);
    } else {
        if (m_previewTimer.isActive()) {
            m_previewTimer.stop();
        }
        startPreviewJob();
    }

    Plasma::ToolTipManager::self()->show(this);
}

static qreal convertToReal(const QString &string)
{
    const int pos = string.indexOf('/');
    if (pos != -1) {
        const int left = string.left(pos).toInt();
        const int right = string.mid(pos + 1).toInt();
        return right > 0 ? qreal(left) / qreal(right) : 0.0;
    }

    return qreal(string.toInt());
}

QString ToolTipWidget::metaInfo() const
{
    const QString mimetype = m_item.mimetype();
    if (!mimetype.startsWith("audio/") && !mimetype.startsWith("image/") &&
        !m_item.mimeTypePtr()->is("application/vnd.oasis.opendocument.text"))
    {
        return QString();
    }

    KFileMetaInfo info = m_item.metaInfo(true, KFileMetaInfo::TechnicalInfo | KFileMetaInfo::ContentInfo);
    QString text;

    if (mimetype.startsWith("audio/")) {
        const QString artist = info.item("http://freedesktop.org/standards/xesam/1.0/core#artist").value().toString();
        const QString title  = info.item("http://freedesktop.org/standards/xesam/1.0/core#title").value().toString();
        const QString album  = info.item("http://freedesktop.org/standards/xesam/1.0/core#album").value().toString();

        if (!artist.isEmpty() || !title.isEmpty() || !album.isEmpty()) {
            text += "<p><table border='0' cellspacing='0' cellpadding='0'>";
            if (!artist.isEmpty()) {
                text += QString("<tr><td>") + i18nc("Music", "Artist:") + QString(" </td><td>") + artist + QString("</td></tr>");
            }
            if (!title.isEmpty()) {
                text += QString("<tr><td>") + i18nc("Music", "Title:") + QString(" </td><td>") + title + QString("</td></tr>");
            }
            if (!album.isEmpty()) {
                text += QString("<tr><td>") + i18nc("Music", "Album:") + QString(" </td><td>") + album + QString("</td></tr>");
            }
            text += "</table>";
        }
    } else if (mimetype.startsWith("image/")) {
        int width             = info.item("http://freedesktop.org/standards/xesam/1.0/core#width").value().toInt();
        int height            = info.item("http://freedesktop.org/standards/xesam/1.0/core#height").value().toInt();
        const QString camera  = info.item("http://freedesktop.org/standards/xesam/1.0/core#cameraModel").value().toString();
        const QString type    = info.item("http://www.w3.org/1999/02/22-rdf-syntax-ns#type").value().toString();
        QString exposureTime  = info.item("http://freedesktop.org/standards/xesam/1.0/core#exposureTime").value().toString();
        QString focalLength   = info.item("http://freedesktop.org/standards/xesam/1.0/core#focalLength").value().toString();
        QString focal35mm     = info.item("http://freedesktop.org/standards/xesam/1.0/core#35mmEquivalent").value().toString();
        QString aperture      = info.item("http://freedesktop.org/standards/xesam/1.0/core#aperture").value().toString();
        QString iso           = info.item("http://freedesktop.org/standards/xesam/1.0/core#isoEquivalent").value().toString();
        QString created       = info.item("http://freedesktop.org/standards/xesam/1.0/core#contentCreated").value().toString();

        text += "<p><table border='0' cellspacing='0' cellpadding='0'>";
        if (width > 0 && height > 0) {
            QString size = QString::number(width) + 'x' + QString::number(height);
            // Add the megapixel count for photos
            if (type == "http://freedesktop.org/standards/xesam/1.0/core#Photo") {
                const qreal pixels = qreal(width * height) / 1e6;
                size += QString(" (") +  ki18np("1 MPixel", "%1 MPixels").subs(pixels, 0, 'f', 1).toString() + QString(")");
            }
            text += QString("<tr><td>") + i18n("Size:") + QString(" </td><td>") + size + QString("</td></tr>");
        }
        if (!camera.isEmpty()) {
            text += QString("<tr><td>") + i18n("Camera:") + QString(" </td><td>") + camera + QString("</td></tr>");
        }
        if (!focalLength.isEmpty()) {
            const qreal length = convertToReal(focalLength);
            focalLength = i18nc("Length in millimeters", "%1 mm", qRound(length));
            if (!focal35mm.isEmpty()) {
                const qreal length = convertToReal(focal35mm);
                focalLength += QString(" (") + i18nc("In photography", "35 mm equivalent: %1 mm", qRound(length)) + QString(")");
            }
            text += QString("<tr><td>") + i18nc("On a camera", "Focal Length:") +
                    QString(" </td><td>") + focalLength + QString("</td></tr>");
        }
        if (!exposureTime.isEmpty()) {
            const qreal time = convertToReal(exposureTime);
            if (time < 1.0) {
                exposureTime = QString("1/") + QString::number(qRound((1.0 / time)));
            } else {
                exposureTime = QString::number(time, 'f', 1);
            }
            text += QString("<tr><td>") + i18nc("On a camera", "Exposure Time:") + QString(" </td><td>") +
                    i18nc("Fraction of a second, or number of seconds", "%1 s", exposureTime) + QString("</td></tr>");
        }
        if (!aperture.isEmpty()) {
            // Convert the APEX value to the F number
            const qreal fnumber = std::sqrt(std::pow(2, convertToReal(aperture)));
            aperture = QString("f/") + QString::number(fnumber, 'f', 1);
            if (aperture.endsWith(".0")) {
                aperture = aperture.left(aperture.length() - 2);
            }
            text += QString("<tr><td>") + i18nc("On a camera", "Aperture:") +
                    QString(" </td><td>") + aperture + QString("</td></tr>");
        }
        if (!iso.isEmpty()) {
            text += QString("<tr><td>") + i18nc("On a camera", "ISO Speed:") +
                    QString(" </td><td>") + iso + QString("</td></tr>");
        }
        if (!created.isEmpty()) {
            const QDateTime dateTime = QDateTime::fromString(created, "yyyy:MM:dd HH:mm:ss");
            text += QString("<tr><td>") + i18n("Time:") + QString(" </td><td>") +
                    KGlobal::locale()->formatDateTime(dateTime, KLocale::ShortDate, true) + QString("</td></tr>");
        }
        text += "</table>";
    } else if (m_item.mimeTypePtr()->is("application/vnd.oasis.opendocument.text")) {
        int wordCount = info.item("http://freedesktop.org/standards/xesam/1.0/core#wordCount").value().toInt();
        int pageCount = info.item("http://freedesktop.org/standards/xesam/1.0/core#pageCount").value().toInt();

        const QString str1 = i18ncp("Inserted as %1 in the message below.", "1 page", "%1 pages", pageCount);
        const QString str2 = i18ncp("Inserted as %2 in the message below.", "1 word", "%1 words", wordCount); 
        if (pageCount > 0) {
            text += QString("<p>") + i18nc("%1 and %2 are the messages translated above.", "%1, %2.", str1, str2);
        } 
    }

    return text;
}

void ToolTipWidget::setContent()
{
    Plasma::ToolTipContent content;
    content.setMainText(m_index.data(Qt::DisplayRole).toString());

    if (m_preview.isNull()) {
        content.setImage(qvariant_cast<QIcon>(m_index.data(Qt::DecorationRole)));
    } else {
        content.setImage(m_preview);
    }

    QString subText;

    if (m_item.isDesktopFile()) {
        // Add the comment in the .desktop file to the subtext.
        // Note that we don't include the mime type for .desktop files,
        // since users will likely be confused about what will happen when
        // they click a "Desktop configuration file" on their desktop.
        KDesktopFile file(m_item.localPath());
        subText = file.readComment();
    } else {
        if (m_item.isMimeTypeKnown()) {
            subText = m_item.mimeComment();
        }

        if (m_item.isDir()) {
            // Include information about the number of files and folders in the directory.
            const QVariant value = m_index.data(KDirModel::ChildCountRole);
            const int count = value.type() == QVariant::Int ? value.toInt() : KDirModel::ChildCountUnknown;

            if (count != KDirModel::ChildCountUnknown) {
                subText += QString("<br>") + i18ncp("Items in a folder", "1 item", "%1 items", count);
            }
        } else {
            // File size
            if (m_item.isFile()) {
                subText += QString("<br>") + KGlobal::locale()->formatByteSize(m_item.size());
            }

            // Add meta info from the strigi analyzers
            subText += metaInfo();
        }
    }

    content.setSubText(subText);
    content.setAutohide(false);

    Plasma::ToolTipManager::self()->setContent(this, content);
}

void ToolTipWidget::startPreviewJob()
{
    QStringList plugins;
    plugins << "imagethumbnail" << "jpegthumbnail";

    m_previewJob = KIO::filePreview(KFileItemList() << m_item, 256, 256, 0, 70, true, true, &plugins);
    connect(m_previewJob, SIGNAL(gotPreview(KFileItem,QPixmap)), SLOT(gotPreview(KFileItem,QPixmap)));
    connect(m_previewJob, SIGNAL(finished(KJob*)), SLOT(previewJobFinished(KJob*)));
}

void ToolTipWidget::gotPreview(const KFileItem &item, const QPixmap &pixmap)
{
    if (item == m_item) {
        m_preview = pixmap;
        setContent();
    } else if (m_item.isNull()) {
        m_preview = QPixmap();
    }
}

void ToolTipWidget::previewJobFinished(KJob *job)
{
    if (job == m_previewJob) {
        m_previewJob = 0;
    }
}

void ToolTipWidget::toolTipAboutToShow()
{
    if (m_index.isValid()) {
        setContent();
        m_hideTimer.start(10000, this);
    } else {
        Plasma::ToolTipManager::self()->clearContent(this);
    }
}

void ToolTipWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_previewTimer.timerId()) {
        m_previewTimer.stop();

        if (m_index.isValid()) {
            startPreviewJob();
        }
    }

    if (event->timerId() == m_hideTimer.timerId()) {
        m_hideTimer.stop();
        Plasma::ToolTipManager::self()->hide(this);
    }
}

#include "tooltipwidget.moc"

