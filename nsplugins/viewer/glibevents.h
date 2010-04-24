/*
  Copyright (c) 2007 Lubos Lunak <l.lunak@suse.cz>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifndef GLIBEVENTS_H
#define GLIBEVENTS_H

#include <config-nsplugins.h>

#ifdef HAVE_GLIB2

#include <QWidget>
#include <QTimer>

#include <glib.h>

class GlibEvents
    : public QWidget
    {
    Q_OBJECT
    public:
        GlibEvents();
        virtual ~GlibEvents();
    private slots:
        void process();
    private:
        QTimer timer;
        bool   checkedEventLoop;
    };
#endif

#endif
