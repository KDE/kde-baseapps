/***************************************************************************
                               konqsidebar.cpp
                             -------------------
    begin                : Sat June 2 16:25:27 CEST 2001
    copyright            : (C) 2001 Joseph Wenninger
    email                : jowenn@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "sidebar_part.h"
#include <kaboutdata.h>

#include <kparts/part.h>
#include <konq_events.h>
#include <kdebug.h>
#include <QApplication>
#include <kacceleratormanager.h>
#include <KLocalizedString>

K_PLUGIN_FACTORY(KonqSidebarFactory,
                 registerPlugin<KonqSidebarPart>();
                )

KonqSidebarPart::KonqSidebarPart(QWidget *parentWidget, QObject *parent, const QVariantList &)
    : KParts::ReadOnlyPart(parent)
{
    KAboutData aboutData("konqsidebartng", i18n("Extended Sidebar"), "0.2");
    aboutData.addAuthor(i18n("Joseph Wenninger"), "", "jowenn@bigfoot.com");
    aboutData.addAuthor(i18n("David Faure"), "", "faure@kde.org");
    setComponentData(aboutData);

    QString currentProfile = parentWidget->window()->property("currentProfile").toString();
    if (currentProfile.isEmpty()) {
        currentProfile = "default";
    }
    m_widget = new Sidebar_Widget(parentWidget, this, currentProfile);
    m_extension = new KonqSidebarBrowserExtension(this, m_widget);
    connect(m_widget, SIGNAL(started(KIO::Job*)),
            this, SIGNAL(started(KIO::Job*)));
    connect(m_widget, SIGNAL(completed()),
            this, SIGNAL(completed()));
    connect(m_extension, SIGNAL(addWebSideBar(QUrl,QString)),
            m_widget, SLOT(addWebSideBar(QUrl,QString)));
    KAcceleratorManager::setNoAccel(m_widget);
    setWidget(m_widget);
}

KonqSidebarPart::~KonqSidebarPart()
{
}

bool KonqSidebarPart::openFile()
{
    return true;
}

bool KonqSidebarPart::openUrl(const QUrl &url)
{
    return m_widget->openUrl(url);
}

void KonqSidebarPart::customEvent(QEvent *ev)
{
    if (KonqFileSelectionEvent::test(ev) ||
            KonqFileMouseOverEvent::test(ev)) {
        // Forward the event to the widget
        QApplication::sendEvent(widget(), ev);
    }
}

////

KonqSidebarBrowserExtension::KonqSidebarBrowserExtension(KonqSidebarPart *part, Sidebar_Widget *widget_)
    : KParts::BrowserExtension(part), widget(widget_)
{
}

#include "sidebar_part.moc"
