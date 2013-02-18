/***************************************************************************
 *   Copyright (C) 2012 by Peter Penz <peter.penz19@gmail.com>             *
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

#include "kstandarditemlistview.h"

#include <KDebug>
#include <KIconLoader>
#include "kstandarditemlistwidget.h"
#include "kstandarditemlistgroupheader.h"

KStandardItemListView::KStandardItemListView(QGraphicsWidget* parent) :
    KItemListView(parent),
    m_itemLayout(DetailsLayout)
{
    setAcceptDrops(true);
    setScrollOrientation(Qt::Vertical);
    setVisibleRoles(QList<QByteArray>() << "text");
}

KStandardItemListView::~KStandardItemListView()
{
}

void KStandardItemListView::setItemLayout(ItemLayout layout)
{
    if (m_itemLayout == layout) {
        return;
    }

    beginTransaction();

    const ItemLayout previous = m_itemLayout;
    m_itemLayout = layout;

    switch (layout) {
    case IconsLayout:
        setScrollOrientation(Qt::Vertical);
        setSupportsItemExpanding(false);
        break;
    case DetailsLayout:
        setScrollOrientation(Qt::Vertical);
        setSupportsItemExpanding(true);
        break;
    case CompactLayout:
        setScrollOrientation(Qt::Horizontal);
        setSupportsItemExpanding(false);
        break;
    default:
        Q_ASSERT(false);
        break;
    }

    onItemLayoutChanged(layout, previous);

    endTransaction();
}

KStandardItemListView::ItemLayout KStandardItemListView::itemLayout() const
{
    return m_itemLayout;
}

KItemListWidgetCreatorBase* KStandardItemListView::defaultWidgetCreator() const
{
    return new KItemListWidgetCreator<KStandardItemListWidget>();
}

KItemListGroupHeaderCreatorBase* KStandardItemListView::defaultGroupHeaderCreator() const
{
    return new KItemListGroupHeaderCreator<KStandardItemListGroupHeader>();
}

void KStandardItemListView::initializeItemListWidget(KItemListWidget* item)
{
    KStandardItemListWidget* standardItemListWidget = qobject_cast<KStandardItemListWidget*>(item);
    Q_ASSERT(standardItemListWidget);

    switch (itemLayout()) {
    case IconsLayout:   standardItemListWidget->setLayout(KStandardItemListWidget::IconsLayout); break;
    case CompactLayout: standardItemListWidget->setLayout(KStandardItemListWidget::CompactLayout); break;
    case DetailsLayout: standardItemListWidget->setLayout(KStandardItemListWidget::DetailsLayout); break;
    default:            Q_ASSERT(false); break;
    }

    standardItemListWidget->setSupportsItemExpanding(supportsItemExpanding());
}


bool KStandardItemListView::itemSizeHintUpdateRequired(const QSet<QByteArray>& changedRoles) const
{
    // The only thing that can modify the item's size hint is the amount of space
    // needed to display the text for the visible roles.
    // Even if the icons have a different size they are always aligned within
    // the area defined by KItemStyleOption.iconSize and hence result in no
    // change of the item-size.
    foreach (const QByteArray& role, visibleRoles()) {
        if (changedRoles.contains(role)) {
            return true;
        }
    }
    return false;
}

void KStandardItemListView::onItemLayoutChanged(ItemLayout current, ItemLayout previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);
    updateLayoutOfVisibleItems();
}

void KStandardItemListView::onScrollOrientationChanged(Qt::Orientation current, Qt::Orientation previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);
    updateLayoutOfVisibleItems();
}

void KStandardItemListView::onSupportsItemExpandingChanged(bool supportsExpanding)
{
    Q_UNUSED(supportsExpanding);
    updateLayoutOfVisibleItems();
}


void KStandardItemListView::polishEvent()
{
    switch (m_itemLayout) {
    case IconsLayout:   applyDefaultStyleOption(style()->pixelMetric(QStyle::PM_LargeIconSize), 2, 4, 8); break;
    case CompactLayout: applyDefaultStyleOption(style()->pixelMetric(QStyle::PM_SmallIconSize),  2, 8, 0); break;
    case DetailsLayout: applyDefaultStyleOption(style()->pixelMetric(QStyle::PM_SmallIconSize),  2, 0, 0); break;
    default:            Q_ASSERT(false); break;
    }

    QGraphicsWidget::polishEvent();
}

void KStandardItemListView::applyDefaultStyleOption(int iconSize,
                                                    int padding,
                                                    int horizontalMargin,
                                                    int verticalMargin)
{
    KItemListStyleOption option = styleOption();

    bool changed = false;
    if (option.iconSize < 0) {
        option.iconSize = iconSize;
        changed = true;
    }
    if (option.padding < 0) {
        option.padding = padding;
        changed = true;
    }
    if (option.horizontalMargin < 0) {
        option.horizontalMargin = horizontalMargin;
        changed = true;
    }
    if (option.verticalMargin < 0) {
        option.verticalMargin = verticalMargin;
        changed = true;
    }

    if (changed) {
        setStyleOption(option);
    }
}

void KStandardItemListView::updateLayoutOfVisibleItems()
{
    if (model()) {
        foreach (KItemListWidget* widget, visibleItemListWidgets()) {
            initializeItemListWidget(widget);
        }
    }
}

#include "kstandarditemlistview.moc"
