/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.countm>

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
#include "ProcessInfo.h"

// Unix
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pwd.h>

// Qt
#include <KDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QRegExp>
#include <QtCore/QTextStream>
#include <QtCore/QStringList>

// KDE
#include <KConfigGroup>
#include <KGlobal>
#include <KSharedConfig>

using namespace Konsole;

ProcessInfo::ProcessInfo(int pid , bool enableEnvironmentRead)
    : _fields( ARGUMENTS | ENVIRONMENT ) // arguments and environments
                                         // are currently always valid,
                                         // they just return an empty
                                         // vector / map respectively
                                         // if no arguments
                                         // or environment bindings
                                         // have been explicitly set
    , _enableEnvironmentRead(enableEnvironmentRead)
    , _pid(pid)
    , _parentPid(0)
    , _foregroundPid(0)
    , _userId(0)
    , _lastError(NoError)
    , _userName(QString())
{
}

ProcessInfo::Error ProcessInfo::error() const { return _lastError; }
void ProcessInfo::setError(Error error) { _lastError = error; }

void ProcessInfo::update() 
{
    readProcessInfo(_pid,_enableEnvironmentRead);
}

QString ProcessInfo::validCurrentDir() const
{
   bool ok = false;

   // read current dir, if an error occurs try the parent as the next
   // best option
   int currentPid = parentPid(&ok);
   QString dir = currentDir(&ok);
   while ( !ok && currentPid != 0 )
   {
       ProcessInfo* current = ProcessInfo::newInstance(currentPid);
       current->update();
       currentPid = current->parentPid(&ok); 
       dir = current->currentDir(&ok);
       delete current;
   }

   return dir;
}

QString ProcessInfo::format(const QString& input) const
{
   bool ok = false;

   QString output(input);

   // search for and replace known marker
   output.replace("%u",userName());
   output.replace("%n",name(&ok));
   output.replace("%c",formatCommand(name(&ok),arguments(&ok),ShortCommandFormat));
   output.replace("%C",formatCommand(name(&ok),arguments(&ok),LongCommandFormat));
   
   QString dir = validCurrentDir();
   output.replace("%D",dir);
   output.replace("%d",formatShortDir(dir));
   
   // remove any remaining %[LETTER] sequences
   // output.replace(QRegExp("%\\w"), QString());

   return output;
}

QString ProcessInfo::formatCommand(const QString& name, 
                                   const QVector<QString>& arguments,
                                   CommandFormat format) const
{
    // TODO Implement me
    return QStringList(QList<QString>::fromVector(arguments)).join(" ");
}

QSet<QString> ProcessInfo::_commonDirNames;

QSet<QString> ProcessInfo::commonDirNames() 
{
    if ( _commonDirNames.isEmpty() )
    {
        KSharedConfigPtr config = KGlobal::config();
        KConfigGroup configGroup = config->group("ProcessInfo");

        QStringList defaults = QStringList() 
                             << "src" << "build" << "debug" << "release" 
                             << "bin" << "lib"   << "libs"  << "tmp" 
                             << "doc" << "docs"  << "data"  << "share"
                             << "examples" << "icons" << "pics" << "plugins" 
                             << "tests" << "media" << "l10n" << "include" 
                             << "includes" << "locale" << "ui";

        _commonDirNames = QSet<QString>::fromList(configGroup.readEntry("CommonDirNames",defaults));

    }

    return _commonDirNames;
}

QString ProcessInfo::formatShortDir(const QString& input) const
{
    QString result;

    QStringList parts = input.split( QDir::separator() );

    // temporarily hard-coded
    QSet<QString> dirNamesToShorten = commonDirNames();

    QListIterator<QString> iter(parts);
    iter.toBack();

    // go backwards through the list of the path's parts
    // adding abbreviations of common directory names
    // and stopping when we reach a dir name which is not
    // in the commonDirNames set
    while ( iter.hasPrevious() )
    {
        QString part = iter.previous();

        if ( dirNamesToShorten.contains(part) )
        {
            result.prepend(QDir::separator() + part[0]);
        }
        else
        {
            result.prepend(part);
            break;
        }
    }

    return result;
}

QVector<QString> ProcessInfo::arguments(bool* ok) const
{
    *ok = _fields & ARGUMENTS;

    return _arguments;
}

QMap<QString,QString> ProcessInfo::environment(bool* ok) const
{
    *ok = _fields & ENVIRONMENT;

    return _environment;
}

bool ProcessInfo::isValid() const
{
    return _fields & PROCESS_ID;
}

int ProcessInfo::pid(bool* ok) const
{
    *ok = _fields & PROCESS_ID;

    return _pid;
}

int ProcessInfo::parentPid(bool* ok) const
{
    *ok = _fields & PARENT_PID;

    return _parentPid;
}

int ProcessInfo::foregroundPid(bool* ok) const
{
    *ok = _fields & FOREGROUND_PID;

    return _foregroundPid;
}

QString ProcessInfo::name(bool* ok) const
{
    *ok = _fields & NAME;

    return _name;
}

int ProcessInfo::userId(bool* ok) const
{
    *ok = _fields & UID;

    return _userId;
}

QString ProcessInfo::userName() const
{
    return _userName;
}

void ProcessInfo::setPid(int pid)
{
    _pid = pid;
    _fields |= PROCESS_ID;
}

void ProcessInfo::setUserId(int uid)
{
    _userId = uid;
    _fields |= UID;
}

void ProcessInfo::setUserName(const QString& name)
{
    _userName = name;
}


void ProcessInfo::setParentPid(int pid)
{
    _parentPid = pid;
    _fields |= PARENT_PID;
}
void ProcessInfo::setForegroundPid(int pid)
{
    _foregroundPid = pid;
    _fields |= FOREGROUND_PID;
}

QString ProcessInfo::currentDir(bool* ok) const
{
    if (ok)
        *ok = _fields & CURRENT_DIR;

    return _currentDir;
}
void ProcessInfo::setCurrentDir(const QString& dir)
{
    _fields |= CURRENT_DIR;
    _currentDir = dir;
}

void ProcessInfo::setName(const QString& name)
{
    _name = name;
    _fields |= NAME;
}
void ProcessInfo::addArgument(const QString& argument)
{
    _arguments << argument;    
}

void ProcessInfo::addEnvironmentBinding(const QString& name , const QString& value)
{
    _environment.insert(name,value);
}

void ProcessInfo::setFileError( QFile::FileError error )
{
    switch ( error )
    {
        case PermissionsError:
            setError( PermissionsError );
            break;
        case NoError:
            setError( NoError );
            break;
        default:
            setError( UnknownError );
    }
}

//
// ProcessInfo::newInstance() is way at the bottom so it can see all of the
// implementations of the UnixProcessInfo abstract class.
//

NullProcessInfo::NullProcessInfo(int pid,bool enableEnvironmentRead)
    : ProcessInfo(pid,enableEnvironmentRead)
{
}

bool NullProcessInfo::readProcessInfo(int /*pid*/ , bool /*enableEnvironmentRead*/)
{
    return false;
}

void NullProcessInfo::readUserName()
{
}

UnixProcessInfo::UnixProcessInfo(int pid,bool enableEnvironmentRead)
    : ProcessInfo(pid,enableEnvironmentRead)
{
}

bool UnixProcessInfo::readProcessInfo(int pid , bool enableEnvironmentRead)
{
    bool ok = readProcInfo(pid);
    if (ok)
    {
        ok |= readArguments(pid);
        ok |= readCurrentDir(pid);
        if ( enableEnvironmentRead )
        {
            ok |= readEnvironment(pid);
        }
    }
    return ok;
}

void UnixProcessInfo::readUserName()
{
    bool ok = false;
    int uid = userId(&ok);
    if (!ok) return;

    struct passwd passwdStruct;
    struct passwd *getpwResult;
    char *getpwBuffer;
    long getpwBufferSize;
    int getpwStatus;

    getpwBufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (getpwBufferSize == -1)
        getpwBufferSize = 16384;

    getpwBuffer = new char[getpwBufferSize];
    if (getpwBuffer == NULL)
        return;
    getpwStatus = getpwuid_r(uid, &passwdStruct, getpwBuffer, getpwBufferSize, &getpwResult);
    if (getpwResult != NULL)
        setUserName(QString(passwdStruct.pw_name));
    else
        setUserName(QString());
    delete [] getpwBuffer;
}


class LinuxProcessInfo : public UnixProcessInfo
{
public:
    LinuxProcessInfo(int pid, bool env) :
        UnixProcessInfo(pid,env)
    {
    }

private:
    virtual bool readProcInfo(int pid)
    {
        // indicies of various fields within the process status file which
        // contain various information about the process
        const int PARENT_PID_FIELD = 3;
        const int PROCESS_NAME_FIELD = 1;
        const int GROUP_PROCESS_FIELD = 7;

        QString parentPidString;
        QString processNameString;
        QString foregroundPidString;
        QString uidLine;
        QString uidString;
        QStringList uidStrings;

        // For user id read process status file ( /proc/<pid>/status )
        //  Can not use getuid() due to it does not work for 'su'
        QFile statusInfo( QString("/proc/%1/status").arg(pid) );
        if ( statusInfo.open(QIODevice::ReadOnly) )
        {
            QTextStream stream(&statusInfo);
            QString statusLine;
            do {
                statusLine = stream.readLine(0);
                if (statusLine.startsWith("Uid:"))
                    uidLine = statusLine;
            } while (!statusLine.isNull() && uidLine.isNull());

            uidStrings << uidLine.split('\t', QString::SkipEmptyParts);
            // Must be 5 entries: 'Uid: %d %d %d %d' and
            // uid string must be less than 5 chars (uint)
            if (uidStrings.size() == 5)
                uidString = uidStrings[1];
            if (uidString.size() > 5)
                uidString.clear();

            bool ok = false;
            int uid = uidString.toInt(&ok);
            if (ok)
                setUserId(uid);
            readUserName();
        }
        else
        {
            setFileError( statusInfo.error() );
            return false;
        }


        // read process status file ( /proc/<pid/stat )
        //
        // the expected file format is a list of fields separated by spaces, using
        // parenthesies to escape fields such as the process name which may itself contain
        // spaces:
        //
        // FIELD FIELD (FIELD WITH SPACES) FIELD FIELD
        //
        QFile processInfo( QString("/proc/%1/stat").arg(pid) );
        if ( processInfo.open(QIODevice::ReadOnly) )
        {
            QTextStream stream(&processInfo);
            QString data = stream.readAll();

            int stack = 0;
            int field = 0;
            int pos = 0;

            while (pos < data.count())
            {
                QChar c = data[pos];

                if ( c == '(' )
                    stack++;
                else if ( c == ')' )
                    stack--;
                else if ( stack == 0 && c == ' ' )
                    field++;
                else
                {
                    switch ( field )
                    {
                        case PARENT_PID_FIELD:
                            parentPidString.append(c);
                            break;
                        case PROCESS_NAME_FIELD:
                            processNameString.append(c);
                            break;
                        case GROUP_PROCESS_FIELD:
                            foregroundPidString.append(c);
                            break;
                    }
                }

                pos++;
            }
        }
        else
        {
            setFileError( processInfo.error() );
            return false;
        }

        // check that data was read successfully
        bool ok = false;
        int foregroundPid = foregroundPidString.toInt(&ok);
        if (ok)
            setForegroundPid(foregroundPid);

        int parentPid = parentPidString.toInt(&ok);
        if (ok)
            setParentPid(parentPid);

        if (!processNameString.isEmpty())
            setName(processNameString);

        // update object state
        setPid(pid);

        return ok;
    }

    virtual bool readArguments(int pid)
    {
        // read command-line arguments file found at /proc/<pid>/cmdline
        // the expected format is a list of strings delimited by null characters,
        // and ending in a double null character pair.

        QFile argumentsFile( QString("/proc/%1/cmdline").arg(pid) );
        if ( argumentsFile.open(QIODevice::ReadOnly) )
        {
            QTextStream stream(&argumentsFile);
            QString data = stream.readAll();

            QStringList argList = data.split( QChar('\0') );

            foreach ( const QString &entry , argList )
            {
                if (!entry.isEmpty())
                    addArgument(entry);
            }
        }
        else
        {
            setFileError( argumentsFile.error() );
        }

        return true;
    }

    virtual bool readCurrentDir(int pid)
    {
        QFileInfo info( QString("/proc/%1/cwd").arg(pid) );

        const bool readable = info.isReadable();

        if ( readable && info.isSymLink() )
        {
            setCurrentDir( info.symLinkTarget() );
            return true;
        }
        else
        {
            if ( !readable )
                setError( PermissionsError );
            else
                setError( UnknownError );

            return false;
        }
    }

    virtual bool readEnvironment(int pid)
    {
        // read environment bindings file found at /proc/<pid>/environ
        // the expected format is a list of KEY=VALUE strings delimited by null
        // characters and ending in a double null character pair.

        QFile environmentFile( QString("/proc/%1/environ").arg(pid) );
        if ( environmentFile.open(QIODevice::ReadOnly) )
        {
            QTextStream stream(&environmentFile);
            QString data = stream.readAll();

            QStringList bindingList = data.split( QChar('\0') );

            foreach( const QString &entry , bindingList )
            {
                QString name;
                QString value;

                int splitPos = entry.indexOf('=');

                if ( splitPos != -1 )
                {
                    name = entry.mid(0,splitPos);
                    value = entry.mid(splitPos+1,-1);

                    addEnvironmentBinding(name,value);
                }
            }
        }
        else
        {
            setFileError( environmentFile.error() );
        }

        return true;
    }
} ;

#ifdef Q_OS_SOLARIS
    // The procfs structure definition requires off_t to be
    // 32 bits, which only applies if FILE_OFFSET_BITS=32.
    // Futz around here to get it to compile regardless,
    // although some of the structure sizes might be wrong.
    // Fortunately, the structures we actually use don't use
    // off_t, and we're safe.
    #if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS==64)
        #undef _FILE_OFFSET_BITS
    #endif
    #include <procfs.h>
#else
    // On non-Solaris platforms, define a fake psinfo structure
    // so that the SolarisProcessInfo class can be compiled
    //
    // That avoids the trap where you change the API and
    // don't notice it in #ifdeffed platform-specific parts
    // of the code.
    struct psinfo {
        int pr_ppid;
        int pr_pgid;
        char* pr_fname;
        char* pr_psargs;
    } ;
    static const int PRARGSZ=1;
#endif

class SolarisProcessInfo : public UnixProcessInfo
{
public:
    SolarisProcessInfo(int pid, bool readEnvironment) 
    : UnixProcessInfo(pid,readEnvironment)
    {
    }
private:
    virtual bool readProcInfo(int pid)
    {
        QFile psinfo( QString("/proc/%1/psinfo").arg(pid) );
        if ( psinfo.open( QIODevice::ReadOnly ) )
        {
            struct psinfo info;
            if (psinfo.read((char *)&info,sizeof(info)) != sizeof(info))
            {
                return false;
            }

            setParentPid(info.pr_ppid);
            setForegroundPid(info.pr_pgid);
            setName(info.pr_fname);
            setPid(pid);

            // Bogus, because we're treating the arguments as one single string
            info.pr_psargs[PRARGSZ-1]=0;
            addArgument(info.pr_psargs);
        }
        return true;
    }

    virtual bool readArguments(int /*pid*/)
    {
        // Handled in readProcInfo()
        return true;
    }

    virtual bool readEnvironment(int /*pid*/)
    {
        // Not supported in Solaris
        return true;
    }

    virtual bool readCurrentDir(int pid)
    {
        QFileInfo info( QString("/proc/%1/path/cwd").arg(pid) );
        const bool readable = info.isReadable();

        if ( readable && info.isSymLink() )
        {
            setCurrentDir( info.symLinkTarget() );
            return true;
        }
        else
        {
            if ( !readable )
                setError( PermissionsError );
            else
                setError( UnknownError );

            return false;
        }
    }
} ;

SSHProcessInfo::SSHProcessInfo(const ProcessInfo& process)
 : _process(process)
{
    bool ok = false;

    // check that this is a SSH process
    const QString& name = _process.name(&ok);

    if ( !ok || name != "ssh" )
    {
        if ( !ok )
            kDebug() << "Could not read process info";
        else
            kDebug() << "Process is not a SSH process";

        return;
    }

    // read arguments
    const QVector<QString>& args = _process.arguments(&ok); 

    // SSH options
    // these are taken from the SSH manual ( accessed via 'man ssh' )
    
    // options which take no arguments
    static const QString noOptionsArguments("1246AaCfgkMNnqsTtVvXxY"); 
    // options which take one argument
    static const QString singleOptionArguments("bcDeFiLlmOopRSw");

    if ( ok )
    {
         // find the username, host and command arguments
         //
         // the username/host is assumed to be the first argument 
         // which is not an option
         // ( ie. does not start with a dash '-' character )
         // or an argument to a previous option.
         //
         // the command, if specified, is assumed to be the argument following
         // the username and host
         //
         // note that we skip the argument at index 0 because that is the
         // program name ( expected to be 'ssh' in this case )
         for ( int i = 1 ; i < args.count() ; i++ )
         {
            // if this argument is an option then skip it, plus any 
            // following arguments which refer to this option
            if ( args[i].startsWith('-') )
            {
                QChar argChar = ( args[i].length() > 1 ) ? args[i][1] : '\0';

                if ( noOptionsArguments.contains(argChar) )
                    continue;
                else if ( singleOptionArguments.contains(argChar) )
                {
                    i++;
                    continue;
                }
            }

            // check whether the host has been found yet
            // if not, this must be the username/host argument 
            if ( _host.isEmpty() )
            {
                // check to see if only a hostname is specified, or whether
                // both a username and host are specified ( in which case they
                // are separated by an '@' character:  username@host )
            
                int separatorPosition = args[i].indexOf('@');
                if ( separatorPosition != -1 )
                {
                    // username and host specified
                    _user = args[i].left(separatorPosition);
                    _host = args[i].mid(separatorPosition+1);
                }
                else
                {
                    // just the host specified
                    _host = args[i];
                }
            }
            else
            {
                // host has already been found, this must be the command argument
                _command = args[i];
            }

         }
    }
    else
    {
        kDebug() << "Could not read arguments";
        
        return;
    }
}

QString SSHProcessInfo::userName() const
{
    return _user;
}
QString SSHProcessInfo::host() const
{
    return _host;
}
QString SSHProcessInfo::command() const
{
    return _command;
}
QString SSHProcessInfo::format(const QString& input) const
{
    QString output(input);
   
    // test whether host is an ip address
    // in which case 'short host' and 'full host'
    // markers in the input string are replaced with
    // the full address
    bool isIpAddress = false;
   
    struct in_addr address;
    if ( inet_aton(_host.toLocal8Bit().constData(),&address) != 0 )
        isIpAddress = true;
    else
        isIpAddress = false;

    // search for and replace known markers
    output.replace("%u",_user);

    if ( isIpAddress )
        output.replace("%h",_host);
    else
        output.replace("%h",_host.left(_host.indexOf('.')));
    
    output.replace("%H",_host);
    output.replace("%c",_command);

    return output;
}

ProcessInfo* ProcessInfo::newInstance(int pid,bool enableEnvironmentRead)
{
#ifdef Q_OS_LINUX
    return new LinuxProcessInfo(pid,enableEnvironmentRead);
#elif defined(Q_OS_SOLARIS)
    return new SolarisProcessInfo(pid,enableEnvironmentRead);
#else
    return new NullProcessInfo(pid,enableEnvironmentRead);
#endif
}

