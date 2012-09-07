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

#include "dirlisterapplet.h"


#include <QtDeclarative>
//#include <QDeclarativeItem>

#include <KDebug>
#include <KConfigDialog>
#include <KConfigGroup>

#include <Plasma/DeclarativeWidget>
#include <Plasma/Package>

DirListerApplet::DirListerApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_declarativeWidget(0)
{
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
}

DirListerApplet::~DirListerApplet()
{
}

void DirListerApplet::init()
{
    setPopupIcon("system-file-manager");
    configChanged();

    Plasma::PackageStructure::Ptr structure = Plasma::PackageStructure::load("Plasma/Generic");
    Plasma::Package *package = new Plasma::Package(QString(), "org.kde.plasma.filemanagement", structure);
    //const QString qmlFile = package->filePath("ui", "DirectoryApplet.qml");
    const QString qmlFile = package->filePath("mainscript");
    delete package;

    kDebug() << " Loading QML File from package: " << qmlFile;
    m_declarativeWidget = new Plasma::DeclarativeWidget(this);

    //qmlRegisterType<DirectoryModel>("org.kde.plasma.filemanagement", 0, 1, "DirectoryModel");

    m_declarativeWidget->setQmlPath(qmlFile);
    m_declarativeWidget->setMinimumSize(220, 250);
    //QTimer::singleShot(1000, this, SLOT(connectObjects()));
}

void DirListerApplet::configChanged()
{
    // ...
}

QGraphicsWidget *DirListerApplet::graphicsWidget()
{
    return m_declarativeWidget;
}

void DirListerApplet::configAccepted()
{
}

void DirListerApplet::createConfigurationInterface(KConfigDialog *parent)
{
    parent->setButtons( KDialog::Ok | KDialog::Cancel | KDialog::Apply );
    connect(parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(configAccepted()));
}

#include "dirlisterapplet.moc"
