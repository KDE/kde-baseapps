/**
 * socks.h
 *
 * Copyright (c) 2001 George Staikos <staikos@kde.org>
 * Copyright (c) 2001 Daniel Molkentin <molkentin@kde.org> (designer port)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _SOCKS_H
#define _SOCKS_H

#include <config-apps.h>

#include <kcmodule.h>

#include "ui_socksbase.h"



class SocksBase : public QWidget, public Ui::SocksBase
{
public:
  SocksBase( QWidget *parent ) : QWidget( parent ) {
    setupUi( this );
  }
};


class KSocksConfig : public KCModule
{
  Q_OBJECT
public:
  KSocksConfig(const KComponentData &componentData, QWidget *parent);
  virtual ~KSocksConfig();

  SocksBase *base;

  void load();
  void save();
  void defaults();

  int buttons();
  QString quickHelp() const;

public Q_SLOTS:
  void configChanged();

private Q_SLOTS:
  void enableChanged();
  void methodChanged(int id);
  void testClicked();
  void chooseCustomLib(KUrlRequester *url);
  void customPathChanged(const QString&);
  void addLibrary();
  void libTextChanged(const QString& lib);
  void addThisLibrary(const QString& lib);
  void removeLibrary();
  void libSelection();

private:
  void setCustomPathEnabled(int id);

  bool _socksEnabled;
  int _useWhat;
};

#endif
