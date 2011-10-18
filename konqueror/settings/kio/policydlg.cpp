/**
 * Copyright (c) 2000- Dawit Alemayehu <adawit@kde.org>
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

// Own
#include "policydlg.h"

// Qt
#include <QtGui/QPushButton>
#include <QtGui/QWhatsThis>
#include <QtGui/QLayout>
#include <QtGui/QLabel>
#include <QtGui/QValidator>

// KDE
#include <klineedit.h>
#include <kcombobox.h>
#include <klocale.h>

class DomainLineValidator : public QValidator
{
public:
  DomainLineValidator(QObject *parent)
  :QValidator(parent)
  {
    setObjectName( QLatin1String("domainValidator" ));
  }

  State validate(QString &input, int &) const
  {
    if (input.isEmpty() || (input == "."))
      return Intermediate;

    int length = input.length();

    for(int i = 0 ; i < length; i++)
    {
      if (!input[i].isLetterOrNumber() && input[i] != '.' && input[i] != '-')
        return Invalid;
    }

    return Acceptable;
  }
};


PolicyDlg::PolicyDlg (const QString& caption, QWidget *parent,
    const char *name)
    : KDialog( parent )
{
  setObjectName( name );
  setModal( true );
  setCaption( caption );
  setButtons( Ok|Cancel );

  m_dlgUI = new PolicyDlgUI (this);
  setMainWidget(m_dlgUI);

  m_dlgUI->leDomain->setValidator(new DomainLineValidator(m_dlgUI->leDomain));
  m_dlgUI->cbPolicy->setMinimumWidth( m_dlgUI->cbPolicy->fontMetrics().maxWidth() * 25 );
  
  enableButtonOk( false );
  connect(m_dlgUI->leDomain, SIGNAL(textChanged(const QString&)),
          SLOT(slotTextChanged(const QString&)));
  connect(m_dlgUI->cbPolicy,SIGNAL(currentIndexChanged(QString)),
          SLOT(slotTextChanged(QString)));
  setFixedSize (sizeHint());
  m_dlgUI->leDomain->setFocus ();
}

void PolicyDlg::setEnableHostEdit( bool state, const QString& host )
{
  if ( !host.isEmpty() )
    m_dlgUI->leDomain->setText( host );
  m_dlgUI->leDomain->setEnabled( state );
}

void PolicyDlg::setPolicy (int policy)
{
  if ( policy > -1 && policy <= static_cast<int>(m_dlgUI->cbPolicy->count()) ) {
    const bool blocked = m_dlgUI->cbPolicy->blockSignals(true);
    m_dlgUI->cbPolicy->setCurrentIndex(policy-1);
    m_dlgUI->cbPolicy->blockSignals(blocked);
  }

  if ( !m_dlgUI->leDomain->isEnabled() )
    m_dlgUI->cbPolicy->setFocus();
}

int PolicyDlg::advice () const
{
  return m_dlgUI->cbPolicy->currentIndex() + 1;
}

QString PolicyDlg::domain () const
{
  return m_dlgUI->leDomain->text();
}

void PolicyDlg::slotTextChanged( const QString& text )
{
  enableButtonOk( text.length() > 1 );
}
#include "policydlg.moc"
