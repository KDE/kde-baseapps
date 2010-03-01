/*
    This file is part of Konsole

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>
    Copyright 2009 by Thomas Dreibholz <dreibh@iem.uni-due.de>

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "Session.h"

// Standard
#include <assert.h>
#include <stdlib.h>
#include <signal.h>

// Qt
#include <QtGui/QApplication>
#include <QtCore/QByteRef>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QStringList>
#include <QtDBus/QtDBus>
#include <QtCore/QDate>

// KDE
#include <KDebug>
#include <KLocale>
#include <KMessageBox>
#include <KNotification>
#include <KProcess>
#include <KRun>
#include <kshell.h>
#include <KStandardDirs>
#include <KPtyDevice>
#include <KUrl>

// Konsole
#include <config-konsole.h>
#include <sessionadaptor.h>

#include "ProcessInfo.h"
#include "Pty.h"
#include "TerminalDisplay.h"
#include "ShellCommand.h"
#include "Vt102Emulation.h"
#include "ZModemDialog.h"

using namespace Konsole;

int Session::lastSessionId = 0;

Session::Session(QObject* parent) :
   QObject(parent)
   , _shellProcess(0)
   , _emulation(0)
   , _monitorActivity(false)
   , _monitorSilence(false)
   , _notifiedActivity(false)
   , _autoClose(true)
   , _wantedClose(false)
   , _silenceSeconds(10)
   , _addToUtmp(true)  
   , _flowControl(true)
   , _fullScripting(false)
   , _sessionId(0)
   , _sessionProcessInfo(0)
   , _foregroundProcessInfo(0)
   , _foregroundPid(0)
   , _zmodemBusy(false)
   , _zmodemProc(0)
   , _zmodemProgress(0)
   , _hasDarkBackground(false)
{
    //prepare DBus communication
    new SessionAdaptor(this);
    _sessionId = ++lastSessionId;
    QDBusConnection::sessionBus().registerObject(QLatin1String("/Sessions/")+QString::number(_sessionId), this);

    //create emulation backend
    _emulation = new Vt102Emulation();

    connect( _emulation, SIGNAL( titleChanged( int, const QString & ) ),
           this, SLOT( setUserTitle( int, const QString & ) ) );
    connect( _emulation, SIGNAL( stateSet(int) ),
           this, SLOT( activityStateSet(int) ) );
    connect( _emulation, SIGNAL( zmodemDetected() ), this ,
            SLOT( fireZModemDetected() ) );
    connect( _emulation, SIGNAL( changeTabTextColorRequest( int ) ),
           this, SIGNAL( changeTabTextColorRequest( int ) ) );
    connect( _emulation, SIGNAL(profileChangeCommandReceived(const QString&)),
           this, SIGNAL( profileChangeCommandReceived(const QString&)) );
    connect( _emulation, SIGNAL(flowControlKeyPressed(bool)) , this, 
             SLOT(updateFlowControlState(bool)) );

    //create new teletype for I/O with shell process
    openTeletype(-1);

    //setup timer for monitoring session activity
    _monitorTimer = new QTimer(this);
    _monitorTimer->setSingleShot(true);
    connect(_monitorTimer, SIGNAL(timeout()), this, SLOT(monitorTimerDone()));
}

void Session::openTeletype(int fd)
{
    if (_shellProcess && isRunning())
    {
        kWarning() << "Attempted to open teletype in a running session.";
        return;
    }

    delete _shellProcess;

    if (fd < 0)
        _shellProcess = new Pty();
    else
        _shellProcess = new Pty(fd);

    _shellProcess->setUtf8Mode(_emulation->utf8());

    //connect teletype to emulation backend
    connect( _shellProcess,SIGNAL(receivedData(const char*,int)),this,
            SLOT(onReceiveBlock(const char*,int)) );
    connect( _emulation,SIGNAL(sendData(const char*,int)),_shellProcess,
            SLOT(sendData(const char*,int)) );
    connect( _emulation,SIGNAL(lockPtyRequest(bool)),_shellProcess,SLOT(lockPty(bool)) );
    connect( _emulation,SIGNAL(useUtf8Request(bool)),_shellProcess,SLOT(setUtf8Mode(bool)) );
    connect( _shellProcess,SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(done(int)) );
    connect( _emulation,SIGNAL(imageSizeChanged(int,int)),this,SLOT(updateWindowSize(int,int)) );
}

WId Session::windowId() const
{
    // Returns a window ID for this session which is used
    // to set the WINDOWID environment variable in the shell
    // process.
    //
    // Sessions can have multiple views or no views, which means
    // that a single ID is not always going to be accurate.
    //
    // If there are no views, the window ID is just 0.  If
    // there are multiple views, then the window ID for the
    // top-level window which contains the first view is
    // returned

    if ( _views.count() == 0 )
       return 0;
    else
    {
        QWidget* window = _views.first();

        Q_ASSERT( window );

        while ( window->parentWidget() != 0 )
            window = window->parentWidget();

        return window->winId();
    }
}

void Session::setDarkBackground(bool darkBackground)
{
    _hasDarkBackground = darkBackground;
}
bool Session::hasDarkBackground() const
{
    return _hasDarkBackground;
}
bool Session::isRunning() const
{
    return _shellProcess->state() == QProcess::Running;
}

void Session::setCodec(QTextCodec* codec)
{
    emulation()->setCodec(codec);
}

bool Session::setCodec(QByteArray name)
{
    QTextCodec *codec = QTextCodec::codecForName(name);
    if (codec) {
        setCodec(codec);
        return true;
    }
    return false;
}

QByteArray Session::codec()
{
    return _emulation->codec()->name();
}

void Session::setProgram(const QString& program)
{
    _program = ShellCommand::expand(program);
}
void Session::setInitialWorkingDirectory(const QString& dir)
{
    _initialWorkingDir = KShell::tildeExpand(ShellCommand::expand(dir));
}
void Session::setArguments(const QStringList& arguments)
{
    _arguments = ShellCommand::expand(arguments);
}

QString Session::currentWorkingDirectory()
{
    // only returned cached value
    if (_currentWorkingDir.isEmpty()) updateWorkingDirectory();
    return _currentWorkingDir;
}
ProcessInfo* Session::updateWorkingDirectory()
{
    ProcessInfo *process = getProcessInfo();
    _currentWorkingDir = process->validCurrentDir();
    return process;
}

QList<TerminalDisplay*> Session::views() const
{
    return _views;
}

void Session::addView(TerminalDisplay* widget)
{
     Q_ASSERT( !_views.contains(widget) );

    _views.append(widget);

    if ( _emulation != 0 )
    {
        // connect emulation - view signals and slots
        connect( widget , SIGNAL(keyPressedSignal(QKeyEvent*)) , _emulation ,
               SLOT(sendKeyEvent(QKeyEvent*)) );
        connect( widget , SIGNAL(mouseSignal(int,int,int,int)) , _emulation ,
               SLOT(sendMouseEvent(int,int,int,int)) );
        connect( widget , SIGNAL(sendStringToEmu(const char*)) , _emulation ,
               SLOT(sendString(const char*)) );

        // allow emulation to notify view when the foreground process
        // indicates whether or not it is interested in mouse signals
        connect( _emulation , SIGNAL(programUsesMouseChanged(bool)) , widget ,
               SLOT(setUsesMouse(bool)) );

        widget->setUsesMouse( _emulation->programUsesMouse() );

        widget->setScreenWindow(_emulation->createWindow());
    }

    //connect view signals and slots
    QObject::connect( widget ,SIGNAL(changedContentSizeSignal(int,int)),this,
                    SLOT(onViewSizeChange(int,int)));

    QObject::connect( widget ,SIGNAL(destroyed(QObject*)) , this ,
                    SLOT(viewDestroyed(QObject*)) );
}

void Session::viewDestroyed(QObject* view)
{
    TerminalDisplay* display = (TerminalDisplay*)view;

    Q_ASSERT( _views.contains(display) );

    removeView(display);
}

void Session::removeView(TerminalDisplay* widget)
{
    _views.removeAll(widget);

    disconnect(widget,0,this,0);

    if ( _emulation != 0 )
    {
        // disconnect
        //  - key presses signals from widget
        //  - mouse activity signals from widget
        //  - string sending signals from widget
        //
        //  ... and any other signals connected in addView()
        disconnect( widget, 0, _emulation, 0);

        // disconnect state change signals emitted by emulation
        disconnect( _emulation , 0 , widget , 0);
    }

    // close the session automatically when the last view is removed
    if ( _views.count() == 0 )
    {
        close();
    }
}

QString Session::checkProgram(const QString& program) const
{
  // Upon a KPty error, there is no description on what that error was...
  // Check to see if the given program is executable.
  QString exec = QFile::encodeName(program);

  if (exec.isEmpty())
      return QString();

  // if 'exec' is not specified, fall back to default shell.  if that 
  // is not set then fall back to /bin/sh
  if ( exec.isEmpty() )
      exec = qgetenv("SHELL");
  if ( exec.isEmpty() )
        exec = "/bin/sh";

  exec = KRun::binaryName(exec, false);
  exec = KShell::tildeExpand(exec);
  QString pexec = KGlobal::dirs()->findExe(exec);
  if ( pexec.isEmpty() ) 
  {
      kError() << i18n("Could not find binary: ") << exec;
    return QString();
  }

  return exec;
}

void Session::terminalWarning(const QString& message)
{
    static const QByteArray warningText = i18n("Warning: ").toLocal8Bit(); 
    QByteArray messageText = message.toLocal8Bit();

    static const char* redPenOn = "\033[1m\033[31m";
    static const char* redPenOff = "\033[0m";

    _emulation->receiveData(redPenOn,strlen(redPenOn));
    _emulation->receiveData("\n\r\n\r",4);
    _emulation->receiveData(warningText.constData(),strlen(warningText.constData()));
    _emulation->receiveData(messageText.constData(),strlen(messageText.constData()));
    _emulation->receiveData("\n\r\n\r",4);
    _emulation->receiveData(redPenOff,strlen(redPenOff));
}
void Session::run()
{
  //check that everything is in place to run the session
  if (_program.isEmpty())
  {
      kDebug() << "Session::run() - program to run not set.";
  }
  if (_arguments.isEmpty())
  {
      kDebug() << "Session::run() - no command line arguments specified.";
  }

  const int CHOICE_COUNT = 3;
  QString programs[CHOICE_COUNT] = {_program,qgetenv("SHELL"),"/bin/sh"};
  QString exec;
  int choice = 0;
  while (choice < CHOICE_COUNT)
  {
    exec = checkProgram(programs[choice]);
    if (exec.isEmpty())
        choice++;
    else
        break;
  }

  // if a program was specified via setProgram(), but it couldn't be found, print a warning
  if (choice != 0 && choice < CHOICE_COUNT && !_program.isEmpty())
  {
      terminalWarning(i18n("Could not find '%1', starting '%2' instead.  Please check your profile settings.",_program,exec)); 
  }
  // if none of the choices are available, print a warning
  else if (choice == CHOICE_COUNT)
  {
      terminalWarning(i18n("Could not find an interactive shell to start."));
      return;
  }
  
  // if no arguments are specified, fall back to program name
  QStringList arguments = _arguments.join(QChar(' ')).isEmpty() ?
                                                   QStringList() << exec : _arguments;

  QString dbusService = QDBusConnection::sessionBus().baseService();
  if (!_initialWorkingDir.isEmpty())
    _shellProcess->setWorkingDirectory(_initialWorkingDir);
  else
    _shellProcess->setWorkingDirectory(QDir::homePath());

  _shellProcess->setFlowControlEnabled(_flowControl);
  _shellProcess->setErase(_emulation->eraseChar());

  // this is not strictly accurate use of the COLORFGBG variable.  This does not
  // tell the terminal exactly which colors are being used, but instead approximates
  // the color scheme as "black on white" or "white on black" depending on whether
  // the background color is deemed dark or not
  QString backgroundColorHint = _hasDarkBackground ? "COLORFGBG=15;0" : "COLORFGBG=0;15";

  int result = _shellProcess->start(exec,
                                  arguments,
                                  _environment << backgroundColorHint,
                                  windowId(),
                                  _addToUtmp,
                                  dbusService,
                                  (QLatin1String("/Sessions/") +
                                   QString::number(_sessionId)));

  if (result < 0)
  {
      terminalWarning(i18n("Could not start program '%1' with arguments '%2'.", exec, arguments.join(" ")));
    return;
  }

  _shellProcess->setWriteable(false);  // We are reachable via kwrited.

  emit started();
}

void Session::setUserTitle( int what, const QString &caption )
{
    //set to true if anything is actually changed (eg. old _nameTitle != new _nameTitle )
    bool modified = false;

    if ((what == IconNameAndWindowTitle) || (what == WindowTitle)) 
    {
           if ( _userTitle != caption ) {
            _userTitle = caption;
            modified = true;
        }
    }

    if ((what == IconNameAndWindowTitle) || (what == IconName))
    {
        if ( _iconText != caption ) {
               _iconText = caption;
            modified = true;
        }
    }

    if (what == TextColor || what == BackgroundColor) 
    {
      QString colorString = caption.section(';',0,0);
      QColor color = QColor(colorString);
      if (color.isValid())
      {
          if (what == TextColor)
                  emit changeForegroundColorRequest(color);
          else
                  emit changeBackgroundColorRequest(color);
      }
    }

    if (what == SessionName) 
    {
        if ( _nameTitle != caption ) {
               setTitle(Session::NameRole,caption);
            return;
        }
    }

    if (what == 31) 
    {
       QString cwd=caption;
       cwd=cwd.replace( QRegExp("^~"), QDir::homePath() );
       emit openUrlRequest(cwd);
    }

    // change icon via \033]32;Icon\007
    if (what == 32) 
    { 
        if ( _iconName != caption ) {
               _iconName = caption;

            modified = true;
        }
    }

    if (what == ProfileChange) 
    {
        emit profileChangeCommandReceived(caption);
        return;
    }

    if ( modified )
        emit titleChanged();
}

QString Session::userTitle() const
{
    return _userTitle;
}
void Session::setTabTitleFormat(TabTitleContext context , const QString& format)
{
    if ( context == LocalTabTitle )
        _localTabTitleFormat = format;
    else if ( context == RemoteTabTitle )
        _remoteTabTitleFormat = format;
}
QString Session::tabTitleFormat(TabTitleContext context) const
{
    if ( context == LocalTabTitle )
        return _localTabTitleFormat;
    else if ( context == RemoteTabTitle )
        return _remoteTabTitleFormat;

    return QString();
}

void Session::monitorTimerDone()
{
  //FIXME: The idea here is that the notification popup will appear to tell the user than output from
  //the terminal has stopped and the popup will disappear when the user activates the session.
  //
  //This breaks with the addition of multiple views of a session.  The popup should disappear
  //when any of the views of the session becomes active
  

  //FIXME: Make message text for this notification and the activity notification more descriptive.    
  if (_monitorSilence) {
    KNotification::event("Silence", i18n("Silence in session '%1'", _nameTitle), QPixmap(),
                    QApplication::activeWindow(),
                    KNotification::CloseWhenWidgetActivated);
    emit stateChanged(NOTIFYSILENCE);
  }
  else
  {
    emit stateChanged(NOTIFYNORMAL);
  }

  _notifiedActivity=false;
}
void Session::updateFlowControlState(bool suspended)
{
    if (suspended)
    {
        if (flowControlEnabled())
        {
            foreach(TerminalDisplay* display,_views)
            {
                if (display->flowControlWarningEnabled())
                    display->outputSuspended(true);
            }
        }
    } 
    else
    {
        foreach(TerminalDisplay* display,_views)
            display->outputSuspended(false);
    }   
}
void Session::activityStateSet(int state)
{
  if (state==NOTIFYBELL)
  {
      emit bellRequest( i18n("Bell in session '%1'",_nameTitle) );
  }
  else if (state==NOTIFYACTIVITY)
  {
    if (_monitorSilence) {
      _monitorTimer->start(_silenceSeconds*1000);
    }

    if ( _monitorActivity ) {
      //FIXME:  See comments in Session::monitorTimerDone()
      if (!_notifiedActivity) {
        KNotification::event("Activity", i18n("Activity in session '%1'", _nameTitle), QPixmap(),
                        QApplication::activeWindow(),
        KNotification::CloseWhenWidgetActivated);
        _notifiedActivity=true;
      }
    }
  }

  if ( state==NOTIFYACTIVITY && !_monitorActivity )
          state = NOTIFYNORMAL;
  if ( state==NOTIFYSILENCE && !_monitorSilence )
          state = NOTIFYNORMAL;

  emit stateChanged(state);
}

void Session::onViewSizeChange(int /*height*/, int /*width*/)
{
  updateTerminalSize();
}

void Session::updateTerminalSize()
{
    QListIterator<TerminalDisplay*> viewIter(_views);

    int minLines = -1;
    int minColumns = -1;

    // minimum number of lines and columns that views require for
    // their size to be taken into consideration ( to avoid problems
    // with new view widgets which haven't yet been set to their correct size )
    const int VIEW_LINES_THRESHOLD = 2;
    const int VIEW_COLUMNS_THRESHOLD = 2;

    //select largest number of lines and columns that will fit in all visible views
    while ( viewIter.hasNext() )
    {
        TerminalDisplay* view = viewIter.next();
        if ( view->isHidden() == false &&
             view->lines() >= VIEW_LINES_THRESHOLD &&
             view->columns() >= VIEW_COLUMNS_THRESHOLD )
        {
            minLines = (minLines == -1) ? view->lines() : qMin( minLines , view->lines() );
            minColumns = (minColumns == -1) ? view->columns() : qMin( minColumns , view->columns() );
            view->processFilters();
        }
    }

    // backend emulation must have a _terminal of at least 1 column x 1 line in size
    if ( minLines > 0 && minColumns > 0 )
    {
        _emulation->setImageSize( minLines , minColumns );
    }
}
void Session::updateWindowSize(int lines, int columns)
{
    Q_ASSERT(lines > 0 && columns > 0);
    _shellProcess->setWindowSize(lines,columns);
}
void Session::refresh()
{
    // attempt to get the shell process to redraw the display
    //
    // this requires the program running in the shell
    // to cooperate by sending an update in response to
    // a window size change
    //
    // the window size is changed twice, first made slightly larger and then
    // resized back to its normal size so that there is actually a change
    // in the window size (some shells do nothing if the
    // new and old sizes are the same)
    //
    // if there is a more 'correct' way to do this, please
    // send an email with method or patches to konsole-devel@kde.org

    const QSize existingSize = _shellProcess->windowSize();
    _shellProcess->setWindowSize(existingSize.height(),existingSize.width()+1);
    _shellProcess->setWindowSize(existingSize.height(),existingSize.width());
}

bool Session::kill(int signal)
{
    int result = ::kill(_shellProcess->pid(),signal);    
    
    if ( result == 0 )
    {
        _shellProcess->waitForFinished();
        return true;
    }
    else
        return false;
}

void Session::close()
{
  _autoClose = true;
  _wantedClose = true;

  if (!isRunning() || !kill(SIGHUP))
  {
     if (isRunning())
     {
        kDebug() << "Process" << _shellProcess->pid() << "did not respond to SIGHUP";

        // close the pty and wait to see if the process finishes.  If it does,
        // the done() slot will have been called so we can return.  Otherwise,
        // emit the finished() signal regardless
        _shellProcess->pty()->close();
        if (_shellProcess->waitForFinished(3000))
            return;

        kWarning() << "Unable to kill process" << _shellProcess->pid();
     }

     // Forced close.
     QTimer::singleShot(1, this, SIGNAL(finished()));
  }
}

void Session::sendText(const QString &text) const
{
  _emulation->sendText(text);
}

void Session::sendMouseEvent(int buttons, int column, int line, int eventType)
{
    _emulation->sendMouseEvent(buttons, column, line, eventType);
}

Session::~Session()
{
  if (_foregroundProcessInfo)
      delete _foregroundProcessInfo;
  if (_sessionProcessInfo)
      delete _sessionProcessInfo;
  delete _emulation;
  delete _shellProcess;
  delete _zmodemProc;
}

void Session::done(int exitStatus)
{
  if (!_autoClose)
  {
    _userTitle = i18n("Finished");
    emit titleChanged();
    return;
  }

  QString message;
  if (!_wantedClose || exitStatus != 0)
  {
    if (_shellProcess->exitStatus() == QProcess::NormalExit)
        message = i18n("Program '%1' exited with status %2.", _program, exitStatus);
    else
        message = i18n("Program '%1' crashed.", _program);

    //FIXME: See comments in Session::monitorTimerDone()
    KNotification::event("Finished", message , QPixmap(),
                         QApplication::activeWindow(),
                         KNotification::CloseWhenWidgetActivated);
  }

  if ( !_wantedClose && _shellProcess->exitStatus() != QProcess::NormalExit )
      terminalWarning(message);
  else
        emit finished();
}

Emulation* Session::emulation() const
{
  return _emulation;
}

QString Session::keyBindings() const
{
  return _emulation->keyBindings();
}

QStringList Session::environment() const
{
  return _environment;
}

void Session::setEnvironment(const QStringList& environment)
{
    _environment = environment;
}

int Session::sessionId() const
{
  return _sessionId;
}

void Session::setKeyBindings(const QString &id)
{
  _emulation->setKeyBindings(id);
}

void Session::setTitle(TitleRole role , const QString& newTitle)
{
    if ( title(role) != newTitle )
    {
        if ( role == NameRole )
            _nameTitle = newTitle;
        else if ( role == DisplayedTitleRole )
            _displayTitle = newTitle;

        emit titleChanged();
    }
}

QString Session::title(TitleRole role) const
{
    if ( role == NameRole )
        return _nameTitle;
    else if ( role == DisplayedTitleRole )
        return _displayTitle;
    else
        return QString();
}

ProcessInfo* Session::getProcessInfo()
{
    ProcessInfo* process;

    if (isForegroundProcessActive())
        process = _foregroundProcessInfo;
    else
    {
        updateSessionProcessInfo();
        process = _sessionProcessInfo;
    }

    return process;
}

void Session::updateSessionProcessInfo()
{
    Q_ASSERT(_shellProcess);
    if (!_sessionProcessInfo)
        _sessionProcessInfo = ProcessInfo::newInstance(processId());
    _sessionProcessInfo->update();
}

bool Session::updateForegroundProcessInfo()
{
    bool valid = (_foregroundProcessInfo != 0);

    // has foreground process changed?
    Q_ASSERT(_shellProcess);
    int pid = _shellProcess->foregroundProcessGroup();
    if (pid != _foregroundPid)
    {
        if (valid)
            delete _foregroundProcessInfo;
        _foregroundProcessInfo = ProcessInfo::newInstance(pid);
        _foregroundPid = pid;
        valid = true;
    }

    if (valid)
    {
        _foregroundProcessInfo->update();
        valid = _foregroundProcessInfo->isValid();
    }

    return valid;
}

QString Session::getDynamicTitle()
{
    // update current directory from process
    ProcessInfo* process = updateWorkingDirectory();

    // format tab titles using process info
    bool ok = false;
    QString title;
    if ( process->name(&ok) == "ssh" && ok )
    {
        SSHProcessInfo sshInfo(*process);
        title = sshInfo.format(tabTitleFormat(Session::RemoteTabTitle));
    }
    else
        title = process->format(tabTitleFormat(Session::LocalTabTitle));

    return title;
}

KUrl Session::getUrl()
{
    QString path;
    
    updateSessionProcessInfo();
    if (_sessionProcessInfo->isValid())
    {
        bool ok = false;

        // check if foreground process is bookmark-able
        if (isForegroundProcessActive())
        {
            // for remote connections, save the user and host
            // bright ideas to get the directory at the other end are welcome :)
            if (_foregroundProcessInfo->name(&ok) == "ssh" && ok)
            {
                SSHProcessInfo sshInfo(*_foregroundProcessInfo);
                path = "ssh://" + sshInfo.userName() + '@' + sshInfo.host();
            }
            else
            {
                path = _foregroundProcessInfo->currentDir(&ok);
                if (!ok)
                    path.clear();
            }
        }
        else // otherwise use the current working directory of the shell process
        {
            path = _sessionProcessInfo->currentDir(&ok);
            if (!ok)
                path.clear();
        }
    }

    return KUrl(path);
}

void Session::setIconName(const QString& iconName)
{
    if ( iconName != _iconName )
    {
        _iconName = iconName;
        emit titleChanged();
    }
}

void Session::setIconText(const QString& iconText)
{
  _iconText = iconText;
}

QString Session::iconName() const
{
  return _iconName;
}

QString Session::iconText() const
{
  return _iconText;
}

void Session::setHistoryType(const HistoryType &hType)
{
  _emulation->setHistory(hType);
}

const HistoryType& Session::historyType() const
{
  return _emulation->history();
}

void Session::clearHistory()
{
    _emulation->clearHistory();
}

QStringList Session::arguments() const
{
  return _arguments;
}

QString Session::program() const
{
  return _program;
}

// unused currently
bool Session::isMonitorActivity() const { return _monitorActivity; }
// unused currently
bool Session::isMonitorSilence()  const { return _monitorSilence; }

void Session::setMonitorActivity(bool _monitor)
{
  _monitorActivity=_monitor;
  _notifiedActivity=false;

  activityStateSet(NOTIFYNORMAL);
}

void Session::setMonitorSilence(bool _monitor)
{
  if (_monitorSilence==_monitor)
    return;

  _monitorSilence=_monitor;
  if (_monitorSilence)
  {
    _monitorTimer->start(_silenceSeconds*1000);
  }
  else
    _monitorTimer->stop();

  activityStateSet(NOTIFYNORMAL);
}

void Session::setMonitorSilenceSeconds(int seconds)
{
  _silenceSeconds=seconds;
  if (_monitorSilence) {
    _monitorTimer->start(_silenceSeconds*1000);
  }
}

void Session::setAddToUtmp(bool set)
{
  _addToUtmp = set;
}

void Session::setFlowControlEnabled(bool enabled)
{
  _flowControl = enabled;

  if (_shellProcess)  
    _shellProcess->setFlowControlEnabled(_flowControl);
  emit flowControlEnabledChanged(enabled);
}
bool Session::flowControlEnabled() const
{
    if (_shellProcess)
            return _shellProcess->flowControlEnabled();
    else
            return _flowControl;
}
void Session::fireZModemDetected()
{
  if (!_zmodemBusy)
  {
    QTimer::singleShot(10, this, SIGNAL(zmodemDetected()));
    _zmodemBusy = true;
  }
}

void Session::cancelZModem()
{
  _shellProcess->sendData("\030\030\030\030", 4); // Abort
  _zmodemBusy = false;
}

void Session::startZModem(const QString &zmodem, const QString &dir, const QStringList &list)
{
  _zmodemBusy = true;
  _zmodemProc = new KProcess();
  _zmodemProc->setOutputChannelMode( KProcess::SeparateChannels );

  *_zmodemProc << zmodem << "-v" << list;

  if (!dir.isEmpty())
     _zmodemProc->setWorkingDirectory(dir);

  connect(_zmodemProc,SIGNAL (readyReadStandardOutput()),
          this, SLOT(zmodemReadAndSendBlock()));
  connect(_zmodemProc,SIGNAL (readyReadStandardError()),
          this, SLOT(zmodemReadStatus()));
  connect(_zmodemProc,SIGNAL (finished(int,QProcess::ExitStatus)),
          this, SLOT(zmodemFinished()));

  _zmodemProc->start();
  
  disconnect( _shellProcess,SIGNAL(receivedData(const char*,int)), this, SLOT(onReceiveBlock(const char*,int)) );
  connect( _shellProcess,SIGNAL(receivedData(const char*,int)), this, SLOT(zmodemRcvBlock(const char*,int)) );

  _zmodemProgress = new ZModemDialog(QApplication::activeWindow(), false,
                                    i18n("ZModem Progress"));

  connect(_zmodemProgress, SIGNAL(user1Clicked()),
          this, SLOT(zmodemFinished()));

  _zmodemProgress->show();
}

void Session::zmodemReadAndSendBlock()
{
  _zmodemProc->setReadChannel( QProcess::StandardOutput );
  QByteArray data = _zmodemProc->readAll();

  if ( data.count() == 0 )
      return;

  _shellProcess->sendData(data.constData(),data.count());
}

void Session::zmodemReadStatus()
{
  _zmodemProc->setReadChannel( QProcess::StandardError );
  QByteArray msg = _zmodemProc->readAll();
  while(!msg.isEmpty())
  {
     int i = msg.indexOf('\015');
     int j = msg.indexOf('\012');
     QByteArray txt;
     if ((i != -1) && ((j == -1) || (i < j)))
     {
       msg = msg.mid(i+1);
     }
     else if (j != -1)
     {
       txt = msg.left(j);
       msg = msg.mid(j+1);
     }
     else
     {
       txt = msg;
       msg.truncate(0);
     }
     if (!txt.isEmpty())
       _zmodemProgress->addProgressText(QString::fromLocal8Bit(txt));
  }
}

void Session::zmodemRcvBlock(const char *data, int len)
{
  QByteArray ba( data, len );

  _zmodemProc->write( ba );
}

void Session::zmodemFinished()
{
  /* zmodemFinished() is called by QProcess's finished() and
      ZModemDialog's user1Clicked(). Therefore, an invocation by
      user1Clicked() will recursively invoke this function again
      when the KProcess is deleted! */
  if (_zmodemProc) {
    KProcess* process = _zmodemProc;
    _zmodemProc = 0;   // Set _zmodemProc to 0 avoid recursive invocations!
    _zmodemBusy = false;
    delete process;    // Now, the KProcess may be disposed safely.

    disconnect( _shellProcess,SIGNAL(receivedData(const char*,int)), this ,SLOT(zmodemRcvBlock(const char*,int)) );
    connect( _shellProcess,SIGNAL(receivedData(const char*,int)), this, SLOT(onReceiveBlock(const char*,int)) );

    _shellProcess->sendData("\030\030\030\030", 4); // Abort
    _shellProcess->sendData("\001\013\n", 3); // Try to get prompt back
    _zmodemProgress->transferDone();
  }
}

void Session::onReceiveBlock( const char* buf, int len )
{
    _emulation->receiveData( buf, len );
    emit receivedData( QString::fromLatin1( buf, len ) );
}

QSize Session::size()
{
  return _emulation->imageSize();
}

void Session::setSize(const QSize& size)
{
  if ((size.width() <= 1) || (size.height() <= 1))
     return;

  emit resizeRequest(size);
}
int Session::processId() const
{
    return _shellProcess->pid();
}

void Session::setTitle(int role , const QString& title)
{
    switch (role) {
    case (0):
        this->setTitle(Session::NameRole, title);
        break;
    case (1):
        this->setTitle(Session::DisplayedTitleRole, title);
        break;
    }
}

QString Session::title(int role) const
{
    switch (role) {
    case (0):
        return this->title(Session::NameRole);
    case (1):
        return this->title(Session::DisplayedTitleRole);
    default:
        return QString();
    }
}

int Session::foregroundProcessId()
{
    int pid;

    bool ok = false;
    pid = getProcessInfo()->pid(&ok);
    if (!ok)
        pid = -1;

    return pid;
}

bool Session::isForegroundProcessActive()
{
    // foreground process info is always updated after this
    return updateForegroundProcessInfo() && (processId() != _foregroundPid);
}

QString Session::foregroundProcessName()
{
    QString name;

    if (updateForegroundProcessInfo()) 
    {
        bool ok = false;
        name = _foregroundProcessInfo->name(&ok);
        if (!ok)
            name.clear();
    }

    return name;
}

void Session::saveSession(KConfigGroup& group)
{
    group.writePathEntry("WorkingDir", currentWorkingDirectory());
    group.writeEntry("LocalTab",       tabTitleFormat(LocalTabTitle));
    group.writeEntry("RemoteTab",      tabTitleFormat(RemoteTabTitle));
}

void Session::restoreSession(KConfigGroup& group)
{
    QString value;

    value = group.readPathEntry("WorkingDir", QString());
    if (!value.isEmpty()) setInitialWorkingDirectory(value);
    value = group.readEntry("LocalTab");
    if (!value.isEmpty()) setTabTitleFormat(LocalTabTitle, value);
    value = group.readEntry("RemoteTab");
    if (!value.isEmpty()) setTabTitleFormat(RemoteTabTitle, value);
}

SessionGroup::SessionGroup(QObject* parent)
    : QObject(parent), _masterMode(0)
{
}
SessionGroup::~SessionGroup()
{
}
int SessionGroup::masterMode() const { return _masterMode; }
QList<Session*> SessionGroup::sessions() const { return _sessions.keys(); }
bool SessionGroup::masterStatus(Session* session) const { return _sessions[session]; }

void SessionGroup::addSession(Session* session)
{
    connect(session,SIGNAL(finished()),this,SLOT(sessionFinished()));
    _sessions.insert(session,false);
}
void SessionGroup::removeSession(Session* session)
{
    disconnect(session,SIGNAL(finished()),this,SLOT(sessionFinished()));
    setMasterStatus(session,false);
    _sessions.remove(session);
}
void SessionGroup::sessionFinished()
{
    Session* session = qobject_cast<Session*>(sender());
    Q_ASSERT(session);
    removeSession(session);
}
void SessionGroup::setMasterMode(int mode)
{
   _masterMode = mode;
}
QList<Session*> SessionGroup::masters() const
{
    return _sessions.keys(true);
}
void SessionGroup::setMasterStatus(Session* session , bool master)
{
    const bool wasMaster = _sessions[session];

    if (wasMaster == master) {
        // No status change -> nothing to do.
        return;
    }
    _sessions[session] = master;

    if(master) {
        connect( session->emulation() , SIGNAL(sendData(const char*,int)) , this,
                 SLOT(forwardData(const char*,int)) );
    }
    else {
        disconnect( session->emulation() , SIGNAL(sendData(const char*,int)) , this,
                    SLOT(forwardData(const char*,int)) );
    }
}
void SessionGroup::forwardData(const char* data, int size)
{
    static bool _inForwardData = false;
    if(_inForwardData) {   // Avoid recursive calls among session groups!
       // A recursive call happens when a master in group A calls forwardData()
       // in group B. If one of the destination sessions in group B is also a
       // master of a group including the master session of group A, this would
       // again call forwardData() in group A, and so on.
       return;
    }
    
    _inForwardData = true;
    QListIterator<Session*> iter(_sessions.keys());
    while(iter.hasNext()) {
        Session* other = iter.next();
        if(!_sessions[other]) {
           other->emulation()->sendString(data, size);
        }
    }
    _inForwardData = false;
}

#include "Session.moc"

