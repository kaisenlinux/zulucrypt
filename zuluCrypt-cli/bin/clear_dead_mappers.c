 /*
 *
 *  Copyright (c) 2012-2015
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

#include "includes.h"
#include "../lib/includes.h"
#include "mount_prefix_path.h"
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libintl.h>
#include <libcryptsetup.h>

static void _remove_mapper( const char * path,stringList_t stl,uid_t uid )
{
	char * m_point = NULL ;
	/*
	 * zuluCryptBindUnmountVolume() is defined in ./bind.c
	 */
	int r = zuluCryptBindUnmountVolume( stl,path,uid ) ;

	if( r == 3 || r == 4 ){
		/*
		 * shared mount is busy or belong to another user
		 */
		return ;
	}

	/*
	 * zuluCryptCloseVolume() is defined in ../lib/close_volume.c
	 */
	r = zuluCryptCloseVolume( path,&m_point ) ;

	if( r == 0 && m_point != NULL ){

		remove( m_point ) ;
		StringFree( m_point ) ;
	}
}

static void _zuluCryptDeleteDeadMountPoints( stringList_t stl,const char * m )
{
	struct dirent * entry ;
	const char * e ;
	string_t st ;
	string_t xt ;

	const char * bl = zuluCryptBitLockerFolderPrefix() ;

	DIR * dir = opendir( m ) ;

	if( dir == NULL ){

		return ;
	}

	while( ( entry = readdir( dir ) ) != NULL ){

		e = entry->d_name ;

		if( !StringAtLeastOneMatch_1( e,".","..",bl,NULL ) ){

			st = String( e ) ;

			zuluCryptEncodeMountEntry( st ) ;

			StringMultiplePrepend( st,"/",m,NULL ) ;

			e = StringAppend( st," " ) ;

			if( StringListHasSequence( stl,e ) < 0 ){

				xt = String_1( m,"/",entry->d_name,NULL ) ;

				if( rmdir( StringContent( xt ) ) != 0 ){

					/*
					 * Failed to delete an unmounted folder for some reason
					 */
				}

				StringDelete( &xt ) ;
			}

			StringDelete( &st ) ;
		}
	}
}

static void _unmount_dead_mount_points( uid_t uid )
{
	stringList_t stl = zuluCryptPartitions( ZULUCRYPTallPartitions,uid ) ;

	stringList_t stx = zuluCryptGetAListOfMountedVolumes() ;

	StringListIterator it ;
	StringListIterator end ;

	string_t st ;

	char * mout_point = NULL ;

	char * m_point = NULL ;

	const char * device ;

	StringListGetIterators( stl,&it,&end ) ;

	while( it != end ){

		st = *it ;

		it++ ;

		StringListRemoveIfPresent_1( stx,st ) ;
	}

	StringListGetIterators( stx,&it,&end ) ;

	while( it != end ){

		st = *it ;

		it++ ;

		if( StringStartsWith( st,"/dev/" ) ){

			device = StringContent( st ) ;

			mout_point = zuluCryptGetMountPointFromPath( device ) ;

			printf( "unmounting : %s\n",device ) ;

			if( zuluCryptUnmountVolume( device,&m_point ) == 0 ){

				if( m_point ){

					rmdir( m_point ) ;
					printf( "unmounted : %s:%s\n",device,m_point ) ;
				}
			}else{
				perror( "failed" ) ;
			}
		}
	}

	StringFree( mout_point ) ;
	StringFree( m_point ) ;
}

void zuluCryptDeleteDeadMountPoints( uid_t uid,stringList_t stl )
{
	string_t st = zuluCryptGetUserName( uid ) ;

	_zuluCryptDeleteDeadMountPoints( stl,StringPrepend( st,"/run/media/private/" ) ) ;
	_zuluCryptDeleteDeadMountPoints( stl,"/run/media/public" ) ;

	_unmount_dead_mount_points( uid ) ;

	StringDelete( &st ) ;
}

void zuluCryptClearDeadMappers( uid_t uid,int s )
{
	struct crypt_device * cd ;
	const char * dir_path = crypt_get_dir() ;
	DIR * dir = opendir( dir_path ) ;
	struct dirent * entry ;
	const char * m ;
	const char * e ;

	char * r ;

	size_t len ;
	size_t len1 ;

	stringList_t stl ;
	string_t p ;
	string_t z ;

	if( dir == NULL ){

		return ;
	}

	/*
	 * zuluCryptGetMoutedList_1()  is defined in ../lib/process_mountinfo.c
	 */
	stl = zuluCryptGetMoutedList_1() ;
	z = String_1( dir_path,"/",NULL ) ;

	len1 = StringLength( z ) ;

	p = String( "zuluCrypt-" ) ;
	m = StringAppendInt( p,uid ) ;
	len = StringLength( p ) ;

	/*
	 * zuluCryptSecurityGainElevatedPrivileges() is defined in security.c
	 */
	zuluCryptSecurityGainElevatedPrivileges() ;

	while( ( entry = readdir( dir ) ) != NULL ){

		if( StringPrefixMatch( entry->d_name,m,len ) ){

			e = StringAppendAt( z,len1,entry->d_name ) ;

			/*
			 * zuluCryptTrueCryptOrVeraCryptVolume() is defined in ../lib/status.c
			 */
			if( zuluCryptVolumeManagedByTcplay( e ) ){

				/*
				 * zuluCryptVolumeDeviceName() is defined in ../lib/status.c
				 */
				r = zuluCryptVolumeDeviceName( e ) ;

				if( *( r + 0 ) != '/' ){

					/*
					 * tcplay seems to report device name as something like "8:33"
					 * when a mapper exists but its underlying device is gone and we exploit
					 * this behavior by checking if path starts with "/" and we assume the
					 * device is gone if it isnt.
					 */
					_remove_mapper( e,stl,uid ) ;
				}

				StringFree( r ) ;
			}else{
				if( crypt_init_by_name( &cd,e ) == 0 ){

					if( crypt_get_device_name( cd ) == NULL ){

						/*
						 * we will get here if none LUKS mapper is active but the underlying device is gone
						 */

						_remove_mapper( e,stl,uid ) ;
					}

					crypt_free( cd ) ;
				}else{
					/*
					* we will get here if the LUKS mapper is active but the underlying device is gone
					*/
					_remove_mapper( e,stl,uid ) ;
				}
			}
		}
	}

	if( s ){

		zuluCryptDeleteDeadMountPoints( uid,stl ) ;
	}

	/*
	 * zuluCryptSecurityDropElevatedPrivileges() is defined in security.c
	 */
	zuluCryptSecurityDropElevatedPrivileges() ;

	StringListDelete( &stl ) ;
	StringMultipleDelete( &p,&z,NULL ) ;
	closedir( dir ) ;
}
