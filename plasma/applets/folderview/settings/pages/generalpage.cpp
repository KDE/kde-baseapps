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

#include "generalpage.h"

#include <KGlobalSettings>

#include "settings/options/generaloptions.h"
#include "folderview.h"

Q_DECLARE_METATYPE(FolderView::LabelType)

GeneralPage::GeneralPage(KConfigDialog *dialog, GeneralOptions *options) : PageBase(dialog), m_options(options)
{
}

void GeneralPage::setupUi()
{
    uiLocation.setupUi(this);

    m_placesModel = new KFilePlacesModel(this);
    m_placesFilterModel = new PlacesFilterModel(this);
    m_placesFilterModel->setSourceModel(m_placesModel);
    uiLocation.placesCombo->setModel(m_placesFilterModel);
    uiLocation.lineEdit->setMode(KFile::Directory);

    uiLocation.titleCombo->addItem(i18n("None"), QVariant::fromValue(FolderView::None));
    uiLocation.titleCombo->addItem(i18n("Default"), QVariant::fromValue(FolderView::PlaceName));
    uiLocation.titleCombo->addItem(i18n("Full Path"), QVariant::fromValue(FolderView::FullPath));
    uiLocation.titleCombo->addItem(i18n("Custom title"), QVariant::fromValue(FolderView::Custom));
}

void GeneralPage::loadSettings()
{
    QDir desktopFolder(KGlobalSettings::desktopPath());
    const bool desktopVisible = desktopFolder != QDir::homePath() && desktopFolder.exists();
    uiLocation.showDesktopFolder->setVisible(desktopVisible);
    if (desktopVisible && m_options->url() == KUrl("desktop:/")) {
        uiLocation.showDesktopFolder->setChecked(true);
        uiLocation.placesCombo->setEnabled(false);
        uiLocation.lineEdit->setEnabled(false);
    } else if (m_options->url() == KUrl("activities:/current/")) {
        uiLocation.showActivity->setChecked(true);
        uiLocation.placesCombo->setEnabled(false);
        uiLocation.lineEdit->setEnabled(false);
    } else {
        QModelIndex index;
        for (int i = 0; i < m_placesFilterModel->rowCount(); i++) {
            const KUrl url = m_placesModel->url(m_placesFilterModel->mapToSource(m_placesFilterModel->index(i, 0)));
            if (url.equals(m_options->url(), KUrl::CompareWithoutTrailingSlash)) {
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
            uiLocation.lineEdit->setUrl(m_options->url());
            uiLocation.placesCombo->setEnabled(false);
        }
    }

    if (m_options->labelType()== FolderView::Custom) {
        uiLocation.titleEdit->setEnabled(true);
        uiLocation.titleEdit->setText(m_options->customLabel());
    } else {
        uiLocation.titleEdit->setEnabled(false);
    }

    for (int i = 0; i < uiLocation.titleCombo->maxCount(); i++) {
       if (m_options->labelType()== uiLocation.titleCombo->itemData(i).value<FolderView::LabelType>()) {
           uiLocation.titleCombo->setCurrentIndex(i);
           break;
       }
    }
}

void GeneralPage::setupModificationSignals()
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
    connect(uiLocation.titleCombo, SIGNAL(currentIndexChanged(int)), parent(), SLOT(settingsModified()));
    connect(uiLocation.titleEdit, SIGNAL(textChanged(QString)), parent(), SLOT(settingsModified()));
}

void GeneralPage::saveSettings()
{
    KUrl url;

    if (uiLocation.showDesktopFolder->isChecked()) {
        url = KUrl("desktop:/");
    } else if (uiLocation.showActivity->isChecked()) {
        url = KUrl("activities:/current/");
    } else if (uiLocation.showPlace->isChecked()) {
        PlacesFilterModel *filter = static_cast<PlacesFilterModel*>(uiLocation.placesCombo->model());
        KFilePlacesModel *model = static_cast<KFilePlacesModel*>(filter->sourceModel());
        url = model->url(filter->mapToSource(filter->index(uiLocation.placesCombo->currentIndex(), 0)));
    } else {
        url = uiLocation.lineEdit->url();
    }

    if (url.isEmpty()) {
        url = KUrl(QDir::homePath());
    }

    m_options->setUrl(url);

    const FolderView::LabelType labelType =
    uiLocation.titleCombo->itemData(uiLocation.titleCombo->currentIndex()).value<FolderView::LabelType>();
    QString customTitle;
    if (labelType == FolderView::Custom) {
        customTitle = uiLocation.titleEdit->text();
    } else {
        customTitle.clear();
    }

    m_options->setLabelType(labelType);
    m_options->setCustomLabel(customTitle);
}

void GeneralPage::setTitleEditEnabled(int index)
{
    if (uiLocation.titleCombo->itemData(index).value<FolderView::LabelType>() == FolderView::Custom) {
        uiLocation.titleEdit->setEnabled(true);
        uiLocation.titleEdit->setFocus();
    } else {
        uiLocation.titleEdit->setEnabled(false);
    }
}


AppletGeneralPage::AppletGeneralPage(KConfigDialog* parent, GeneralOptions* options): GeneralPage(parent, options)
{
}

ContainmentGeneralPage::ContainmentGeneralPage(KConfigDialog* parent, GeneralOptions* options): GeneralPage(parent, options)
{
}

void ContainmentGeneralPage::setupUi()
{
    GeneralPage::setupUi();

    uiLocation.titleLabel->hide();
    uiLocation.titleCombo->hide();
    uiLocation.titleEdit->hide();
}

#include "generalpage.moc"
