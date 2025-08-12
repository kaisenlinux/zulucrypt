/*
 *
 *  Copyright ( c ) 2011-2015
 *  name : Francis Banyikwa
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  ( at your option ) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <QObject>
#include <QDir>
#include <pwd.h>
#include <grp.h>
#include <termios.h>

#include <memory>

#include <QTranslator>
#include <QEventLoop>
#include <QDebug>
#include <QCoreApplication>
#include <blkid/blkid.h>
#include <QByteArray>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QTableWidget>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QProcessEnvironment>
#include <unistd.h>
#include <pwd.h>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMetaObject>
#include <QtNetwork/QLocalSocket>
#include <QJsonObject>
#include <QJsonDocument>

#include "../zuluCrypt-cli/constants.h"
#include "../zuluCrypt-cli/bin/bash_special_chars.h"
#include "version.h"
#include "locale_path.h"
#include "mount_prefix_path.h"
#include "dialogmsg.h"
#include "support_whirlpool.h"
#include "readonlywarning.h"
#include "../plugins/plugins.h"
#include "plugin.h"
#include "install_prefix.h"
#include "utility.h"
#include "executablesearchpaths.h"
#include "zuluPolkit.h"
#include "luks_slot_status.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <gcrypt.h>

#include "../zuluCrypt-cli/pluginManager/libzuluCryptPluginManager.h"

#include "../zuluCrypt-cli/utility/process/process.h"

#include "plugin_path.h"

#include "../zuluCrypt-cli/utility/process/process.h"

#include "reuse_mount_point.h"
#include "share_mount_prefix_path.h"

#include "zuluPolkit.h"

#include "favorites.h"

struct jsonResult
{
	bool finished ;
	int exitCode ;
	int exitStatus ;
	QByteArray stdError ;
	QByteArray stdOut ;
};

static std::function< void() > _failed_to_connect_to_zulupolkit ;

static QSettings * _settings ;

static debugWindow * _debugWindow ;

static QWidget * _mainWindow ;

static void _post_backend_cmd( const QString& b )
{
	QString a = "***************************\n" ;
	QString c = "\n***************************" ;

	_debugWindow->UpdateOutPut( a + b + c,false ) ;
}

void utility::setDebugWindow( debugWindow * w )
{
	_debugWindow = w ;
}

void utility::mainWindowWidget( QWidget * e )
{
	_mainWindow = e ;
}

QWidget * utility::mainWindowWidget()
{
	return _mainWindow ;
}


std::unique_ptr< utility::RandomDataSource > utility::RandomDataSource::get( utility::RandomDataSource::types type )
{
	if( type == utility::RandomDataSource::types::urandom ){

		return std::make_unique< utility::UrandomDataSource >() ;
	}else{
		return std::make_unique< utility::CRandDataSource >() ;
	}
}

utility::RandomDataSource::~RandomDataSource()
{
}

static QByteArray _json_command( const QByteArray& cookie,
				 const QByteArray& password,
				 const QString& exe,
				 const QString& path = QString(),
				 const QByteArray& data = QByteArray() )
{
	QJsonObject obj ;

	obj.insert( "cookie",cookie.constData() ) ;
	obj.insert( "password",password.constData() ) ;
	obj.insert( "command",exe.toLatin1().constData() ) ;
	obj.insert( "path",path.toLatin1().constData() ) ;
	obj.insert( "data",data.constData() ) ;

	return QJsonDocument( obj ).toJson( QJsonDocument::JsonFormat::Indented ) ;
}

static jsonResult _json_result( const QByteArray& e )
{
	if( !e.isEmpty() ){

		QJsonParseError error ;

		auto doc = QJsonDocument::fromJson( e,&error ) ;

		if( error.error == QJsonParseError::NoError ){

			auto obj = doc.object() ;

			return { obj.value( "finished" ).toBool(),
				 obj.value( "exitCode" ).toInt(),
				 obj.value( "exitStatus" ).toInt(),
				 obj.value( "stdError" ).toString().toUtf8(),
				 obj.value( "stdOut" ).toString().toUtf8() } ;
		}

	}

	return { false,255,255,"","" } ;
}

static bool _connected( QLocalSocket& s )
{
	s.connectToServer( utility::helperSocketPath() ) ;

	for( int i = 0 ; ; i++ ){

		if( s.waitForConnected() ){

			return true ;

		}else if( i == 2 ){

			utility::debug() << "ERROR: Failed To Connect To zuluPolkit" ;
			break ;
		}else{
			utility::debug() << s.errorString() ;

			utility::Task::suspendForOneSecond() ;
		}
	}

	return false ;
}

static QByteArray _cookie ;
static bool _run_through_polkit ;

::Task::future< utility::Task >& utility::Task::run( const QString& exe,USEPOLKIT e )
{
	return utility::Task::run( exe,-1,e ) ;
}

::Task::future< utility::Task >& utility::Task::run( const QString& exe,int s,USEPOLKIT e )
{
	return ::Task::run( [ = ](){

		auto env = QProcessEnvironment::systemEnvironment() ;

		return utility::Task( exe,s,env,_cookie,[](){ umask( 0 ) ; },e ) ;
	} ) ;
}

void utility::polkitFailedWarning( std::function< void() > e )
{
	_failed_to_connect_to_zulupolkit = std::move( e ) ;
}

void utility::Task::execute( const QString& exe,
			     int waitTime,
			     const QProcessEnvironment& env,
			     const QByteArray& password,
			     std::function< void() > f,
			     USEPOLKIT polkit )
{
	if( polkit == USEPOLKIT::True && utility::useZuluPolkit() ){

		QLocalSocket s ;

		if( _connected( s ) ){

			_post_backend_cmd( exe ) ;

			s.write( _json_command( _cookie,password,exe ) ) ;

			s.waitForBytesWritten() ;

			s.waitForReadyRead( -1 ) ;

			auto e = _json_result( s.readAll() ) ;

			m_finished   = e.finished ;
			m_exitCode   = e.exitCode ;
			m_exitStatus = e.exitStatus ;
			m_stdError   = e.stdError ;
			m_stdOut     = e.stdOut ;
		}else{
			_failed_to_connect_to_zulupolkit() ;

			m_finished   = false ;
			m_exitCode   = -1 ;
			m_exitStatus = -1 ;
			m_stdError   = "" ;
			m_stdOut     = QObject::tr( "zuluCrypt: Failed To Establish Connection With zuluPolkit" ).toLatin1() ;
		}
	}else{
		_post_backend_cmd( exe ) ;

#if QT_VERSION < QT_VERSION_CHECK( 5,15,0 )
		auto p = ::Task::process::run( exe,{},waitTime,password,env,std::move( f ) ).get() ;
#else
		auto s = QProcess::splitCommand( exe ) ;

		auto ee = s.first() ;
		s.removeFirst() ;

		auto p = ::Task::process::run( ee,s,waitTime,password,env,std::move( f ) ).get() ;
#endif
		m_finished   = p.finished() ;
		m_exitCode   = p.exit_code() ;
		m_exitStatus = p.exit_status() ;
		m_stdOut     = p.std_out() ;
		m_stdError   = p.std_error() ;
	}
}

void utility::setDefaultEnvironment()
{
}

QString utility::passwordSocketPath()
{
	return "/tmp/zuluCrypt-" + QString::number( getuid() ) ;
}

#if QT_VERSION > QT_VERSION_CHECK( 5,0,0 )
	#include <QStandardPaths>
	QString utility::socketPath()
	{
		return QStandardPaths::writableLocation( QStandardPaths::RuntimeLocation ) ;
	}
#else
	#include <QDesktopServices>
	QString utility::socketPath()
	{
		return QDesktopServices::storageLocation( QDesktopServices::DataLocation ) ;
	}
#endif

void utility::setSettingsObject( QSettings * e )
{
	_settings = e ;
}

QSettings& utility::settingsObject()
{
	return *_settings ;
}

static QString zuluPolkitExe()
{
	auto exe = utility::executableFullPath( "pkexec" ) ;

	if( exe.isEmpty() ){

		return QString() ;
	}else{
		return QString( "%1 %2 %3 fork" ).arg( exe,zuluPolkitPath,utility::helperSocketPath() ) ;
	}
}

static ::Task::future< utility::Task >& _start_zulupolkit( const QString& e )
{
	utility::UrandomDataSource randSource ;

	if( randSource.open() ){

		_cookie = randSource.getData( 16 ).toHex() ;
	}else{
		_cookie = utility::CRandDataSource().getData( 16 ).toHex() ;
	}

	return ::Task::run( [ = ]{

		return utility::Task( e,
				      -1,
				      utility::systemEnvironment(),
				      _cookie,
				      [](){},
				      utility::Task::USEPOLKIT::False ) ;
	} ) ;
}

static bool _enable_polkit_support( const QString& m )
{
	struct stat st ;

	if( m == "zuluCrypt" ){

		stat( ZULUCRYPTzuluCrypt,&st ) ;
	}else{
		stat( zuluMountPath,&st ) ;
	}

	bool s = st.st_mode & S_ISUID ;

	return !s ;
}

void utility::startHelper( QObject * obj,const QString& arg,const QString& exe,
				     utility::startHelperStatus status )
{
	if( _enable_polkit_support( exe ) ){

		auto exe = zuluPolkitExe() ;

		if( !exe.isEmpty() ){

			_start_zulupolkit( exe ).then( [ = ]( const utility::Task& e ){

				_run_through_polkit = e.success() ;

				utility::invokeMethod( obj,[ s = e.success(),arg,status ](){

					status.success( s,arg ) ;
				} ) ;
			} ) ;
		}else{
			DialogMsg().ShowUIOK( QObject::tr( "ERROR" ),
					      QObject::tr( "Failed to locate pkexec executable" ) ) ;

			utility::invokeMethod( obj,[ = ]{ status.error() ; } ) ;
		}
	}else{
		utility::invokeMethod( obj,[ = ](){ status.success( true,arg ) ; } ) ;
	}
}

QString utility::helperSocketPath()
{
	auto a = QString::number( getuid() ) ;
	auto b = QCoreApplication::applicationName() ;

	return QString( "/tmp/zuluCrypt-%1/%2.polkit.socket" ).arg( a,b ) ;
}

bool utility::useZuluPolkit()
{
	return _run_through_polkit ;
}

bool utility::requireSystemPermissions( const QString& e,utility::background_thread thread )
{
	const char * exe ;
	const char * group ;

	if( QCoreApplication::applicationName() == "zuluCrypt" ){

		exe = ZULUCRYPTzuluCrypt" -S" ;
		group = "zulucrypt" ;
	}else{
		exe = zuluMountPath" -S" ;
		group = "zulumount" ;
	}

	auto s = [ & ](){

		if( thread == utility::background_thread::True ){

			return utility::Task::run( exe ).get().stdOut() ;
		}else{
			return utility::Task::run( exe ).await().stdOut() ;
		}
	}() ;

	if( utility::split( s ).contains( e ) ){

		if( utility::userBelongsToGroup( group ) ){

			return false ;
		}else{
			return true ;
		}
	}else{
		return false ;
	}
}

bool utility::enablePolkit( utility::background_thread thread )
{
	if( _run_through_polkit ){

		return true ;
	}

	auto exe = zuluPolkitExe() ;

	if( !exe.isEmpty() ){

		auto socketPath = utility::helperSocketPath() ;

		if( thread == utility::background_thread::True ){

			if( _start_zulupolkit( exe ).get().success() ){

				_run_through_polkit = true ;

				while( !utility::pathExists( socketPath ) ){

					utility::Task::waitForOneSecond() ;
				}
			}
		}else{
			if( _start_zulupolkit( exe ).await().success() ){

				_run_through_polkit = true ;

				while( !utility::pathExists( socketPath ) ){

					utility::Task::suspendForOneSecond() ;
				}
			}
		}
	}

	return _run_through_polkit ;
}

void utility::quitHelper()
{
	auto e = utility::helperSocketPath() ;

	if( utility::pathExists( e ) ){

		QLocalSocket s ;

		s.connectToServer( e ) ;

		if( s.waitForConnected() ){

			s.write( _json_command( _cookie,"","exit" ) ) ;

			s.waitForBytesWritten() ;
		}
	}
}

std::pair< bool,QByteArray > utility::privilegedReadConfigFile( const QString& path )
{
	if( utility::enablePolkit( utility::background_thread::False ) ){

		QLocalSocket s ;

		if( _connected( s ) ){

			s.write( _json_command( _cookie,QByteArray(),"Read",path ) ) ;

			s.waitForBytesWritten() ;

			s.waitForReadyRead() ;

			return { true,_json_result( s.readAll() ).stdOut } ;
		}
	}

	return { false,QByteArray() } ;
}

void utility::privilegedWriteConfigFile( const QByteArray& data,const QString& path )
{
	if( utility::enablePolkit( utility::background_thread::False ) ){

		QLocalSocket s ;

		if( _connected( s ) ){

			s.write( _json_command( _cookie,QByteArray(),"Write",path,data ) ) ;

			s.waitForBytesWritten() ;
		}
	}
}

bool utility::reUseMountPointPath()
{
	return REUSE_MOUNT_POINT ;
}

bool utility::reUseMountPoint()
{
	return utility::reUseMountPointPath() ;
}

int utility::getUID()
{
	return static_cast< int >( getuid() ) ;
}

int utility::getUserID()
{
	return utility::getUID() ;
}

int utility::getGUID( int uid )
{
	auto e = getpwuid( static_cast< uid_t >( uid ) ) ;

	if( e ){

		return static_cast< int >( e->pw_gid ) ;
	}else{
		return uid ;
	}
}

QString utility::getStringUserID()
{
	return QString::number( utility::getUserID() ) ;
}

QString utility::appendUserUID( const QString& e )
{
	if( utility::useZuluPolkit() ){

		return e + " -K " + utility::getStringUserID() ;
	}else{
		return e ;
	}
}

static passwd * _getPassWd()
{
	return getpwuid( static_cast< uid_t >( utility::getUserID() ) ) ;
}

QString utility::userName()
{
	return _getPassWd()->pw_name ;
}

QString utility::homePath()
{
	return getpwuid( static_cast< uid_t >( utility::getUserID() ) )->pw_dir ;
}

static int _help()
{
	std::cout << "\n" << VERSION_STRING << std::endl ;

	QString helpMsg = QObject::tr( "\n\
options:\n\
	-d   path to where a volume to be auto unlocked/mounted is located\n\
	-m   tool to use to open a default file manager(default tool is xdg-open)\n\
	-e   start the application without showing the GUI\n" ) ;

	std::cout << helpMsg.toLatin1().constData() << std::endl ;

	return 0 ;
}

static bool _printHelpOrVersionInfo()
{
	QStringList q = QCoreApplication::arguments() ;
	return q.contains( "-h" )        ||
	       q.contains( "-help" )     ||
	       q.contains( "--help" )    ||
	       q.contains( "-v" )        ||
	       q.contains(  "-version" ) ||
	       q.contains( "--version" ) ;
}

int utility::startApplication( const char * appName,std::function<int()> start )
{
	QCoreApplication::setApplicationName( appName ) ;

	if( _printHelpOrVersionInfo() ){

		return _help() ;
	}else{
		return start() ;
	}
}

void utility::keySend( const QString& path,const QByteArray& key )
{
	::Task::exec( [ path,key ](){

		auto handle = ::zuluCryptPluginManagerOpenConnection( path.toLatin1().constData() ) ;

		if( handle ){

			size_t size = static_cast< size_t >( key.size() ) ;

			/*
			 * ZULUCRYPT_KEYFILE_MAX_SIZE is defined in ../zuluCrypt-cli/constants.h
			 * The variable holds the maximum keyfile size
			 */

			if( size > ZULUCRYPT_KEYFILE_MAX_SIZE ){

				size = ZULUCRYPT_KEYFILE_MAX_SIZE ;
			}

			::zuluCryptPluginManagerSendKey( handle,key.constData(),size ) ;
			::zuluCryptPluginManagerCloseConnection( handle ) ;
		}
	} ) ;
}

void utility::keySend( const QString& keyPath,const QString& key )
{
	utility::keySend( keyPath,key.toLatin1() ) ;
}

void utility::addPluginsToMenu( QMenu& menu )
{
	QStringList e ;

	QDir dir( ZULUCRYPTpluginPath ) ;

	if( dir.exists() ){

		e = dir.entryList() ;

		e.removeOne( "zuluCrypt-testKey" ) ;
		e.removeOne( "." ) ;
		e.removeOne( ".." ) ;
		e.removeOne( "keyring" ) ;
		e.removeOne( "kwallet" ) ;
	}

	auto _add_actions = [ &menu ]( const QStringList& r ){

		for( const auto& it : r ){

			menu.addAction( it )->setObjectName( it ) ;
		}
	} ;

	_add_actions( e ) ;
}

::Task::future<int>& utility::exec( const QString& exe )
{
	return ::Task::run( [ exe ](){ return utility::Task( exe ).exitCode() ; } ) ;
}

::Task::future<QStringList>& utility::luksEmptySlots( const QString& volumePath )
{
	return ::Task::run( [ volumePath ](){

		auto e = utility::appendUserUID( "%1 -b -d \"%2\"" ) ;

		auto r = utility::Task( e.arg( ZULUCRYPTzuluCrypt,volumePath ) ) ;

		if( r.success() ){

			const auto& s = r.stdOut() ;

			int i = 0 ;

			for( const auto& it : s ){

				if( it == '1' || it == '3' ){

					i++ ;
				}
			}

			return QStringList{ QString::number( i ),QString::number( s.size() - 1 ) } ;
		}

		return QStringList() ;
	} ) ;
}

::Task::future<QString>& utility::getUUIDFromPath( const QString& dev )
{
	return ::Task::run( [ dev ](){

		auto device = dev ;
		device = device.replace( "\"", "\"\"\"" ) ;
		auto exe = utility::appendUserUID( "%1 -U -d \"%2\"" ).arg( ZULUCRYPTzuluCrypt,device ) ;

		auto r = utility::Task( exe ) ;

		if( r.success() ){

			QString uuid = r.stdOut() ;
			uuid.remove( "\n" ) ;

			if( uuid == "UUID=\"\"" ){

				return QString() ;
			}else{
				return uuid ;
			}
		}else{
			return QString() ;
		}
	} ) ;
}

::Task::future<bool>& utility::openPath( const QString& path,const QString& opener )
{
	return ::Task::run( [ = ](){

		auto e = opener + " " + utility::Task::makePath( path ) ;

		return utility::Task::run( e,utility::Task::USEPOLKIT::False ).get().failed() ;
	} ) ;
}

void utility::openPath( const QString& path,const QString& opener,
			QWidget * obj,const QString& title,const QString& msg )
{
	openPath( path,opener ).then( [ title,msg,obj ]( bool failed ){

		if( failed && obj ){

			DialogMsg m( obj ) ;
			m.ShowUIOK( title,msg ) ;
		}
	} ) ;
}

static ::Task::future<QString>& _getKey( LXQt::Wallet::Wallet& wallet,const QString& volumeID )
{
	return ::Task::run( [ & ]()->QString{

		decltype( wallet.readValue( volumeID ) ) key ;

		if( volumeID.startsWith( "UUID=" ) ){

			key = wallet.readValue( volumeID ) ;
		}else{
			auto uuid = utility::getUUIDFromPath( volumeID ).get() ;

			if( uuid.isEmpty() ){

				key = wallet.readValue( utility::getVolumeID( volumeID ) ) ;
			}else{
				key = wallet.readValue( uuid ) ;

				if( key.isEmpty() ){

					key = wallet.readValue( volumeID ) ;
				}
			}
		}

		return key ;
	} ) ;
}

utility::wallet utility::getKey( LXQt::Wallet::Wallet& wallet,const QString& keyID,const QString& app )
{
	utility::wallet w{ false,false,"" } ;

	auto s = wallet.backEnd() ;

	if( s == LXQt::Wallet::BackEnd::kwallet || s == LXQt::Wallet::BackEnd::libsecret ){

		if( s == LXQt::Wallet::BackEnd::kwallet ){

			w.opened = wallet.open( "default",utility::applicationName() ) ;
		}else{
			w.opened = wallet.open( utility::walletName(),utility::applicationName() ) ;
		}

		if( w.opened ){

			w.key = _getKey( wallet,keyID ).await() ;
		}

	}else if( s == LXQt::Wallet::BackEnd::internal ){

		auto walletName = utility::walletName() ;
		auto appName    = utility::applicationName() ;

		if( LXQt::Wallet::walletExists( s,walletName,appName ) ){

			wallet.setImage( utility::getIcon( app ) ) ;

			if( wallet.opened() ){

				w.opened = true ;
			}else{
				w.opened = wallet.open( walletName,appName ) ;
			}

			if( w.opened ){

				w.key = _getKey( wallet,keyID ).await() ;
				w.notConfigured = false ;
			}
		}else{
			w.notConfigured = true ;
		}
	}

	return w ;
}

static quint64 _volumeSize( const QString& e,size_t size )
{
	utility::fileHandle h ;

	if( size == 0 ){

		if( h.open( e ) ){

			return h.size() ;
		}else{
			return 0 ;
		}
	}else{
		return size ;
	}
}

static int _openVolume( const QString& e )
{
	return open( e.toLatin1().constData(),O_RDWR ) ;
}

static std::function< void( int ) > _closeFunction( const QString& e )
{
	return [ & ]( int fd ){

		if( fd != -1 ){

			for( int i = 0 ; i < 5 ; i++ ){

				if( close( fd ) == 0 ){

					utility::Task( utility::appendUserUID( "%1 -q -d \"%2\"" ).arg( ZULUCRYPTzuluCrypt,e ) ) ;

					break ;
				}
			}
		}
	} ;
}

static bool _writeToVolume( int fd,std::array< char,1024 >& buffer )
{
	return write( fd,buffer.data(),buffer.size() ) != -1 ;
}

::Task::future< int >& utility::clearVolume( const QString& volume,
					     std::atomic_bool * exit,
					     size_t volumeSize,
					     std::function< void( quint64 size,quint64 offset ) > function )
{
	return ::Task::run( [ volume,exit,volumeSize,function ](){

		auto volumePath = volume ;

		volumePath.replace( "\"","\"\"\"" ) ;

		auto a = utility::appendUserUID( "%1 -k -J -d \"%2\"" ).arg( ZULUCRYPTzuluCrypt,volumePath ) ;

		int r = utility::Task( a ).exitCode() ;

		if( r != 0 ){

			return r ;
		}else{
			auto volumeMapperPath = utility::mapperPath( volume ) ;

			utility::fileHandle f( _openVolume( volumeMapperPath ),_closeFunction( volumePath ) ) ;

			int fd = f.handle() ;

			if( fd == -1 ){

				return 1 ;
			}else{
				std::array< char,1024 > buffer ;

				quint64 size         = _volumeSize( volumeMapperPath,volumeSize ) ;
				quint64 size_written = 0 ;

				if( size == 0 ){

					return 0 ;
				}

				while( _writeToVolume( fd,buffer ) ){

					if( *exit ){
						/*
						 * erasedevice::taskResult() has info on why we return 5 here.
						 */
						return 5 ;
					}else{
						size_written += buffer.size() ;

						function( size,size_written ) ;

						if( size_written >= size ){

							break ;
						}
					}
				}
			}

			return 0 ;
		}
	} ) ;
}

QString utility::cryptMapperPath()
{
	//return QString( crypt_get_dir() )
	return "/dev/mapper/" ;
}

bool utility::userIsRoot()
{
	return getuid() == 0 ;
}

QString utility::shareMountPointToolTip()
{
	QString x ;
#if USE_HOME_PATH_AS_MOUNT_PREFIX
	x = QDir::homePath() + "/" ;
#else
	x = "/run/media/private/" ;
#endif
	return QObject::tr( "\
If the option is checked,a primary private mount point will be created in \"%1\"\n\
and a secondary publicly accessible \"mirror\" mount point will be created in \"%2\"" ).arg( x,SHARE_MOUNT_PREFIX "/" ) ;
}

QString utility::shareMountPointToolTip( const QString& path )
{
	auto s = QString( SHARE_MOUNT_PREFIX "/" ) + path.split( "/" ).last() ;

	if( QFile::exists( s ) ){

		return QObject::tr( "public mount point: " ) + s ;
	}else{
		return QString() ;
	}
}

QString utility::sharedMountPointPath( const QString& path )
{
	if( path == "/" ){

		return QString() ;
	}else{
		auto s = SHARE_MOUNT_PREFIX "/" + path.split( "/" ).last() ;

		if( QFile::exists( s ) ){

			return s ;
		}else{
			return QString() ;
		}
	}
}

bool utility::pathPointsToAFile( const QString& path )
{
	utility::fileHandle h ;

	if( h.open( path ) ){

		return h.isFile() ;
	}else{
		return false ;
	}
}

bool utility::pathPointsToAFolder( const QString& path )
{
	utility::fileHandle h ;

	if( h.open( path ) ){

		return h.isFolder() ;
	}else{
		return false ;
	}
}

bool utility::useDmCryptForRandomData()
{
	if( !_settings->contains( "UseDmCryptForRandomData" ) ){

		_settings->setValue( "UseDmCryptForRandomData",false ) ;
	}

	return _settings->value( "UseDmCryptForRandomData" ).toBool() ;
}

QString utility::localizationLanguage( const QString& program )
{
	Q_UNUSED( program )

	if( _settings->contains( "LocalizationLanguage" ) ){

		return _settings->value( "LocalizationLanguage" ).toString() ;
	}else{
		_settings->setValue( "LocalizationLanguage","en_US" ) ;

		return "en_US" ;
	}
}

void utility::setLocalizationLanguage( const QString& program,const QString& language )
{
	Q_UNUSED( program )
	_settings->setValue( "LocalizationLanguage",language ) ;
}

QString utility::localizationLanguagePath( const QString& program )
{
	return QString( TRANSLATION_PATH ) + program ;
}


QString utility::walletName()
{
	return "zuluCrypt" ;
}

QString utility::applicationName()
{
	return "zuluCrypt" ;
}

bool utility::pathIsReadable( const QString& path,bool isFolder )
{
	QFileInfo s( path ) ;

	if( isFolder ){

		return s.isReadable() && s.isDir() ;
	}else{
		return s.isReadable() && s.isFile() ;
	}
}

bool utility::pathIsWritable( const QString& path,bool isFolder )
{
	QFileInfo s( path ) ;

	if( isFolder ){

		return s.isWritable() && s.isDir() ;
	}else{
		return s.isWritable() && s.isFile() ;
	}
}

bool utility::configDirectoriesAreNotWritable( QWidget * w )
{
	auto a = utility::socketPath() ;
	auto b = utility::passwordSocketPath() ;

	QDir().mkpath( b ) ;

	utility::changePathOwner( b ) ;
	utility::changePathPermissions( b,0700 ) ;

	if( utility::pathIsWritable( a ) && utility::pathIsWritable( b ) ){

		return false ;
	}else{
		auto e = QObject::tr( "\"%1\" and \"%2\" Folders Must Be Writable." ).arg( a,b ) ;
		DialogMsg( w ).ShowUIOK( QObject::tr( "ERROR" ),e ) ;
		return true ;
	}
}

bool utility::setOpenVolumeReadOnly( QWidget * parent,bool checked,const QString& app )
{
	return readOnlyWarning::showWarning( parent,checked,app ) ;
}

bool utility::getOpenVolumeReadOnlyOption( const QString& app )
{
	return readOnlyWarning::getOpenVolumeReadOnlyOption( app ) ;
}

QString utility::keyPath()
{
	utility::UrandomDataSource randomSource ;

	QByteArray data ;

	if( randomSource.open() ){

		data = randomSource.getData( 64 ) ;
	}else{
		data = utility::CRandDataSource().getData( 64 ) ;
	}

	QString a = utility::passwordSocketPath() ;

	return QString( "%1/%2" ).arg( a,utility::hashPath( data ).mid( 1 ) ) ;
}

bool utility::eventFilter( QObject * gui,QObject * watched,QEvent * event,std::function< void() > function )
{
	if( watched == gui ){

		if( event->type() == QEvent::KeyPress ){

			auto keyEvent = static_cast< QKeyEvent* >( event ) ;

			if( keyEvent->key() == Qt::Key_Escape ){

				function() ;

				return true ;
			}
		}
	}

	return false ;
}

QStringList utility::split( const QString& str,char token )
{
	if( str.isEmpty() ){

		return {} ;
	}

#if QT_VERSION < QT_VERSION_CHECK( 5,15,0 )
	return str.split( token,QString::SkipEmptyParts ) ;
#else
	return str.split( token,Qt::SkipEmptyParts ) ;
#endif
}

QStringList utility::split( const QByteArray& str,char token )
{
	return  utility::split( QString( str ),token ) ;
}

QStringList utility::split( const QByteArray& str,const char * token )
{
	return  utility::split( QString( str ),token ) ;
}

QStringList utility::split( const QString& str,const char * token )
{
	if( str.isEmpty() ){

		return {} ;
	}

#if QT_VERSION < QT_VERSION_CHECK( 5,15,0 )
	return str.split( token,QString::SkipEmptyParts ) ;
#else
	return str.split( token,Qt::SkipEmptyParts ) ;
#endif
}

bool utility::mapperPathExists( const QString& path )
{
	return utility::pathExists( utility::mapperPath( path ) ) ;
}

QString utility::mountPath( const QString& path )
{
	auto pass = _getPassWd() ;

#if USE_HOME_PATH_AS_MOUNT_PREFIX
	return QString( "%1/%2" ).arg( QString( pass->pw_dir ) ).arg( path ) ;
#else
	return QString( "/run/media/private/%1/%2" ).arg( QString( pass->pw_dir ).split( "/" ).last(),path ) ;
#endif
}

QString utility::homeMountPath( const QString& path )
{
	return QString( "%1/%2" ).arg( _getPassWd()->pw_dir,path ) ;
}

QString utility::mountPathPostFix( const QString& path )
{
	if( path.isEmpty() ){

		return path ;
	}else{
		auto _usable_mount_point = []( const QString& e ){

			if( utility::reUseMountPointPath() ){

				if( utility::pathExists( e ) ){

					return utility::pathPointsToAFolder( e ) ;
				}else{
					return true ;
				}

			}else{
				return !utility::pathExists( e ) ;
			}
		} ;

		auto e = utility::mountPath( path ) ;

		if( _usable_mount_point( e ) ){

			return path ;
		}else{
			QString z ;

			for( int i = 1 ; i < 1000 ; i++ ){

				z = QString::number( i ) ;

				if( _usable_mount_point( QString( "%1_%2" ).arg( e,z ) ) ){

					return QString( "%1_%2" ).arg( path,z ) ;
				}
			}

			return path ;
		}
	}
}

QString utility::mapperPath( const QString& r,const QString& component )
{
	auto rpath = r ;

	auto path = utility::cryptMapperPath() + "zuluCrypt-" + utility::getStringUserID() ;

	if( rpath.startsWith( "UUID=" ) ){

		rpath.remove( '\"' ) ;

		rpath.replace( "UUID=","UUID-" ) ;

		path += "-" + rpath + utility::hashPath( rpath.toLatin1() ) ;
	}else{
		if( component.isEmpty() ){

			path += "-NAAN-" + rpath.split( "/" ).last() + utility::hashPath( rpath.toLatin1() ) ;
		}else{
			path += component + rpath.split( "/" ).last() + utility::hashPath( rpath.toLatin1() ) ;
		}
	}

	for( const auto& it : BASH_SPECIAL_CHARS ){

		path.replace( it,'_' ) ;
	}
	return path ;
}

QString utility::hashPath( const QByteArray& p )
{
	int l = p.size() ;

	uint32_t hash = 0 ;

	auto key = p.constData() ;

	for( int i = 0 ; i < l ; i++ ){

		hash += static_cast< unsigned int >( *( key + i ) ) ;

		hash += ( hash << 10 ) ;

		hash ^= ( hash >> 6 ) ;
	}

	hash += ( hash << 3 ) ;

	hash ^= ( hash >> 11 ) ;

	hash += ( hash << 15 ) ;

	return "-" + QString::number( hash ) ;
}

bool utility::pathExists( const QString& path )
{
	return QFile::exists( path ) ;
}

bool utility::canCreateFile( const QString& path )
{
	auto s = path ;

	auto e = s.lastIndexOf( '/' ) ;

	if( e != -1 ){

		s.truncate( e ) ;
	}

	return utility::pathIsWritable( s ) ;
}

QString utility::resolvePath( const QString& path )
{
	if( path.size() == 1 && path.at( 0 ) == QChar( '~' ) ){

		return utility::homePath() + "/" ;

	}else if( path.startsWith( "~/" ) ){

		return utility::homePath() + "/" + path.mid( 2 ) ;

	}else if( path.startsWith( "UUID= ") ){

		return path ;

	}else if( path.startsWith( "/dev/" ) ){

		return path ;

	}else if( path.startsWith( "file://" ) ){

		return path.mid( 7 ) ;
	}else{
		QDir r( path ) ;

		auto rp = r.canonicalPath() ;

		if( rp.isEmpty() ) {

			return path ;
		}else{
			return rp ;
		}
	}
}

QString utility::executableFullPath( const QString& f )
{
	QString e = f ;

	if( e.startsWith( "/" ) ){

		auto s =  QDir( f ).canonicalPath() ;

		for( const auto& it : utility::executableSearchPaths() ){

			if( s.startsWith( it ) ){

				return s ;
			}
		}

		return QString() ;
	}

	if( e == "ecryptfs" ){

		e = "ecryptfs-simple" ;
	}

	QString exe ;

	for( const auto& it : utility::executableSearchPaths() ){

		exe = it + e ;

		if( utility::pathExists( exe ) ){

			return exe ;
		}
	}

	return QString() ;
}


static QString _absolute_exe_path( const QString& exe )
{
	auto e = utility::executableFullPath( exe ) ;

	if( e.isEmpty() ){

		return exe ;
	}else{
		return e ;
	}
}

QString utility::cmdArgumentValue( const QStringList& l,const QString& arg,const QString& defaulT )
{
	int j = l.size() ;

	for( int i = 0 ; i < j ; i++ ){

		if( l.at( i ) == arg ){

			auto e = [ & ](){

				if( i + 1 < j ){

					return l.at( i + 1 ) ;
				}else{
					return defaulT ;
				}
			} ;

			if( arg == "-m" ){

				return _absolute_exe_path( e() ) ;
			}else{
				return e() ;
			}
		}
	}

	return defaulT ;
}

QString utility::deviceIDToPartitionID( const QString& id )
{
	if( id.startsWith( "/dev/disk/by-id" ) ){

		auto l = id.split( '\t' ) ;

		if( l.size() > 1 ){

			QDir d( l.first() ) ;

			auto e = d.canonicalPath() ;

			if( e.isEmpty() ){

				return l.first() + '\t' + l.at( 1 ) ;
			}else{
				return e + '\t' + l.at( 1 ) ;
			}
		}else{
			return id ;
		}
	}else{
		return id ;
	}
}

static QString _partition_id_to_device_id( const QString& id,bool expand )
{
	if( id.startsWith( "/dev/" ) ){

		auto l = utility::directoryList( "/dev/disk/by-id" ) ;

		QDir r ;

		for( const auto& it : l ){

			const auto& e = it ;

			if( !e.startsWith( "dm" ) ){

				auto q = QString( "/dev/disk/by-id/%1" ).arg( e ) ;

				r.setPath( q ) ;

				if( r.canonicalPath() == id ){

					if( expand ){

						return q ;
					}else{
						return e ;
					}
				}
			}
		}
		return id ;
	}else{
		return id ;
	}
}

QString utility::getVolumeID( const QString& id,bool expand )
{
	return _partition_id_to_device_id( id,expand ) ;
}


bool utility::showWarningOnExtendingCoverFile()
{
	if( !_settings->contains( "ShowWarningOnExtendingCoverFile" ) ){

		 utility::showWarningOnExtendingCoverFile( true ) ;
	}

	return _settings->value( "ShowWarningOnExtendingCoverFile" ).toBool() ;
}

void utility::showWarningOnExtendingCoverFile( bool e )
{
	_settings->setValue( "ShowWarningOnExtendingCoverFile",e ) ;
}

void utility::addToFavorite( const QString& dev,const QString& m_point )
{
	if( !( dev.isEmpty() || m_point.isEmpty() ) ){

		QStringList s ;

		if( _settings->contains( "Favotites" ) ){

			 s = _settings->value( "Favotites" ).toStringList() ;
		}

		auto e = QString( "%1\t%2" ).arg( _partition_id_to_device_id( dev,true ),m_point ) ;

		s.append( e ) ;

		_settings->setValue( "Favotites",s ) ;
	}
}

QStringList readFavorites()
{
	if( _settings->contains( "Favotites" ) ){

		 QStringList l ;

		 for( const auto& it : _settings->value( "Favotites" ).toStringList() ){

			 if( it.startsWith( "/dev/disk/by-id" ) ){

				 l.append( utility::deviceIDToPartitionID( it ) ) ;
			 }else{
				 l.append( it ) ;
			 }
		 }

		 return l ;
	}else{
		return QStringList() ;
	}
}

bool utility::unMountVolumesOnLogout()
{
	if( !_settings->contains( "unMountVolumesOnLogout" ) ){

		_settings->setValue( "unMountVolumesOnLogout",false ) ;
	}

	return _settings->value( "unMountVolumesOnLogout" ).toBool() ;
}

void utility::readFavorites( QMenu * m,bool truncate,bool showFolders )
{
	m->clear() ;

	auto _add_action = [ m,truncate ]( const favorites::entry& e ){

		auto ac = new QAction( m ) ;

		if( truncate ){

			ac->setText( e.volumePath ) ;
		}else{
			ac->setText( e.volumePath + "\t" + e.mountPointPath ) ;
		}

		ac->setEnabled( !e.volumePath.startsWith( "/dev/disk/by-id" ) ) ;

		return ac ;
	} ;

	m->addAction( new QAction( QObject::tr( "Manage Favorites" ),m ) ) ;

	m->addAction( new QAction( QObject::tr( "Mount All" ),m ) ) ;

	m->addSeparator() ;

	favorites::instance().entries( [ & ]( const favorites::entry& e ){

		if( showFolders ){

			m->addAction( _add_action( e ) ) ;
		}else{
			const auto& s = e.volumePath ;

			if( utility::pathExists( s ) ){

				if( !utility::pathPointsToAFolder( s ) ){

					m->addAction( _add_action( e ) ) ;
				}
			}
		}
	} ) ;
}

int utility::favoriteClickedOption( const QString& opt )
{
	if( opt == QObject::tr( "Manage Favorites" ) ){

		return 1 ;

	}else if( opt == QObject::tr( "Mount All" ) ){

		return 2 ;
	}else{
		return 3 ;
	}
}

bool utility::userHasGoodVersionOfWhirlpool()
{
#ifdef GCRYPT_VERSION_NUMBER
	return GCRYPT_VERSION_NUMBER >= 0x010601 && SUPPORT_WHIRLPOOL ;
#else
	return SUPPORT_WHIRLPOOL ;
#endif
}

void utility::licenseInfo( QWidget * parent )
{
	QString s = "\nGpg key fingerprint: E3AF84691424AD00E099003502FC64E8DEBF43A8" ;

	QString license = QString( "%1\n\n\
This program is free software: you can redistribute it and/or modify \
it under the terms of the GNU General Public License as published by \
the Free Software Foundation, either version 2 of the License, or \
( at your option ) any later version.\n\
\n\
This program is distributed in the hope that it will be useful,\
but WITHOUT ANY WARRANTY; without even the implied warranty of \
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the \
GNU General Public License for more details." ).arg( VERSION_STRING + s ) ;


	DialogMsg m( parent ) ;
	m.ShowUIInfo( QObject::tr( "about zuluCrypt" ),false,license ) ;
}

static utility::array_t _default_dimensions( const char * defaults )
{
	auto l = QString( defaults ).split( ' ' ) ;

	utility::array_t e ;

	auto f = e.data() ;

	auto j = l.size() ;

	for( int i = 0 ; i < j ; i++ ){

		*( f + i ) = l.at( i ).toInt() ;
	}

	return e ;
}

static utility::array_t _dimensions( const char * defaults,int size )
{
	if( _settings->contains( "Dimensions" ) ){

		auto l = _settings->value( "Dimensions" ).toStringList() ;

		utility::array_t p ;

		if( l.size() != size || size > int( p.size() ) ){

			utility::debug() << "Failed to parse config file" ;
			return _default_dimensions( defaults ) ;
		}

		auto f = p.data() ;

		auto j = l.size() ;

		for( int i = 0 ; i < j ; i++ ){

			bool ok ;

			int e = l.at( i ).toInt( &ok ) ;

			if( ok ){

				*( f + i ) = e ;
			}else{
				qDebug() << "failed to parse config file option" ;
				return _default_dimensions( defaults ) ;
			}
		}

		return p ;
	}else{
		return _default_dimensions( defaults ) ;
	}
}

utility::array_t utility::getWindowDimensions( const QString& application )
{
	if( application == "zuluCrypt" ){

		return _dimensions( "297 189 782 419 298 336 100",7 ) ;
	}else{
		return _dimensions( "205 149 910 477 220 320 145 87 87",9 ) ;
	}
}

void utility::setWindowDimensions( const QString& application,const std::initializer_list<int>& e )
{
	Q_UNUSED( application )

	QStringList s ;

	for( const auto& it : e ){

		s.append( QString::number( it ) ) ;
	}

	_settings->setValue( "Dimensions",s ) ;
}

QFont utility::getFont( QWidget * widget )
{
	if( !_settings->contains( "Font") ){

		return widget->font() ;
	}else{
		auto l = _settings->value( "Font" ).toStringList() ;

		if( l.size() >= 4 ){

			QFont F ;

			const QString& fontFamily = l.at( 0 ) ;
			const QString& fontSize   = l.at( 1 ) ;
			const QString& fontStyle  = l.at( 2 ) ;
			const QString& fontWeight = l.at( 3 ) ;

			F.setFamily( fontFamily ) ;

			F.setPointSize( fontSize.toInt() ) ;

			if( fontStyle == "normal" ){

				F.setStyle( QFont::StyleNormal ) ;

			}else if( fontStyle == "italic" ){

				F.setStyle( QFont::StyleItalic ) ;
			}else{
				F.setStyle( QFont::StyleOblique ) ;
			}

			if( fontWeight == "normal" ){

				F.setWeight( QFont::Normal ) ;
			}else{
				F.setWeight( QFont::Bold ) ;
			}

			return F ;
		}else{
			return widget->font() ;
		}
	}
}

void utility::saveFont( const QFont& Font )
{
	auto s = QString( "%1\n%2\n" ).arg( Font.family(),QString::number( Font.pointSize() ) ) ;

	if( Font.style() == QFont::StyleNormal ){

		s = s + "normal\n" ;

	}else if( Font.style() == QFont::StyleItalic ){

		s = s + "italic\n" ;
	}else{
		s = s + "oblique\n" ;
	}

	if( Font.weight() == QFont::Normal ){

		s = s + "normal\n" ;
	}else{
		s = s + "bold" ;
	}

	_settings->setValue( "Font",utility::split( s,'\n' ) ) ;
}

bool utility::runningInMixedMode()
{
	return utility::useZuluPolkit() ;
}

bool utility::notRunningInMixedMode()
{
	return !utility::runningInMixedMode() ;
}

bool utility::userBelongsToGroup( const char * groupname )
{
	auto user = getpwuid( static_cast< gid_t >( utility::getUserID() ) ) ;

	if( user != nullptr ){

		auto grp = getgrnam( groupname ) ;

		if( grp != nullptr ){

			auto name  = user->pw_name ;

			for( auto e = grp->gr_mem ; *e ; e++ ){

				if( strcmp( *e,name ) == 0 ){

					return true ;
				}
			}
		}
	}

	return false ;
}

int utility::pluginKey( QWidget * w,QByteArray * key,const QString& p )
{
	plugins::plugin pluginType ;
	QString pluginString ;
	QVector<QString> exe ;

	if( p == "hmac" ){

		pluginType   = plugins::plugin::hmac_key ;
		pluginString = QObject::tr( "hmac plugin.\n\nThis plugin generates a key using below formular:\n\nkey = hmac(sha256,passphrase,keyfile contents)" ) ;

	}else if( p == "keykeyfile" ){

		pluginType   = plugins::plugin::keyKeyFile ;
		pluginString = QObject::tr( "keykeyfile plugin.\n\nThis plugin generates a key using below formular:\n\nkey = passphrase + keyfile contents" ) ;

	}else if( p == "gpg" ){

		pluginType   = plugins::plugin::gpg ;
		pluginString = QObject::tr( "gpg plugin.\n\nThis plugin retrives a key locked in a gpg file with a symmetric key" ) ;

		if( utility::pathExists( "/usr/bin/gpg" ) ){

			exe.append( "/usr/bin/gpg" ) ;

		}else if( utility::pathExists( "/usr/local/bin/gpg" ) ){

			exe.append( "/usr/local/bin/gpg" ) ;

		}else if( utility::pathExists( "/usr/sbin/gpg" ) ){

			exe.append( "/usr/sbin/gpg" ) ;

		}else{

			DialogMsg msg( w ) ;

			msg.ShowUIOK( QObject::tr( "ERROR" ),QObject::tr( "Could not find \"gpg\" executable in \"/usr/local/bin\",\"/usr/bin\" and \"/usr/sbin\"" ) ) ;

			return 1 ;
		}

	}else{
		return 1 ;
	}

	QEventLoop l ;

	plugin::instance( w,pluginType,[ & ]( const QByteArray& e ){

		*key = e ;

		if( e.isEmpty() ){

			l.exit( 1 ) ;
		}else{
			l.exit( 0 ) ;
		}

	},pluginString,exe ) ;

	return l.exec() ;
}

void utility::showTrayIcon( QAction * ac,QSystemTrayIcon& trayIcon,bool show )
{
	bool opt_show = true ;

	if( ac ){

		if( !_settings->contains( "ShowTrayIcon" ) ){

			_settings->setValue( "ShowTrayIcon",true ) ;
		}

		ac->setCheckable( true ) ;

		if( _settings->value( "ShowTrayIcon" ).toBool() ){

			ac->setChecked( true ) ;
			opt_show = true ;
		}else{
			ac->setChecked( false ) ;
			opt_show = false ;
		}
	}

	if( show ? show : opt_show ){

		trayIcon.show() ;

		utility::Timer( 2000,[ &trayIcon ]( int counter ){

			if( counter < 6 ){

				if( !trayIcon.isVisible() || !QSystemTrayIcon::isSystemTrayAvailable() ){

					/*
					 * Try again to show the icon
					 */
					trayIcon.show() ;

					return false ;
				}else{
					/*
					 * The icon is visible, exiting
					 */
					return true ;
				}
			}else{
				/*
				 * We waited long enough for the icon to show up, giving up.
				 */

				return true ;
			}
		} ) ;
	}
}

void utility::trayProperty( QSystemTrayIcon * trayIcon,bool zuluCrypt )
{
	Q_UNUSED( zuluCrypt )

	if( _settings->contains( "ShowTrayIcon" ) ){

		if( _settings->value( "ShowTrayIcon" ).toBool() ){

			_settings->setValue( "ShowTrayIcon",false ) ;
			trayIcon->hide() ;
		}else{
			_settings->setValue( "ShowTrayIcon",true ) ;
			trayIcon->show() ;
		}
	}
}

class translator
{
public:
	void set( const QString& app,const QByteArray& r,int s )
	{
		QCoreApplication::installTranslator( [ & ](){

			auto f = m_translator.data() ;
			auto e = *( f + s ) ;

			if( e ){

				QCoreApplication::removeTranslator( e ) ;

				delete e ;
			}

			e = new QTranslator() ;

			auto mm = e->load( r.constData(),utility::localizationLanguagePath( app ) ) ;

			if( !mm ){

				//??
			}

			*( f + s ) = e ;

			return e ;
		}() ) ;
	}
	~translator()
	{
		for( auto e : m_translator ){

			if( e ){

				/*
				 * QApplication appear to already be gone by the time we get here.
				 */
				//QCoreApplication::removeTranslator( e ) ;

				delete e ;
			}
		}
	}

private:
	std::array< QTranslator *,2 > m_translator = { { nullptr,nullptr } } ;
} static _translator ;

static void _selectOption( QMenu * m,const QString& opt )
{
	utility::selectMenuOption s( m,false ) ;
	s.selectOption( opt ) ;
}

void utility::setLocalizationLanguage( bool translate,QMenu * m,const QString& app )
{
	auto r = utility::localizationLanguage( app ).toLatin1() ;

	if( translate ){

		_translator.set( app,r,0 ) ;

		if( app == "zuluMount-gui" ){

			/*
			 * We are loading zuluCrypt-gui translation file to get translations for
			 * lxqtwallet strings.
			 */
			_translator.set( "zuluCrypt-gui",r,1 ) ;
		}
	}else{
		QDir d( utility::localizationLanguagePath( app ) ) ;

		auto t = d.entryList() ;

		if( !t.isEmpty() ){

			t.removeOne( "." ) ;
			t.removeOne( ".." ) ;

			for( auto& it : t ){

				m->addAction( it.remove( ".qm" ) )->setCheckable( true ) ;
			}
		}

		_selectOption( m,r ) ;
	}
}

void utility::languageMenu( QWidget * w,QMenu * m,QAction * ac,const char * app )
{
	Q_UNUSED( w )

	auto e = ac->text() ;

	e.remove( "&" ) ;

	utility::setLocalizationLanguage( app,e ) ;

	utility::setLocalizationLanguage( true,m,app ) ;

	_selectOption( m,e ) ;

	return ;
}

QStringList utility::directoryList( const QString& e )
{
	QDir d( e ) ;

	auto l = d.entryList() ;

	l.removeOne( "." ) ;
	l.removeOne( ".." ) ;

	return l ;
}

void utility::setIconMenu( const QString& app,QAction * ac,QWidget * w,
			   std::function< void( const QString& ) >&& function )
{
	ac->setMenu( [ & ](){

		auto m = new QMenu( w ) ;

		auto n = new utility::selectMenuOption( m,true,std::move( function ) ) ;

		w->connect( m,SIGNAL( triggered( QAction * ) ),n,SLOT( selectOption( QAction * ) ) ) ;

		if( !_settings->contains( "IconName" ) ){

			_settings->setValue( "IconName",app ) ;
		}

		QString s = _settings->value( "IconName" ).toString() ;

		QDir d( INSTALL_PREFIX "/share/icons/hicolor/48x48/apps/" ) ;

		d.setNameFilters( { "zuluCrypt.*","zuluMount.*" } ) ;

		for( auto& it : d.entryList() ){

			if( it.startsWith( app ) ){

				if( it != "zuluCrypt.png" && it != "zuluMount.png" ){

					it.remove( app + "." ) ;
				}

				it.remove( ".png" ) ;

				auto ac = m->addAction( it ) ;

				ac->setCheckable( true ) ;

				ac->setChecked( it == s ) ;
			}
		}

		return m ;
	}() ) ;
}

void utility::setIcons( const QString& app,const QString& iconName )
{
	Q_UNUSED( app )
	_settings->setValue( "IconName",iconName ) ;
}

QIcon utility::getIcon( const QString& app )
{
	if( !_settings->contains( "IconName" ) ){

		_settings->setValue( "IconName",app ) ;
	}

	QString e = _settings->value( "IconName" ).toString() ;

	if( e == "zuluCrypt" || e == "zuluMount" ){

		QIcon icon( INSTALL_PREFIX "/share/icons/hicolor/48x48/apps/" + e + ".png" ) ;
		return QIcon::fromTheme( app,icon ) ;
	}else{
		return QIcon( INSTALL_PREFIX "/share/icons/hicolor/48x48/apps/" + app + "." + e + ".png" ) ;
	}
}

QString utility::autoSetVolumeAsVeraCrypt()
{
	if( !_settings->contains( "AutoSetVolumeAsVeraCryptType" ) ){

		_settings->setValue( "AutoSetVolumeAsVeraCryptType",QString() ) ;
	}

	return _settings->value( "AutoSetVolumeAsVeraCryptType" ).toString() ;
}

void utility::autoSetVolumeAsVeraCrypt( const QString& opt )
{
	_settings->setValue( "AutoSetVolumeAsVeraCryptType",opt ) ;
}

int utility::defaultUnlockingVolumeType()
{
	if( !_settings->contains( "DefaultUnlockingVolumeType" ) ){

		_settings->setValue( "DefaultUnlockingVolumeType",0 ) ;
	}

	return _settings->value( "DefaultUnlockingVolumeType" ).toInt() ;
}

void utility::defaultUnlockingVolumeType( int e )
{
	_settings->setValue( "DefaultUnlockingVolumeType",e ) ;
}

void utility::autoOpenFolderOnMount( const QString& app,bool e )
{
	Q_UNUSED( app )
	_settings->setValue( "AutoOpenFolderOnMount",e ) ;
}

bool utility::autoOpenFolderOnMount( const QString& app )
{
	Q_UNUSED( app )

	if( _settings->contains( "AutoOpenFolderOnMount" ) ){

		return _settings->value( "AutoOpenFolderOnMount" ).toBool() ;
	}else{
		_settings->setValue( "AutoOpenFolderOnMount",true ) ;

		return true ;
	}
}

QString utility::defaultPlugin()
{
	if( !_settings->contains( "DefaultPlugin" ) ){

		QMenu m ;
		utility::addPluginsToMenu( m ) ;
		auto s = m.actions() ;

		if( s.size() ){

			_settings->setValue( "DefaultPlugin",s.at( 0 )->text() ) ;
		}
	}

	return _settings->value( "DefaultPlugin" ).toString() ;
}

void utility::setDefaultPlugin( const QString& e )
{
	if( !e.isEmpty() ){

		_settings->setValue( "DefaultPlugin",e ) ;
	}
}

QString utility::powerOffCommand()
{
	if( _settings->contains( "PowerOffCommand" ) ){

		return _settings->value( "PowerOffCommand" ).toString() ;
	}else{
		_settings->setValue( "PowerOffCommand","" ) ;

		return QString() ;
	}
}

void utility::setDoNotMinimizeToTray( bool e )
{
	_settings->setValue( "doNotMinimizeToTray",e ) ;
}

bool utility::doNotMinimizeToTray()
{
	if( !_settings->contains( "doNotMinimizeToTray" ) ){

		_settings->setValue( "doNotMinimizeToTray",false ) ;
	}

	return _settings->value( "doNotMinimizeToTray" ).toBool() ;
}

void utility::mountWithSharedMountPoint( bool e )
{
	_settings->setValue( "mountWithSharedMountPoint",e ) ;
}

bool utility::mountWithSharedMountPoint()
{
	if( !_settings->contains( "mountWithSharedMountPoint" ) ){

		_settings->setValue( "mountWithSharedMountPoint",false ) ;
	}

	return _settings->value( "mountWithSharedMountPoint" ).toBool() ;
}

QString utility::prettyfySpaceUsage( quint64 s )
{
	auto _convert = [ & ]( const char * p,double q ){

		auto e = QString::number( double( s ) / q,'f',2 ) ;

		e.remove( ".00" ) ;

		return QString( "%1 %2" ).arg( e,p ) ;
	} ;

	switch( QString::number( s ).size() ){

		case 0 :
		case 1 : case 2 : case 3 :

			return QString( "%1 B" ).arg( QString::number( s ) ) ;

		case 4 : case 5 : case 6 :

			return _convert( "KB",1024 ) ;

		case 7 : case 8 : case 9 :

			return _convert( "MB",1048576 ) ;

		case 10: case 11 : case 12 :

			return _convert( "GB",1073741824 ) ;

		default:
			return _convert( "TB",1024.0 * 1073741824 ) ;
	}
}

QStringList utility::plainDmCryptOptions()
{
	if( _settings->contains( "PlainDmCryptOptions" ) ){

		return _settings->value( "PlainDmCryptOptions" ).toStringList() ;
	}else{
		QStringList s = {

			{ "aes.cbc-essiv:sha256.256.sha256" },
			{ "aes.cbc-essiv:sha256.256.sha512" },
			{ "aes.cbc-essiv:sha256.256.sha1" },
			{ "aes.cbc-essiv:sha256.256.ripemd160" },
			{ "aes.cbc-essiv:sha256.512.sha256" },
			{ "aes.cbc-essiv:sha256.512.sha512" },
			{ "aes.cbc-essiv:sha256.512.sha1" },
			{ "aes.cbc-essiv:sha256.512.ripemd160" },
			{ "aes.xts-plain64.256.sha256" },
			{ "aes.xts-plain64.256.sha512" },
			{ "aes.xts-plain64.256.sha1" },
			{ "aes.xts-plain64.256.ripemd160" },
			{ "aes.xts-plain64.512.sha256" },
			{ "aes.xts-plain64.512.sha512" },
			{ "aes.xts-plain64.512.sha1" },
			{ "aes.xts-plain64.512.ripemd160" },
		};

		_settings->setValue( "PlainDmCryptOptions",s ) ;

		return s ;
	}
}

QString utility::KWalletDefaultName()
{
	if( !_settings->contains( "KWalletDefaultName" ) ){

		_settings->setValue( "KWalletDefaultName","default" ) ;
	}

	return _settings->value( "KWalletDefaultName" ).toString() ;
}

QStringList utility::supportedFileSystems()
{
	if( _settings->contains( "SupportedFileSystems" ) ){

		return _settings->value( "SupportedFileSystems" ).toStringList() ;
	}else{
		QStringList s{ "ext4","vfat","ntfs","ext2","ext3","exfat","ntfs","btrfs" } ;

		_settings->setValue( "SupportedFileSystems",s ) ;

		return s ;
	}
}

std::pair< bool,QByteArray > utility::getKeyFromNetwork( const QString& e )
{
	Q_UNUSED( e )

	return { false,QByteArray() } ;
#if 0
	if( !_settings->contains( "NetworkAddress" ) ){

		return _settings->setValue( "NetworkAddress","127.0.0.1" ) ;
	}

	QFile f( utility::homePath() + "/.zuluCrypt/network" ) ;

	if( !f.open( QIODevice::ReadOnly ) ){

		return { false,"" } ;
	}

	QUrl url ;

	QByteArray data = "device=" + utility::split( e,' ' ).last().toLatin1() ;

	QByteArray host ;

	for( const auto& it : utility::split( f.readAll() ) ){

		if( it.startsWith( "url=" ) ){

			url.setUrl( it.toLatin1().constData() + 4 ) ;

		}else if( it.startsWith( "host=" ) ){

			host = it.toLatin1().constData() + 5 ;
		}else{
			data += "&" + it ;
		}
	}

	QNetworkRequest n( url ) ;

	n.setRawHeader( "Host",host ) ;
	n.setRawHeader( "Content-Type","application/x-www-form-urlencoded" ) ;

	NetworkAccessManager m ;

	auto s = m.post( n,data ) ;

	if( s->error() == QNetworkReply::NoError ){

		return { true,s->readAll() } ;
	}else{
		return { false,"" } ;
	}
#endif
}

void utility::setHDPI( const QString& e )
{
	Q_UNUSED( e )

#if QT_VERSION >= 0x050600

	#if QT_VERSION < QT_VERSION_CHECK( 6,0,0 )

		QApplication::setAttribute( Qt::AA_EnableHighDpiScaling ) ;
	#endif
	if( !_settings->contains( "ScaleFactor" ) ){

		_settings->setValue( "ScaleFactor","1" ) ;
	}

	qputenv( "QT_SCALE_FACTOR",_settings->value( "ScaleFactor" ).toString().toLatin1() ) ;
#endif
}

bool utility::platformIsLinux()
{
	return true ;
}

bool utility::platformIsOSX()
{
	return false ;
}

QStringList utility::executableSearchPaths()
{
	return ::executableSearchPaths::values() ;
}

QString utility::executableSearchPaths( const QString& e )
{
	if( e.isEmpty() ){

		return utility::executableSearchPaths().join( ":" ) ;
	}else{
		return e + ":" + utility::executableSearchPaths().join( ":" ) ;
	}
}

static inline bool _terminalEchoOff( struct termios * old,struct termios * current )
{
	if( tcgetattr( 1,old ) != 0 ){

		return false ;
	}

	*current = *old;
	current->c_lflag &= static_cast< unsigned int >( ~ECHO ) ;

	if( tcsetattr( 1,TCSAFLUSH,current ) != 0 ){

		return false ;
	}else{
		return true ;
	}
}

QString utility::readPassword( bool addNewLine )
{
	std::cout << "Password: " << std::flush ;

	struct termios old ;
	struct termios current ;

	_terminalEchoOff( &old,&current ) ;

	QString s ;
	int e ;

	int m = 1024 ;

	for( int i = 0 ; i < m ; i++ ){

		e = std::getchar() ;

		if( e == '\n' || e == -1 ){

			break ;
		}else{
			s += static_cast< char >( e ) ;
		}
	}

	tcsetattr( 1,TCSAFLUSH,&old ) ;

	if( addNewLine ){

		std::cout << std::endl ;
	}

	return s ;
}

QString utility::fileManager()
{
	if( _settings->contains( "FileManager" ) ){

		return _settings->value( "FileManager" ).toString() ;
	}else{
		_settings->setValue( "FileManager","xdg-open" ) ;

		return "xdg-open" ;
	}
}

void utility::setFileManager( const QString& e )
{
	if( e.isEmpty() ){

		_settings->setValue( "FileManager","xdg-open" ) ;
	}else{
		_settings->setValue( "FileManager",e ) ;
	}
}

QProcessEnvironment utility::systemEnvironment()
{
	auto e = QProcessEnvironment::systemEnvironment() ;

	e.insert( "zuluCryptRuntimePath",utility::passwordSocketPath() + "/" ) ;

	e.insert( "CRYFS_NO_UPDATE_CHECK","TRUE" ) ;
	e.insert( "CRYFS_FRONTEND","noninteractive" ) ;

	e.insert( "LANG","C" ) ;

	e.insert( "PATH",utility::executableSearchPaths( e.value( "PATH" ) ) ) ;

	return e ;
}

bool utility::clearPassword()
{
	if( _settings->contains( "ClearPassword" ) ){

		return _settings->value( "ClearPassword" ).toBool() ;
	}else{
		_settings->setValue( "ClearPassword",true ) ;

		return true ;
	}
}

bool utility::readOnlyOption()
{
	if( _settings->contains( "ReadOnly" ) ){

		return _settings->value( "ReadOnly" ).toBool() ;
	}else{
		_settings->setValue( "ReadOnly",false ) ;

		return false ;
	}
}

void utility::readOnlyOption( bool e )
{
	_settings->setValue( "ReadOnly",e ) ;
}

bool utility::readOnlyWarning()
{
	if( _settings->contains( "ReadOnlyWarning" ) ){

		return _settings->value( "ReadOnlyWarning" ).toBool() ;
	}else{
		_settings->setValue( "ReadOnlyWarning",true ) ;

		return true ;
	}
}

void utility::readOnlyWarning( bool e )
{
	_settings->setValue( "ReadOnlyWarning",e ) ;
}

QString utility::failedToStartzuluPolkit()
{
	return QObject::tr( "Failed To Start Helper Application.\n\n\"org.zulucrypt.zulupolkit.policy\" polkit file is misconfigured,\nzuluPolkit executable could not be found\n or pkexec failed to start zuluPolkit." ) ;
}

static utility2::result< int > _convert_string_to_version( const QString& e )
{
	auto _convert = []( const QString& e )->utility2::result< int >{

		bool ok ;

		auto s = e.toInt( &ok ) ;

		if( ok ){

			return s  ;
		}else{
			return {} ;
		}
	} ;

	auto s = utility::split( e,'.' ) ;

	auto components = s.size() ;

	int major = 1000000 ;
	int minor = 1000 ;
	int patch = 1 ;

	if( components == 1 ){

		auto a = _convert( s.first() ) ;

		if( a ){

			return major * a.value() ;
		}

	}else if( components == 2 ){

		auto a = _convert( s.at( 0 ) ) ;
		auto b = _convert( s.at( 1 ) ) ;

		if( a && b ){

			return major * a.value() + minor * b.value() ;
		}

	}else if( components == 3 ){

		auto a = _convert( s.at( 0 ) ) ;
		auto b = _convert( s.at( 1 ) ) ;
		auto c = _convert( s.at( 2 ) ) ;

		if( a && b && c ){

			return major * a.value() + minor * b.value() + patch * c.value() ;
		}
	}

	return {} ;
}

static utility2::result< QString > _installed_version( const QString& backend )
{
	auto _remove_junk = []( QString e ){

		e.replace( "v","" ).replace( ";","" ) ;

		QString m ;

		for( int s = 0 ; s < e.size() ; s++ ){

			auto n = e.at( s ) ;

			if( n == '.' || ( n >= '0' && n <= '9' ) ){

				m += n ;
			}else{
				break ;
			}
		}

		return m ;
	} ;

	auto exe = utility::executableFullPath( backend ) ;

	if( exe.isEmpty() ){

		return {} ;
	}

	auto cmd = [ & ](){

		if( backend == "securefs" ){

			return backend + " version" ;
		}else{
			return backend + " --version" ;
		}
	}() ;

	auto s = utility::systemEnvironment() ;

	auto r = [ & ](){

		if( backend == "encfs" ){

			return QString( ::Task::process::run( cmd,{},-1,{},s ).get().std_error() ) ;
		}else{
			return QString( ::Task::process::run( cmd,{},-1,{},s ).get().std_out() ) ;
		}
	}() ;

	if( r.isEmpty() ){

		return {} ;
	}

	auto m = utility::split( utility::split( r,'\n' ).first(),' ' ) ;

	if( utility::equalsAtleastOne( backend,"cryfs","encfs","sshfs" ) ){

		if( m.size() >= 3 ){

			return _remove_junk( m.at( 2 ) ) ;
		}

	}else if( utility::equalsAtleastOne( backend,"gocryptfs","securefs","ecryptfs-simple" ) ){

		if( m.size() >= 2 ){

			return _remove_junk( m.at( 1 ) ) ;
		}
	}

	return {} ;
}

::Task::future< utility2::result< QString > >& utility::backEndInstalledVersion( const QString& backend )
{
	return ::Task::run( _installed_version,backend ) ;
}

static utility2::result< int > _installedVersion( const QString& backend )
{
	auto s = utility::backEndInstalledVersion( backend ).get() ;

	if( s && !s.value().isEmpty() ){

		return _convert_string_to_version( s.value() ) ;
	}else{
		return {} ;
	}
}

template< typename Function >
::Task::future< utility2::result< bool > >& _compare_versions( const QString& backend,
							      const QString& version,
							      Function compare )
{
	return ::Task::run( [ = ]()->utility2::result< bool >{

		auto installed = _installedVersion( backend ) ;
		auto guard_version = _convert_string_to_version( version ) ;

		if( installed && guard_version ){

			return compare( installed.value(),guard_version.value() ) ;
		}else{
			return {} ;
		}
	} ) ;
}

::Task::future< utility2::result< bool > >& utility::backendIsGreaterOrEqualTo( const QString& backend,
										const QString& version )
{
	return _compare_versions( backend,version,std::greater_equal<int>() ) ;
}

::Task::future< utility2::result< bool > >& utility::backendIsLessThan( const QString& backend,
									const QString& version )
{
	return _compare_versions( backend,version,std::less<int>() ) ;
}

utility::UrandomDataSource::UrandomDataSource() :
	m_file( "/dev/urandom" )
{
}

bool utility::UrandomDataSource::open()
{
	m_file.open( QIODevice::ReadOnly ) ;

	return m_file.isOpen() ;
}

qint64 utility::UrandomDataSource::getData( char * data,qint64 size )
{
	return m_file.read( data,size ) ;
}

QByteArray utility::UrandomDataSource::getData( qint64 size )
{
	return m_file.read( size ) ;
}

utility::CRandDataSource::CRandDataSource()
{
}

bool utility::CRandDataSource::open()
{
	return true ;
}

qint64 utility::CRandDataSource::getData( char * data,qint64 size )
{
	time_t t ;

	srand( static_cast< unsigned int >( time( &t ) ) ) ;

	for( int i = 0 ; i < size ; i++ ){

	   *( data + i ) = static_cast< char >( rand() ) ;
	}

	return size ;
}

QByteArray utility::CRandDataSource::getData( qint64 size )
{
	QByteArray data ;

	data.resize( static_cast< int >( size ) ) ;

	utility::CRandDataSource().getData( data.data(),size ) ;

	return data ;
}

QString utility::loopDevicePath( const QString& e )
{
	const auto s = QDir( "/sys/block" ).entryList() ;

	QFile file ;

	for( const auto& it : s ){

		if( it.startsWith( "loop" ) ){

			QString m = "/sys/block/" + it + "/loop/backing_file" ;

			if( utility::pathExists( m ) ){

				file.setFileName( m ) ;

				file.open( QIODevice::ReadOnly ) ;

				QString s = file.readAll() ;

				file.close() ;

				if( s.startsWith( e ) ){

					return "/dev/" + it ;
				}
			}
		}
	}

	return QString() ;
}

static bool _removeYkchalrespNewLineCharacter()
{
	if( !_settings->contains( "RemoveYkchalrespNewLineCharacter" ) ){

		_settings->setValue( "RemoveYkchalrespNewLineCharacter",true ) ;
	}

	return _settings->value( "RemoveYkchalrespNewLineCharacter" ).toBool() ;
}

static QString _ykchalrespArguments()
{
	if( !_settings->contains( "YkchalrespArguments" ) ){

		_settings->setValue( "YkchalrespArguments","-2 -i -" ) ;
	}

	return _settings->value( "YkchalrespArguments" ).toString() ;
}

static QString _ykchalresp_path()
{
	static QString m = utility::executableFullPath( "ykchalresp" ) ;
	return m ;
}


utility2::result<QByteArray> utility::yubiKey( const QString& challenge )
{
	QString exe = _ykchalresp_path() ;

	if( !exe.isEmpty() ){

		auto args = utility::split( _ykchalrespArguments(),' ' ) ;

		auto s = [ & ](){

			if( challenge.isEmpty() ){

				return ::Task::process::run( exe,"\n" ).await() ;
			}else{
				return ::Task::process::run( exe,args,challenge.toUtf8() ).await() ;
			}
		}() ;

		_post_backend_cmd( exe + " " + args.join( " " ) ) ;

		if( s.success() ){

			auto m = s.std_out() ;

			if( _removeYkchalrespNewLineCharacter() ){

				m.replace( "\n","" ) ;
			}

			return m ;
		}else{
			std::cout << "Failed to get a responce from ykchalresp" << std::endl ;
			std::cout << "StdOUt:" << s.std_out().constData() << std::endl ;
			std::cout << "StdError:" << s.std_error().constData() << std::endl ;
		}
	}

	return {} ;
}

utility::progress::progress( int s,std::function< void( const utility::progress::result& )> function ) :
	m_offset_last( 0 ),
	m_total_time( 0 ),
	m_function( std::move( function ) ),
	m_duration( s ),
	m_time( m_duration.timer() ),
	m_previousTime( static_cast< double >( m_time.currentMSecsSinceEpoch() ) )
{
}

void utility::progress::update_progress( quint64 size,quint64 offset )
{
	int i = int( ( offset * 100 / size ) ) ;

	auto time_expired = m_duration.passed() ;

	if( !time_expired ){

		m_duration.reset() ;
	}

	if( i > m_progress || time_expired ){

		m_progress = i ;

		double currentTime = static_cast< double >( m_time.currentMSecsSinceEpoch() ) ;

		double time_diff = ( currentTime - m_previousTime ) / 1000 ;
		double offset_diff = static_cast< double >( offset - m_offset_last ) ;

		m_total_time = m_total_time + time_diff ;

		QString current_speed = this->speed( offset_diff,time_diff ) ;

		QString average_speed = this->speed( static_cast< double >( offset ),m_total_time ) ;

		double avg_speed = static_cast< double >( offset ) / m_total_time ;

		double remaining_data = static_cast< double >( size - offset ) ;

		QString eta = this->time( remaining_data / avg_speed ) ;

		m_function( { current_speed,
			      average_speed,
			      eta,
			      this->time( m_total_time ),
			      i } ) ;

		m_offset_last = offset ;
		m_previousTime = currentTime ;
	}
}

std::function< void( quint64 size,quint64 offset ) > utility::progress::updater_quint()
{
	return [ this ]( quint64 size,quint64 offset ){

		this->update_progress( size,offset ) ;
	} ;
}

std::function< void( qint64 size,qint64 offset ) > utility::progress::updater_qint()
{
	return [ this ]( qint64 size,qint64 offset ){

		this->update_progress( quint64( size ),quint64( offset ) ) ;
	} ;
}

QString utility::progress::time( double s )
{
	int milliseconds = int( s ) * 1000 ;
	int seconds      = milliseconds / 1000;
	milliseconds     = milliseconds % 1000;
	int minutes      = seconds / 60 ;
	seconds          = seconds % 60 ;
	int hours        = minutes / 60 ;
	minutes          = minutes % 60 ;

	QTime time ;
	time.setHMS( hours,minutes,seconds,milliseconds ) ;
	return time.toString( "hh:mm:ss" ) ;
}

QString utility::progress::speed( double size,double time )
{
	QString s ;

	if( size < 1024 ){

		s = "B" ;

	}else if( size <= 1024 * 1024 ){

		s = "KB" ;
		size = size / 1024 ;

	}else if( size <= 1024 * 1024 * 1024 ){

		s = "MB" ;
		size = size / ( 1024 * 1024 ) ;

	}else if( size <= 1024 * 1024 * 1024 * 1024ll ){

		s = "GB" ;
		size = size / ( 1024 * 1024 * 1024 ) ;
	}else{
		s = "B" ;
	}

	size = size / time ;

	return QString::number( size,'f',2 ) + " " + s + "/s" ;
}

utility::duration::duration( long miliseconds ) : m_milliseconds( miliseconds )
{
	this->reset() ;
}

bool utility::duration::passed()
{
	auto now = m_time.currentMSecsSinceEpoch() ;

	if( now - m_start_time >= m_milliseconds ){

		this->reset() ;

		return true ;
	}else{
		return false ;
	}
}

void utility::duration::reset()
{
	m_start_time = m_time.currentMSecsSinceEpoch() ;
}

QDateTime& utility::duration::timer()
{
	return m_time ;
}

bool utility::showOnlyOccupiedSlots()
{
	if( !_settings->contains( "ShowOnlyOccupiedSlots" ) ){

		_settings->setValue( "ShowOnlyOccupiedSlots",true ) ;
	}

	return _settings->value( "ShowOnlyOccupiedSlots" ).toBool() ;
}

void utility::showOnlyOccupiedSlots( bool e )
{
	_settings->setValue( "ShowOnlyOccupiedSlots",e ) ;
}

bool utility::canShowKeySlotProperties()
{
	return SUPPORT_crypt_keyslot_get_pbkdf ;
}

bool utility::libCryptSetupLibraryNotFound()
{
	return false ;
	//return !utility::pathExists( CRYPTSETUP_LIBRARY_PATH ) ;
}

void utility::setFileSystemOptions( QString& exe,
				    const QString& device,
				    const QString& mountpoint,
				    const QString& mountOptions )
{
	QString mOpts ;

	favorites::instance().entries( [ & ]( const favorites::entry& e ){

		if( e.volumePath == device && e.mountPointPath == mountpoint ){

			mOpts = e.mountOptions ;

			return true ;
		}

		return false ;
	} ) ;

	if( mOpts.isEmpty() ){

		if( mountOptions.isEmpty() ){
			/*
			 * Both are empty, do nothing.
			 */
		}else{
			exe += " -Y " + mountOptions ;
		}
	}else{
		if( mountOptions.isEmpty() ){

			exe += " -Y " + mOpts ;
		}else{
			/*
			 * remove duplicate entries
			 */
			auto m = utility::split( mountOptions,',' ) ;

			for( const auto& it : utility::split( mOpts,',' ) ){

				m.removeAll( it ) ;
			}

			if( m.isEmpty() ){

				exe += " -Y " + mOpts ;
			}else{
				exe += " -Y " + mOpts + "," + m.join( ',' ) ;
			}
		}
	}
}

QString utility::fileSystemOptions( const QString& path )
{
	QString m ;

	if( !path.isEmpty() ){

		favorites::instance().entries( [ & ]( const favorites::entry& e ){

			if( e.volumePath == path ){

				m = e.mountOptions ;
				return true ;
			}

			return false ;
		} ) ;
	}

	return m ;
}

QString utility::pathToUUID( const QString& path )
{
	if( path.startsWith( "UUID=" ) ){

		return path ;
	}else{
		auto z = utility::getUUIDFromPath( path ).await() ;

		if( z.isEmpty() ){

			return utility::getVolumeID( path ) ;
		}else{
			return z ;
		}
	}
}
