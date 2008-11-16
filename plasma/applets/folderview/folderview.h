/*
 *   Copyright © 2008 Fredrik Höglund <fredrik@kde.org>
 *   Copyright © 2008 Rafael Fernández López <ereslibre@kde.org>
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

#include <QCache>
#include <QPersistentModelIndex>
#include <QSortFilterProxyModel>
#include <QListView>
#include <QStyleOption>
#include <QPointer>
#include <QBasicTimer>

#include <KActionCollection>
#include <KMimeType>

#include <plasma/containment.h>
#include "ui_folderviewFilterConfig.h"
#include "ui_folderviewDisplayConfig.h"
#include "ui_folderviewLocationConfig.h"
#include "ui_folderviewPreviewConfig.h"

class KDirModel;
class KFileItemDelegate;
class KFilePreviewGenerator;
class KNewMenu;
class QItemSelectionModel;
class ProxyModel;
class IconView;
class Label;


class FolderView : public Plasma::Containment
{
    Q_OBJECT

public:
    FolderView(QObject *parent, const QVariantList &args);
    ~FolderView();

    void init();
    void saveState(KConfigGroup &config) const;
    void paintInterface(QPainter *painter, const QStyleOptionGraphicsItem *option, const QRect &contentsRect);
    void setPath(const QString&);

protected:
    void createConfigurationInterface(KConfigDialog *parent);
    QList<QAction*> contextualActions();
    void constraintsEvent(Plasma::Constraints);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
    void dropEvent(QGraphicsSceneDragDropEvent *event);

protected slots:
    // These slots are for KonqPopupMenu
    void copy();
    void cut();
    void paste();
    void pasteTo();
    void refreshIcons();
    void moveToTrash(Qt::MouseButtons, Qt::KeyboardModifiers);
    void deleteSelectedIcons();

    void undoTextChanged(const QString &text);
    void toggleIconsLocked(bool locked);
    void toggleAlignToGrid(bool align);
    void toggleDirectoriesFirst(bool enable);
    void sortingChanged(QAction *action);
    void aboutToShowCreateNew();

    void activated(const QModelIndex &index);
    void indexesMoved(const QModelIndexList &indexes);
    void contextMenuRequest(QWidget *widget, const QPoint &screenPos);

    void configAccepted();
    void filterChanged(int index);
    void selectUnselectAll();
    void fontSettingsChanged();
    void iconSettingsChanged(int group);
    void themeChanged();
    void clipboardDataChanged();
    void updateScreenRegion();
    void showPreviewConfigDialog();

private:
    void setupIconView();
    void setUrl(const KUrl &url);
    QSize iconSize() const;
    QSize gridSize() const;
    void createActions();
    void updateSortActionsState();
    void updateIconViewState();
    void saveIconPositions() const;
    KUrl::List selectedUrls() const;
    void showContextMenu(QWidget *widget, const QPoint &pos, const QModelIndexList &indexes);
    void timerEvent(QTimerEvent *event);

private:
    KFileItemDelegate *m_delegate;
    KFilePreviewGenerator *m_previewGenerator;
    QItemSelectionModel *m_selectionModel;
    ProxyModel *m_model;
    KDirModel *m_dirModel;
    IconView *m_iconView;
    Label *m_label;
    KUrl m_url;
    QColor m_textColor;
    QString m_titleText;
    int m_filterType;
    QString m_filterFiles;
    QStringList m_filterFilesMimeList;
    QPointer<KNewMenu> m_newMenu;
    KActionCollection m_actionCollection;
    QActionGroup *m_sortingGroup;
    int m_sortColumn;
    Ui::folderviewFilterConfig uiFilter;
    Ui::folderviewDisplayConfig uiDisplay;
    Ui::folderviewLocationConfig uiLocation;
    Ui::folderviewPreviewConfig uiPreviewConfig;
    bool m_sortDirsFirst;
    bool m_showPreviews;
    bool m_drawShadows;
    bool m_iconsLocked;
    bool m_alignToGrid;
    QString m_customLabel;
    QStringList m_previewPlugins;
    int m_customIconSize;
    int m_numTextLines;
    QListView::Flow m_flow;
    QBasicTimer m_delayedSaveTimer;
};



// ---------------------------------------------------------------------------



class MimeModel : public QStringListModel
{
public:
    MimeModel(QObject *parent = 0);

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;
    virtual QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

private:
    KMimeType::List m_mimetypes;
    QMap<KMimeType*, Qt::CheckState> m_state;
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

#endif
