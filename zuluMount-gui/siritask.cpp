/*
 *
 *  Copyright (c) 2014-2015
 *  name : Francis Banyikwa
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "siritask.h"
#include "bin_path.h"

#include <QDir>
#include <QString>
#include <QDebug>
#include <QFile>

using cs = siritask::status ;

static QString _makePath( const QString& e )
{
	return utility::Task::makePath( e ) ;
}

static bool _m( const QString& e )
{
	return utility::Task( utility::appendUserUID( e ) ).success() ;
}

static bool _delete_folder( const QString& m )
{
	return _m( QString( "%1 -b %2" ).arg( zuluMountPath,m ) ) ;
}

static bool _create_folder( const QString& m )
{
	return _m( QString( "%1 -B %2" ).arg( zuluMountPath,m ) ) ;
}

template< typename ... T >
static bool _deleteFolders( const T& ... m )
{
	bool s = false ;

	for( const auto& it : { m ... } ){

		s = _delete_folder( it ) ;
	}

	return s ;
}

template< typename T >
static bool _ecryptfs( const T& e )
{
	return utility::equalsAtleastOne( e,"ecryptfs","ecryptfs-simple" ) ;
}

template< typename T >
static utility::Task::USEPOLKIT _ecryptfs_1( const T& e )
{
	if( _ecryptfs( e ) ){

		return utility::Task::USEPOLKIT::True ;
	}else{
		return utility::Task::USEPOLKIT::False ;
	}
}

static bool _ecryptfs_illegal_path( const siritask::options& opts )
{
	if( _ecryptfs( opts.type ) && utility::useZuluPolkit() ){

		return opts.cipherFolder.contains( " " ) || opts.plainFolder.contains( " " ) ;
	}else{
		return false ;
	}
}

static QString _wrap_su( const QString& s )
{
	auto su = utility::executableFullPath( "su" ) ;

	if( su.isEmpty() ){

		return s ;
	}else{
		return QString( "%1 - -c \"%2\"" ).arg( su,QString( s ).replace( "\"","'" ) ) ;
	}
}

bool siritask::deleteMountFolder( const QString& m )
{
	if( utility::reUseMountPoint() ){

		return false ;
	}else{
		return _deleteFolders( m ) ;
	}
}

Task::future< bool >& siritask::encryptedFolderUnMount( const QString& cipherFolder,
							const QString& mountPoint,
							const QString& fileSystem )
{
	return Task::run( [ = ](){

		auto ecryptfs = _ecryptfs( fileSystem ) ;

		auto cmd = [ & ](){

			if( ecryptfs ){

				auto exe = utility::executableFullPath( "ecryptfs-simple" ) ;

				auto s = exe + " -k " + _makePath( cipherFolder ) ;

				if( utility::useZuluPolkit() ){

					return _wrap_su( s ) ;
				}else{
					return s ;
				}
			}else{
				if( utility::platformIsOSX() ){

					return "umount " + _makePath( mountPoint ) ;
				}else{
					return "fusermount -u " + _makePath( mountPoint ) ;
				}
			}
		} ;

		const int max_count = 5 ;

		if( ecryptfs ){

			bool not_set = true ;

			auto exe = cmd() ;

			for( int i = 0 ; i < max_count ; i++ ){

				auto s = utility::Task::run( exe,10000,utility::Task::USEPOLKIT::True ).get() ;

				if( s.success() ){

					return true ;
				}else{
					if( not_set && s.stdError().contains( "error: failed to set gid" ) ){

						if( utility::enablePolkit( utility::background_thread::True ) ){

							not_set = false ;

							exe = cmd() ;
						}else{
							return false ;
						}
					}else{
						utility::Task::waitForOneSecond() ;
					}
				}
			}
		}else{
			for( int i = 0 ; i < max_count ; i++ ){

				auto s = utility::Task::run( cmd(),10000,utility::Task::USEPOLKIT::False ).get() ;

				if( s.success() ){

					return true ;
				}else{
					utility::Task::waitForOneSecond() ;
				}
			}
		}

		return false ;
	} ) ;
}

static QString _args( const QString& exe,const siritask::options& opt,
		      const QString& configFilePath,
		      bool create )
{
	auto cipherFolder = _makePath( opt.cipherFolder ) ;

	auto mountPoint   = _makePath( opt.plainFolder ) ;

	const auto& type = opt.type ;

	auto mountOptions = [ & ](){

		if( !opt.mOpt.isEmpty() ){

			if( type == "cryfs" ){

				return QString( "--unmount-idle %1" ).arg( opt.mOpt ) ;

			}else if( type == "encfs" ){

				return QString( "--idle=%1" ).arg( opt.mOpt ) ;
			}
		}

		return QString() ;
	}() ;

	auto separator = [ & ](){

		if( type == "cryfs" ){

			return "--" ;

		}else if( type == "encfs" ){

			return "-S" ;
		}else{
			return "" ;
		}
	}() ;

	auto configPath = [ & ](){

		if( type.isOneOf( "cryfs","gocryptfs","securefs","ecryptfs" ) ){

			if( !configFilePath.isEmpty() ){

				return "--config " + _makePath( configFilePath ) ;
			}
		}

		return QString() ;
	}() ;

	if( type.isOneOf( "gocryptfs","securefs" ) ){

		QString mode = [ & ](){

			if( opt.ro ){

				return "-o ro" ;
			}else{
				return "-o rw" ;
			}
		}() ;

		if( type == "gocryptfs" ){

			if( create ){

				auto e = QString( "%1 --init %2 %3" ) ;
				return e.arg( exe,configPath,cipherFolder ) ;
			}else{

				mode += ",fsname=gocryptfs@" + cipherFolder ;

				auto e = QString( "%1 %2 %3 %4 %5" ) ;
				return e.arg( exe,configPath,cipherFolder,mountPoint,mode ) ;
			}
		}else{
			if( create ){

				auto e = QString( "%1 create %2 %3" ) ;
				return e.arg( exe,configPath,cipherFolder ) ;
			}else{
				auto e = QString( "%1 mount -b %2 %3 %4 %5,fsname=securefs@%6,subtype=securefs" ) ;
				return e.arg( exe,configPath,cipherFolder,mountPoint,mode,cipherFolder ) ;
			}
		}

	}else if( _ecryptfs( type ) ){

		auto _options = []( const std::initializer_list< const char * >& e ){

			QString q = "-o key=passphrase" ;

			for( const auto& it : e ){

				q += it ;
			}

			return q ;
		} ;

		auto mode = [ & ](){

			if( opt.ro ){

				return "--readonly" ;
			}else{
				return "" ;
			}
		}() ;

		auto e = QString( "%1 %2 %3 -a %4 %5 %6" ) ;

		auto s = [ & ]{

			if( create ){

				auto s = _options( { ",ecryptfs_passthrough=n",
						     ",ecryptfs_enable_filename_crypto=y",
						     ",ecryptfs_key_bytes=32",
						     ",ecryptfs_cipher=aes" } ) ;

				return e.arg( exe,s,mode,configPath,cipherFolder,mountPoint ) ;
			}else{
				return e.arg( exe,_options( {} ),mode,configPath,cipherFolder,mountPoint ) ;
			}
		}() ;

		if( utility::runningInMixedMode() ){

			return _wrap_su( s ) ;
		}else{
			return s ;
		}
	}else{
		auto e = QString( "%1 %2 %3 %4 %5 %6 -o fsname=%7@%8,subtype=%9" ) ;

		auto opts = e.arg( exe,cipherFolder,mountPoint,mountOptions,configPath,
				   separator,type.name(),cipherFolder,type.name() ) ;

		if( opt.ro ){

			return opts + ",ro" ;
		}else{
			return opts + ",rw" ;
		}
	}
}

enum class status_type{ exeName,exeNotFound } ;
static siritask::status _status( const siritask::volumeType& app,status_type s )
{
	if( s == status_type::exeNotFound ){

		if( app == "cryfs" ){

			return cs::cryfsNotFound ;

		}else if( app == "encfs" ){

			return cs::encfsNotFound ;

		}else if( app == "securefs" ){

			return cs::securefsNotFound ;

		}else if( _ecryptfs( app ) ){

			return cs::ecryptfs_simpleNotFound ;
		}else{
			return cs::gocryptfsNotFound ;
		}
	}else{
		if( app == "cryfs" ){

			return cs::cryfs ;

		}else if( app == "encfs" ){

			return cs::encfs ;

		}else if( app == "securefs" ){

			return cs::securefs ;

		}else if( _ecryptfs( app ) ){

			return cs::ecryptfs ;
		}else{
			return cs::gocryptfs ;
		}
	}
}

static siritask::cmdStatus _status( const utility::Task& r,siritask::status s,bool stdOut )
{
	if( r.success() ){

		return siritask::status::success ;
	}

	siritask::cmdStatus e = { r.exitCode(),stdOut ? r.stdOut() : r.stdError() } ;

	const auto msg = e.msg().toLower() ;

	/*
	 *
	 * When trying to figure out what error occured,check for status value
	 * if the backend supports them and fallback to parsing output strings
	 * if backend does not support error codes.
	 *
	 */

	if( s == siritask::status::ecryptfs ){

		if( msg.contains( "operation not permitted" ) ){

			e = siritask::status::ecrypfsBadExePermissions ;

		}else if( msg.contains( "error: mount failed" ) ){

			e = s ;
		}

	}else if( s == siritask::status::cryfs ){

		const auto& m = e.msg() ;

		if( msg.contains( "password" ) ){

			e = s ;

		}else if( m.contains( "This filesystem is for CryFS" ) &&
			  m.contains( "It has to be migrated" ) ){

			e = siritask::status::cryfsMigrateFileSystem ;
		}

	}else if( s == siritask::status::encfs ){

		if( msg.contains( "password" ) ){

			e = s ;
		}

	}else if( s == siritask::status::gocryptfs ){

		/*
		 * This error code was added in gocryptfs 1.2.1
		 */
		if( e.exitCode() == 12 ){

			e = s ;
		}else{
			if( msg.contains( "password" ) ){

				e = s ;
			}
		}

	}else if( s == siritask::status::securefs ){

		if( msg.contains( "password" ) ){

			e = s ;
		}
	}

	return e ;
}

static siritask::cmdStatus _cmd( bool create,const siritask::options& opt,
		const QString& password,const QString& configFilePath )
{
	const auto& app = opt.type ;

	auto exe = app.executableFullPath() ;

	if( exe.isEmpty() ){

		return _status( app,status_type::exeNotFound ) ;
	}else{
		auto _run = [ & ](){

			auto s = utility::Task( _args( exe,opt,configFilePath,create ),
						20000,
						utility::systemEnvironment(),
						password.toLatin1(),
						[](){},
						_ecryptfs_1( app ) ) ;

			return _status( s,_status( app,status_type::exeName ),app == "encfs" ) ;
		} ;

		auto e = _run() ;

		if( e == siritask::status::ecrypfsBadExePermissions ){

			if( utility::enablePolkit( utility::background_thread::True ) ){

				e = _run() ;
			}
		}

		if( e != siritask::status::success ){

			auto s = QString::number( e.exitCode() ) ;
			auto m = e.msg() ;

			while( true ){

				if( m.endsWith( '\n' ) ){

					m.truncate( m.size() - 1 ) ;
				}else{
					break ;
				}
			}

			utility::debug() << "-------------------------" ;
			utility::debug() << QString( "Backend Generated Output:\nExit Code: %1" ).arg( s ) ;
			utility::debug() << QString( "Exit String: \"%1\"" ).arg( m ) ;
			utility::debug() << "-------------------------" ;
		}

		return e ;
	}
}

static QString _configFilePath( const siritask::options& opt )
{
	if( opt.configFilePath.startsWith( "/" ) || opt.configFilePath.isEmpty() ){

		return opt.configFilePath ;
	}else{
		return utility::homePath() + "/" + opt.configFilePath ;
	}
}

Task::future< siritask::cmdStatus >& siritask::encryptedFolderMount( const options& opt,bool reUseMountPoint )
{
	return Task::run( [ opt,reUseMountPoint ]()->siritask::cmdStatus{

		auto _mount = [ reUseMountPoint ]( const QString& app,const options& copt,
				const QString& configFilePath )->siritask::cmdStatus{

			auto opt = copt ;

			opt.type = app ;

			if( _ecryptfs_illegal_path( opt ) ){

				return cs::ecryptfsIllegalPath ;
			}

			if( _create_folder( opt.plainFolder ) || reUseMountPoint ){

				auto e = _cmd( false,opt,opt.key,configFilePath ) ;

				if( e == cs::success ){

					opt.openFolder( opt.plainFolder ) ;
				}else{
					siritask::deleteMountFolder( opt.plainFolder ) ;
				}

				return e ;
			}else{
				return cs::failedToCreateMountPoint ;
			}
		} ;

		if( opt.configFilePath.isEmpty() ){

			if( utility::pathExists( opt.cipherFolder + "/cryfs.config" ) ){

				return _mount( "cryfs",opt,QString() ) ;

			}else if( utility::pathExists( opt.cipherFolder + "/gocryptfs.conf" ) ){

				return _mount( "gocryptfs",opt,QString() ) ;

			}else if( utility::pathExists( opt.cipherFolder + "/.securefs.json" ) ){

				return _mount( "securefs",opt,QString() ) ;

			}else if( utility::pathExists( opt.cipherFolder + "/.ecryptfs.config" ) ){

				return _mount( "ecryptfs",opt,opt.cipherFolder + "/.ecryptfs.config" ) ;
			}else{
				auto encfs6 = opt.cipherFolder + "/.encfs6.xml" ;
				auto encfs5 = opt.cipherFolder + "/.encfs5" ;
				auto encfs4 = opt.cipherFolder + "/.encfs4" ;

				if( utility::atLeastOnePathExists( encfs6,encfs5,encfs4 ) ){

					return _mount( "encfs",opt,QString() ) ;
				}else{
					return cs::unknown ;
				}
			}
		}else{
			auto e = _configFilePath( opt ) ;

			if( utility::pathExists( e ) ){

				if( e.endsWith( "gocryptfs.conf" ) ){

					return _mount( "gocryptfs",opt,e ) ;

				}else if( e.endsWith( "securefs.json" ) ){

					return _mount( "securefs",opt,e ) ;

				}else if( e.endsWith( "ecryptfs.config" ) ){

					return _mount( "ecryptfs",opt,e ) ;

				}else if( e.endsWith( "cryfs.config" ) ){

					return _mount( "cryfs",opt,e ) ;
				}else{
					return cs::unknown ;
				}
			}else{
				return cs::unknown ;
			}
		}
	} ) ;
}

Task::future< siritask::cmdStatus >& siritask::encryptedFolderCreate( const options& opt )
{
	return Task::run( [ opt ]()->siritask::cmdStatus{

		if( _create_folder( opt.cipherFolder ) ){

			if( _create_folder( opt.plainFolder ) ){

				auto e = _cmd( true,opt,[ & ]()->QString{

					if( opt.type.isOneOf( "cryfs","gocryptfs" ) ){

						return opt.key ;

					}else if( opt.type == "securefs" ){

						return opt.key + "\n" + opt.key ;

					}else if( opt.type == "ecryptfs" ){

						return opt.key ;
					}else{
						return "p\n" + opt.key ;
					}

				}(),[ & ](){

					auto e = _configFilePath( opt ) ;

					if( e.isEmpty() && opt.type == "ecryptfs" ){

						return opt.cipherFolder + "/.ecryptfs.config" ;
					}else{
						return e ;
					}
				}() ) ;

				if( e == cs::success ){

					if( opt.type.isOneOf( "gocryptfs","securefs" ) ){

						e = siritask::encryptedFolderMount( opt,true ).get() ;

						if( e != cs::success ){

							_deleteFolders( opt.cipherFolder,opt.plainFolder ) ;
						}
					}else{
						opt.openFolder( opt.plainFolder ) ;
					}
				}else{
					_deleteFolders( opt.plainFolder,opt.cipherFolder ) ;
				}

				return e ;
			}else{
				_deleteFolders( opt.cipherFolder ) ;

				return cs::failedToCreateMountPoint ;
			}
		}else{
			return cs::failedToCreateMountPoint ;
		}
	} ) ;
}
