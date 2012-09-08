/***************************************************************************
 *                                                                         *
 *   Copyright 2012 Sebastian Kügler <sebas@kde.org>                       *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

import QtQuick 1.1
import org.kde.plasma.components 0.1 as PlasmaComponents
//import org.kde.plasma.extras 0.1 as PlasmaExtras
import org.kde.qtextracomponents 0.1 as QtExtras
import org.kde.plasma.filemanagement 0.1


Item {
    id: directoryApplet

    width: 400
    height: 400

    property int iconSize: 64

    DirectoryModel {
        id: dirModel

    }

    //Rectangle { anchors.fill: parent; color: "green"; opacity: 0.2 }

    GridView {
        id: dirGrid
        anchors.fill: parent
        cellWidth: iconSize * 1.5
        cellHeight: iconSize * 2
        //spacing: iconSize / 4
        model: dirModel

        delegate: Item {
            id: fileDelegate
            width: GridView.view.cellWidth
            height: GridView.view.cellHeight

            clip: true
            QtExtras.QIconItem {
                id: fileIcon
                icon: iconName
                width: iconSize
                height: width
                anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; }
            }

            PlasmaComponents.Label {
                id: nameLabel
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 3
                width: parent.width
                font.pointSize: theme.smallestFont.pointSize
                //width: paintedWidth > parent.width ? parent.width : paintedWidth
                anchors {
                    top: fileIcon.bottom;
                    //topMargin: iconSize/4;
                    horizontalCenter: fileIcon.horizontalCenter;
                }
                text: name
            }
            Rectangle { anchors.fill: parent; color: "green"; opacity: 0.2 }
        }
    }

    Component.onCompleted: print("DirectoryApplet loaded.")
}