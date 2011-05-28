/*
  This is an encapsulation of the  Netscape plugin API.

  Copyright (c) 2000 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
                     Stefan Schimanski <1Stein@gmx.de>
                2003-2005 George Staikos <staikos@kde.org>
                2007, 2008, 2010 Maksim Orlovich <maksim@kde.org>
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

#include "nsplugin.h"
#include "pluginhost_xembed.h"
#include "pluginhost_xt.h"
#include "resolve.h"
#include "classadaptor.h"
#include "instanceadaptor.h"
#include "vieweradaptor.h"
#include "scripting.h"

#include "nsplugins_callback_interface.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <QDir>
#include <QFile>
#include <QTimer>
#include <QApplication>

#ifdef Bool
#undef Bool
#endif

#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kio/netaccess.h>
#include <kprotocolmanager.h>
#include <klibrary.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <ktemporaryfile.h>
#include <kurl.h>
#include <QX11Info>
#include <QProcess>

#include <X11/Intrinsic.h>
#include <X11/Composite.h>
#include <X11/Constraint.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>

// provide these symbols when compiling with gcc 3.x

#if defined __GNUC__ && defined __GNUC_MINOR__
# define KDE_GNUC_PREREQ(maj, min) \
  ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
# define KDE_GNUC_PREREQ(maj, min) 0
#endif


#if defined(__GNUC__) && KDE_GNUC_PREREQ(3,0)
extern "C" void* __builtin_new(size_t s)
{
   return operator new(s);
}

extern "C" void __builtin_delete(void* p)
{
   operator delete(p);
}

extern "C" void* __builtin_vec_new(size_t s)
{
   return operator new[](s);
}

extern "C" void __builtin_vec_delete(void* p)
{
   operator delete[](p);
}

extern "C" void __pure_virtual()
{
   abort();
}
#endif

// The NSPluginInstance is always the ndata of the instance. Sometimes, plug-ins will call an instance-specific function
// with a NULL instance. To workaround this, we remember the last NSPluginInstance produced with the
// NSPluginClass::newInstance() method. This specifically works around Flash and Shockwave which do e.g. call NPN_Useragent
// with a NULL instance When we call NPP_New.
// At the moment we do setLastPluginInstance() only if the NSPluginInstance is created. Probably it would be more logical
// to do that more often to prevent some wired situations where we may end with the wrong NSPluginInstance for a plugin.
NSPluginInstance* NSPluginInstance::s_lastPluginInstance = 0;
NSPluginInstance* NSPluginInstance::lastPluginInstance() { return s_lastPluginInstance; }
void NSPluginInstance::setLastPluginInstance(NSPluginInstance* inst) { s_lastPluginInstance = inst; }
static NSPluginInstance* pluginViewForInstance(NPP instance)
{
    if (instance && instance->ndata)
        return static_cast<NSPluginInstance*>(instance->ndata);
    return NSPluginInstance::lastPluginInstance();
}

// server side functions -----------------------------------------------------

// allocate memory

namespace kdeNsPluginViewer  {
void *g_NPN_MemAlloc(uint32_t size)
{
   void *mem = ::malloc(size);

   //kDebug(1431) << "g_NPN_MemAlloc(), size=" << size << " allocated at " << mem;

   return mem;
}


// free memory
void g_NPN_MemFree(void *ptr)
{
   //kDebug(1431) << "g_NPN_MemFree() at " << ptr;
   if (ptr)
     ::free(ptr);
}

}

uint32_t g_NPN_MemFlush(uint32_t size)
{
   Q_UNUSED(size);
   //kDebug(1431) << "g_NPN_MemFlush()";
   // MAC OS only..  we don't use this
   return 0;
}


// redraw
void g_NPN_ForceRedraw(NPP /*instance*/)
{
   // http://devedge.netscape.com/library/manuals/2002/plugin/1.0/npn_api3.html#999401
   // FIXME
   kDebug(1431) << "g_NPN_ForceRedraw() [unimplemented]";
}


// invalidate rect
void g_NPN_InvalidateRect(NPP /*instance*/, NPRect* /*invalidRect*/)
{
   // http://devedge.netscape.com/library/manuals/2002/plugin/1.0/npn_api7.html#999503
   // FIXME
   kDebug(1431) << "g_NPN_InvalidateRect() [unimplemented]";
}


// invalidate region
void g_NPN_InvalidateRegion(NPP /*instance*/, NPRegion /*invalidRegion*/)
{
   // http://devedge.netscape.com/library/manuals/2002/plugin/1.0/npn_api8.html#999528
   // FIXME
   kDebug(1431) << "g_NPN_InvalidateRegion() [unimplemented]";
}


// get value
NPError g_NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
   kDebug(1431) << "g_NPN_GetValue(), variable=" << static_cast<int>(variable);
   NSPluginInstance* inst = pluginViewForInstance(instance);

   switch (variable)
   {
      case NPNVxDisplay:
         *(void**)value = QX11Info::display();
         return NPERR_NO_ERROR;
      case NPNVxtAppContext:
         *(void**)value = XtDisplayToApplicationContext(QX11Info::display());
         return NPERR_NO_ERROR;
      case NPNVjavascriptEnabledBool:
         *(bool*)value = true;
         return NPERR_NO_ERROR;
      case NPNVasdEnabledBool:
         // SmartUpdate - we don't do this
         *(bool*)value = false;
         return NPERR_NO_ERROR;
      case NPNVisOfflineBool:
         // Offline browsing - no thanks
         *(bool*)value = false;
         return NPERR_NO_ERROR;
      case NPNVSupportsXEmbedBool:
         // ### may depend on event loop setting
         *(bool*)value = true;
         return NPERR_NO_ERROR;
      case NPNVToolkit:
         // This is messy. OSS things want to see "Gtk2" here;
         // but commercial flash works better if we return something else.
         // So we return a KHTML classic, since we can work with
         // the community members far easier.
         *(NPNToolkitType*)value = (NPNToolkitType)0xFEEDABEE;
         return NPERR_NO_ERROR;
      case NPNVWindowNPObject:
         if (inst && inst->scripting()) {
            *(NPObject**)value = inst->scripting()->acquireWindow();
            return NPERR_NO_ERROR;
         } else {
            kDebug(1431) << "script object queried, but no scripting active";
            return NPERR_INVALID_PARAM;
         }
#if 0
      case NPNVPluginElementNPObject:
         if (inst && inst->scripting()) {
            *(NPObject**)value = inst->scripting()->acquirePluginElement();
            return NPERR_NO_ERROR;
         } else {
            kDebug(1431) << "script object queried, but no scripting active";
            return NPERR_INVALID_PARAM;
         }
#endif
      default:
         kDebug(1431) << "g_NPN_GetValue(), [unimplemented] variable=" << variable;
         return NPERR_INVALID_PARAM;
   }
}


NPError g_NPN_DestroyStream(NPP instance, NPStream* stream,
                          NPReason reason)
{
   // FIXME: is this correct?  I imagine it is not.  (GS)
   kDebug(1431) << "g_NPN_DestroyStream()";

   NSPluginInstance *inst = pluginViewForInstance(instance);
   inst->streamFinished( (NSPluginStream *)stream->ndata );

   switch (reason) {
   case NPRES_DONE:
      return NPERR_NO_ERROR;
   case NPRES_USER_BREAK:
      // FIXME: notify the user
   case NPRES_NETWORK_ERR:
      // FIXME: notify the user
   default:
      return NPERR_GENERIC_ERROR;
   }
}


NPError g_NPN_RequestRead(NPStream* /*stream*/, NPByteRange* /*rangeList*/)
{
   // http://devedge.netscape.com/library/manuals/2002/plugin/1.0/npn_api16.html#999734
   kDebug(1431) << "g_NPN_RequestRead() [unimplemented]";

   // FIXME
   return NPERR_GENERIC_ERROR;
}

NPError g_NPN_NewStream(NPP /*instance*/, NPMIMEType /*type*/,
                      const char* /*target*/, NPStream** /*stream*/)
{
   // http://devedge.netscape.com/library/manuals/2002/plugin/1.0/npn_api12.html#999628
   kDebug(1431) << "g_NPN_NewStream() [unimplemented]";

   // FIXME
   // This creates a stream from the plugin to the browser of type "type" to
   // display in "target"
   return NPERR_GENERIC_ERROR;
}

int32_t g_NPN_Write(NPP /*instance*/, NPStream* /*stream*/, int32_t /*len*/, void* /*buf*/)
{
   // http://devedge.netscape.com/library/manuals/2002/plugin/1.0/npn_api21.html#999859
   kDebug(1431) << "g_NPN_Write() [unimplemented]";

   // FIXME
   return 0;
}


// URL functions
NPError g_NPN_GetURL(NPP instance, const char *url, const char *target)
{
   kDebug(1431) << "g_NPN_GetURL: url=" << url << " target=" << target;

   NSPluginInstance *inst = pluginViewForInstance(instance);
   if (inst) {
      inst->requestURL( QString::fromLatin1(url), QString(),
                        QString::fromLatin1(target), 0 );
   }

   return NPERR_NO_ERROR;
}


NPError g_NPN_GetURLNotify(NPP instance, const char *url, const char *target,
                         void* notifyData)
{
    kDebug(1431) << "g_NPN_GetURLNotify: url=" << url << " target=" << target << " inst=" << (void*)instance;
   NSPluginInstance *inst = pluginViewForInstance(instance);
   if (inst) {
      kDebug(1431) << "g_NPN_GetURLNotify: ndata=" << (void*)inst;
      inst->requestURL( QString::fromLatin1(url), QString(),
                        QString::fromLatin1(target), notifyData, true );
   }

   return NPERR_NO_ERROR;
}


NPError g_NPN_PostURLNotify(NPP instance, const char* url, const char* target,
                     uint32_t len, const char* buf, NPBool file, void* notifyData)
{
// http://devedge.netscape.com/library/manuals/2002/plugin/1.0/npn_api14.html
   kDebug(1431) << "g_NPN_PostURLNotify() [incomplete]";
   kDebug(1431) << "url=[" << url << "] target=[" << target << "]";
   QByteArray postdata;
   KParts::OpenUrlArguments args;
   KParts::BrowserArguments browserArgs;

   if (len == 0) {
      return NPERR_NO_DATA;
   }

   if (file) { // buf is a filename
      QFile f(buf);
      if (!f.open(QIODevice::ReadOnly)) {
         return NPERR_FILE_NOT_FOUND;
      }

      // FIXME: this will not work because we need to strip the header out!
      postdata = f.readAll();
      f.close();
   } else {    // buf is raw data
      // First strip out the header
      const char *previousStart = buf;
      uint32_t l;
      bool previousCR = true;

      for (l = 1;; ++l) {
         if (l == len) {
            break;
         }

         if (buf[l-1] == '\n' || (previousCR && buf[l-1] == '\r')) {
            if (previousCR) { // header is done!
               if ((buf[l-1] == '\r' && buf[l] == '\n') ||
                   (buf[l-1] == '\n' &&  buf[l] == '\r'))
                  l++;
               l++;
               previousStart = &buf[l-1];
               break;
            }

            QString thisLine = QString::fromLatin1(previousStart, &buf[l-1] - previousStart).trimmed();

            previousStart = &buf[l];
            previousCR = true;

            kDebug(1431) << "Found header line: [" << thisLine << "]";
            if (thisLine.startsWith("Content-Type: ")) {
               browserArgs.setContentType(thisLine);
            }
         } else {
            previousCR = false;
         }
      }

      postdata = QByteArray(previousStart, len - l + 1);
   }

   kDebug(1431) << "Post data: " << postdata.size() << " bytes";
#if 0
   QFile f("/tmp/nspostdata");
   f.open(QIODevice::WriteOnly);
   f.write(postdata);
   f.close();
#endif

   if (!target || !*target) {
      // Send the results of the post to the plugin
      // (works by default)
   } else if (!strcmp(target, "_current") || !strcmp(target, "_self") ||
              !strcmp(target, "_top")) {
      // Unload the plugin, put the results in the frame/window that the
      // plugin was loaded in
      // FIXME
   } else if (!strcmp(target, "_new") || !strcmp(target, "_blank")){
      // Open a new browser window and write the results there
      // FIXME
   } else {
      // Write the results to the specified frame
      // FIXME
   }

   NSPluginInstance *inst = pluginViewForInstance(instance);
   if (inst && !inst->normalizedURL(QString::fromLatin1(url)).isNull()) {
      inst->postURL( QString::fromLatin1(url), postdata, browserArgs.contentType(),
                     QString::fromLatin1(target), notifyData, args, browserArgs, true );
   } else {
      // Unsupported / insecure
      return NPERR_INVALID_URL;
   }

   return NPERR_NO_ERROR;
}


NPError g_NPN_PostURL(NPP instance, const char* url, const char* target,
                      uint32_t len, const char* buf, NPBool file)
{
// http://devedge.netscape.com/library/manuals/2002/plugin/1.0/npn_api13.html
   kDebug(1431) << "g_NPN_PostURL()";
   kDebug(1431) << "url=[" << url << "] target=[" << target << "]";
   QByteArray postdata;
   KParts::OpenUrlArguments args;
   KParts::BrowserArguments browserArgs;

   if (len == 0) {
      return NPERR_NO_DATA;
   }

   if (file) { // buf is a filename
      QFile f(buf);
      if (!f.open(QIODevice::ReadOnly)) {
         return NPERR_FILE_NOT_FOUND;
      }

      // FIXME: this will not work because we need to strip the header out!
      postdata = f.readAll();
      f.close();
   } else {    // buf is raw data
      // First strip out the header
      const char *previousStart = buf;
      uint32_t l;
      bool previousCR = true;

      for (l = 1;; ++l) {
         if (l == len) {
            break;
         }

         if (buf[l-1] == '\n' || (previousCR && buf[l-1] == '\r')) {
            if (previousCR) { // header is done!
               if ((buf[l-1] == '\r' && buf[l] == '\n') ||
                   (buf[l-1] == '\n' &&  buf[l] == '\r'))
                  l++;
               l++;
               previousStart = &buf[l-1];
               break;
            }

            QString thisLine = QString::fromLatin1(previousStart, &buf[l-1] - previousStart).trimmed();

            previousStart = &buf[l];
            previousCR = true;

            kDebug(1431) << "Found header line: [" << thisLine << "]";
            if (thisLine.startsWith("Content-Type: ")) {
               browserArgs.setContentType(thisLine);
            }
         } else {
            previousCR = false;
         }
      }

      postdata = QByteArray(previousStart, len - l + 1);
   }

   kDebug(1431) << "Post data: " << postdata.size() << " bytes";
#if 0
   QFile f("/tmp/nspostdata");
   f.open(QIODevice::WriteOnly);
   f.write(postdata);
   f.close();
#endif

   if (!target || !*target) {
      // Send the results of the post to the plugin
      // (works by default)
   } else if (!strcmp(target, "_current") || !strcmp(target, "_self") ||
              !strcmp(target, "_top")) {
      // Unload the plugin, put the results in the frame/window that the
      // plugin was loaded in
      // FIXME
   } else if (!strcmp(target, "_new") || !strcmp(target, "_blank")){
      // Open a new browser window and write the results there
      // FIXME
   } else {
      // Write the results to the specified frame
      // FIXME
   }

   NSPluginInstance *inst = pluginViewForInstance(instance);
   if (inst && !inst->normalizedURL(QString::fromLatin1(url)).isNull()) {
      inst->postURL( QString::fromLatin1(url), postdata, browserArgs.contentType(),
                     QString::fromLatin1(target), 0L, args, browserArgs, false );
   } else {
      // Unsupported / insecure
      return NPERR_INVALID_URL;
   }

   return NPERR_NO_ERROR;
}


// display status message
void g_NPN_Status(NPP instance, const char *message)
{
   kDebug(1431) << "g_NPN_Status(): " << message;

   if (!instance)
      return;

   // turn into an instance signal
   NSPluginInstance *inst = pluginViewForInstance(instance);

   inst->emitStatus(message);
}


static QByteArray uaStore;
static QByteArray uaEmpty("Gecko");

// inquire user agent
const char *g_NPN_UserAgent(NPP instance)
{
    if (!instance)
        return uaEmpty.data();

   if (uaStore.isEmpty()) {
        KProtocolManager kpm;
        QString agent = kpm.userAgentForHost("nspluginviewer");
        uaStore = agent.toLatin1();
    }

    kDebug(1431) << "g_NPN_UserAgent() = " << uaStore;
    return uaStore.data();
}

// inquire version information
void g_NPN_Version(int *plugin_major, int *plugin_minor, int *browser_major, int *browser_minor)
{
   kDebug(1431) << "g_NPN_Version()";

   // FIXME: Use the sensible values
   *browser_major = NP_VERSION_MAJOR;
   *browser_minor = NP_VERSION_MINOR;

   *plugin_major = NP_VERSION_MAJOR;
   *plugin_minor = NP_VERSION_MINOR;
}


void g_NPN_ReloadPlugins(NPBool reloadPages)
{
   // http://devedge.netscape.com/library/manuals/2002/plugin/1.0/npn_api15.html#999713
   kDebug(1431) << "g_NPN_ReloadPlugins()";
   QString prog = KGlobal::dirs()->findExe("nspluginscan");

   if (reloadPages) {
      // This is the proper way, but it cannot be done because we have no
      // handle to the caller!  How stupid!  We cannot force all konqi windows
      // to reload - that would be evil.
      //p.start(K3Process::Block);
      // Let's only allow the caller to be reloaded, not everything.
      //if (_callback)
      //   _callback->reloadPage();
      QProcess::startDetached(prog);
   } else {
      QProcess::startDetached(prog);
   }
}


// JAVA functions
void *g_NPN_GetJavaEnv()
{
   kDebug(1431) << "g_NPN_GetJavaEnv() [unimplemented]";
   // FIXME - what do these do?  I can't find docs, and even Mozilla doesn't
   //         implement them
   return 0;
}


void* g_NPN_GetJavaPeer(NPP /*instance*/)
{
   kDebug(1431) << "g_NPN_GetJavaPeer() [unimplemented]";
   // FIXME - what do these do?  I can't find docs, and even Mozilla doesn't
   //         implement them
   return 0;
}


NPError g_NPN_SetValue(NPP /*instance*/, NPPVariable variable, void* /*value*/)
{
   kDebug(1431) << "g_NPN_SetValue() [unimplemented]";
   switch (variable) {
   case NPPVpluginWindowBool:
      // FIXME
      // If true, the plugin is windowless.  If false, it is in a window.
   case NPPVpluginTransparentBool:
      // FIXME
      // If true, the plugin is displayed transparent
   default:
      return NPERR_GENERIC_ERROR;
   }
}



/**
 These two are in the ABI version 16 which we don't claim to support, but 
 flash uses anyway.
*/
static void g_NPN_PushPopupsEnabledState(NPP /*instance*/, NPBool enabled)
{
   kDebug(1431) << "[unimplemented]" << enabled;
}

static void g_NPN_PopPopupsEnabledState(NPP /*instance*/)
{
   kDebug(1431) << "[unimplemented]";
}

/******************************************************************/

static int s_instanceCounter = 0;

NSPluginInstance::NSPluginInstance(NPPluginFuncs *pluginFuncs,
                                   KLibrary *handle,
                                   const QString &url, const QString &mimeType,
                                   const QStringList &argn, const QStringList &argv,
                                   const QString &appId, const QString &callbackId,
                                   bool embed,
                                   QObject *parent )
   : QObject( parent )
{
    // Setup d-bus infrastructure early, will need it for script stuff
    kdeNsPluginViewer::initDBusTypes();

    // The object name is the dbus object path
   (void) new InstanceAdaptor( this );
   setObjectName( QLatin1String( "/Instance_" ) + QString::number( ++s_instanceCounter ) );
   QDBusConnection::sessionBus().registerObject( objectName(), this );
   _callback = new org::kde::nsplugins::CallBack( appId, callbackId, QDBusConnection::sessionBus() );


   _embedded = false;
   _destroyed = false;
   _handle = handle;
   _numJSRequests = 0;
   _scripting = 0;

   // set the current plugin instance
   NSPluginInstance::setLastPluginInstance(this);

   memcpy(&_pluginFuncs, pluginFuncs, sizeof(_pluginFuncs));

   // See want the scripting stuff very early, since newp can use it.
   _scripting = new ScriptExportEngine(this);
   
   // copy parameters over, and extract out base url if specified
   int argc = argn.count();
   char **_argn = new char*[argc];
   char **_argv = new char*[argc];
   
   QString baseURL = url;
   for (int i=0; i<argc; i++)
   {
      QByteArray encN = argn[i].toUtf8();
      QByteArray encV = argv[i].toUtf8();

      _argn[i] = strdup(encN.constData());
      _argv[i] = strdup(encV.constData());

      if (argn[i] == QLatin1String("__KHTML__PLUGINBASEURL"))
         baseURL = argv[i];
      if (argn[i] == QLatin1String("__KHTML__PLUGINPAGEURL"))
         _pageURL = argv[i];
      kDebug(1431) << "argn=" << _argn[i] << " argv=" << _argv[i];
   }

   // create plugin instance
   char* mime = strdup(mimeType.toAscii().constData());
   _npp = (NPP)malloc(sizeof(NPP_t));   // I think we should be using
                                        // malloc here, just to be safe,
                                        // since the nsplugin plays with
                                        // this thing
   memset(_npp, 0, sizeof(NPP_t));
   _npp->ndata = this;

   // create actual plugin's instance
   NPError error = _pluginFuncs.newp(mime, _npp, embed ? NP_EMBED : NP_FULL,
                                     argc, _argn, _argv, 0);

   kDebug(1431) << "NPP_New = " << (int)error;

   // free arrays with arguments
   delete [] _argn;
   delete [] _argv;

   // check for error
   if ( error!=NPERR_NO_ERROR )
   {
      ::free(_npp);
      ::free(mime);
      kDebug(1431) << "<- newp failed";
      _initializedOK = false;
      return;
   }

   _initializedOK = true;

   KUrl base(baseURL);
   base.setFileName( QString() );
   _baseURL = base.url();

   _timer = new QTimer( this );
   connect( _timer, SIGNAL(timeout()), SLOT(timer()) );

   kDebug(1431) << "NSPluginInstance::NSPluginInstance";
   kDebug(1431) << "pdata = " << _npp->pdata;
   kDebug(1431) << "ndata = " << _npp->ndata;

   // now let's see if the plugin offers any scriptable stuff, too.
   _scripting->connectToPlugin();

   // Create the appropriate host for the plugin type.
   _pluginHost = 0;
   int result = 0; /* false */
   //### iceweasel does something odd here --- it enabled XEmbed for swfdec,
   // even though that doesn't provide GetValue at all(!)
   if (NPGetValue(NPPVpluginNeedsXEmbed, &result) == NPERR_NO_ERROR && result) {
      kDebug(1431) << "plugin reqests XEmbed";
      _pluginHost = new PluginHostXEmbed(this);
   } else {
      kDebug(1431) << "plugin requests Xt";
      _pluginHost = new PluginHostXt(this);
   }

   XSync(QX11Info::display(), false);
}

NSPluginInstance::~NSPluginInstance()
{
   kDebug(1431) << "-> ~NSPluginInstance";
   destroy();
   kDebug(1431) << "<- ~NSPluginInstance";
}

// Note: here we again narrow the types to the same ulong as LiveConnectExtension uses,
// so we don't end up with extra precision floating around
NSLiveConnectResult NSPluginInstance::lcGet(qulonglong objid, const QString& field)
{
    NSLiveConnectResult result;
    if (_scripting) {
        KParts::LiveConnectExtension::Type type;
        unsigned long outId;
        result.success = _scripting->get(static_cast<unsigned long>(objid), field,
                                         type, outId, result.value);
        result.type  = type;
        result.objid = outId;
    }
    return result;
}

bool NSPluginInstance::lcPut(qulonglong objid, const QString& field, const QString& value)
{
    if (_scripting)
        return _scripting->put(static_cast<unsigned long>(objid), field, value);

    return false;
}

NSLiveConnectResult NSPluginInstance::lcCall(qulonglong objid, const QString& func,
                                             const QStringList& args)
{
    NSLiveConnectResult result;
    if (_scripting) {
        KParts::LiveConnectExtension::Type type;
        unsigned long outId;
        result.success = _scripting->call(static_cast<unsigned long>(objid), func, args,
                                          type, outId, result.value);
        result.type = type;
        result.objid = outId;
    }
    return result;
}

void NSPluginInstance::lcUnregister(qulonglong objid)
{
    if (_scripting)
        _scripting->unregister(static_cast<unsigned long>(objid));
}

void NSPluginInstance::destroy()
{
    if ( !_destroyed ) {

        kDebug(1431) << "delete streams";
        qDeleteAll( _waitingRequests );

        while ( !_streams.isEmpty() ) {
            NSPluginStreamBase *s = _streams.takeFirst();
                s->stop();
            delete s;
        }

        kDebug(1431) << "delete tempfiles";
        qDeleteAll( _tempFiles );

        kDebug(1431) << "delete callbacks";
        delete _callback;
        _callback = 0;

        kDebug(1431) << "delete scripting";
        delete _scripting;
        _scripting = 0;

        if (!_initializedOK) {
            _destroyed = true;
            return;
        }

        kDebug(1431) << "destroy plugin";
        NPSavedData *saved = 0;

        // As of 7/31/01, nsplugin crashes when used with Qt
        // linked with libGL if the destroy function is called.
        // A patch on that date hacked out the following call.
        // On 11/17/01, Jeremy White has reenabled this destroy
        // in a an attempt to better understand why this crash
        // occurs so that the real problem can be found and solved.
        // It's possible that a flaw in the SetWindow call
        // caused the crash and it is now fixed.
        if ( _pluginFuncs.destroy )
            _pluginFuncs.destroy( _npp, &saved );

        if (saved && saved->len && saved->buf)
          g_NPN_MemFree(saved->buf);
        if (saved)
          g_NPN_MemFree(saved);

        delete _pluginHost;
        _pluginHost = 0;

        if (_npp) {
            ::free(_npp);   // matched with malloc() in newInstance
        }

        _destroyed = true;
    }
}


void NSPluginInstance::shutdown()
{
    NSPluginClass *cls = dynamic_cast<NSPluginClass*>(parent());
    //destroy();
    if (cls) {
        cls->destroyInstance( this );
    }
}

bool NSPluginInstance::hasPendingJSRequests() const
{
    return _numJSRequests > 0;
}

void NSPluginInstance::timer()
{
    if (!_embedded) {
         _timer->setSingleShot( true );
         _timer->start( 100 );
         return;
    }

    //_streams.clear();

    // start queued requests
    kDebug(1431) << "looking for waiting requests";
    while ( !_waitingRequests.isEmpty() ) {
        kDebug(1431) << "request found";
        Request req( *_waitingRequests.head() );
        delete _waitingRequests.dequeue();

        QString url;

        // Note: sync javascript: handling with requestURL

        // make absolute url
        if ( req.url.left(11).toLower()=="javascript:" )
            url = req.url;
        else if ( KUrl::isRelativeUrl(req.url) ) {
            KUrl bu( _baseURL );
            KUrl absUrl( bu, req.url );
            url = absUrl.url();
        } else if ( req.url[0]=='/' && KUrl(_baseURL).hasHost() ) {
            KUrl absUrl( _baseURL );
            absUrl.setPath( req.url );
            url = absUrl.url();
        } else
            url = req.url;

        // non empty target = frame target
        if ( !req.target.isEmpty())
        {
            if (_callback)
            {
                if ( req.post ) {
                    _callback->postURL( url, req.target, req.data, req.mime );
                } else {
                    _callback->requestURL( url, req.target );
                }
                if ( req.notify ) {
                    NPURLNotify( req.url, NPRES_DONE, req.notify );
                }
            }
        } else {
            if (!url.isEmpty())
            {
                kDebug(1431) << "Starting new stream " << req.url;

                if (req.post) {
                    // create stream
                    NSPluginStream *s = new NSPluginStream( this );
                    connect( s, SIGNAL(finished(NSPluginStreamBase*)),
                             SLOT(streamFinished(NSPluginStreamBase*)) );
                    _streams.append( s );

                    kDebug() << "posting to " << url;

                    emitStatus( i18n("Submitting data to %1", url) );
                    s->post( url, req.data, req.mime, req.notify, req.args, req.browserArgs );
                } else if (url.toLower().startsWith("javascript:")){
                    if (_callback) {
                        static int _jsrequestid = 0;
                        _jsrequests.insert(_jsrequestid, new Request(req));
                        _callback->evalJavaScript(_jsrequestid++, url.mid(11));
                    } else {
                        --_numJSRequests;
                        kDebug() << "No callback for javascript: url!";
                    }
                } else {
                    // create stream
                    NSPluginStream *s = new NSPluginStream( this );
                    connect( s, SIGNAL(finished(NSPluginStreamBase*)),
                             SLOT(streamFinished(NSPluginStreamBase*)) );
                    _streams.append( s );

                    kDebug() << "getting " << url;

                    emitStatus( i18n("Requesting %1", url) );
                    s->get( url, req.mime, req.notify, req.reload );
                }

                //break;
            }
        }
    }
}


QString NSPluginInstance::normalizedURL(const QString& url) const {

    // ### for dfaure:  KUrl(KUrl("http://www.youtube.com/?v=JvOSnRD5aNk"), KUrl("javascript:window.location+"__flashplugin_unique__"));

    //### hack, prolly evil, etc.
    if (url.startsWith("javascript:"))
       return url;

    KUrl bu( _baseURL );
    KUrl inURL(bu, url);
    KConfig _cfg( "kcmnspluginrc" );
    KConfigGroup cfg(&_cfg, "Misc");

    if (!cfg.readEntry("HTTP URLs Only", false) ||
	inURL.protocol() == "http" ||
        inURL.protocol() == "https" ||
        inURL.protocol() == "javascript") {
        return inURL.url();
    }

    // Allow: javascript:, http, https, or no protocol (match loading)
    kDebug(1431) << "NSPluginInstance::normalizedURL - I don't think so.  http or https only!";
    return QString();
}


void NSPluginInstance::requestURL( const QString &url, const QString &mime,
                                   const QString &target, void *notify, bool forceNotify, bool reload )
{
    // Generally this should already be done, but let's be safe for now.
    QString nurl = normalizedURL(url);
    if (nurl.isNull()) {
        return;
    }
    
    // We dispatch JS events in target for empty target GET only.(see timer());
    if (target.isEmpty() && nurl.left(11).toLower()=="javascript:")
         ++_numJSRequests;

    kDebug(1431) << "NSPluginInstance::requestURL url=" << nurl << " target=" << target << " notify=" << notify << "JS jobs now:" << _numJSRequests;
    _waitingRequests.enqueue( new Request( nurl, mime, target, notify, forceNotify, reload ) );
    _timer->setSingleShot( true );
    _timer->start( 100 );
}


void NSPluginInstance::postURL( const QString &url, const QByteArray& data,
                                const QString &mime,
                                const QString &target, void *notify,
                                const KParts::OpenUrlArguments& args,
                                const KParts::BrowserArguments& browserArgs,
                                bool forceNotify )
{
    // Generally this should already be done, but let's be safe for now.
    QString nurl = normalizedURL(url);
    if (nurl.isNull()) {
        return;
    }

    kDebug(1431) << "NSPluginInstance::postURL url=" << nurl << " target=" << target << " notify=" << notify;
    _waitingRequests.enqueue( new Request( nurl, data, mime, target, notify, args, browserArgs, forceNotify) );
    _timer->setSingleShot( true );
    _timer->start( 100 );
}


void NSPluginInstance::emitStatus(const QString &message)
{
    if( _callback )
      _callback->statusMessage( message );
}


void NSPluginInstance::streamFinished( NSPluginStreamBase* strm )
{
   kDebug(1431) << "-> NSPluginInstance::streamFinished";
   emitStatus( QString() );
   _streams.removeOne(strm);
   strm->deleteLater();
   _timer->setSingleShot( true );
   _timer->start( 100 );
}

void NSPluginInstance::setupWindow(int winId, int w, int h)
{
   kDebug(1431) << "-> NSPluginInstance::setupWindow( winid =" << winId << " w=" << w << ", h=" << h << " ) ";
   if (_pluginHost)
      _pluginHost->setupWindow(winId, w, h);
   else
    kWarning(1431) << "No plugin host!";

   kDebug(1431) << "<- NSPluginInstance::setupWindow";
   _width  = w;
   _height = h;
   _embedded = true;
}

void NSPluginInstance::resizePlugin(int clientWinId, int w, int h)
{
   kDebug() << _width << w << _height << h << _embedded;
   if (!_embedded)
      return;
   if (w == _width && h == _height)
      return;
   _pluginHost->resizePlugin(clientWinId, w, h);
   _width  = w;
   _height = h;
}


void NSPluginInstance::javascriptResult(int id, const QString &result) {
    QMap<int, Request*>::iterator i = _jsrequests.find( id );
    if (i != _jsrequests.end()) {
        --_numJSRequests;

        Request *req = i.value();
        _jsrequests.erase( i );
        NSPluginStream *s = new NSPluginStream( this );
        connect( s, SIGNAL(finished(NSPluginStreamBase*)),
                 SLOT(streamFinished(NSPluginStreamBase*)) );
        _streams.append( s );

        int len = result.length();
        s->create( req->url, QString("text/plain"), req->notify, req->forceNotify );
        kDebug(1431) << "javascriptResult has been called with: "<<result << "num JS requests now:" << _numJSRequests;
        if (len > 0) {
            QByteArray data(len + 1, 0);
            memcpy(data.data(), result.toLatin1(), len);
            data[len] = 0;
            s->process(data, 0);
        } else {
            len = 7; //  "unknown"
            QByteArray data(len + 1, 0);
            memcpy(data.data(), "unknown", len);
            data[len] = 0;
            s->process(data, 0);
        }
        s->finish(false);

        delete req;
    }
}


NPError NSPluginInstance::NPGetValue(NPPVariable variable, void *value)
{
    if( value==0 ) {
        kDebug() << "FIXME: value==0 in NSPluginInstance::NPGetValue";
        return NPERR_GENERIC_ERROR;
    }

    if (!_pluginFuncs.getvalue)
        return NPERR_GENERIC_ERROR;

    NPError error = _pluginFuncs.getvalue(_npp, variable, value);

    CHECK(GetValue,error);
}


NPError NSPluginInstance::NPSetValue(NPNVariable variable, void *value)
{
    if( value==0 ) {
        kDebug() << "FIXME: value==0 in NSPluginInstance::NPSetValue";
        return NPERR_GENERIC_ERROR;
    }

    if (!_pluginFuncs.setvalue)
        return NPERR_GENERIC_ERROR;

    NPError error = _pluginFuncs.setvalue(_npp, variable, value);

    CHECK(SetValue,error);
}


NPError NSPluginInstance::NPSetWindow(NPWindow *window)
{
    if( window==0 ) {
        kDebug() << "FIXME: window==0 in NSPluginInstance::NPSetWindow";
        return NPERR_GENERIC_ERROR;
    }

    if (!_pluginFuncs.setwindow)
        return NPERR_GENERIC_ERROR;

    NPError error = _pluginFuncs.setwindow(_npp, window);

    CHECK(SetWindow,error);
}


NPError NSPluginInstance::NPDestroyStream(NPStream *stream, NPReason reason)
{
    if( stream==0 ) {
        kDebug() << "FIXME: stream==0 in NSPluginInstance::NPDestroyStream";
        return NPERR_GENERIC_ERROR;
    }

    if (!_pluginFuncs.destroystream)
        return NPERR_GENERIC_ERROR;

    NPError error = _pluginFuncs.destroystream(_npp, stream, reason);

    CHECK(DestroyStream,error);
}


NPError NSPluginInstance::NPNewStream(NPMIMEType type, NPStream *stream, NPBool seekable, uint16_t *stype)
{
    if( stream==0 ) {
        kDebug() << "FIXME: stream==0 in NSPluginInstance::NPNewStream";
        return NPERR_GENERIC_ERROR;
    }

    if( stype==0 ) {
        kDebug() << "FIXME: stype==0 in NSPluginInstance::NPNewStream";
        return NPERR_GENERIC_ERROR;
    }

    if (!_pluginFuncs.newstream)
        return NPERR_GENERIC_ERROR;

    NPError error = _pluginFuncs.newstream(_npp, type, stream, seekable, stype);

    CHECK(NewStream,error);
}


void NSPluginInstance::NPStreamAsFile(NPStream *stream, const char *fname)
{
    if( stream==0 ) {
        kDebug() << "FIXME: stream==0 in NSPluginInstance::NPStreamAsFile";
        return;
    }

    if( fname==0 ) {
        kDebug() << "FIXME: fname==0 in NSPluginInstance::NPStreamAsFile";
        return;
    }

    if (!_pluginFuncs.asfile)
        return;

    _pluginFuncs.asfile(_npp, stream, fname);
}


int32_t NSPluginInstance::NPWrite(NPStream *stream, int32_t offset, int32_t len, void *buf)
{
    if( stream==0 ) {
        kDebug() << "FIXME: stream==0 in NSPluginInstance::NPWrite";
        return 0;
    }

    if( buf==0 ) {
        kDebug() << "FIXME: buf==0 in NSPluginInstance::NPWrite";
        return 0;
    }

    if (!_pluginFuncs.write)
        return 0;

    return _pluginFuncs.write(_npp, stream, offset, len, buf);
}


int32_t NSPluginInstance::NPWriteReady(NPStream *stream)
{
    if( stream==0 ) {
        kDebug() << "FIXME: stream==0 in NSPluginInstance::NPWriteReady";
        return 0;
    }

    if (!_pluginFuncs.writeready)
        return 0;

    return _pluginFuncs.writeready(_npp, stream);
}


void NSPluginInstance::NPURLNotify(const QString &url, NPReason reason, void *notifyData)
{
   if (!_pluginFuncs.urlnotify)
      return;

   _pluginFuncs.urlnotify(_npp, url.toAscii(), reason, notifyData);
}


void NSPluginInstance::addTempFile(KTemporaryFile *tmpFile)
{
   _tempFiles.append(tmpFile);
}

static bool has_focus = false;

void NSPluginInstance::gotFocusIn()
{
  has_focus = true;
}

void NSPluginInstance::gotFocusOut()
{
  has_focus = false;
}

#include <dlfcn.h>
// Prevent plugins from polling the keyboard regardless of focus.
static int (*real_xquerykeymap)( Display*, char[32] ) = NULL;

extern "C" KDE_EXPORT
int XQueryKeymap( Display* dpy, char k[32] )
{
    if( real_xquerykeymap == NULL )
        real_xquerykeymap = (int (*)( Display*, char[32] )) dlsym( RTLD_NEXT, "XQueryKeymap" );
    if( has_focus )
        return real_xquerykeymap( dpy, k );
    memset( k, 0, 32 );
    return 1;
}


/***************************************************************************/

NSPluginViewer::NSPluginViewer( QObject *parent )
   : QObject( parent )
{
   (void) new ViewerAdaptor( this );
   QDBusConnection::sessionBus().registerObject( "/Viewer", this );

    QObject::connect(QDBusConnection::sessionBus().interface(),
                     SIGNAL(serviceOwnerChanged(const QString&, const QString&, const QString&)),
                     this, SLOT(appChanged( const QString&, const QString&, const QString&)));
}


NSPluginViewer::~NSPluginViewer()
{
   kDebug(1431) << "NSPluginViewer::~NSPluginViewer";
}


void NSPluginViewer::appChanged( const QString& id, const QString& oldOwner, const QString& newOwner) {
   Q_UNUSED(id);

   if ( oldOwner.isEmpty() || !newOwner.isEmpty() ) // only care about unregistering apps
        return;

   QMap<QString, NSPluginClass*>::iterator it = _classes.begin();
   const QMap<QString, NSPluginClass*>::iterator end = _classes.end();
   for ( ; it != end; ++it )
   {
      if (it.value()->app() == oldOwner) {
         it = _classes.erase(it);
      }
   }

   if (_classes.isEmpty()) {
      shutdown();
   }
}


void NSPluginViewer::shutdown()
{
   kDebug(1431) << "NSPluginViewer::shutdown";
   _classes.clear();
   qApp->quit();
}


QDBusObjectPath NSPluginViewer::newClass( const QString& plugin, const QString& senderId )
{
   kDebug(1431) << "NSPluginViewer::NewClass( " << plugin << ")";

   // search existing class
   NSPluginClass *cls = _classes.value( plugin );
   if ( !cls ) {
       // create new class
       cls = new NSPluginClass( plugin, this );
       cls->setApp(senderId.toLatin1());
       if ( cls->error() ) {
           kError(1431) << "Can't create plugin class" << endl;
           delete cls;
           return QDBusObjectPath("/null");
       }

       _classes.insert( plugin, cls );
   }

   return QDBusObjectPath(cls->objectName());
}


/****************************************************************************/

static int s_classCounter = 0;

bool NSPluginClass::s_initedGTK = false;

typedef void gtkInitFunc(int *argc, char ***argv);

NSPluginClass::NSPluginClass( const QString &library,
                              QObject *parent )
   : QObject( parent )
{
    (void) new ClassAdaptor( this );
    // The object name is used to store the dbus object path
    setObjectName( QLatin1String( "/Class_" ) + QString::number( ++s_classCounter ) );
    QDBusConnection::sessionBus().registerObject( objectName(), this );

    // initialize members
    _handle = new KLibrary(QFile::encodeName(library), KGlobal::mainComponent(), this);
    _libname = library;
    _constructed = false;
    _error = true;
    _NP_GetMIMEDescription = 0;
    _NP_Initialize = 0;
    _NP_Shutdown = 0;

    _timer = new QTimer( this );
    connect( _timer, SIGNAL(timeout()), SLOT(timer()) );

    // check lib handle
    if (!_handle->load()) {
        kDebug(1431) << "Could not dlopen " << library;
        return;
    }

    // get exported lib functions
    _NP_GetMIMEDescription = (NP_GetMIMEDescriptionUPP *)_handle->resolveFunction("NP_GetMIMEDescription");
    _NP_Initialize = (NP_InitializeUPP *)_handle->resolveFunction("NP_Initialize");
    _NP_Shutdown = (NP_ShutdownUPP *)_handle->resolveFunction("NP_Shutdown");

    // check for valid returned ptrs
    if (!_NP_GetMIMEDescription) {
        kDebug(1431) << "Could not get symbol NP_GetMIMEDescription";
        return;
    }

    if (!_NP_Initialize) {
        kDebug(1431) << "Could not get symbol NP_Initialize";
        return;
    }

    if (!_NP_Shutdown) {
        kDebug(1431) << "Could not get symbol NP_Shutdown";
        return;
    }

    // initialize plugin
    kDebug(1431) << "Plugin library " << library << " loaded!";

    // see if it uses gtk
    if (!s_initedGTK) {
        gtkInitFunc* gtkInit = (gtkInitFunc*)_handle->resolveFunction("gtk_init");
        if (gtkInit) {
            kDebug(1431) << "Calling gtk_init for the plugin";
            // Prevent gtk_init() from replacing the X error handlers, since the Gtk
            // handlers abort when they receive an X error, thus killing the viewer.
            int (*old_error_handler)(Display*,XErrorEvent*) = XSetErrorHandler(0);
            int (*old_io_error_handler)(Display*) = XSetIOErrorHandler(0);
            gtkInit(0, 0);
            XSetErrorHandler(old_error_handler);
            XSetIOErrorHandler(old_io_error_handler);
            s_initedGTK = true;
        }
    }

    _constructed = true;
    _error = initialize()!=NPERR_NO_ERROR;
}


NSPluginClass::~NSPluginClass()
{
    qDeleteAll( _instances );
    qDeleteAll( _trash );

    shutdown();
    if (_handle)
      _handle->unload();
}


void NSPluginClass::timer()
{
    // delete instances
    while ( !_trash.isEmpty() ) {
        NSPluginInstance *it = _trash.takeFirst();
        int i = _instances.indexOf(it);
        if ( i != -1 )
            delete _instances.takeAt(i);
	else // there should be no instansces in trash, which are not in _instances
    	    delete it;
    }
}


int NSPluginClass::initialize()
{
   kDebug(1431) << "NSPluginClass::Initialize()";

   if ( !_constructed )
      return NPERR_GENERIC_ERROR;

   // initialize nescape exported functions
   memset(&_pluginFuncs, 0, sizeof(_pluginFuncs));
   memset(&_nsFuncs, 0, sizeof(_nsFuncs));

   _pluginFuncs.size = sizeof(_pluginFuncs);
   _nsFuncs.size = sizeof(_nsFuncs);
   _nsFuncs.version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
   _nsFuncs.geturl = g_NPN_GetURL;
   _nsFuncs.posturl = g_NPN_PostURL;
   _nsFuncs.requestread = g_NPN_RequestRead;
   _nsFuncs.newstream = g_NPN_NewStream;
   _nsFuncs.write = g_NPN_Write;
   _nsFuncs.destroystream = g_NPN_DestroyStream;
   _nsFuncs.status = g_NPN_Status;
   _nsFuncs.uagent = g_NPN_UserAgent;
   _nsFuncs.memalloc = g_NPN_MemAlloc;
   _nsFuncs.memfree = g_NPN_MemFree;
   _nsFuncs.memflush = g_NPN_MemFlush;
   _nsFuncs.reloadplugins = g_NPN_ReloadPlugins;
   _nsFuncs.getJavaEnv = g_NPN_GetJavaEnv;
   _nsFuncs.getJavaPeer = g_NPN_GetJavaPeer;
   _nsFuncs.geturlnotify = g_NPN_GetURLNotify;
   _nsFuncs.posturlnotify = g_NPN_PostURLNotify;
   _nsFuncs.getvalue = g_NPN_GetValue;
   _nsFuncs.setvalue = g_NPN_SetValue;
   _nsFuncs.invalidaterect = g_NPN_InvalidateRect;
   _nsFuncs.invalidateregion = g_NPN_InvalidateRegion;
   _nsFuncs.forceredraw = g_NPN_ForceRedraw;

   ScriptExportEngine::fillInScriptingFunctions(&_nsFuncs);
   
   _nsFuncs.pushpopupsenabledstate = g_NPN_PushPopupsEnabledState;
   _nsFuncs.poppopupsenabledstate =  g_NPN_PopPopupsEnabledState;

   // initialize plugin
   NPError error = _NP_Initialize(&_nsFuncs, &_pluginFuncs);
   assert(_nsFuncs.size == sizeof(_nsFuncs));
   CHECK(Initialize,error);
}


QString NSPluginClass::getMIMEDescription()
{
   return _NP_GetMIMEDescription();
}


void NSPluginClass::shutdown()
{
    kDebug(1431) << "NSPluginClass::shutdown error=" << _error;
    if( _NP_Shutdown && !_error )
        _NP_Shutdown();
}


QDBusObjectPath NSPluginClass::newInstance( const QString &url, const QString &mimeType, bool embed,
                                    const QStringList &argn, const QStringList &argv,
                                    const QString &appId, const QString &callbackId, bool reload )
{
    kDebug(1431) << url << mimeType;

    if ( !_constructed )
        return QDBusObjectPath("/null");

    // Create plugin instance object
    NSPluginInstance *inst = new NSPluginInstance( &_pluginFuncs, _handle,
                                                    url, mimeType, argn, argv,
                                                    appId, callbackId, embed, this );
    if ( !inst->wasInitializedOK() ) {
        delete inst;
        return QDBusObjectPath("/null");
    }

    // create source stream
    if ( !url.isEmpty() )
        inst->requestURL( url, mimeType, QString(), 0, false, reload );

    _instances.append( inst );
    return QDBusObjectPath(inst->objectName());
}


void NSPluginClass::destroyInstance( NSPluginInstance* inst )
{
    // be sure we don't deal with a dangling pointer
    if ( NSPluginInstance::lastPluginInstance() == inst )
        NSPluginInstance::setLastPluginInstance(0);

    // mark for destruction
    _trash.append( inst );
    timer(); //_timer->start( 0, TRUE );
}


/****************************************************************************/

NSPluginStreamBase::NSPluginStreamBase( NSPluginInstance *instance )
   : QObject( instance ), _instance(instance), _stream(0), _tempFile(0L),
     _pos(0), _queuePos(0), _error(false)
{
   _informed = false;
}


NSPluginStreamBase::~NSPluginStreamBase()
{
   if (_stream) {
      _instance->NPDestroyStream( _stream, NPRES_USER_BREAK );
      if (_stream && _stream->url)
          free(const_cast<char*>(_stream->url));
      delete _stream;
      _stream = 0;
   }

   delete _tempFile;
   _tempFile = 0;
}


void NSPluginStreamBase::stop()
{
    finish( true );
}

void NSPluginStreamBase::inform()
{

    if (! _informed)
    {
        KUrl src(_url);

        _informed = true;

        // inform the plugin
        _instance->NPNewStream( _mimeType.isEmpty() ? (char *) "text/plain" :  (char*)_mimeType.data(),
                    _stream, false, &_streamType );
        kDebug(1431) << "NewStream stype=" << _streamType << " url=" << _url << " mime=" << _mimeType;

        // prepare data transfer
        _tempFile = 0L;

        if ( _streamType==NP_ASFILE || _streamType==NP_ASFILEONLY ) {
            _onlyAsFile = _streamType==NP_ASFILEONLY;
            if ( KUrl(_url).isLocalFile() )  {
                kDebug(1431) << "local file";
                // local file can be passed directly
                _fileURL = KUrl(_url).toLocalFile();

                // without streaming stream is finished already
                if ( _onlyAsFile ) {
                    kDebug() << "local file AS_FILE_ONLY";
                    finish( false );
                }
            } else {
                kDebug() << "remote file";

                // stream into temporary file (use lower() in case the
                // filename as an upper case X in it)
                _tempFile = new KTemporaryFile;
                _tempFile->open();
                _fileURL = _tempFile->fileName();
                kDebug() << "saving into " << _fileURL;
            }
        }
    }

}

bool NSPluginStreamBase::create( const QString& url, const QString& mimeType, void *notify, bool forceNotify)
{
    if ( _stream )
        return false;

    _url = url;
    _notifyData = notify;
    _pos = 0;
    _tries = 0;
    _onlyAsFile = false;
    _streamType = NP_NORMAL;
    _informed = false;
    _forceNotify = forceNotify;

    // create new stream
    _stream = new NPStream;
    _stream->ndata = this;
    _stream->url = strdup(url.toAscii());
    _stream->end = 0;
    _stream->pdata = 0;
    _stream->lastmodified = 0;
    _stream->notifyData = _notifyData;

    _mimeType = mimeType;

    return true;
}

void NSPluginStreamBase::updateURL( const KUrl& newURL )
{
    _url = newURL;
    free(const_cast<char*>(_stream->url));
    _stream->url = strdup(_url.url().toLatin1().data());
}

int NSPluginStreamBase::process( const QByteArray &data, int start )
{
   int32_t max, sent, to_sent, len;
#ifdef __GNUC__
#warning added a const_cast
#endif
   char *d = const_cast<char*>(data.data()) + start;

   to_sent = data.size() - start;
   while (to_sent > 0)
   {
      inform();

      max = _instance->NPWriteReady(_stream);
      //kDebug(1431) << "to_sent == " << to_sent << " and max = " << max;
      len = qMin(max, to_sent);

      //kDebug(1431) << "-> Feeding stream to plugin: offset=" << _pos << ", len=" << len;
      sent = _instance->NPWrite( _stream, _pos, len, d );
      //kDebug(1431) << "<- Feeding stream: sent = " << sent;

      if (sent == 0) // interrupt the stream for a few ms
          break;

      if (sent < 0) {
          // stream data rejected/error
          kDebug(1431) << "stream data rejected/error";
          _error = true;
          break;
      }

      if (_tempFile) {
          _tempFile->write(d, sent);
      }

      to_sent -= sent;
      _pos += sent;
      d += sent;
   }

   return data.size() - to_sent;
}


bool NSPluginStreamBase::pump()
{
    //kDebug(1431) << "queue pos " << _queuePos << ", size " << _queue.size();

    inform();

    // Suspend until JS handled..
    if (_instance->hasPendingJSRequests())
       return false;

    if ( _queuePos<_queue.size() ) {
        int newPos;

        // handle AS_FILE_ONLY streams
        if ( _onlyAsFile ) {
            if (_tempFile) {
                _tempFile->write( _queue, _queue.size() );
	    }
            newPos = _queuePos+_queue.size();
        } else {
            // normal streams
            newPos = process( _queue, _queuePos );
        }

        // count tries
        if ( newPos==_queuePos )
            _tries++;
        else
            _tries = 0;

        _queuePos = newPos;
    }

    // return true if queue finished
    return _queuePos>=_queue.size();
}


void NSPluginStreamBase::queue( const QByteArray &data )
{
    _queue = data;
    _queue.detach();
    _queuePos = 0;
    _tries = 0;

/*
    kDebug(1431) << "new queue size=" << data.size()
                  << " data=" << (void*)data.data()
                  << " queue=" << (void*)_queue.data() << " qsize="
                  << _queue.size() << endl;
*/
}


void NSPluginStreamBase::finish( bool err )
{
    kDebug(1431) << "finish error=" << err;

    _queue.resize( 0 );
    _pos = 0;
    _queuePos = 0;

    inform();

    if ( !err ) {
        if ( _tempFile ) {
            _tempFile->close();
            _instance->addTempFile( _tempFile );
            _tempFile = 0;
        }

        if ( !_fileURL.isEmpty() ) {
            kDebug() << "stream as file " << _fileURL;
             _instance->NPStreamAsFile( _stream, _fileURL.toAscii() );
        }

        _instance->NPDestroyStream( _stream, NPRES_DONE );
        if (_notifyData || _forceNotify)
            _instance->NPURLNotify( _url.url(), NPRES_DONE, _notifyData );
    } else {
        // close temp file
        if ( _tempFile ) {
            _tempFile->close();
	}

        // destroy stream
        _instance->NPDestroyStream( _stream, NPRES_NETWORK_ERR );
        if (_notifyData || _forceNotify)
            _instance->NPURLNotify( _url.url(), NPRES_NETWORK_ERR, _notifyData );
    }

    // delete stream
    if (_stream && _stream->url)
        free(const_cast<char *>(_stream->url));
    delete _stream;
    _stream = 0;

    // destroy NSPluginStream object
    emit finished( this );
}


/****************************************************************************/

NSPluginBufStream::NSPluginBufStream( class NSPluginInstance *instance )
    : NSPluginStreamBase( instance )
{
    _timer = new QTimer( this );
    connect( _timer, SIGNAL(timeout()), this, SLOT(timer()) );
}


NSPluginBufStream::~NSPluginBufStream()
{

}


bool NSPluginBufStream::get( const QString& url, const QString& mimeType,
                             const QByteArray &buf, void *notifyData,
                             bool singleShot )
{
    _singleShot = singleShot;
    if ( create( url, mimeType, notifyData ) ) {
        queue( buf );
        _timer->setSingleShot( true );
        _timer->start( 100 );
    }

    return false;
}


void NSPluginBufStream::timer()
{
    bool finished = pump();
    if ( _singleShot )
        finish( false );
    else {

        if ( !finished && tries()<=8 ) {
            _timer->setSingleShot( true );
            _timer->start( 100 );
        } else
            finish( error() || tries()>8 );
    }
}



/****************************************************************************/

NSPluginStream::NSPluginStream( NSPluginInstance *instance )
    : NSPluginStreamBase( instance ), _job(0)
{
   _resumeTimer = new QTimer( this );
   connect(_resumeTimer, SIGNAL(timeout()), this, SLOT(resume()));
}


NSPluginStream::~NSPluginStream()
{
    if ( _job )
        _job->kill( KJob::Quietly );
}


bool NSPluginStream::get( const QString& url, const QString& mimeType,
                          void *notify, bool reload )
{
    // create new stream
    if ( create( url, mimeType, notify ) ) {
        // start the kio job
        _job = KIO::get(KUrl( url ), KIO::NoReload, KIO::HideProgressInfo);
        _job->addMetaData("errorPage", "false");
        _job->addMetaData("AllowCompressedPage", "false");
        if (reload) {
            _job->addMetaData("cache", "reload");
        }
        connect(_job, SIGNAL(data(KIO::Job *, const QByteArray &)),
                SLOT(data(KIO::Job *, const QByteArray &)));
        connect(_job, SIGNAL(result(KJob *)), SLOT(result(KJob *)));
        connect(_job, SIGNAL(totalSize(KJob *, qulonglong )),
                SLOT(totalSize(KJob *, qulonglong)));
        connect(_job, SIGNAL(mimetype(KIO::Job *, const QString &)),
                SLOT(mimetype(KIO::Job *, const QString &)));
        connect(_job, SIGNAL(redirection(KIO::Job *, const KUrl&)),
                SLOT(redirection(KIO::Job *, const KUrl&)));
    }

    return false;
}


bool NSPluginStream::post( const QString& url, const QByteArray& data,
                           const QString& mimeType, void *notify, const KParts::OpenUrlArguments& args,
                           const KParts::BrowserArguments& browserArgs )
{
    Q_UNUSED( args )
    // create new stream
    if ( create( url, mimeType, notify ) ) {
        // start the kio job
        _job = KIO::http_post(KUrl( url ), data, KIO::HideProgressInfo);
        _job->addMetaData("content-type", browserArgs.contentType());
        _job->addMetaData("errorPage", "false");
        _job->addMetaData("AllowCompressedPage", "false");
        connect(_job, SIGNAL(data(KIO::Job *, const QByteArray &)),
                SLOT(data(KIO::Job *, const QByteArray &)));
        connect(_job, SIGNAL(result(KJob *)), SLOT(result(KJob *)));
        connect(_job, SIGNAL(totalSize(KJob *, qulonglong )),
                SLOT(totalSize(KJob *, qulonglong)));
        connect(_job, SIGNAL(mimetype(KIO::Job *, const QString &)),
                SLOT(mimetype(KIO::Job *, const QString &)));
        connect(_job, SIGNAL(redirection(KIO::Job *, const KUrl&)),
                SLOT(redirection(KIO::Job *, const KUrl&)));
    }

    return false;
}


void NSPluginStream::data(KIO::Job*, const QByteArray &data)
{
    //kDebug(1431) << "NSPluginStream::data - job=" << (void*)job << " data size=" << data.size();
    queue( data );
    if ( !pump() ) {
        _job->suspend();
        _resumeTimer->setSingleShot( true );
        _resumeTimer->start( 100 );
    }
}

void NSPluginStream::redirection(KIO::Job * /*job*/, const KUrl& url)
{
    updateURL(url);
}

void NSPluginStream::totalSize(KJob * job, qulonglong size)
{
    kDebug(1431) << "NSPluginStream::totalSize - job=" << (void*)job << " size=" << KIO::number(size);
    _stream->end = size;
}

void NSPluginStream::mimetype(KIO::Job * job, const QString &mimeType)
{
    kDebug(1431) << "NSPluginStream::QByteArray - job=" << (void*)job << " mimeType=" << mimeType;
    _mimeType = mimeType;
}




void NSPluginStream::resume()
{
   if ( error() || tries()>8 ) {
       _job->kill( KJob::Quietly );
       finish( true );
       return;
   }

   if ( pump() ) {
      kDebug(1431) << "resume job";
      _job->resume();
   } else {
       kDebug(1431) << "restart timer";
       _resumeTimer->setSingleShot( true );
       _resumeTimer->start( 100 );
   }
}


void NSPluginStream::result(KJob *job)
{
   int err = job->error();
   _job = 0;
   finish( err!=0 || error() );
}

#include "nsplugin.moc"
// vim: ts=4 sw=4 et
// kate: indent-width 4; replace-tabs on; tab-width 4; space-indent on;
