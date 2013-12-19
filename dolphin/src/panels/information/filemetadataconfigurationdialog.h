/***************************************************************************
 *   Copyright (C) 2010 by Peter Penz <peter.penz19@gmail.com>             *
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

#ifndef FILEMETADATACONFIGURATIONDIALOG_H
#define FILEMETADATACONFIGURATIONDIALOG_H

#include <KDialog>
#include <KFileItem>
#include "config-nepomuk.h"

#ifndef HAVE_BALOO
class KFileMetaDataConfigurationWidget;
#else
namespace Baloo {
    class FileMetaDataConfigWidget;
}
#endif

class QLabel;

/**
 * @brief Dialog which allows to configure which meta data should be shown
 *        in the KFileMetaDataWidget.
 */
class FileMetaDataConfigurationDialog : public KDialog
{
    Q_OBJECT

public:
    explicit FileMetaDataConfigurationDialog(QWidget* parent = 0);
    virtual ~FileMetaDataConfigurationDialog();

    /**
     * Sets the items, for which the visibility of the meta data should
     * be configured. Note that the visibility of the meta data is not
     * bound to the items itself, the items are only used to determine
     * which meta data should be configurable. For example when a JPEG image
     * is set as item, it will be configurable which EXIF data should be
     * shown. If an audio file is set as item, it will be configurable
     * whether the artist, album name, ... should be shown.
     */
    void setItems(const KFileItemList& items);
    KFileItemList items() const;

    /**
     * Sets the description that is shown above the list
     * of meta data. Per default the translated text for
     * "Select which data should be shown." is set.
     */
    void setDescription(const QString& description);
    QString description() const;

protected slots:
    virtual void slotButtonClicked(int button);

private:
    QLabel* m_descriptionLabel;
#ifndef HAVE_BALOO
    KFileMetaDataConfigurationWidget* m_configWidget;
#else
    Baloo::FileMetaDataConfigWidget* m_configWidget;
#endif
};

#endif
