/*******************************************************************
* kdatecombo.h
* 
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of 
* the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
* 
******************************************************************/

#ifndef KDATECOMBO_H
#define KDATECOMBO_H

#include <QWidget>
#include <QDate>

#include <kcombobox.h>

/**
  *@author Beppe Grimaldi
  */

class KDatePicker;
class QFrame;

class KDateCombo : public KComboBox  {
   Q_OBJECT

public:
	KDateCombo(QWidget *parent=0);
	explicit KDateCombo(const QDate & date, QWidget *parent=0);
	~KDateCombo();

	QDate & getDate(QDate *currentDate);
	bool setDate(const QDate & newDate);

private:
   QFrame * popupFrame;
   KDatePicker * datePicker;

   void initObject(const QDate & date);

   QString date2String(const QDate &);
   QDate & string2Date(const QString &, QDate * );

protected:
  bool eventFilter (QObject*, QEvent*) Q_DECL_OVERRIDE;
  void mousePressEvent (QMouseEvent * e) Q_DECL_OVERRIDE;

protected Q_SLOTS:
   void dateEnteredEvent(const QDate &d=QDate());
};

#endif
