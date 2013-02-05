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

#include "settings/options/displayoptions.h"
#include "previewpluginsmodel.h"

DisplayPage::DisplayPage(KConfigDialog* parent, DisplayOptions* options): PageBase(parent), m_options(options)
{
    m_iconSizes = QList<int>() << 16 << 22 << 32 << 48 << 64 << 128;
}

void DisplayPage::setupUi()
{
    uiDisplay.setupUi(this);

    uiDisplay.sizeSlider->setRange(0, m_iconSizes.size() - 1);

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

    uiDisplay.flowCombo->addItem(i18n("Top to Bottom, Left to Right"), QVariant::fromValue(IconView::TopToBottom));
    uiDisplay.flowCombo->addItem(i18n("Top to Bottom, Right to Left"), QVariant::fromValue(IconView::TopToBottomRightToLeft));
    uiDisplay.flowCombo->addItem(i18n("Left to Right, Top to Bottom"), QVariant::fromValue(IconView::LeftToRight));
    uiDisplay.flowCombo->addItem(i18n("Right to Left, Top to Bottom"), QVariant::fromValue(IconView::RightToLeft));
}

void DisplayPage::loadSettings()
{
    uiDisplay.sizeSlider->setValue(m_iconSizes.indexOf(iconSize().width()));

    uiDisplay.alignToGrid->setChecked(m_options->alignToGrid());
    uiDisplay.clickToView->setChecked(m_options->clickToView());
    uiDisplay.lockInPlace->setChecked(m_options->iconsLocked());
    uiDisplay.drawShadows->setChecked(m_options->drawShadows());
    uiDisplay.showPreviews->setChecked(m_options->showPreviews());
    uiDisplay.previewsAdvanced->setEnabled(m_options->showPreviews());
    uiDisplay.numLinesEdit->setValue(m_options->numTextLines());
    uiDisplay.colorButton->setColor(textColor());

    for (int i = 0; i < uiDisplay.sortCombo->maxCount(); i++) {
       if (m_options->sortColumn() == uiDisplay.sortCombo->itemData(i).toInt()) {
           uiDisplay.sortCombo->setCurrentIndex(i);
           break;
       }
    }

    for (int i = 0; i < uiDisplay.flowCombo->maxCount(); i++) {
       if (m_options->flow() == uiDisplay.flowCombo->itemData(i).value<IconView::Flow>()) {
           uiDisplay.flowCombo->setCurrentIndex(i);
           break;
       }
    }
}

void DisplayPage::setupModificationSignals()
{
    connect(uiDisplay.previewsAdvanced, SIGNAL(clicked()), this, SLOT(showPreviewConfigDialog()));
    connect(uiDisplay.showPreviews, SIGNAL(toggled(bool)), uiDisplay.previewsAdvanced, SLOT(setEnabled(bool)));

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

// ==========Helper functions========

void DisplayPage::showPreviewConfigDialog()
{
    QWidget *widget = new QWidget;
    uiPreviewConfig.setupUi(widget);

    PreviewPluginsModel *model = new PreviewPluginsModel(this);
    model->setCheckedPlugins(m_options->previewPlugins());

    uiPreviewConfig.listView->setModel(model);

    KDialog *dialog = new KDialog;
    dialog->setMainWidget(widget);

    if (dialog->exec() == KDialog::Accepted) {
       m_options->setPreviewPlugins(model->checkedPlugins());
    }

    delete widget;
    delete dialog;
    delete model;
}

void DisplayPage::saveSettings()
{
    // TODO
}


AppletDisplayPage::AppletDisplayPage(KConfigDialog* parent, DisplayPage* options): DisplayPage(parent, options)
{
}

void AppletDisplayPage::setupUi()
{
    DisplayPage::setupUi();

    uiDisplay.flowLabel->hide();
    uiDisplay.flowCombo->hide();
}


ContainmentDisplayPage::ContainmentDisplayPage(KConfigDialog* parent, DisplayOptions* options): DisplayPage(parent, options)
{
}


#include "displaypage.moc"
