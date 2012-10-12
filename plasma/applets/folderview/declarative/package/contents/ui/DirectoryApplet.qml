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

    property int iconSize: 48
    property bool selectionModeEnabled: false // too experimental / broken right now

    DirectoryModel {
        id: dirModel
        sortDirectoriesFirst: true
        fileNameFilter: "*.png"
    }

    //Rectangle { anchors.fill: parent; color: "green"; opacity: 0.2 }

    Item {
        id: selection
        //parent: dirGrid

        property int selectionX
        property int selectionY
        property int selectionWidth
        property int selectionHeight

        Rectangle {
            id: selectionRect
            color: "#51AECFFF"
            //opacity: 0.9
            border.width: 1
            border.color: "#51AECF"

            x: parent.selectionX
            y: parent.selectionY
            width: parent.selectionWidth
            height: parent.selectionHeight
        }

    }


    QtExtras.MouseEventListener {
        id: mel
        anchors.fill: parent

        onPressed: {
            if (!selectionModeEnabled) return;
            selection.selectionX = mouse.x;
            selection.selectionY = mouse.y;
            selection.selectionWidth = 0;
            selection.selectionHeight = 0;
            dirGrid.interactive = false;
        }
        onPositionChanged: {
            if (!selectionModeEnabled) return;

            var w = (mouse.x - selection.selectionX);
            var h = (mouse.y - selection.selectionY);
            var _x = selection.selectionX;
            var _y = selection.selectionY;

            if (w > 0) {
                selection.selectionWidth = w;
            } else {
                print( " X, W: " + _x + ", " + (w*-1));
                selection.selectionX = selection.selectionX + w;
                selection.selectionWidth = w;
            }

            if (h > 0) {
                selection.selectionHeight = (mouse.y - selection.selectionY);
            } else {

            }

        }

        onReleased: {
            if (!selectionModeEnabled) return;
            // reset selection
            dirGrid.interactive = true;
            selection.selectionX = mouse.x;
            selection.selectionY = mouse.y;
            selection.selectionWidth = 0;
            selection.selectionHeight = 0;
        }
        GridView {
            id: dirGrid
            anchors.fill: parent
            cellWidth: iconSize * 1.5
            cellHeight: iconSize * 2
            model: dirModel
            clip: true

            delegate: Item {
                id: fileDelegate
                width: GridView.view.cellWidth
                height: GridView.view.cellHeight

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
                    anchors {
                        top: fileIcon.bottom;
                        horizontalCenter: fileIcon.horizontalCenter;
                    }
                    text: name
                }
                //Rectangle { anchors.fill: parent; color: "green"; opacity: 0.2 }
            }
        }

    }

    Component.onCompleted: print("DirectoryApplet loaded.")
}