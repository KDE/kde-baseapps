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

#include <KConfigGroup>

#include "iconview.h"
#include "proxymodel.h"
#include "folderview.h"

/**
 * Base class for the different config pane option classes.
 * Also contains the static helper enum <-> string functions.
 */
class OptionsBase : public QObject
{
    Q_OBJECT

public:
    OptionsBase(KConfigGroup *, QObject *parent);

    /** Load default option values before reading from a configuration group. */
    virtual void loadDefaults() = 0;

    /**
     * Read option values from a configuration group using current values as the default ones.
     */
    virtual void loadSettings() = 0;

    /** 
     * Helper functions for reliable writing and reading of enums from configuration files.
     */
    static QString sortOrderEnumToString(Qt::SortOrder);
    static Qt::SortOrder sortOrderStringToEnum(const QString&);

    static QString filterModeEnumToString(ProxyModel::FilterMode);
    static ProxyModel::FilterMode filterModeStringToEnum(const QString&);

    static QString iconFlowEnumToString(IconView::Flow);
    static IconView::Flow iconFlowStringToEnum(const QString&);

    static QString labelTypeEnumToString(FolderView::LabelType);
    static FolderView::LabelType labelTypeStringToEnum(const QString &);

protected:
    KConfigGroup *m_cg;
};

#endif
