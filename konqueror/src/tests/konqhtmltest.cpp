/* This file is part of the KDE project
   Copyright (C) 2008 David Faure <faure@kde.org>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <konqmisc.h>
#include <khtml_part.h>
#include <ktemporaryfile.h>
#include <kstandarddirs.h>
#include <ktoolbar.h>
#include <kdebug.h>
#include <QScrollArea>
#include <qtest_kde.h>
#include <qtest_gui.h>

#include <konqmainwindow.h>
#include <konqviewmanager.h>
#include <konqview.h>

#include <QObject>

class KonqHtmlTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        //qRegisterMetaType<KonqView *>("KonqView*");
    }
    void cleanupTestCase()
    {
        // in case some test broke, don't assert in khtmlglobal...
        deleteAllMainWindows();
    }
    void loadSimpleHtml()
    {
        KonqMainWindow mainWindow;
        // we specify the mimetype so that we don't have to wait for a KonqRun
        mainWindow.openUrl(0, KUrl("data:text/html, <p>Hello World</p>"), "text/html");
        KonqView* view = mainWindow.currentView();
        QVERIFY(view);
        QVERIFY(view->part());
        QVERIFY(QTest::kWaitForSignal(view, SIGNAL(viewCompleted(KonqView*)), 20000));
        QCOMPARE(view->serviceType(), QString("text/html"));
        //KHTMLPart* part = qobject_cast<KHTMLPart *>(view->part());
        //QVERIFY(part);
    }

    void loadDirectory() // #164495
    {
        KonqMainWindow mainWindow;
        mainWindow.openUrl(0, KUrl(QDir::homePath()), "text/html");
        KonqView* view = mainWindow.currentView();
        kDebug() << "Waiting for first completed signal";
        QVERIFY(QTest::kWaitForSignal(view, SIGNAL(viewCompleted(KonqView*)), 20000)); // error calls openUrlRequest
        kDebug() << "Waiting for first second signal";
        QVERIFY(QTest::kWaitForSignal(view, SIGNAL(viewCompleted(KonqView*)), 20000)); // which then opens the right part
        QCOMPARE(view->serviceType(), QString("inode/directory"));
    }

    void rightClickClose() // #149736
    {
        QPointer<KonqMainWindow> mainWindow = new KonqMainWindow;
        // we specify the mimetype so that we don't have to wait for a KonqRun
        mainWindow->openUrl(0, KUrl(
                "data:text/html, <script type=\"text/javascript\">"
                "function closeMe() { window.close(); } "
                "document.onmousedown = closeMe; "
                "</script>"), QString("text/html"));
        QPointer<KonqView> view = mainWindow->currentView();
        QVERIFY(view);
        QVERIFY(QTest::kWaitForSignal(view, SIGNAL(viewCompleted(KonqView*)), 10000));
        QWidget* widget = partWidget(view);
        qDebug() << "Clicking on" << widget;
        QTest::mousePress(widget, Qt::RightButton);
        qApp->processEvents();
        QVERIFY(!view); // deleted
        QVERIFY(!mainWindow); // the whole window gets deleted, in fact
    }

    void windowOpen()
    {
        // We have to use the same protocol for both the orig and dest urls.
        // KAuthorized would forbid a data: URL to redirect to a file: URL for instance.
        KTemporaryFile tempFile;
        QVERIFY(tempFile.open());
        tempFile.write("<script>document.write(\"Opener=\" + window.opener);</script>");

        KTemporaryFile origTempFile;
        QVERIFY(origTempFile.open());
        origTempFile.write(
            "<html><script>"
            "function openWindow() { window.open('" + KUrl(tempFile.fileName()).url().toUtf8() + "'); } "
            "document.onmousedown = openWindow; "
            "</script>"
            );
        tempFile.close();
        const QString origFile = origTempFile.fileName();
        origTempFile.close();

        KonqMainWindow* mainWindow = new KonqMainWindow;
        const QString profile = KStandardDirs::locate("data", "konqueror/profiles/webbrowsing");
        mainWindow->viewManager()->loadViewProfileFromFile(profile, "webbrowsing", KUrl(origFile));
        QCOMPARE(KMainWindow::memberList().count(), 1);
        KonqView* view = mainWindow->currentView();
        QVERIFY(view);
        QVERIFY(QTest::kWaitForSignal(view, SIGNAL(viewCompleted(KonqView*)), 10000));
        qApp->processEvents();
        QWidget* widget = partWidget(view);
        kDebug() << "Clicking on the khtmlview";
        QTest::mousePress(widget, Qt::LeftButton);
        qApp->processEvents(); // openurlrequestdelayed
        qApp->processEvents(); // browserrun
        hideAllMainWindows(); // TODO: why does it appear nonetheless? hiding too early? hiding too late?
        QTest::qWait(10); // just in case there's more delayed calls :)
        // Did it open a window?
        QCOMPARE(KMainWindow::memberList().count(), 2);
        KMainWindow* newWindow = KMainWindow::memberList().last();
        QVERIFY(newWindow != mainWindow);
        compareToolbarSettings(mainWindow, newWindow);
        // Does the window contain exactly one tab?
        QTabWidget* tab = newWindow->findChild<QTabWidget*>();
        QVERIFY(tab);
        QCOMPARE(tab->count(), 1);
        KonqFrame* frame = qobject_cast<KonqFrame *>(tab->widget(0));
        QVERIFY(frame);
        KHTMLPart* part = qobject_cast<KHTMLPart *>(frame->part());
        QVERIFY(part);
        part->selectAll();
        const QString text = part->selectedText();
        QCOMPARE(text, QString("Opener=[object Window]"));
        deleteAllMainWindows();
    }

private:
    // Return the main widget for the given KonqView; used for clicking onto it
    static QWidget* partWidget(KonqView* view)
    {
        QWidget* widget = view->part()->widget();
        if (QScrollArea* scrollArea = qobject_cast<QScrollArea*>(widget))
            widget = scrollArea->widget();
        return widget;
    }

    // Delete all KonqMainWindows
    static void deleteAllMainWindows()
    {
        const QList<KMainWindow*> windows = KMainWindow::memberList();
        qDeleteAll(windows);
    }

    static void hideAllMainWindows()
    {
        const QList<KMainWindow*> windows = KMainWindow::memberList();
        kDebug() << "hiding" << windows.count() << "windows";
        Q_FOREACH(KMainWindow* window, windows)
            window->hide();
    }

    void compareToolbarSettings(KMainWindow* mainWindow, KMainWindow* newWindow)
    {
        QVERIFY(mainWindow != newWindow);
        KToolBar* firstToolBar = mainWindow->toolBars().first();
        QVERIFY(firstToolBar);
        KToolBar* newFirstToolBar = newWindow->toolBars().first();
        QVERIFY(newFirstToolBar);
        QCOMPARE(firstToolBar->toolButtonStyle(), newFirstToolBar->toolButtonStyle());
    }
};


QTEST_KDEMAIN_WITH_COMPONENTNAME( KonqHtmlTest, GUI, "konqueror" )

#include "konqhtmltest.moc"
