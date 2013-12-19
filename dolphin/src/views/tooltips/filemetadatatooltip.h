/***************************************************************************
 *   Copyright (C) 2010 by Peter Penz <peter.penz19@gmail.com>             *
 *   Copyright (C) 2008 by Fredrik Höglund <fredrik@kde.org>               *
 *   Copyright (C) 2012 by Mark Gaiser <markg85@gmail.com>                 *
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

#ifndef FILEMETADATATOOLTIP_H
#define FILEMETADATATOOLTIP_H

#include <QWidget>
#include "config-nepomuk.h"

class KFileItemList;
class QLabel;

#ifndef HAVE_BALOO
class KFileMetaDataWidget;
#else
namespace Baloo {
    class FileMetaDataWidget;
}
#endif

/**
 * @brief Tooltip, that shows the meta information and a preview of one
 *        or more files.
 */
class FileMetaDataToolTip : public QWidget
{
    Q_OBJECT

public:
    FileMetaDataToolTip(QWidget* parent = 0);
    virtual ~FileMetaDataToolTip();

    void setPreview(const QPixmap& pixmap);
    QPixmap preview() const;

    void setName(const QString& name);
    QString name() const;

    /**
     * Sets the items for which the meta data should be shown.
     * The signal metaDataRequestFinished() will be emitted,
     * as soon as the meta data for the items has been received.
     */
    void setItems(const KFileItemList& items);
    KFileItemList items() const;

signals:
    /**
     * Is emitted after the meta data has been received for the items
     * set by FileMetaDataToolTip::setItems().
     */
    void metaDataRequestFinished(const KFileItemList& items);

protected:
    virtual void paintEvent(QPaintEvent* event);
    virtual void showEvent(QShowEvent *);

private:
    QLabel* m_preview;
    QLabel* m_name;
#ifndef HAVE_BALOO
    KFileMetaDataWidget* m_fileMetaDataWidget;
#else
    Baloo::FileMetaDataWidget* m_fileMetaDataWidget;
#endif
};

#endif
