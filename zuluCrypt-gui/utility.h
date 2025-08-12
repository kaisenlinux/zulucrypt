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

#ifndef MISCFUNCTIONS_H
#define MISCFUNCTIONS_H

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QEvent>
#include <QProcess>
#include <QThreadPool>
#include <QRunnable>
#include <QMetaObject>
#include <QDebug>
#include <QWidget>
#include <QDialog>
#include <QEventLoop>
#include <QTimer>
#include <QMenu>
#include <QVector>
#include <QSystemTrayIcon>
#include <QAction>
#include <QEventLoop>
#include <QIcon>
#include <QByteArray>
#include <QEvent>
#include <functional>
#include <memory>
#include <array>
#include <utility>
#include <chrono>
#include <QtNetwork/QLocalSocket>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>

#include <blkid/blkid.h>

#include "utility2.h"
#include "task.hpp"
#include "lxqt_wallet.h"
#include "debugwindow.h"

#include <QObject>
#include <QLabel>
#include <QPushButton>
#include <QLabel>
#include <QDateTime>
#include <QTime>

#include <poll.h>
#include <fcntl.h>

#include <iostream>
#include <atomic>

namespace utility
{
	template< typename Sender,typename Signal,typename Receiver,typename Slot>
	void connect( Sender sender,Signal signal,Receiver receiver,Slot slot )
	{
		QObject::connect( sender,signal,receiver,slot,Qt::QueuedConnection ) ;
	}

	class label
	{
	public:
		void setOptions( QLabel * l,QPushButton * b )
		{
			m_label = l ;
			m_pushButton = b ;

			m_label->setVisible( false ) ;
			m_pushButton->setVisible( false ) ;

			QObject::connect( m_pushButton,&QPushButton::clicked,[ this ](){

				this->hide() ;

				if( m_runEventLoop ){

					m_eventLoop.exit() ;
				}
			} ) ;
		}
		void show( const QString& e,bool runEventLopp = true )
		{
			m_runEventLoop = runEventLopp ;
			m_label->setText( e ) ;
			m_label->setVisible( true ) ;
			m_pushButton->setVisible( true ) ;
			m_pushButton->setFocus() ;

			if( runEventLopp ){

				m_eventLoop.exec() ;
			}
		}
		void hide()
		{
			m_label->clear() ;
			m_label->setVisible( false ) ;
			m_pushButton->setVisible( false ) ;
		}
	private:
		bool m_runEventLoop ;
		QLabel * m_label ;
		QPushButton * m_pushButton ;
		QEventLoop m_eventLoop ;
	};
}
namespace utility
{
	struct RandomDataSource
	{
		enum class types{ urandom,crand };
		static std::unique_ptr< RandomDataSource > get( types = types::urandom ) ;
		virtual ~RandomDataSource() ;
		virtual bool open() = 0 ;
		virtual qint64 getData( char * data,qint64 size ) = 0 ;
		virtual QByteArray getData( qint64 size ) = 0 ;
	} ;

	class UrandomDataSource : public RandomDataSource
	{
	public:
		UrandomDataSource() ;
		bool open() override ;
		qint64 getData( char * data,qint64 size ) override ;
		QByteArray getData( qint64 size ) override ;
	private:
		QFile m_file;
	} ;

	class CRandDataSource : public RandomDataSource
	{
	public:
		CRandDataSource() ;
		bool open() override ;
		qint64 getData( char * data,qint64 size ) override ;
		QByteArray getData( qint64 size ) override ;
	} ;

	class debug
	{
	public:
		debug( bool s = true ) : m_stdout( s )
		{
		}

		template< typename T >
		utility::debug operator<<( const T& e )
		{
			if( m_stdout ){

				std::cout << e << std::endl ;
			}else{
				std::cerr << e << std::endl ;
			}

			return utility::debug( m_stdout ) ;
		}

		utility::debug operator<<( const QByteArray& e )
		{
			if( m_stdout ){

				std::cout << e.constData() << std::endl ;
			}else{
				std::cerr << e.constData() << std::endl ;
			}

			return utility::debug( m_stdout ) ;
		}

		utility::debug operator<<( const QString& e )
		{
			if( m_stdout ){

				std::cout << e.toLatin1().constData() << std::endl ;
			}else{
				std::cerr << e.toLatin1().constData() << std::endl ;
			}

			return utility::debug( m_stdout ) ;
		}

		utility::debug operator<<( const QStringList& e )
		{
			if( m_stdout ){

				for( const auto& it : e ){

					std::cout << it.toLatin1().constData() << std::endl ;
				}
			}else{
				for( const auto& it : e ){

					std::cerr << it.toLatin1().constData() << std::endl ;
				}
			}

			return utility::debug( m_stdout ) ;
		}
	private:
		bool m_stdout ;
	};
}

namespace utility
{
	class selectMenuOption : public QObject
	{
		Q_OBJECT
	public:
		using function_t = std::function< void( const QString& e ) > ;

		selectMenuOption( QMenu * m,bool e,
				  function_t && f = []( const QString& e ){ Q_UNUSED( e ) } ) :
		m_menu( m ),m_function( f )
		{
			if( e ){

				this->setParent( m ) ;
			}
		}
	public slots :
		void selectOption( const QString& f )
		{
			for( const auto& it : m_menu->actions() ){

				QString e = it->text() ;

				e.remove( "&" ) ;

				it->setChecked( f == e ) ;
			}

			m_function( f ) ;
		}
		void selectOption( QAction * ac )
		{
			auto e = ac->text() ;

			e.remove( "&" ) ;

			this->selectOption( e ) ;
		}
	private:
		QMenu * m_menu ;
		std::function< void( const QString& ) > m_function ;
	};
}

namespace utility
{
	int getUID() ;
	int getUserID() ;
	int getGUID( int uid ) ;

	QString getStringUserID() ;
	QString appendUserUID( const QString& ) ;
	QString homePath() ;

	template< typename T >
	void changePathOwner( const T& f )
	{
		int uid = utility::getUID() ;
		int gid = utility::getGUID( uid ) ;

		int fd = f.handle() ;

		if( uid != -1 && fd != -1 ){

			if( fchown( fd,uid,gid ) ){;}
		}
	}

	static inline void changePathOwner( const char * path )
	{
		int uid = utility::getUID() ;
		int gid = utility::getGUID( uid ) ;

		if( uid != -1 ){

			if( chown( path,uid,gid ) ){;}
		}
	}

	static inline void changePathOwner( const QString& path )
	{
		utility::changePathOwner( path.toLatin1().constData() ) ;
	}

	template< typename T >
	void changePathPermissions( const T& f,int mode = 0666 )
	{
		if( fchmod( f.handle(),mode ) ){;}
	}

	static inline void changePathPermissions( const QString& f,int mode = 0666 )
	{
		if( chmod( f.toLatin1().constData(),mode ) ){;}
	}
}

namespace utility
{
	struct wallet
	{
		bool opened ;
		bool notConfigured ;
		QString key ;
	};

	bool libCryptSetupLibraryNotFound() ;
	int startApplication( const char * appName,std::function<int()> ) ;

	class invokeMethodImp : public QObject
	{
		Q_OBJECT
	public:
		void run()
		{
			emit start() ;

			this->deleteLater() ;
		}
	signals:
		void start() ;
	} ;

	class invokeMethodImp1 : public QObject
	{
		Q_OBJECT
	public:
		template< typename Function >
		invokeMethodImp1( Function f ) : m_function( std::move( f ) )
		{
			auto m = Qt::QueuedConnection ;
			connect( this,&invokeMethodImp1::start,this,&invokeMethodImp1::run,m ) ;
			emit start() ;
		}
		void run()
		{
			m_function() ;
			this->deleteLater() ;
		}
	signals:
		void start() ;
	private:
		std::function< void() > m_function ;
	} ;

	template< typename ReceiverObject,
		  typename Method,
		  typename std::enable_if< std::is_member_function_pointer< Method >::value,int >::type = 0 >
	void invokeMethod( ReceiverObject obj,Method method )
	{
		auto m = new invokeMethodImp() ;

		QObject::connect( m,&invokeMethodImp::start,obj,method,Qt::QueuedConnection ) ;

		m->run() ;

		//QTimer::singleShot( 0,obj,method ) ;
	}

	template< typename ReceiverObject,
		  typename Function,
		  typename std::enable_if< !std::is_member_function_pointer< Function >::value,int >::type = 0 >
	void invokeMethod( ReceiverObject obj,Function function )
	{
		Q_UNUSED( obj )
		new invokeMethodImp1( std::move( function ) ) ;
		//QTimer::singleShot( 0,obj,std::move( function ) ) ;
	}

	wallet getKey( LXQt::Wallet::Wallet&,const QString& keyID,
		       const QString& app = QString() ) ;

	QString cmdArgumentValue( const QStringList&,const QString& arg,
				  const QString& defaulT = QString() ) ;

	QIcon getIcon( const QString& ) ;
	void setIcons( const QString&,const QString& ) ;
	void setIconMenu( const QString& app,QAction * ac,QWidget *,
			  std::function< void( const QString& ) >&& ) ;

	QString fileSystemOptions( const QString& path ) ;

	QString autoSetVolumeAsVeraCrypt() ;
	void autoSetVolumeAsVeraCrypt( const QString& ) ;

	int defaultUnlockingVolumeType() ;
	void defaultUnlockingVolumeType( int ) ;

	bool showOnlyOccupiedSlots() ;
	void showOnlyOccupiedSlots( bool ) ;

	void autoOpenFolderOnMount( const QString&,bool ) ;
	bool autoOpenFolderOnMount( const QString& ) ;

	QProcessEnvironment systemEnvironment() ;

	enum class background_thread{ True,False } ;

	bool requireSystemPermissions( const QString&,utility::background_thread = utility::background_thread::False ) ;
	bool enablePolkit( utility::background_thread ) ;

	void setSettingsObject( QSettings * ) ;
	QSettings& settingsObject() ;

	::Task::future< utility2::result< QString > >& backEndInstalledVersion( const QString& backend ) ;

	::Task::future< utility2::result< bool > >& backendIsLessThan( const QString& backend,
								      const QString& version ) ;

	::Task::future< utility2::result< bool > >& backendIsGreaterOrEqualTo( const QString& backend,
									      const QString& version ) ;

	utility2::result< QByteArray > yubiKey( const QString& ) ;
	bool useDmCryptForRandomData() ;
	void setDefaultEnvironment() ;
	QString passwordSocketPath() ;
	QString socketPath() ;
	bool clearPassword() ;
	bool userBelongsToGroup( const char * groupname ) ;
	bool runningInMixedMode( void ) ;
	bool notRunningInMixedMode( void ) ;
	void addToFavorite( const QString& dev,const QString& m_point ) ;
	void readFavorites( QMenu *,bool = false,bool = true ) ;
	void setHDPI( const QString& ) ;
	bool pathExists( const QString& ) ;
	bool canCreateFile( const QString& ) ;
	bool useZuluPolkit( void ) ;
	struct startHelperStatus
	{
		std::function< void( bool,QString ) > success ;
		std::function< void() > error ;
	};

	void startHelper( QObject *,const QString&,const QString&,startHelperStatus ) ;
	void setDebugWindow( debugWindow * w ) ;
	QString deviceIDToPartitionID( const QString& ) ;
	QString fileManager( void ) ;
	QString loopDevicePath( const QString& ) ;
	void setFileManager( const QString& ) ;
	QString failedToStartzuluPolkit() ;
	QString prettyfySpaceUsage( quint64 ) ;
	QString resolvePath( const QString& ) ;
	QString hashPath( const QByteArray& ) ;
	QWidget * mainWindowWidget() ;
	void mainWindowWidget( QWidget * ) ;
	QString cryptMapperPath( void ) ;
	QString mapperPath( const QString&,const QString& component = QString() ) ;
	QString getVolumeID( const QString&,bool = false ) ;
	bool userIsRoot( void ) ;
	bool mapperPathExists( const QString& path ) ;
	QString mountPath( const QString& ) ;
	QString homeMountPath( const QString& ) ;
	QString mountPathPostFix( const QString& ) ;
	QString userName( void ) ;
	QString shareMountPointToolTip( void ) ;
	QString shareMountPointToolTip( const QString& ) ;
	QString sharedMountPointPath( const QString& ) ;
	QString defaultPlugin( void ) ;
	void setDefaultPlugin( const QString& ) ;
	bool pathPointsToAFile( const QString& ) ;
	bool pathPointsToAFolder( const QString& ) ;
	bool configDirectoriesAreNotWritable( QWidget * ) ;
	QString localizationLanguage( const QString& ) ;
	QString localizationLanguagePath( const QString& ) ;
	void setLocalizationLanguage( const QString&,const QString& ) ;
	QString walletName( void ) ;
	QString applicationName( void ) ;
	QString readPassword( bool = true ) ;
	bool pathIsReadable( const QString&,bool isFolder = true ) ;
	bool pathIsWritable( const QString&,bool isFolder = true ) ;
	bool setOpenVolumeReadOnly( QWidget * parent,bool check,const QString& app ) ;
	bool getOpenVolumeReadOnlyOption( const QString& app ) ;
	QString keyPath( void ) ;
	void keySend( const QString& keyPath,const QByteArray& key ) ;
	void keySend( const QString& keyPath,const QString& key ) ;
	bool eventFilter( QObject * gui,QObject * watched,QEvent * event,std::function< void() > ) ;
	QStringList split( const QString&,char token = '\n' ) ;
	QStringList split( const QByteArray&,char token = '\n' ) ;
	QStringList split( const QByteArray&,const char * token ) ;
	QStringList split( const QString&,const char * token ) ;
	QStringList directoryList( const QString& ) ;
	QStringList plainDmCryptOptions( void ) ;
	QStringList supportedFileSystems( void ) ;
	QString KWalletDefaultName( void ) ;
	bool userHasGoodVersionOfWhirlpool( void ) ;
	void licenseInfo( QWidget * ) ;
	void showTrayIcon( QAction *,QSystemTrayIcon&,bool = true ) ;
	void trayProperty( QSystemTrayIcon *,bool = true ) ;
	void setDoNotMinimizeToTray( bool ) ;
	bool doNotMinimizeToTray() ;
	bool mountWithSharedMountPoint() ;
	void mountWithSharedMountPoint( bool ) ;
	bool platformIsLinux( void ) ;
	bool platformIsOSX( void ) ;
	QString pathToUUID( const QString& path ) ;
	QStringList executableSearchPaths( void ) ;
	QString executableSearchPaths( const QString& ) ;
	QString helperSocketPath( void ) ;
	void quitHelper( void ) ;
	std::pair< bool,QByteArray > getKeyFromNetwork( const QString& ) ;

	QString powerOffCommand( void ) ;

	void setFileSystemOptions( QString& exe,
				   const QString& device,
				   const QString& mountpoint,
				   const QString& mountOptions = QString() ) ;

	int favoriteClickedOption( const QString& ) ;

	QString executableFullPath( const QString& ) ;

	bool showWarningOnExtendingCoverFile() ;
	void showWarningOnExtendingCoverFile( bool ) ;

	bool reUseMountPointPath( void ) ;
	bool reUseMountPoint( void ) ;

	bool readOnlyOption( void ) ;
	void readOnlyOption( bool ) ;

	bool canShowKeySlotProperties() ;

	bool readOnlyWarning( void ) ;
	void readOnlyWarning( bool ) ;

	void polkitFailedWarning( std::function< void() > ) ;

	void setLocalizationLanguage( bool translate,QMenu * ac,const QString& ) ;
	void languageMenu( QWidget *,QMenu *,QAction *,const char * ) ;

	bool unMountVolumesOnLogout( void ) ;

	using array_t = std::array< int,10 > ;

	utility::array_t getWindowDimensions( const QString& application ) ;
	void setWindowDimensions( const QString& application,const std::initializer_list<int>& ) ;

	void addPluginsToMenu( QMenu& ) ;

	int pluginKey( QWidget *,QByteArray *,const QString& ) ;

	std::pair< bool,QByteArray > privilegedReadConfigFile( const QString& ) ;
	void privilegedWriteConfigFile( const QByteArray&,const QString& ) ;

	QFont getFont( QWidget * ) ;
	void saveFont( const QFont& ) ;

	::Task::future< int >& clearVolume( const QString& volumePath,
					    std::atomic_bool * exit,
					    size_t size,
					    std::function< void( quint64 size,quint64 offset ) > ) ;
	::Task::future< int >& exec( const QString& ) ;
	::Task::future< QStringList >& luksEmptySlots( const QString& volumePath ) ;
	::Task::future< QString >& getUUIDFromPath( const QString& ) ;
	::Task::future< bool >& openPath( const QString& path,const QString& opener ) ;

	void openPath( const QString& path,const QString& opener,QWidget *,const QString&,const QString& ) ;
}

namespace utility
{
	template< typename ... F >
	bool atLeastOnePathExists( const F& ... f ){

		for( const auto& it : { f ... } ){

			if( utility::pathExists( it ) ){

				return true ;
			}
		}

		return false ;
	}

	template< typename E,typename ... F >
	bool containsAtleastOne( const E& e,const F& ... f )
	{
		for( const auto& it : { f ... } ){

			if( e.contains( it ) ){

				return true ;
			}
		}

		return false ;
	}

	template< typename E,typename ... F >
	bool startsWithAtLeastOne( const E& e,const F& ... f )
	{
		for( const auto& it : { f ... } ){

			if( e.startsWith( it ) ){

				return true ;
			}
		}

		return false ;
	}

	template< typename E,typename ... F >
	bool endsWithAtLeastOne( const E& e,const F& ... f )
	{
		for( const auto& it : { f ... } ){

			if( e.endsWith( it ) ){

				return true ;
			}
		}

		return false ;
	}

	template< typename E,typename ... F >
	bool equalsAtleastOne( const E& e,const F& ... f )
	{
		for( const auto& it : { f ... } ){

			if( e == it ){

				return true ;
			}
		}

		return false ;
	}
}

namespace utility
{
	class fileHandle
	{
	public:
		fileHandle()
		{
		}
		fileHandle( int r ) : m_fd( r )
		{
		}
		fileHandle( int r,std::function< void( int ) > cmd ) :
			m_fd( r ),m_releaseResource( std::move( cmd ) )
		{
		}
		bool open( const char * filePath,bool ro = true )
		{
			if( ro ){

				m_fd = ::open( filePath,O_RDONLY ) ;
			}else{
				m_fd = ::open( filePath,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH ) ;
			}

			m_path = filePath ;

			return m_fd != -1 ;
		}
		bool open( const QString& filePath,bool ro = true )
		{
			return this->open( filePath.toLatin1().constData(),ro ) ;
		}
		bool isFile()
		{
			struct stat st ;
			fstat( m_fd,&st ) ;
			return S_ISREG( st.st_mode ) != 0 ;
		}
		bool isFolder()
		{
			struct stat st ;
			fstat( m_fd,&st ) ;
			return S_ISDIR( st.st_mode ) != 0 ;
		}
		quint64 size()
		{
			return static_cast< quint64 >( blkid_get_dev_size( m_fd ) ) ;
		}
		void unlink()
		{
			m_unlink = true ;
		}
		int handle() const
		{
			return m_fd ;
		}
		const char * path()
		{
			return m_path.constData() ;
		}
		bool opened() const
		{
			return m_fd != -1 ;
		}
		char getChar() const
		{
			char z ;

			while( true ){

				while( ::read( m_fd,&z,1 ) != 1 ){;}

				if( z > ' ' && z < '~' ){

					/*
					 * we are creating a keyfile that is made up
					 * of only printable characters
					 */
					break ;
				}
			}

			return z ;
		}
		void writeChar( char r ) const
		{
			if( ::write( m_fd,&r,1 ) ){;}
		}
		~fileHandle()
		{
			m_releaseResource( m_fd ) ;

			if( m_unlink ){

				::unlink( m_path.constData() ) ;
			}
		}
	private:
		bool m_unlink = false ;

		int m_fd = -1 ;

		QByteArray m_path ;

		std::function< void( int ) > m_releaseResource = []( int fd ){

			if( fd != -1 ){

				::close( fd ) ;
			}
		} ;
	} ;
}

namespace utility
{
	class monitor_mountinfo
	{
	public:
		monitor_mountinfo()
		{
			m_handle.open( "/proc/self/mountinfo" ) ;
			m_monitor.fd     = m_handle.handle() ;
			m_monitor.events = POLLPRI ;
		}
		operator bool()
		{
			return m_handle.opened() ;
		}
		bool gotEvent() const
		{
			poll( &m_monitor,1,-1 ) ;
			return true ;
		}
	private:
		utility::fileHandle m_handle ;
		mutable struct pollfd m_monitor ;
	};
}

namespace utility
{
	class Task
	{
	public :
		enum class USEPOLKIT{ True,False } ;
		static ::Task::future< utility::Task >& run( const QString& exe,USEPOLKIT e ) ;

		static ::Task::future< utility::Task >& run( const QString& exe,int,USEPOLKIT e ) ;

		static ::Task::future< utility::Task >& run( const QString& exe )
		{
			return ::Task::run( [ exe ](){ return utility::Task( exe ) ; } ) ;
		}
		static void wait( int s )
		{
			sleep( s ) ;
		}
		static void waitForOneSecond( void )
		{
			sleep( 1 ) ;
		}
		static void waitForTwoSeconds( void )
		{
			sleep( 2 ) ;
		}
		static void suspendForOneSecond( void )
		{
			utility::Task::suspend( 1 ) ;
		}
		static void suspend( int s )
		{
			QTimer t ;

			QEventLoop l ;

			QObject::connect( &t,SIGNAL( timeout() ),&l,SLOT( quit() ) ) ;

			t.start( 1000 * s ) ;

			l.exec() ;
		}
		static QString makePath( QString e )
		{
			e.replace( "\"","\"\"\"" ) ;

			return "\"" + e + "\"" ;
		}
		Task()
		{
		}
		Task( const QString& exe,int waitTime = -1,const QProcessEnvironment& env = utility::systemEnvironment(),
		      const QByteArray& password = QByteArray(),const std::function< void() >& f = [](){},USEPOLKIT e = USEPOLKIT::True )
		{
			this->execute( exe,waitTime,env,password,f,e ) ;
		}
		Task( const QString& exe,const QProcessEnvironment& env,const std::function< void() >& f,USEPOLKIT e = USEPOLKIT::True )
		{
			this->execute( exe,-1,env,QByteArray(),f,e ) ;
		}
		QStringList splitOutput( char token ) const
		{
			return utility::split( m_stdOut,token ) ;
		}
		void stdOut( const QByteArray& r )
		{
			m_stdOut = r ;
		}
		const QByteArray& stdOut() const
		{
			return m_stdOut ;
		}
		const QByteArray& stdError() const
		{
			return m_stdError ;
		}
		int exitCode() const
		{
			return m_exitCode ;
		}
		int exitStatus() const
		{
			return m_exitStatus ;
		}
		bool success() const
		{
			return m_exitCode == 0 && m_exitStatus == QProcess::NormalExit && m_finished == true ;
		}
		bool failed() const
		{
			return !this->success() ;
		}
		bool finished() const
		{
			return m_finished ;
		}
		bool ok() const
		{
			return this->splitOutput( '\n' ).size() > 12 ;
		}
	private:
		void execute( const QString& exe,int waitTime,const QProcessEnvironment& env,
			      const QByteArray& password,std::function< void() > f,USEPOLKIT ) ;
		QByteArray m_stdOut ;
		QByteArray m_stdError ;

		int m_exitCode ;
		int m_exitStatus ;
		bool m_finished ;
	};
}

namespace utility
{
	class veraCryptWarning : public QObject
	{
		Q_OBJECT
	public:
		veraCryptWarning()
		{
			connect( &m_timer,SIGNAL( timeout() ),this,SLOT( update() ) ) ;
		}
		void setWarningLabel( QLabel * l )
		{
			m_label = l ;
			m_label->setVisible( false ) ;
		}
		void show( QString w )
		{
			m_warning = std::move( w ) ;
			this->show( true ) ;
		}
		void stopTimer()
		{
			m_timer.stop() ;
		}
		void show( bool show )
		{
			if( show ){
				m_label->setVisible( true ) ;
				m_timer.start( 1000 * 1 ) ;
				this->update() ;
			}
		}
		void hide()
		{
			m_timer.stop() ;
			m_time = 0 ;
			m_label->setVisible( false ) ;
			m_label->setText( m_warning + tr( "Elapsed time: 0 seconds" ) ) ;
		}
	private slots:
		void update()
		{
			QString e ;
			if( m_time >= 60 ){

				e = tr( "Elapsed time: %0 minutes" ).arg( QString::number( m_time / 60,'f',2 ) ) ;
			}else{
				e = tr( "Elapsed time: %0 seconds" ).arg( QString::number( m_time ) ) ;
			}

			m_time++ ;

			m_label->setText( m_warning + e ) ;
		}
	private:
		QLabel * m_label ;
		QTimer m_timer ;
		float m_time = 0 ;
		QString m_warning = tr( "Please be patient as unlocking a VeraCrypt volume may take a very long time.\n\n" ) ;
	};
}

namespace utility
{
	static inline ::Task::future< void > * startTask( std::function< void() > task,
							  std::function< void() > continuation )
	{
		auto& e = ::Task::run( std::move( task ) ) ;

		e.then( std::move( continuation ) ) ;

		return std::addressof( e ) ;
	}

	static inline void stopTask( ::Task::future< void > * task,
				      std::function< void() >& function )
	{
		if( task ){

			auto e = task->first_thread() ;

			if( e->isRunning() ){

				e->terminate() ;
			}else{
				function() ;
			}
		}else{
			function() ;
		}
	}
}

namespace utility {
	class duration{
	public:
		duration( long miliseconds ) ;
		bool passed() ;
		void reset() ;
		template< typename Function >
		void passed( Function&& function )
		{
			if( this->passed() ){

				function() ;
			}
		}
		QDateTime& timer() ;
	private:
		long m_milliseconds ;
		qint64 m_start_time ;
		QDateTime m_time ;
	};
}

namespace utility{

static inline void Timer( int interval,std::function< bool( int ) > function )
{
	class Timer{
	public:
		Timer( int interval,std::function< bool( int ) > function ) :
			m_function( std::move( function ) )
		{
			auto timer = new QTimer() ;

			QObject::connect( timer,&QTimer::timeout,[ timer,this ](){

				m_counter++ ;

				if( m_function( m_counter ) ){

					timer->stop() ;

					timer->deleteLater() ;

					delete this ;
				}
			} ) ;

			timer->start( interval ) ;
		}
	private:
		int m_counter = 0 ;
		std::function< bool( int ) > m_function ;
	} ;

	new Timer( interval,std::move( function ) ) ;
}

class progress{
public:
	struct result
	{
		const QString& current_speed ;
		const QString& average_speed ;
		const QString& eta ;
		const QString& total_time ;
		int percentage_done ;
	} ;

	progress( int s,std::function< void( const result& ) > function ) ;
	std::function< void( quint64 size,quint64 offset ) > updater_quint() ;
	std::function< void( qint64 size,qint64 offset ) > updater_qint() ;
private:
	void update_progress( quint64 size,quint64 offset ) ;
	QString time( double s ) ;
	QString speed( double size,double time ) ;

	int m_progress = 0 ;
	quint64 m_offset_last ;
	double m_total_time ;
	std::function< void( const result& ) > m_function ;
	utility::duration m_duration ;
	QDateTime& m_time ;
	double m_previousTime ;
};

}
#endif // MISCFUNCTIONS_H
