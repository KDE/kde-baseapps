/*
 *  Copyright (c) 2001 David Faure <david@mandrakesoft.com>
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

#ifndef BEHAVIOUR_H
#define BEHAVIOUR_H

#include <kcmodule.h>
#include <kconfig.h>
#include <ksharedconfig.h>
#include <QtCore/QStringList>

class QCheckBox;
class QLabel;
class QSpinBox;

class KUrlRequester;

class KBehaviourOptions : public KCModule
{
  Q_OBJECT
public:
  explicit KBehaviourOptions(QWidget *parent, const QVariantList &args = QVariantList());
    ~KBehaviourOptions();
  virtual void load();
  virtual void save();
  virtual void defaults();

protected Q_SLOTS:
  void updateWinPixmap(bool);

private:
  KSharedConfig::Ptr g_pConfig;
  QString groupname;

  QCheckBox *cbNewWin;

  QLabel *winPixmap;

  QCheckBox *cbMoveToTrash;
  QCheckBox *cbDelete;
  QCheckBox *cbShowDeleteCommand;
};

#endif // BEHAVIOUR_H
