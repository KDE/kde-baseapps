/*
 *   Copyright © 2009 Fredrik Höglund <fredrik@kde.org>
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

#ifndef EDITOR_H
#define EDITOR_H

#include <QGraphicsProxyWidget>
#include <QAbstractItemDelegate>
#include <QModelIndex>
#include <KTextEdit>

class QStyleOptionViewItemV4;


class ItemEditor : public QGraphicsProxyWidget
{
    Q_OBJECT
    
public:
    ItemEditor(QGraphicsWidget *parent, const QStyleOptionViewItemV4 &option,
               const QModelIndex &index);
    ~ItemEditor();

    KTextEdit *nativeWidget() const { return m_editor; }

signals:
    void closeEditor(QGraphicsWidget *editor, QAbstractItemDelegate::EndEditHint hint = QAbstractItemDelegate::NoHint);

protected:
    void commitData();
    bool eventFilter(QObject *watched, QEvent *event);

private:
    KTextEdit *m_editor;
    QModelIndex m_index;
    bool m_uncommitted;
};

#endif

