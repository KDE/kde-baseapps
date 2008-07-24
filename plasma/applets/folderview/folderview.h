/*
 *   Copyright © 2008 Fredrik Höglund <fredrik@kde.org>
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

#ifndef FOLDERVIEW_H
#define FOLDERVIEW_H

#include <QPersistentModelIndex>
#include <QSortFilterProxyModel>
#include <QStyleOption>
#include <QPointer>

#include <KActionCollection>
#include <KMimeType>

#include <plasma/containment.h>
#include "ui_folderviewConfig.h"

class KDirModel;
class KFileItemDelegate;
class KNewMenu;
class QItemSelectionModel;
class ProxyModel;
class ScrollBar;

struct ViewItem
{
    QRect rect;
};

class FolderView : public Plasma::Containment
{
    Q_OBJECT

public:
    FolderView(QObject *parent, const QVariantList &args);
    ~FolderView();

    void init();
    void paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect &contentsRect);
    void setPath(const QString&);

protected:
    void createConfigurationInterface(KConfigDialog *parent);
    QList<QAction*> contextualActions();

    QPointF mapToViewport(const QPointF &point) const;
    QPointF mapFromViewport(const QPointF &point) const;
    QRectF mapToViewport(const QRectF &point) const;
    QRectF mapFromViewport(const QRectF &point) const;

private slots:
    void rowsInserted(const QModelIndex &parent, int first, int last);
    void rowsRemoved(const QModelIndex &parent, int first, int last);
    void modelReset();
    void layoutChanged();
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void configAccepted();
    void customFolderToggled(bool checked);
    void fontSettingsChanged();
    void iconSettingsChanged(int group);
    void themeChanged();
    void aboutToShowCreateNew();
    void clipboardDataChanged();
    void scrollBarValueChanged(int);

    // These slots are for KonqPopupMenu
    void copy();
    void cut();
    void paste();
    void pasteTo();
    void refreshIcons();
    void renameSelectedIcon();
    void moveToTrash(Qt::MouseButtons, Qt::KeyboardModifiers);
    void deleteSelectedIcons();
    void undoTextChanged(const QString &text);

    void commitData(QWidget *editor);
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint);
    
    void filterChanged(int index);
    void selectUnselectAll();

private:
    void createActions();
    KUrl::List selectedUrls() const;
    void showContextMenu(QWidget *widget, const QPoint &pos, const QModelIndexList &indexes);
    int columnsForWidth(qreal width) const;
    void layoutItems();
    void updateScrollBar();
    QRect scrollBackbufferContents();
    void markAreaDirty(const QRect &rect);
    void markAreaDirty(const QRectF &rect) { markAreaDirty(rect.toAlignedRect()); }
    void markEverythingDirty();
    void updateTextShadows(const QColor &textColor);
    QModelIndex indexAt(const QPointF &point);
    QRectF visualRect(const QModelIndex &index);
    QSize iconSize() const;
    QSize gridSize() const;
    QStyleOptionViewItemV4 viewOptions() const;
    void startDrag(const QPointF &pos, QWidget *widget);
    void constraintsEvent(Plasma::Constraints constraints);
    void focusInEvent(QFocusEvent *event);
    void focusOutEvent(QFocusEvent *event);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void wheelEvent(QGraphicsSceneWheelEvent *event);
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
    void dropEvent(QGraphicsSceneDragDropEvent *event);

private:
    KFileItemDelegate *m_delegate;
    KDirModel *m_dirModel;
    ProxyModel *m_model;
    ScrollBar *m_scrollBar;
    QPixmap m_pixmap;
    QPixmap m_topFadeTile;
    QPixmap m_bottomFadeTile;
    QRegion m_dirtyRegion;
    QItemSelectionModel *m_selectionModel;
    KUrl m_url;
    int m_titleHeight;
    int m_lastScrollValue;
    bool m_viewScrolled;
    int m_filterType;
    QString m_filterFiles;
    QStringList m_filterFilesMimeList;
    QFont m_font;
    QPointer<KNewMenu> m_newMenu;
    KActionCollection m_actionCollection;
    QVector<ViewItem> m_items;
    int m_columns;
    bool m_layoutValid;
    bool m_layoutBroken;
    QPersistentModelIndex m_hoveredIndex;
    QPersistentModelIndex m_pressedIndex;
    QPersistentModelIndex m_editorIndex;
    QRect m_rubberBand;
    QRectF m_viewportRect;
    QPointF m_buttonDownPos;
    QTime m_pressTime;
    Ui::folderviewConfig ui;
    bool m_doubleClick;
    bool m_dragInProgress;
};



// ---------------------------------------------------------------------------



class MimeModel : public QStringListModel
{
public:
    MimeModel(QObject *parent = 0);
    
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    
private:
    KMimeType::List m_mimetypes;
};



// ---------------------------------------------------------------------------



class ProxyMimeModel : public QSortFilterProxyModel
{
Q_OBJECT

public:
    ProxyMimeModel(QObject *parent = 0);
    
    virtual void setSourceModel(QAbstractItemModel *sourceModel);
    
public slots:
    void setFilter(const QString &filter);
    
protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
    
private:
    QString m_filter;
};

K_EXPORT_PLASMA_APPLET(folderview, FolderView)

#endif
