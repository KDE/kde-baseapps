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

#ifndef OPTIONS_H
#define OPTIONS_H

#include <QObject>
#include <QColor>
#include <QStringList>

#include <KConfigGroup>
#include <KUrl>

#include "../iconview.h"
#include "../proxymodel.h"


class Options : public QObject
{
    Q_OBJECT

public:

    Options();

    void loadDefaults();
    void loadSettings(KConfigGroup &);
    void writeSettings(KConfigGroup &);

    static QString sortOrderEnumToString(Qt::SortOrder);
    static Qt::SortOrder sortOrderStringToEnum(const QString&);

    static QString filterModeEnumToString(ProxyModel::FilterMode);
    static ProxyModel::FilterMode filterModeStringToEnum(const QString&);

    static QString iconFlowEnumToString(IconView::Flow);
    static IconView::Flow iconFlowStringToEnum(const QString&);

private:

    KUrl m_url;

    QColor m_textColor;
    QString m_titleText;
    int m_sortColumn;
    Qt::SortOrder m_sortOrder;
    bool m_sortDirsFirst;
    bool m_showPreviews;
    bool m_drawShadows;
    bool m_iconsLocked;
    bool m_alignToGrid;
    bool m_clickToView;
    bool m_showSelectionMarker;
    bool m_userSelectedShowAllFiles;
    bool m_blankLabel;
    QString m_customLabel;
    QStringList m_previewPlugins;
    int m_customIconSize;
    int m_numTextLines;
    IconView::Flow m_flow;

    ProxyModel::FilterMode m_filterType;
    QString m_filterFiles;
    QStringList m_filterFilesMimeList;
};

#endif
