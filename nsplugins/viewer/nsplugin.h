/*
  This is an encapsulation of the  Netscape plugin API.

  Copyright (c) 2000 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
                     Stefan Schimanski <1Stein@gmx.de>
                2003-2005 George Staikos <staikos@kde.org>
                2007, 2008 Maksim Orlovich     <maksim@kde.org>
                2006, 2007, 2008 Apple Inc.
                2008 Collabora, Ltd.
                2008 Sebastian Sauer <mail@dipe.org>

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

#ifndef __NS_PLUGIN_H__
#define __NS_PLUGIN_H__

#include <QObject>

#include <QMap>
#include <QPointer>
#include <QQueue>
#include <QList>

#include <KDebug>

#include <kparts/browserextension.h>  // for OpenUrlArguments
#include <kio/job.h>
#include <QDBusObjectPath>

#include <QX11EmbedWidget>

#define XP_UNIX
#define MOZ_X11
#include "sdk/npupp.h"

typedef char* NP_GetMIMEDescriptionUPP(void);
typedef NPError NP_InitializeUPP(NPNetscapeFuncs*, NPPluginFuncs*);
typedef NPError NP_ShutdownUPP(void);

#include <X11/Intrinsic.h>
#include <fixx11h.h>

void quitXt();

class OrgKdeNspluginsCallBackInterface;
class KLibrary;
class QTimer;


class NSPluginStreamBase : public QObject
{
Q_OBJECT
friend class NSPluginInstance;
public:
  NSPluginStreamBase( class NSPluginInstance *instance );
  ~NSPluginStreamBase();

  KUrl url() { return _url; }
  int pos() { return _pos; }
  void stop();

Q_SIGNALS:
  void finished( NSPluginStreamBase *strm );

protected:
  void finish( bool err );
  bool pump();
  bool error() { return _error; }
  void queue( const QByteArray &data );
  bool create( const QString& url, const QString& mimeType, void *notify, bool forceNotify = false );
  int tries() { return _tries; }
  void inform( );
  void updateURL( const KUrl& newUrl );

  class NSPluginInstance *_instance;
  uint16 _streamType;
  NPStream *_stream;
  void *_notifyData;
  KUrl _url;
  QString _fileURL;
  QString _mimeType;
  QByteArray _data;
  class KTemporaryFile *_tempFile;

private:
  int process( const QByteArray &data, int start );

  unsigned int _pos;
  QByteArray _queue;
  int _queuePos;
  int _tries;
  bool _onlyAsFile;
  bool _error;
  bool _informed;
  bool _forceNotify;
};


class NSPluginStream : public NSPluginStreamBase
{
  Q_OBJECT

public:
  NSPluginStream( class NSPluginInstance *instance );
  ~NSPluginStream();

    bool get(const QString& url, const QString& mimeType, void *notifyData, bool reload = false);
    bool post(const QString& url, const QByteArray& data, const QString& mimeType, void *notifyData,
              const KParts::OpenUrlArguments& args,
              const KParts::BrowserArguments& browserArgs);

protected Q_SLOTS:
  void data(KIO::Job *job, const QByteArray &data);
  void totalSize(KJob *job, qulonglong size);
  void mimetype(KIO::Job * job, const QString &mimeType);
  void result(KJob *job);
  void redirection(KIO::Job *job, const KUrl& url);
  void resume();

protected:
  QPointer<KIO::TransferJob> _job;
  QTimer *_resumeTimer;
};


class NSPluginBufStream : public NSPluginStreamBase
{
  Q_OBJECT

public:
  NSPluginBufStream( class NSPluginInstance *instance );
  ~NSPluginBufStream();

  bool get( const QString& url, const QString& mimeType, const QByteArray &buf, void *notifyData, bool singleShot=false );

protected Q_SLOTS:
  void timer();

protected:
  QTimer *_timer;
  bool _singleShot;
};

class PluginHost;

class NSPluginInstance : public QObject
{
  Q_OBJECT

public:

  // constructor, destructor
  NSPluginInstance( NPP privateData, NPPluginFuncs *pluginFuncs, KLibrary *handle,
		    const QString &src, const QString &mime,
                    const QString &appId, const QString &callbackId, bool embed,
		    QObject *parent );
  ~NSPluginInstance();

  // DBus-exported functions
  void shutdown();
  void setupWindow(int winId, int w, int h);
  void resizePlugin(int clientWinId, int w, int h);
  void javascriptResult(int id, const QString &result);
  void gotFocusIn();
  void gotFocusOut();

  // last via NSPluginClass::newInstance() produced NSPluginInstance instance.
  static NSPluginInstance* lastPluginInstance();
  static void setLastPluginInstance(NSPluginInstance*);

  // value handling
  NPError NPGetValue(NPPVariable variable, void *value);
  NPError NPSetValue(NPNVariable variable, void *value);

  // window handling
  NPError NPSetWindow(NPWindow *window);

  // stream functions
  NPError NPDestroyStream(NPStream *stream, NPReason reason);
  NPError NPNewStream(NPMIMEType type, NPStream *stream, NPBool seekable, uint16 *stype);
  void NPStreamAsFile(NPStream *stream, const char *fname);
  int32 NPWrite(NPStream *stream, int32 offset, int32 len, void *buf);
  int32 NPWriteReady(NPStream *stream);

  // URL functions
  void NPURLNotify(const QString &url, NPReason reason, void *notifyData);

  // Event handling
  uint16 HandleEvent(void *event);

  // signal emitters
  void emitStatus( const QString &message);
  void requestURL( const QString &url, const QString &mime,
		   const QString &target, void *notify, bool forceNotify = false, bool reload = false );
  void postURL( const QString &url, const QByteArray& data, const QString &mime,
                const QString &target, void *notify, const KParts::OpenUrlArguments& args,
                const KParts::BrowserArguments& browserArgs, bool forceNotify = false );

  QString normalizedURL(const QString& url) const;
  
  bool hasPendingJSRequests() const;

public Q_SLOTS:
  void streamFinished( NSPluginStreamBase *strm );

  void timer();

private:
  friend class NSPluginStreamBase;

  void destroy();

  bool _destroyed;
  bool _embedded;
  void addTempFile(KTemporaryFile *tmpFile);
  QList<KTemporaryFile *> _tempFiles;
  OrgKdeNspluginsCallBackInterface *_callback;
  QList<NSPluginStreamBase *> _streams;
  KLibrary *_handle;
  QTimer *_timer;

  NPP      _npp;
  NPPluginFuncs _pluginFuncs;

  PluginHost*      _pluginHost; // Manages embedding of the plugin into us
  int _width, _height;          // last size we used;

  QString _baseURL;

  struct Request
  {
      // A GET request
      Request( const QString &_url, const QString &_mime,
	       const QString &_target, void *_notify, bool _forceNotify = false,
               bool _reload = false)
	  { url=_url; mime=_mime; target=_target; notify=_notify; post=false; forceNotify = _forceNotify; reload = _reload; }

      // A POST request
      Request( const QString &_url, const QByteArray& _data,
               const QString &_mime, const QString &_target, void *_notify,
               const KParts::OpenUrlArguments& _args,
               const KParts::BrowserArguments& _browserArgs,
               bool _forceNotify = false)
      {
          url=_url; mime=_mime; target=_target;
          notify=_notify; post=true; data=_data; args=_args; browserArgs=_browserArgs;
          forceNotify = _forceNotify;
      }

      QString url;
      QString mime;
      QString target;
      QByteArray data;
      bool post;
      bool forceNotify;
      bool reload;
      void *notify;
      KParts::OpenUrlArguments args;
      KParts::BrowserArguments browserArgs;
  };

  QQueue<Request *> _waitingRequests;
  QMap<int, Request*> _jsrequests;
  int _numJSRequests; // entered in earlier than _jsrequests.
  
  static NSPluginInstance* s_lastPluginInstance;
};


class NSPluginClass : public QObject
{
  Q_OBJECT
public:

  NSPluginClass( const QString &library, QObject *parent );
  ~NSPluginClass();

  QString getMIMEDescription();
  QDBusObjectPath newInstance(const QString &url, const QString &mimeType, bool embed,
                      const QStringList &argn, const QStringList &argv,
                      const QString &appId, const QString &callbackId, bool reload);
  void destroyInstance( NSPluginInstance* inst );
  bool error() { return _error; }

  void setApp(const QByteArray& app) { _app = app; }
  const QByteArray& app() const { return _app; }

protected Q_SLOTS:
  void timer();

private:
  int initialize();
  void shutdown();

  KLibrary *_handle;
  QString  _libname;
  bool _constructed;
  bool _error;
  QTimer *_timer;

  NP_GetMIMEDescriptionUPP *_NP_GetMIMEDescription;
  NP_InitializeUPP *_NP_Initialize;
  NP_ShutdownUPP *_NP_Shutdown;

  QList<NSPluginInstance *> _instances;
  QList<NSPluginInstance *> _trash;

  QByteArray _app;
  NPPluginFuncs _pluginFuncs;
  NPNetscapeFuncs _nsFuncs;

  // If plugins use gtk, we call the gtk_init function for them ---
  // but only do it once.
  static bool s_initedGTK;
};


class NSPluginViewer : public QObject
{
    Q_OBJECT
public:
   NSPluginViewer( QObject *parent );
   virtual ~NSPluginViewer();

   void shutdown();
   QDBusObjectPath newClass( const QString& plugin, const QString& senderId );

private Q_SLOTS:
   void appChanged( const QString& id,  const QString& oldOwner, const QString& newOwner);

private:
   QMap<QString, NSPluginClass *> _classes;
};


#endif
