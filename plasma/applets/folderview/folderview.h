/*
 *   Copyright © 2008, 2009 Fredrik Höglund <fredrik@kde.org>
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
#include <QStyleOption>
#include <QPointer>
#include <QBasicTimer>

#include <KActionCollection>
#include <KMimeType>

#include <Solid/Networking>

#include <plasma/containment.h>

#include "iconview.h"

#include "ui_folderviewFilterConfig.h"
#include "ui_folderviewDisplayConfig.h"
#include "ui_folderviewLocationConfig.h"
#include "ui_folderviewPreviewConfig.h"

#include <config-folderview.h>

class KDirModel;
class KFileItemDelegate;
class KFilePlacesModel;
class KFilePreviewGenerator;
class KNewMenu;
class KWindowListMenu;
class KFileItemActions;
class KJob;
class QItemSelectionModel;
class FolderView;
class ProxyModel;
class IconView;
class IconWidget;
class ListView;
class Label;
class Dialog;


/**
 * Helper class that downloads a wallpaper image asynchronously to a suitable
 * temporary directory in the user's home folder, and applies it to the given
 * folderview containment when the download finishes.
 *
 * The class deletes itself automatically when the operation is completed.
 */
class RemoteWallpaperSetter : public QObject
{
    Q_OBJECT

public:
    RemoteWallpaperSetter(const KUrl &url, FolderView *containment);

private slots:
    void result(KJob *job);
};


// ----------------------------------------------------------------------------



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
    void setWallpaper(const KUrl &url);

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
    void renameSelectedIcon();

    void undoTextChanged(const QString &text);
    void toggleIconsLocked(bool locked);
    void toggleAlignToGrid(bool align);
    void toggleDirectoriesFirst(bool enable);
    void sortingChanged(QAction *action);
    void aboutToShowCreateNew();
    void aboutToShowWindowList();
    void updateIconWidget();
    void iconWidgetClicked();

    void addPanel();
    void addPanel(const QString &plugin);
    void runCommand();
    void lockScreen();
    void logout();

    void activated(const QModelIndex &index);
    void indexesMoved(const QModelIndexList &indexes);
    void contextMenuRequest(QWidget *widget, const QPoint &screenPos);

    void configAccepted();
    void filterChanged(int index);
    void selectUnselectAll();
    void fontSettingsChanged();
    void iconSettingsChanged(int group);
    void themeChanged();
    void networkStatusChanged(Solid::Networking::Status status);
    void clipboardDataChanged();
    void updateScreenRegion();
    void showPreviewConfigDialog();
 
private:
    void addActions(AbstractItemView *view);
    void setupIconView();
    void setUrl(const KUrl &url);
    QSize iconSize() const;
    QSize gridSize() const;
    void createActions();
    void updateSortActionsState();
    void updateListViewState();
    void updateIconViewState();
    void saveIconPositions() const;
    KUrl::List selectedUrls(bool forTrash) const;
    void showContextMenu(QWidget *widget, const QPoint &pos, const QModelIndexList &indexes);
    void timerEvent(QTimerEvent *event);

private:
    KFileItemDelegate *m_delegate;
    QPointer<KFilePreviewGenerator> m_previewGenerator;
    QItemSelectionModel *m_selectionModel;
    ProxyModel *m_model;
    KDirModel *m_dirModel;
    KFilePlacesModel *m_placesModel;
    KFileItemActions *m_itemActions;
    IconView *m_iconView;
    ListView *m_listView;
    Label *m_label;
    IconWidget *m_iconWidget;
    Dialog *m_dialog;
    QIcon m_icon;
    KUrl m_url;
    QColor m_textColor;
    QString m_titleText;
    int m_filterType;
    QString m_filterFiles;
    QStringList m_filterFilesMimeList;
    QPointer<KNewMenu> m_newMenu;
#ifdef HAVE_KWORKSPACE
    KWindowListMenu *m_windowListMenu;
#endif
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
    bool m_userSelectedShowAllFiles;
    QString m_customLabel;
    QStringList m_previewPlugins;
    int m_customIconSize;
    int m_numTextLines;
    IconView::Flow m_flow;
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
