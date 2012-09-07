/***************************************************************************
 *   Copyright 2012 by Sebastian Kügler <sebas@kde.org>                    *
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

#ifndef OWNCLOUDAPPLET_H
#define OWNCLOUDAPPLET_H

#include <Plasma/PopupApplet>
#include <Plasma/Label>

#include <QDeclarativeComponent>

namespace Plasma {
    class DeclarativeWidget;
}

class DirListerApplet : public Plasma::PopupApplet
{
    Q_OBJECT
    public:
        DirListerApplet(QObject *parent, const QVariantList &args);
        ~DirListerApplet();

        void init();
        QGraphicsWidget *graphicsWidget();

    public Q_SLOTS:
        void configChanged();

    protected:
        void createConfigurationInterface(KConfigDialog *parent);

    protected Q_SLOTS:
        void configAccepted();

    private:
        Plasma::DeclarativeWidget *m_declarativeWidget;
};

K_EXPORT_PLASMA_APPLET(dirlister, DirListerApplet)

#endif
