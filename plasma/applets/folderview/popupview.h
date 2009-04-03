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

#ifndef POPUPVIEW_H
#define POPUPVIEW_H

#include <QBasicTimer>
#include <QWidget>

#include <KActionCollection>
#include <KUrl>

namespace Plasma {
    class Applet;
    class FrameSvg;
    class BusyWidget;
}

class QGraphicsView;
class QGraphicsWidget;
class QGraphicsScene;
class KDirModel;
class KFileItemDelegate;
class KFilePlacesModel;
class KFilePreviewGenerator;
class KNewMenu;
class KFileItemActions;
class QItemSelectionModel;
class QModelIndex;
class ProxyModel;
class IconView;

class PopupView : public QWidget
{
    Q_OBJECT
 
public:
    PopupView(const KUrl &url, const QPoint &pos, const IconView *parentView);
    ~PopupView();

    void delayedHide();
    bool dragInProgress();

protected:
    void paintEvent(QPaintEvent *event);
    void contextMenuEvent(QContextMenuEvent *event);
    void resizeEvent(QResizeEvent *event);
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void timerEvent(QTimerEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);

signals:
    void requestClose();

private:
    void createActions();
    bool callOnParent(const char *method);
    KUrl::List selectedUrls() const;

private slots:
    void init();
    void activated(const QModelIndex &index);
    void setBusy(bool);
    void contextMenuRequest(QWidget *widget, const QPoint &screenPos);
    void maybeClose();
    void cancelHideTimer();
    void aboutToShowCreateNew();
    void undoTextChanged(const QString &text);

    // These slots are for KonqPopupMenu
    void cut();
    void copy();
    void paste();
    void pasteTo();
    void moveToTrash(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    void deleteSelectedIcons();
    void renameSelectedIcon();
 
private:
    Plasma::FrameSvg *m_background;
    QGraphicsScene *m_scene;
    QGraphicsView *m_view;
    const IconView *m_parentView;
    Plasma::BusyWidget *m_busyWidget;
    IconView *m_iconView;
    KDirModel *m_dirModel;
    ProxyModel *m_model;
    KFileItemDelegate *m_delegate;
    QItemSelectionModel *m_selectionModel;
    KFilePreviewGenerator *m_previewGenerator;
    KUrl m_url;
    KActionCollection m_actionCollection;
    KNewMenu *m_newMenu;
    KFileItemActions *m_itemActions;
    QBasicTimer m_hideTimer;
    bool m_showingMenu;
};

#endif

