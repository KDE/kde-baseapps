/*
 *   Copyright © 2009, 2010 Fredrik Höglund <fredrik@kde.org>
 *   Copyright © 2009 Bruno Bigras <bigras.bruno@gmail.com>
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

#include "actionoverlay.h"
#include "asyncfiletester.h"
#include "iconview.h"

#include <Plasma/PaintUtils>
#include <Plasma/Animator>
#include <Plasma/Svg>

#include <QPainter>
#include <QGraphicsGridLayout>

ActionIcon::ActionIcon(QGraphicsItem* parent)
    : QGraphicsWidget(parent),
      m_pressed(false),
      m_sunken(false)
{
    setAcceptHoverEvents(true);
    setCacheMode(DeviceCoordinateCache);

    m_icon = new Plasma::Svg(this);
    m_icon->setImagePath("widgets/action-overlays");
    m_icon->setContainsMultipleImages(true);

    setMinimumSize(m_icon->elementSize("add-normal"));
    setMaximumSize(minimumSize());

    show();
}

void ActionIcon::setElement(const QString &element)
{
    m_element = element;
}

void ActionIcon::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    QString element = m_element;

    if (m_sunken) {
        element += "-pressed";
    } else if (isUnderMouse()) {
        element += "-hover";
    } else {
        element += "-normal";
    }

    m_icon->paint(painter, rect(), element);
}

void ActionIcon::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)

    m_pressed = true;
    m_sunken = true;
    update();
}

void ActionIcon::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)

    m_pressed = false;
    m_sunken = false;
    if (isUnderMouse()) {
        emit clicked();
    }
    update();
}

void ActionIcon::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)

    if (m_sunken != isUnderMouse()) {
        m_sunken = isUnderMouse();
        update();
    }
}

void ActionIcon::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    emit iconHoverEnter();
    update();
}

void ActionIcon::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)

    emit iconHoverLeave();
    update();
}


// --------------------------------------------------------


ActionOverlay::ActionOverlay(AbstractItemView* parent)
        : QGraphicsWidget(parent)
{
    m_toggleButton = new ActionIcon(this);
    m_openButton = new ActionIcon(this);
    m_openButton->setElement("open");

    m_showFolderButton = true;
    m_showSelectionButton = true;

    m_layout = new QGraphicsGridLayout(this);
    m_layout->setContentsMargins(4, 4, 4, 4);
    m_layout->setSpacing(1);
    m_layout->addItem(m_toggleButton, 0, 0);
    m_layout->addItem(m_openButton, 1, 0);

    connect(parentWidget(), SIGNAL(entered(QModelIndex)), this, SLOT(entered(QModelIndex)));
    connect(parentWidget(), SIGNAL(left(QModelIndex)), this, SLOT(left(QModelIndex)));
    connect(parentWidget(), SIGNAL(modelChanged()), this, SLOT(modelChanged()));

    connect(m_toggleButton, SIGNAL(clicked()), this, SLOT(toggleSelection()));
    connect(m_openButton, SIGNAL(clicked()), this, SLOT(openPopup()));

    m_hideActionOverlayIconTimer = new QTimer(this);
    connect(m_hideActionOverlayIconTimer, SIGNAL(timeout()), this, SLOT(timeout()));

    connect(m_toggleButton, SIGNAL(iconHoverEnter()), m_hideActionOverlayIconTimer, SLOT(stop()));
    connect(m_toggleButton, SIGNAL(iconHoverLeave()), m_hideActionOverlayIconTimer, SLOT(start()));
    connect(m_openButton,   SIGNAL(iconHoverEnter()), m_hideActionOverlayIconTimer, SLOT(stop()));
    connect(m_openButton,   SIGNAL(iconHoverLeave()), m_hideActionOverlayIconTimer, SLOT(start()));

    connect(parent->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(close()));

    m_hideActionOverlayIconTimer->setInterval(500);
    m_hideActionOverlayIconTimer->setSingleShot(true);

    fadeIn = Plasma::Animator::create(Plasma::Animator::FadeAnimation, this);
    fadeIn->setProperty("startOpacity", 0);
    fadeIn->setProperty("targetOpacity", 1);
    fadeIn->setTargetWidget(this);

    fadeOut = Plasma::Animator::create(Plasma::Animator::FadeAnimation, this);
    fadeOut->setProperty("startOpacity", 1);
    fadeOut->setProperty("targetOpacity", 0);
    fadeOut->setTargetWidget(this);
    connect(fadeOut, SIGNAL(finished()), SLOT(close()));

    hide();
}

void ActionOverlay::toggleSelection()
{
    AbstractItemView *view = static_cast<AbstractItemView*>(parentWidget());
    QItemSelectionModel *m_selectionModel = view->selectionModel();

    if (m_hoverIndex.isValid()) {
        const QModelIndex oldCurrent = m_selectionModel->currentIndex();
        m_selectionModel->select(m_hoverIndex, QItemSelectionModel::Toggle);
        m_selectionModel->setCurrentIndex(m_hoverIndex, QItemSelectionModel::NoUpdate);
        m_toggleButton->setElement(m_selectionModel->isSelected(m_hoverIndex) ? "remove" : "add");
        view->markAreaDirty(view->visualRect(m_hoverIndex));
        if (oldCurrent.isValid() && oldCurrent != m_hoverIndex) {
            view->markAreaDirty(view->visualRect(oldCurrent));
        }
    }
}

void ActionOverlay::openPopup()
{
    if (IconView *view = qobject_cast<IconView*>(parentWidget())) {
        view->openPopup(m_hoverIndex);
    }
}

QPersistentModelIndex ActionOverlay::hoverIndex()
{
    return m_hoverIndex;
}

void ActionOverlay::entered(const QModelIndex &index)
{
    m_hideActionOverlayIconTimer->stop();

    if (index.isValid()) {
        AbstractItemView *view = static_cast<AbstractItemView*>(parentWidget());
        QItemSelectionModel *m_selectionModel = view->selectionModel();
        m_toggleButton->setElement(m_selectionModel->isSelected(index) ? "remove" : "add");
        setPos(view->mapFromViewport(view->visualRect(index)).topLeft());
        show();
        if (m_hoverIndex != index) {
            m_toggleButton->update();
            fadeOut->stop();
            fadeIn->start();
        }
        m_hoverIndex = index;
        IconView *iview = qobject_cast<IconView*>(view);
        if (iview && iview->clickToViewFolders()) {
            AsyncFileTester::checkIfFolder(index, this, "checkIfFolderResult");
        }
    }
}

void ActionOverlay::checkIfFolderResult(const QModelIndex &index, bool isFolder)
{
    if (index == m_hoverIndex) {
        m_openButton->setVisible(isFolder);
    }
}

void ActionOverlay::left(const QModelIndex &index)
{
    Q_UNUSED(index);

    if (!m_hideActionOverlayIconTimer->isActive()) {
        m_hideActionOverlayIconTimer->start();
    }
}

void ActionOverlay::timeout()
{
    // allow the animation to restart after hiding the ActionOverlayIcon even if m_hoverIndex didn't change
    m_hoverIndex = QPersistentModelIndex();

    if (isVisible() && (fadeOut->state() != QAbstractAnimation::Running)) {
        fadeIn->stop();
        fadeOut->start();
    }
}

void ActionOverlay::forceHide(HideHint hint)
{
    m_hideActionOverlayIconTimer->stop();
    if (hint == FadeOut) {
        timeout();
    } else {
        hide();
    }
}

void ActionOverlay::rowsRemoved(const QModelIndex & parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);

    if (!m_hoverIndex.isValid()) {
        hide();
    }
}

void ActionOverlay::modelChanged()
{
    AbstractItemView *view = static_cast<AbstractItemView*>(parentWidget());

    QAbstractItemModel *mod = view->model();
    connect(mod, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(rowsRemoved(QModelIndex,int,int)));
}

void ActionOverlay::setShowFolderButton(bool show)
{
  if (m_showFolderButton != show) {
    m_showFolderButton = show;
    toggleShowActionButton(show, m_openButton, 1);
  }
}

void ActionOverlay::setShowSelectionButton(bool show)
{
  if (m_showSelectionButton!= show) {
    m_showSelectionButton = show;
    toggleShowActionButton(show, m_toggleButton, 0);
  }
}

bool ActionOverlay::showFolderButton() const
{
  return m_showFolderButton;
}
bool ActionOverlay::showSelectionButton() const
{
  return m_showSelectionButton;
}

QSizeF ActionOverlay::iconSize() const
{
    return m_openButton->geometry().size();
}

void ActionOverlay::toggleShowActionButton(bool show, ActionIcon* button, unsigned int pos)
{
  if (show && m_layout->itemAt(pos, 0) != button) {
    m_layout->addItem(button, pos, 0);
    button->show();
  } else if (m_layout->itemAt(pos, 0) == button) {
    button->hide();
#if QT_VERSION >= 0x040800
    m_layout->removeItem(button);
#else
    /* find the index of the item: yeah, this is ugly... on the other hand, this works in Qt 4.7,
     * and if you have a look at the 4.8 source, this is exactly what it does. */
    int index = -1;
    for (int i = 0; i < m_layout->count(); ++i) {
      if (m_layout->itemAt(i) == button) {
        index = i;
        break;
      }
    }
    Q_ASSERT(index >= 0); // the button *is* part of the layout, so we have to find it
    m_layout->removeAt(index);
#endif
  }
}
