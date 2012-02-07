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

#include "animator.h"
#include "abstractitemview.h"


HoverAnimation::HoverAnimation(AbstractItemView *view, const QModelIndex &index)
    : QAbstractAnimation(view), m_index(index), m_curve(QEasingCurve::InQuad)
{
}

void HoverAnimation::setEasingCurve(QEasingCurve::Type type)
{
    m_curve.setType(type);
}

qreal HoverAnimation::progress() const
{
    return m_curve.valueForProgress(m_progress);
}

void HoverAnimation::updateCurrentTime(int currentTime)
{
    m_progress = qreal(currentTime) / duration();

    AbstractItemView *view = static_cast<AbstractItemView*>(parent());
    view->markAreaDirty(view->visualRect(m_index));
}



// --------------------------------------------------------


    
Animator::Animator(AbstractItemView *view)
    : QObject(view)
{
    m_effectsOn = (KGlobalSettings::graphicEffectsLevel() >= KGlobalSettings::SimpleAnimationEffects); //do not animate if the graphics effects are set to Low CPU
    connect(KGlobalSettings::self(), SIGNAL(settingsChanged(int)), SLOT(graphicsEffectsToggled(int)));
    connect(view, SIGNAL(entered(QModelIndex)), SLOT(entered(QModelIndex)));
    connect(view, SIGNAL(left(QModelIndex)), SLOT(left(QModelIndex)));
}

Animator::~Animator()
{
}

HoverAnimation *Animator::findHoverAnimation(const QModelIndex &index) const
{
    foreach (HoverAnimation *animation, m_list) {
        if (animation->index() == index) {
            return animation;
        }
    }

    return 0;
}

void Animator::animate(Type type, const QModelIndex &index)
{
    AbstractItemView *view = static_cast<AbstractItemView*>(parent());
    HoverAnimation *animation = findHoverAnimation(index);

    if (!animation) {
        animation = new HoverAnimation(view, index);
        connect(animation, SIGNAL(destroyed(QObject*)), SLOT(animationDestroyed(QObject*)));
        m_list.append(animation);
    }

    if (type == HoverEnter) {
        animation->setDirection(QAbstractAnimation::Forward);
    } else {
        animation->setDirection(QAbstractAnimation::Backward);
    }

    if (animation->state() != QAbstractAnimation::Running) {
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

qreal Animator::hoverProgress(const QModelIndex &index) const
{
    if (HoverAnimation *animation = findHoverAnimation(index)) {
        return animation->progress();
    }

    return (index == m_hoveredIndex) ? 1.0 : 0.0;
}

void Animator::animationDestroyed(QObject *obj)
{
    m_list.removeOne(static_cast<HoverAnimation*>(obj));
}

void Animator::entered(const QModelIndex &index)
{
    m_hoveredIndex = index;
    if (m_effectsOn) {
        animate(HoverEnter, index);
    }
}

void Animator::left(const QModelIndex &index)
{
    m_hoveredIndex = QModelIndex();
    if (m_effectsOn) {
        animate(HoverLeave, index);
    }
}

void Animator::graphicsEffectsToggled(int category)
{
    if ( (category == KGlobalSettings::SETTINGS_STYLE) && (KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects) ) {
        m_effectsOn = true;
    } else {
        m_effectsOn = false;
    }
}

#include "animator.moc"

