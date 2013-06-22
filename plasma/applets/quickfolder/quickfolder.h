/*
 *   Copyright © 2013 Ignat Semenov <ragnarokk91@gmail.com>
 *   Copyright © 2008, 2009 Fredrik Höglund <fredrik@kde.org>
 *   Copyright © 2008 Rafael Fernández López <ereslibre@kde.org>
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

#ifndef QUICKFOLDER_H
#define QUICKFOLDER_H

#include <plasma/containment.h>


class QuickFolder : public Plasma::Containment
{
    Q_OBJECT

public:
    QuickFolder(QObject *parent, const QVariantList &args);
    ~QuickFolder() {}

    void init() {}
    void saveState(KConfigGroup &group) const { Q_UNUSED(group) }
    void createConfigurationInterface(KConfigDialog *dialog) { Q_UNUSED(dialog) }
    void setPath(const QString& path) { Q_UNUSED(path) }
    void setWallpaper(const KUrl &url) { Q_UNUSED(url) }
};

#endif
