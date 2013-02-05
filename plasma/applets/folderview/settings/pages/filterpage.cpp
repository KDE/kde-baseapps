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

#include "settings/options/filteroptions.h"

#include "proxymodel.h"

Q_DECLARE_METATYPE(ProxyModel::FilterMode)

FilterPage::FilterPage(KConfigDialog* parent, OptionsBase* settings): PageBase(parent, settings)
{
    m_mimeModel = new MimeModel(uiFilter.filterFilesList);
    m_proxyMimeModel = new ProxyMimeModel(uiFilter.filterFilesList);
    m_proxyMimeModel->setSourceModel(m_mimeModel);
}

void FilterPage::setupUi()
{
    uiFilter.setupUi(this);

    uiFilter.filterFilesList->setModel(m_proxyMimeModel);

    uiFilter.filterCombo->addItem(i18n("Show All Files"), QVariant::fromValue(ProxyModel::NoFilter));
    uiFilter.filterCombo->addItem(i18n("Show Files Matching"), QVariant::fromValue(ProxyModel::FilterShowMatches));
    uiFilter.filterCombo->addItem(i18n("Hide Files Matching"), QVariant::fromValue(ProxyModel::FilterHideMatches));

}

void FilterPage::loadSettings()
{
    FilterOptions *options= static_cast<FilterOptions*>(m_options);

    uiFilter.filterFilesPattern->setText(options->filterFiles());

    for (int i = 0; i < uiFilter.filterCombo->maxCount(); i++) {
       if (options->filterMode() == uiFilter.filterCombo->itemData(i).value<ProxyModel::FilterMode>()) {
           uiFilter.filterCombo->setCurrentIndex(i);
           break;
       }
    }

    filterChanged(options->filterMode());

    QStringList mimeList = options->filterFilesMimeList();
    if (mimeList.count()) {
        for (int i = 0; i < m_proxyMimeModel->rowCount(); i++) {
            const QModelIndex index = m_proxyMimeModel->index(i, 0);
            const KMimeType *mime = static_cast<KMimeType*>(m_proxyMimeModel->mapToSource(index).internalPointer());
            if (mime && mimeList.contains(mime->name())) {
                mimeList.removeAll(mime->name());
                uiFilter.filterFilesList->model()->setData(index, Qt::Checked, Qt::CheckStateRole);
                options->setFilterFilesMimeList(mimeList);
            }
        }
    }
}

void FilterPage::setupModificationSignals()
{
    connect(uiFilter.searchMimetype, SIGNAL(textChanged(QString)), m_proxyMimeModel, SLOT(setFilter(QString)));
    connect(uiFilter.filterCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(filterChanged(int)));
    connect(uiFilter.selectAll, SIGNAL(clicked(bool)), this, SLOT(selectAllMimetypes()));
    connect(uiFilter.deselectAll, SIGNAL(clicked(bool)), this, SLOT(deselectAllMimeTypes()));

    connect(uiFilter.filterCombo, SIGNAL(currentIndexChanged(int)), parent(), SLOT(settingsModified()));
    connect(uiFilter.filterFilesPattern, SIGNAL(textChanged(QString)), parent(), SLOT(settingsModified()));
    connect(uiFilter.filterFilesList->model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), parent(), SLOT(settingsModified()));
}

// ==========Helper functions========

void FilterPage::selectAllMimetypes()
{
    toggleAllMimetypes(Qt::Checked);
}

void FilterPage::deselectAllMimeTypes()
{
    toggleAllMimetypes(Qt::Unchecked);
}

void FilterPage::toggleAllMimetypes(Qt::CheckState state)
{
    for (int i = 0; i < uiFilter.filterFilesList->model()->rowCount(); i++) {
        const QModelIndex index = uiFilter.filterFilesList->model()->index(i, 0);
        uiFilter.filterFilesList->model()->setData(index, state, Qt::CheckStateRole);
    }
}

void FilterPage::filterChanged(int index)
{
    const ProxyModel::FilterMode filterMode = uiFilter.filterCombo->itemData(index).value<ProxyModel::FilterMode>();
    const bool filterActive = (filterMode != ProxyModel::NoFilter);

    uiFilter.filterFilesPattern->setEnabled(filterActive);
    uiFilter.searchMimetype->setEnabled(filterActive);
    uiFilter.filterFilesList->setEnabled(filterActive);
    uiFilter.selectAll->setEnabled(filterActive);
    uiFilter.deselectAll->setEnabled(filterActive);
    if (filterActive) {
      for (int i = 0; i < uiFilter.filterFilesList->model()->rowCount(); i++) {
        const QModelIndex index = uiFilter.filterFilesList->model()->index(i, 0);
        uiFilter.filterFilesList->model()->setData(index, Qt::Checked, Qt::CheckStateRole);
      }
    }
}

void FilterPage::saveSettings()
{
    FilterOptions *options= static_cast<FilterOptions*>(m_options);

    options->setFilterFiles(uiFilter.filterFilesPattern->text());

    const ProxyModel::FilterMode filterMode =
    uiFilter.filterCombo->itemData(uiFilter.filterCombo->currentIndex()).value<ProxyModel::FilterMode>();
    options->setFilterMode(filterMode);

    // Now, we have to iterate over all items (not only the filtered ones). For that reason we have
    // to ask the source model, not the proxy model.
    QStringList selectedItems;
    ProxyMimeModel *proxyModel = static_cast<ProxyMimeModel*>(uiFilter.filterFilesList->model());
    for (int i = 0; i < proxyModel->sourceModel()->rowCount(); i++) {
        const QModelIndex index = proxyModel->sourceModel()->index(i, 0);
        if (index.model()->data(index, Qt::CheckStateRole).toInt() == Qt::Checked) {
            KMimeType *mime = static_cast<KMimeType*>(index.internalPointer());
            if (mime) {
                selectedItems << mime->name();
            }
        }
    }
    options->setFilterFilesMimeList(selectedItems);

    options->writeSettings();
}

#include "filterpage.moc"
