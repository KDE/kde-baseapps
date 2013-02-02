/*
 *   Copyright © 2013 Ignat Semenov <ignat.semenov@blue-systems.org>
 *   Copyright © 2008, 2009 Fredrik Höglund <fredrik@kde.org>
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

#include "locationpage.h"

#include <KGlobalSettings>

#include "../folderview.h"


LocationPage::LocationPage(KConfigDialog *dialog, Options *settings) : PageBase(dialog, settings)
{
}

void LocationPage::preSetupUi()
{
    uiLocation.setupUi(this);

    m_placesModel = new KFilePlacesModel(this);
    m_placesFilterModel = new PlacesFilterModel(this);
    m_placesFilterModel->setSourceModel(m_placesModel);
    uiLocation.placesCombo->setModel(m_placesFilterModel);
    uiLocation.lineEdit->setMode(KFile::Directory);
}

void LocationPage::setupUi()
{
    QDir desktopFolder(KGlobalSettings::desktopPath());
    const bool desktopVisible = desktopFolder != QDir::homePath() && desktopFolder.exists();
    uiLocation.showDesktopFolder->setVisible(desktopVisible);
    if (desktopVisible && m_url == KUrl("desktop:/")) {
        uiLocation.showDesktopFolder->setChecked(true);
        uiLocation.placesCombo->setEnabled(false);
        uiLocation.lineEdit->setEnabled(false);
    } else if (m_url == KUrl("activities:/current/")) {
        uiLocation.showActivity->setChecked(true);
        uiLocation.placesCombo->setEnabled(false);
        uiLocation.lineEdit->setEnabled(false);
    } else {
        QModelIndex index;
        for (int i = 0; i < m_placesFilterModel->rowCount(); i++) {
            const KUrl url = m_placesModel->url(m_placesFilterModel->mapToSource(m_placesFilterModel->index(i, 0)));
            if (url.equals(m_url, KUrl::CompareWithoutTrailingSlash)) {
                index = m_placesFilterModel->index(i, 0);
                break;
            }
        }
        if (index.isValid()) {
            uiLocation.placesCombo->setCurrentIndex(index.row());
            uiLocation.showPlace->setChecked(true);
            uiLocation.lineEdit->setEnabled(false);
        } else {
            uiLocation.showCustomFolder->setChecked(true);
            uiLocation.lineEdit->setUrl(m_url);
            uiLocation.placesCombo->setEnabled(false);
        }
    }

    uiLocation.titleCombo->addItem(i18n("None"), QVariant::fromValue(FolderView::None));
    uiLocation.titleCombo->addItem(i18n("Default"), QVariant::fromValue(FolderView::PlaceName));
    uiLocation.titleCombo->addItem(i18n("Full Path"), QVariant::fromValue(FolderView::FullPath));
    uiLocation.titleCombo->addItem(i18n("Custom title"), QVariant::fromValue(FolderView::Custom));

    if (m_labelType == FolderView::Custom) {
        uiLocation.titleEdit->setEnabled(true);
        uiLocation.titleEdit->setText(m_customLabel);
    } else {
        uiLocation.titleEdit->setEnabled(false);
    }

    // The label is not shown when the applet is acting as a containment,
    // so don't confuse the user by making it editable.
    if (isContainment()) {
        uiLocation.titleLabel->hide();
        uiLocation.titleCombo->hide();
        uiLocation.titleEdit->hide();
    }

    for (int i = 0; i < uiLocation.titleCombo->maxCount(); i++) {
       if (m_labelType == uiLocation.titleCombo->itemData(i).value<FolderView::LabelType>()) {
           uiLocation.titleCombo->setCurrentIndex(i);
           break;
       }
    }
}

void LocationPage::postSetupUI()
{
    connect(uiLocation.showPlace, SIGNAL(toggled(bool)), uiLocation.placesCombo, SLOT(setEnabled(bool)));
    connect(uiLocation.showCustomFolder, SIGNAL(toggled(bool)), uiLocation.lineEdit, SLOT(setEnabled(bool)));
    connect(uiLocation.titleCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setTitleEditEnabled(int)));

    connect(uiLocation.showDesktopFolder, SIGNAL(toggled(bool)), parent(), SLOT(settingsModified()));
    connect(uiLocation.showActivity, SIGNAL(toggled(bool)), parent(), SLOT(settingsModified()));
    connect(uiLocation.showPlace, SIGNAL(toggled(bool)), parent(), SLOT(settingsModified()));
    connect(uiLocation.showCustomFolder, SIGNAL(toggled(bool)), parent(), SLOT(settingsModified()));
    connect(uiLocation.placesCombo, SIGNAL(currentIndexChanged(int)), parent(), SLOT(settingsModified()));
    connect(uiLocation.lineEdit, SIGNAL(textChanged(QString)), parent(), SLOT(settingsModified()));
    connect(uiLocation.titleCombo, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
    connect(uiLocation.titleEdit, SIGNAL(textChanged(QString)), parent, SLOT(settingsModified()));
}

void LocationPage::saveSettings()
{
    // TODO
}

void LocationPage::setTitleEditEnabled(int index)
{
    if (uiLocation.titleCombo->itemData(index).value<FolderView::LabelType>() == FolderView::Custom) {
        uiLocation.titleEdit->setEnabled(true);
        uiLocation.titleEdit->setFocus();
    } else {
        uiLocation.titleEdit->setEnabled(false);
    }
}

#include "locationpage.moc"
