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

#ifndef GENERALOPTIONS_H
#define GENERALOPTIONS_H

#include "settings/optionsbase.h"

class GeneralOptions : public OptionsBase
{

public:
    GeneralOptions();

public:
    KUrl url() const { return m_url; }
    FolderView::LabelType labelType() const { return m_labelType; }
    QString customLabel() const { return m_customLabel; }

    void setUrl(const KUrl& url) { m_url = url; }
    void setLabelType(FolderView::LabelType labelType) { m_labelType = labelType; }
    void setCustomLabel(const QString& customLabel) { m_customLabel = customLabel; }

    virtual void loadDefaults();
    virtual void loadSettings(KConfigGroup&);
    virtual void writeSettings(KConfigGroup&);

protected:
    KUrl m_url;
    FolderView::LabelType m_labelType;
    QString m_customLabel;
};

#endif
