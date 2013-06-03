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
#include <QTime>

#include <KActionCollection>
#include <KUrl>

namespace Plasma {
    class FrameSvg;
    class BusyWidget;
}

class QGraphicsView;
class QGraphicsScene;
class KDirModel;
class KFileItemDelegate;
class KFilePreviewGenerator;
class KNewFileMenu;
class KFileItemActions;
class QItemSelectionModel;
class QModelIndex;
class ProxyModel;
class IconView;

class PopupView : public QWidget
{
    Q_OBJECT

public:
    PopupView(const QModelIndex &index, const QPoint &pos,
              const bool &showPreview, const QStringList &previewPlugins,
              const IconView *parentView);
    ~PopupView();

    void delayedHide();
    bool dragInProgress();

    static QTime lastOpenCloseTime() { return s_lastOpenClose; }

protected:
    void showEvent(QShowEvent *event);
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
    void showContextMenu(QWidget *widget, const QPoint &pos, const QList<QModelIndex> &indexes);

private slots:
    void init();
    void activated(const QModelIndex &index);
    void openWithDialogAboutToShow();
    void setBusy(bool);
    void createBusyWidgetIfNeeded();
    void contextMenuRequest(QWidget *widget, const QPoint &screenPos);
    void maybeClose();
    void closeThisAndParentPopup();
    void hideThisAndParentPopup();
    void cancelHideTimer();
    void aboutToShowCreateNew();
    void emptyTrashBin();
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
    const ProxyModel *m_parentViewModel;
    KDirModel *m_dirModel;
    ProxyModel *m_model;
    KFileItemDelegate *m_delegate;
    QItemSelectionModel *m_selectionModel;
    KFilePreviewGenerator *m_previewGenerator;
    KUrl m_url;
    KActionCollection m_actionCollection;
    KNewFileMenu *m_newMenu;
    KFileItemActions *m_itemActions;
    QBasicTimer m_hideTimer;
    bool m_showingMenu;
    bool m_showPreview;
    bool m_busy;
    bool m_delayedClose;
    QStringList m_previewPlugins;
    static QTime s_lastOpenClose;
};

#endif

