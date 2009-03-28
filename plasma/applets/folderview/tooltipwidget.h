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

#ifndef TOOLTIPWIDGET_H
#define TOOLTIPWIDGET_H

#include <QModelIndex>
#include <QBasicTimer>
#include <QGraphicsWidget>
#include <KFileItem>
#include <QPixmap>

namespace KIO {
    class PreviewJob;
}

class QModelIndex;
class AbstractItemView;

/**
 * This widget registers with the plasma tooltip manager, and is moved around
 * in the view and resized to the size of the hovered item, so that the tooltip
 * will be positioned correctly when it's shown.
 *
 * It is also responsible for starting preview jobs and updating the the tooltip
 * contents when a preview has been generated.
 *
 * While the widget is created as a child widget of the view, it is always hidden.
 */
class ToolTipWidget : public QGraphicsWidget
{
    Q_OBJECT

public:
    ToolTipWidget(AbstractItemView *parent);
    void updateToolTip(const QModelIndex &index, const QRectF &rect);

private:
    QString metaInfo() const;
    void setContent();
    void startPreviewJob();
    void timerEvent(QTimerEvent *);

private slots:
    void gotPreview(const KFileItem &item, const QPixmap &pixmap);
    void previewJobFinished(KJob *job);
    void toolTipAboutToShow();

private:
    AbstractItemView * const m_view;
    KIO::PreviewJob *m_previewJob;
    KFileItem m_item;
    QModelIndex m_index;
    QPixmap m_preview;
    QBasicTimer m_previewTimer;
    QBasicTimer m_hideTimer;
};

#endif

