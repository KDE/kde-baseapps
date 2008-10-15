/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KONQRUN_H
#define KONQRUN_H

#include <kparts/browserrun.h>
#include <QtCore/QPointer>
#include <kservice.h>
#include "konqopenurlrequest.h"

class KonqMainWindow;
class KonqView;

class KonqRun : public KParts::BrowserRun
{
  Q_OBJECT
public:
  /**
   * Create a KonqRun instance, associated to the main view and an
   * optional child view.
   */
  KonqRun( KonqMainWindow* mainWindow, KonqView *childView,
           const KUrl &url, const KonqOpenURLRequest & req = KonqOpenURLRequest(),
           bool trustedSource = false );

  virtual ~KonqRun();

  /**
   * Returns true if we found the mimetype for the given url.
   */
  bool wasMimeTypeFound() const { return m_bFoundMimeType; }

  KonqView *childView() const;

  const QString & typedUrl() const { return m_req.typedUrl; }

  KUrl mailtoURL() const { return m_mailto; }

protected:
  virtual void foundMimeType( const QString & _type );
  virtual void handleError( KJob * job );
  virtual void init();
  virtual void scanFile();

protected Q_SLOTS:
  void slotRedirection( KIO::Job *, const KUrl& );

private:
    bool tryOpenView(const QString& mimeType, bool associatedAppIsKonqueror);
  QPointer<KonqMainWindow> m_pMainWindow;
  QPointer<KonqView> m_pView;
  bool m_bFoundMimeType;
  KonqOpenURLRequest m_req;
  KUrl m_mailto;
};

#endif // KONQRUN_H
