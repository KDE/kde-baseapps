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

#ifndef ANIMATOR_H
#define ANIMATOR_H

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QModelIndex>

#include <kglobalsettings.h>

class AbstractItemView;

class HoverAnimation : public QAbstractAnimation
{
public:
    HoverAnimation(AbstractItemView *view, const QModelIndex &index);
    
    QModelIndex index() const { return m_index; }
    qreal progress() const;

    void setEasingCurve(QEasingCurve::Type type);
    int duration() const { return 250; }

protected:
    void updateCurrentTime(int currentTime);

private:
    QModelIndex m_index;
    QEasingCurve m_curve;
    qreal m_progress;
};


// -----------------------------------------------------------------------


class Animator : public QObject
{
    Q_OBJECT
    
public:
    enum Type { HoverEnter, HoverLeave };

    Animator(AbstractItemView *view);
    ~Animator();

    HoverAnimation *findHoverAnimation(const QModelIndex &index) const;
    void animate(Type type, const QModelIndex &index);
    qreal hoverProgress(const QModelIndex &index) const;

private slots:
    void entered(const QModelIndex &index);
    void left(const QModelIndex &index);
    void animationDestroyed(QObject *object);
    void graphicsEffectsToggled(int category);

private:
    QList<HoverAnimation*> m_list;
    QPersistentModelIndex m_hoveredIndex;
    bool m_effectsOn;
};

#endif

