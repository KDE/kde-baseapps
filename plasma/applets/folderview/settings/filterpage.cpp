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

#include "filterpage.h"

#include "../proxymodel.h"

FilterPage::FilterPage(KConfigDialog* parent, Settings* settings): PageBase(parent, settings)
{
}

void FilterPage::preSetupUi()
{
    uiFilter.setupUi(this);

    m_mimeModel = new MimeModel(uiFilter.filterFilesList);
    m_proxyMimeModel = new ProxyMimeModel(uiFilter.filterFilesList);
    m_proxyMimeModel->setSourceModel(m_mimeModel);
    uiFilter.filterFilesList->setModel(m_proxyMimeModel);
}

void FilterPage::setupUi()
{
    uiFilter.filterFilesPattern->setText(m_filterFiles);

    uiFilter.filterCombo->addItem(i18n("Show All Files"), QVariant::fromValue(ProxyModel::NoFilter));
    uiFilter.filterCombo->addItem(i18n("Show Files Matching"), QVariant::fromValue(ProxyModel::FilterShowMatches));
    uiFilter.filterCombo->addItem(i18n("Hide Files Matching"), QVariant::fromValue(ProxyModel::FilterHideMatches));

    for (int i = 0; i < uiFilter.filterCombo->maxCount(); i++) {
       if (m_filterType == uiFilter.filterCombo->itemData(i).value<ProxyModel::FilterMode>()) {
           uiFilter.filterCombo->setCurrentIndex(i);
           break;
       }
    }

    filterChanged(m_filterType);

    if (m_filterFilesMimeList.count()) {
        for (int i = 0; i < pMimeModel->rowCount(); i++) {
            const QModelIndex index = pMimeModel->index(i, 0);
            const KMimeType *mime = static_cast<KMimeType*>(pMimeModel->mapToSource(index).internalPointer());
            if (mime && m_filterFilesMimeList.contains(mime->name())) {
                m_filterFilesMimeList.removeAll(mime->name());
                uiFilter.filterFilesList->model()->setData(index, Qt::Checked, Qt::CheckStateRole);
            }
        }
    }
}

void FilterPage::postSetupUI()
{
    connect(uiFilter.searchMimetype, SIGNAL(textChanged(QString)), m_proxyMimeModel, SLOT(setFilter(QString)));
    connect(uiFilter.filterCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(filterChanged(int)));
    connect(uiFilter.selectAll, SIGNAL(clicked(bool)), this, SLOT(selectUnselectAll()));
    connect(uiFilter.deselectAll, SIGNAL(clicked(bool)), this, SLOT(selectUnselectAll()));

    connect(uiFilter.filterCombo, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
    connect(uiFilter.filterFilesPattern, SIGNAL(textChanged(QString)), parent, SLOT(settingsModified()));
    connect(uiFilter.filterFilesList->model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), parent, SLOT(settingsModified()));
}

// ==========Helper functions========
void FilterPage::selectUnselectAll()
{
    Qt::CheckState state = sender() == uiFilter.selectAll ? Qt::Checked : Qt::Unchecked;
    for (int i = 0; i < uiFilter.filterFilesList->model()->rowCount(); i++) {
        const QModelIndex index = uiFilter.filterFilesList->model()->index(i, 0);
        uiFilter.filterFilesList->model()->setData(index, state, Qt::CheckStateRole);
    }
}

void FilterPage::filterChanged(int)
{
    const ProxyModel::FilterMode filterMode = uiFilter.filterCombo->itemData(index).value<ProxyModel::FilterMode>();
    const bool filterActive = (filterMode != ProxyModel::NoFilter);

    uiFilter.filterFilesPattern->setEnabled(filterActive);
    uiFilter.searchMimetype->setEnabled(filterActive);
    uiFilter.filterFilesList->setEnabled(filterActive);
    uiFilter.selectAll->setEnabled(filterActive);
    uiFilter.deselectAll->setEnabled(filterActive);
    if ((filterActive) && (m_userSelectedShowAllFiles == 0)) {
      for (int i = 0; i < uiFilter.filterFilesList->model()->rowCount(); i++) {
        const QModelIndex index = uiFilter.filterFilesList->model()->index(i, 0);
        uiFilter.filterFilesList->model()->setData(index, Qt::Checked, Qt::CheckStateRole);
      }
    }
}

void FilterPage::saveSettings()
{
    // TODO
}

#include "filterpage.moc"
