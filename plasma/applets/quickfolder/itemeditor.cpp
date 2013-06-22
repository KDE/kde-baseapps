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

#include "itemeditor.h"

#include <KTextEdit>
#include <QAbstractItemModel>
#include <QAbstractItemDelegate>

#include <KMimeType>


ItemEditor::ItemEditor(QGraphicsWidget *parent, const QStyleOptionViewItemV4 &option,
                       const QModelIndex &index)
    : QGraphicsProxyWidget(parent),
      m_index(index),
      m_uncommitted(true)
{
    // Create the editor
    m_editor = new KTextEdit();
    m_editor->setAttribute(Qt::WA_NoSystemBackground);
    m_editor->setAcceptRichText(false);
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setAlignment(option.displayAlignment);
    m_editor->installEventFilter(this);

    // Set the editor data
    const QVariant value = index.data(Qt::EditRole);
    const QString text = value.toString();
    m_editor->insertPlainText(text);
    m_editor->selectAll();

    const QString extension = KMimeType::extractKnownExtension(text);
    if (!extension.isEmpty()) {
        // The filename contains an extension. Assure that only the filename
        // gets selected.
        const int selectionLength = text.length() - extension.length() - 1;
        QTextCursor cursor = m_editor->textCursor();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, selectionLength);
        m_editor->setTextCursor(cursor);
    }

    setWidget(m_editor);
}

ItemEditor::~ItemEditor()
{
}

void ItemEditor::commitData()
{
    if (m_uncommitted) {
        const_cast<QAbstractItemModel*>(m_index.model())->setData(m_index, m_editor->toPlainText(), Qt::EditRole);
        m_uncommitted = false;
    }
}

bool ItemEditor::eventFilter(QObject *watched, QEvent *event)
{
    KTextEdit *editor = qobject_cast<KTextEdit*>(watched);
    if (!editor) {
        return false;
    }

    switch (event->type())
    {
    case QEvent::KeyPress:
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            commitData();
            emit closeEditor(this, QAbstractItemDelegate::NoHint);
            return true;

        case Qt::Key_Enter:
        case Qt::Key_Return:
            if (!editor->toPlainText().isEmpty()) {
                commitData();
                emit closeEditor(this, QAbstractItemDelegate::SubmitModelCache);
            }
            return true;

        case Qt::Key_Escape:
            emit closeEditor(this, QAbstractItemDelegate::RevertModelCache);
            return true;

        default:
            return false;
        } // switch (keyEvent->key())
    } // case QEvent::KeyPress

    case QEvent::FocusOut:
    {
        if (m_uncommitted) {
            commitData();
            emit closeEditor(this, QAbstractItemDelegate::NoHint);
        }
        return true;
    }

    default:
        return false;
    } // switch (event->type())
}

#include "itemeditor.moc"

