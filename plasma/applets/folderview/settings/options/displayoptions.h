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

#ifndef DISPLAYOPTIONS_H
#define DISPlAYOPTIONS_H

#include "../optionsbase.h"

class DisplayOptions : public OptionsBase
{

public:
    DisplayOptions(KConfigGroup *group, QObject *parent);

public:
    // getters
    int sortColumn() const { return m_sortColumn; }
    Qt::SortOrder sortOrder() const { return m_sortOrder; }
    bool sortDirsFirst() const { return m_sortDirsFirst; }

    IconView::Flow flow() const { return m_flow; }
    bool showPreviews() const { return m_showPreviews; }
    QStringList previewPlugins() const { return m_previewPlugins; }

    QColor textColor() const { return m_textColor; }
    bool drawShadows() const { return m_drawShadows; }
    bool iconsLocked() const { return m_iconsLocked; }
    bool alignToGrid() const { return m_alignToGrid; }
    bool clickToView() const { return m_clickToView; }
    bool showSelectionMarker() const { return m_showSelectionMarker; }
    int customIconSize() { return m_customIconSize; }
    int numTextLines() const { return m_numTextLines; }

    // setters
    void setSortColumn(int column) { m_sortColumn = column; }
    void setSortOrder(Qt::SortOrder order) { m_sortOrder = order; }
    void setSortDirsFirst(bool on) { m_sortDirsFirst = on; }

    void setFlow(IconView::Flow) { m_flow = flow; }
    void setShowPreviews(bool on) { m_showPreviews = on; }
    void setPreviewPlugins(const QStringList &list) { m_previewPlugins = list; }

    void setTextColor(QColor color) { m_textColor = color; }
    void setDrawShadows(bool on) { m_drawShadows = on; }
    void setIconsLocked(bool on) { m_iconsLocked = on; }
    void setAlignToGrid(bool on) { m_alignToGrid = on; }
    void setClickToView(bool on) { m_alignToGrid = on; }
    void setShowSelectionMarker(bool on) { m_showSelectionMarker = on; }
    void setCustomIconSize(int size) { m_customIconSize = size; }
    void setNumTextLines(int number) { m_numTextLines = number; }

    virtual void loadDefaults();
    virtual void loadSettings();
    virtual void writeSettings();

protected:
    int m_sortColumn;
    Qt::SortOrder m_sortOrder;
    bool m_sortDirsFirst;

    IconView::Flow m_flow;
    bool m_showPreviews;
    QStringList m_previewPlugins;

    QColor m_textColor;
    bool m_drawShadows;
    bool m_iconsLocked;
    bool m_alignToGrid;
    bool m_clickToView;
    bool m_showSelectionMarker;
    int m_customIconSize;
    int m_numTextLines;
};

/**
 * Helper class, encapsulates different default flow settings in the containment and applet version of the applet
 */
class AppletDisplayOptions : public DisplayOptions
{
public:
    AppletDisplayOptions(KConfigGroup* group);

    void loadDefaults();
};

class ContainmentDisplayOptions : public DisplayOptions
{
public:
    ContainmentDisplayOptions(KConfigGroup* group);

    void loadDefaults();
};

#endif
