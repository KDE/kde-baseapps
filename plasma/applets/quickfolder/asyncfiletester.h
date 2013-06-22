/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
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

#ifndef ASYNCFILETESTER_H
#define ASYNCFILETESTER_H


#include <QObject>
#include <QModelIndex>
#include <QWeakPointer>

class KJob;
class KUrl;


class AsyncFileTester : public QObject
{
    Q_OBJECT
    
public:
    /* Checks if the file the index refers to is a folder and calls the given member in
     * the object with the result.
     *
     * The index must belong to a ProxyModel, and the member function must be a slot
     * with the following signature:
     *
     * checkIfFolderResult(const QModelIndex &index, bool result)
     */ 
    static void checkIfFolder(const QModelIndex &index, QObject *object, const char *method);

private:
    AsyncFileTester(const QModelIndex &index, QObject *object, const char *member);
    void delayedFolderCheck(const KUrl &url);
    static void callResultMethod(QObject *object, const char *member, const QModelIndex &index, bool result);
 
private slots:
    void statResult(KJob *job);

private:
    QModelIndex index;
    QWeakPointer<QObject> object;
    const char *member;
};

#endif

