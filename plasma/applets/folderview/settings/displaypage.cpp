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

#include "displaypage.h"

#include <KDirModel>

#include "../iconview.h"
#include "previewpluginsmodel.h"

DisplayPage::DisplayPage(KConfigDialog* parent, Settings* settings): PageBase(parent, settings)
{
    uiDisplay.setupUi(this);

    m_titleLineEdit = new KLineEdit(this);
    m_titleLineEdit->setClearButtonShown(false);
    m_titleLineEdit->setClickMessage(i18n("Title"));
    uiDisplay.titleEdit->setLineEdit(m_titleLineEdit);

    QString configTitleText = m_customLabel;
    if (m_customLabel.isEmpty() || m_customLabel == "___EMPTY___") {
        configTitleText = i18n("None");
    } else if (m_customLabel == "___DEFAULT___") {
        configTitleText = i18n("Default");
    } else if (m_customLabel == "___FULL___") {
        configTitleText = i18n("Full path");
    } else {
        uiDisplay.titleEdit->addItem(m_customLabel);
    }

    uiDisplay.titleEdit->addItem(i18n("None"));
    uiDisplay.titleEdit->addItem(i18n("Default"));
    uiDisplay.titleEdit->addItem(i18n("Full path"));
    uiDisplay.titleEdit->setCurrentItem(configTitleText);
    uiDisplay.titleEdit->completionBox()->setActivateOnSelect(true);
    uiDisplay.titleEdit->setCompletionMode(KGlobalSettings::CompletionPopupAuto);
    connect(uiDisplay.titleEdit->lineEdit(), SIGNAL(editingFinished()), this, SLOT(setTitleText()));

    const QList<int> iconSizes = QList<int>() << 16 << 22 << 32 << 48 << 64 << 128;
    uiDisplay.sizeSlider->setRange(0, iconSizes.size() - 1);
    uiDisplay.sizeSlider->setValue(iconSizes.indexOf(iconSize().width()));

//     Let iconview always be there for now
//     // Only add "Unsorted" as an option when we're showing an icon view, since the list view
//     // doesn't allow the user to rearrange the icons.
//     if (m_iconView) {
        uiDisplay.sortCombo->addItem(i18nc("Sort Icons", "Unsorted"), -1);
//     }
    uiDisplay.sortCombo->addItem(KGlobal::locale()->removeAcceleratorMarker(m_actionCollection.action("sort_name")->text()), KDirModel::Name);
    uiDisplay.sortCombo->addItem(KGlobal::locale()->removeAcceleratorMarker(m_actionCollection.action("sort_size")->text()), KDirModel::Size);
    uiDisplay.sortCombo->addItem(KGlobal::locale()->removeAcceleratorMarker(m_actionCollection.action("sort_type")->text()), KDirModel::Type);
    uiDisplay.sortCombo->addItem(KGlobal::locale()->removeAcceleratorMarker(m_actionCollection.action("sort_date")->text()), KDirModel::ModifiedTime);

    uiDisplay.flowCombo->addItem(i18n("Top to Bottom, Left to Right"), IconView::TopToBottom);
    uiDisplay.flowCombo->addItem(i18n("Top to Bottom, Right to Left"), IconView::TopToBottomRightToLeft);
    uiDisplay.flowCombo->addItem(i18n("Left to Right, Top to Bottom"), IconView::LeftToRight);
    uiDisplay.flowCombo->addItem(i18n("Right to Left, Top to Bottom"), IconView::RightToLeft);

    uiDisplay.alignToGrid->setChecked(m_alignToGrid);
    uiDisplay.clickToView->setChecked(m_clickToView);
    uiDisplay.lockInPlace->setChecked(m_iconsLocked);
    uiDisplay.drawShadows->setChecked(m_drawShadows);
    uiDisplay.showPreviews->setChecked(m_showPreviews);
    uiDisplay.previewsAdvanced->setEnabled(m_showPreviews);
    uiDisplay.numLinesEdit->setValue(m_numTextLines);
    uiDisplay.colorButton->setColor(textColor());

    for (int i = 0; i < uiDisplay.sortCombo->maxCount(); i++) {
       if (m_sortColumn == uiDisplay.sortCombo->itemData(i).toInt()) {
           uiDisplay.sortCombo->setCurrentIndex(i);
           break;
       }
    }

    for (int i = 0; i < uiDisplay.flowCombo->maxCount(); i++) {
       if (m_flow == uiDisplay.flowCombo->itemData(i).toInt()) {
           uiDisplay.flowCombo->setCurrentIndex(i);
           break;
       }
    }

    connect(uiDisplay.previewsAdvanced, SIGNAL(clicked()), this, SLOT(showPreviewConfigDialog()));
    connect(uiDisplay.showPreviews, SIGNAL(toggled(bool)), uiDisplay.previewsAdvanced, SLOT(setEnabled(bool)));

    connect(uiDisplay.titleEdit, SIGNAL(textChanged(QString)), parent(), SLOT(settingsModified()));
    connect(uiDisplay.flowCombo, SIGNAL(currentIndexChanged(int)), parent(), SLOT(settingsModified()));
    connect(uiDisplay.sortCombo, SIGNAL(currentIndexChanged(int)), parent(), SLOT(settingsModified()));
    connect(uiDisplay.sizeSlider, SIGNAL(valueChanged(int)), parent(), SLOT(settingsModified()));
    connect(uiDisplay.showPreviews, SIGNAL(toggled(bool)), parent(), SLOT(settingsModified()));
    connect(uiDisplay.lockInPlace, SIGNAL(toggled(bool)), parent(), SLOT(settingsModified()));
    connect(uiDisplay.alignToGrid, SIGNAL(toggled(bool)), parent(), SLOT(settingsModified()));
    connect(uiDisplay.clickToView, SIGNAL(toggled(bool)), parent(), SLOT(settingsModified()));
    connect(uiDisplay.numLinesEdit, SIGNAL(valueChanged(int)), parent(), SLOT(settingsModified()));
    connect(uiDisplay.colorButton, SIGNAL(changed(QColor)), parent(), SLOT(settingsModified()));
    connect(uiDisplay.drawShadows, SIGNAL(toggled(bool)), parent(), SLOT(settingsModified()));
}

void DisplayPage::showPreviewConfigDialog()
{
    QWidget *widget = new QWidget;
    uiPreviewConfig.setupUi(widget);

    PreviewPluginsModel *model = new PreviewPluginsModel(this);
    model->setCheckedPlugins(m_previewPlugins);

    uiPreviewConfig.listView->setModel(model);

    KDialog *dialog = new KDialog;
    dialog->setMainWidget(widget);

    if (dialog->exec() == KDialog::Accepted) {
        m_previewPlugins = model->checkedPlugins();
    }

    delete widget;
    delete dialog;
    delete model;
}

void DisplayPage::saveSettings()
{

}

#include "displaypage.moc"
