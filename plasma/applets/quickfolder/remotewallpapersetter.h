/*
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

#ifndef REMOTE_WALLPAPER_SETTER
#define REMOTE_WALLPAPER_SETTER

#include <QObject>

class QuickFolder;
class KUrl;
class KJob;


/**
 * Helper class that downloads a wallpaper image asynchronously to a suitable
 * temporary directory in the user's home folder, and applies it to the given
 * folderview containment when the download finishes.
 *
 * The class deletes itself automatically when the operation is completed.
 */
class RemoteWallpaperSetter : public QObject
{
    Q_OBJECT

public:
    RemoteWallpaperSetter(const KUrl &url, QuickFolder *containment);

private slots:
    void result(KJob *job);
};

#endif
