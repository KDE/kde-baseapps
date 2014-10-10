/* vi: ts=8 sts=4 sw=4
 *
 * This file is part of the KDE project, module kdesu.
 * Copyright (C) 2000 Geert Jansen <jansen@kde.org>

 Permission to use, copy, modify, and distribute this software
 and its documentation for any purpose and without fee is hereby
 granted, provided that the above copyright notice appear in all
 copies and that both that the copyright notice and this
 permission notice and warranty disclaimer appear in supporting
 documentation, and that the name of the author not be used in
 advertising or publicity pertaining to distribution of the
 software without specific, written prior permission.

 The author disclaim all warranties with regard to this
 software, including all implied warranties of merchantability
 and fitness.  In no event shall the author be liable for any
 special, indirect or consequential damages or any damages
 whatsoever resulting from loss of use, data or profits, whether
 in an action of contract, negligence or other tortious action,
 arising out of or in connection with the use or performance of
 this software.

 */

#include <KLocalizedString>
#include <kaboutdata.h>

#include <kmessagebox.h>
#include <kuser.h>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <KDBusService>
#include <QApplication>

#include "passwd.h"
#include "passwddlg.h"
#include "kdepasswd_version.h"

int main(int argc, char **argv)
{
    KLocalizedString::setApplicationDomain("kdepasswd");

    KAboutData aboutData(QLatin1String("kdepasswd"), i18n("KDE passwd"),
            QLatin1String(KDEPASSWD_VERSION_STRING), i18n("Changes a UNIX password."),
            KAboutLicense::Artistic, i18n("Copyright (c) 2000 Geert Jansen"));
    aboutData.addAuthor(i18n("Geert Jansen"), i18n("Maintainer"), QLatin1String("jansen@kde.org"));

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon::fromTheme(QLatin1String("preferences-desktop-user-password")));
    QCommandLineParser parser;
    KAboutData::setApplicationData(aboutData);
    parser.addVersionOption();
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList() <<  QLatin1String("+[user]"), i18n("Change password of this user")));
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    KDBusService service(KDBusService::Unique);

    KUser ku;
    QString user;
    bool bRoot = ku.isSuperUser();

    if (parser.positionalArguments().count())
        user = parser.positionalArguments().at(0);

    /* You must be able to run "kdepasswd loginName" */
    if ( !user.isEmpty() && user!=KUser().loginName() && !bRoot)
    {
        KMessageBox::sorry(0, i18n("You need to be root to change the password of other users."));
        return 1;
    }

    QByteArray oldpass;
    if (!bRoot)
    {
        int result = KDEpasswd1Dialog::getPassword(oldpass);
        if (result != KDEpasswd1Dialog::Accepted)
            return 1;
    }

    KDEpasswd2Dialog *dlg = new KDEpasswd2Dialog(oldpass, user.toLocal8Bit());


    dlg->exec();
    if (dlg->result() == KDEpasswd2Dialog::Rejected)
        return 1;

    return 0;
}

