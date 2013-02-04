/*
 *   Copyright © 2012 Ignat Semenov <ignat.semenov@blue-systems.com>
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

#include "optionsbase.h"

#include <KGlobalSettings>
#include <KDirModel>


QString OptionsBase::sortOrderEnumToString(Qt::SortOrder order)
{
    if (order == Qt::AscendingOrder) {
        return "ascending";
    } else {
        return "descending";
    }
}

Qt::SortOrder OptionsBase::sortOrderStringToEnum(const QString& order)
{
    if (order == "ascending") {
       return Qt::AscendingOrder;
    } else {
       return Qt::DescendingOrder;
    }
}

QString OptionsBase::iconFlowEnumToString(IconView::Flow flow)
{
    switch (flow) {
        case IconView::LeftToRight:
            return "leftToRight";
            break;
        case IconView::RightToLeft:
            return "rightToLeft";
            break;
        case IconView::TopToBottom:
            return "topToBottomLeftToRight";
            break;
        default:
            return "topToBotomRightToLeft";
            break;
    }
}

IconView::Flow OptionsBase::iconFlowStringToEnum(const QString& flow)
{
    if (flow == "leftToRight") {
        return IconView::LeftToRight;
    } else if (flow == "rightToLeft") {
        return IconView::RightToLeft;
    } else if (flow == "topToBottomLeftToRight") {
        return IconView::TopToBottom;
    } else {
        return IconView::TopToBottomRightToLeft;
    }
}

QString OptionsBase::filterModeEnumToString(ProxyModel::FilterMode mode)
{
    switch (mode) {
        case ProxyModel::NoFilter:
            return "noFilter";
            break;
        case ProxyModel::FilterShowMatches:
            return "filterShowMatches";
            break;
        default:
            return "filterHideMatches";
            break;
    }
}

ProxyModel::FilterMode OptionsBase::filterModeStringToEnum(const QString& mode)
{
    if (mode == "noFilter") {
        return ProxyModel::NoFilter;
    } else if (mode == "filterShowMatches") {
        return ProxyModel::FilterShowMatches;
    } else {
        return ProxyModel::FilterHideMatches;
    }
}

QString OptionsBase::labelTypeEnumToString(FolderView::LabelType type)
{
    switch (type) {
        case FolderView::None:
            return "none";
            break;
        case FolderView::PlaceName:
            return "placeName";
            break;
        case FolderView::FullPath:
            return "fullPath";
            break;
        default:
            return "custom";
            break;
    }
}

FolderView::LabelType OptionsBase::labelTypeStringToEnum(const QString& type)
{
    if (type == "none") {
        return FolderView::None;
    } else if (type == "placeName") {
        return FolderView::PlaceName;
    } else if (type == "fullPath") {
        return FolderView::FullPath;
    } else {
        return FolderView::Custom;
    }
}