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
#include <sys/syscall.h>
#include <libcryptsetup.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "veracrypt_pim.h"

static void _chown( const char * x,uid_t y,gid_t z )
{
	if( chown( x,y,z ) ){}
}
static void _chmod( const char * x,mode_t y )
{
	if( chmod( x,y ) ){}
}
static void _write( int x,const void * y,size_t z )
{
	if( write( x,y,z ) ){}
}
static void _close( int x )
{
	if( close( x ) ){}
}

const char * zuluCryptCryptsetupTCRYPTType( void )
{
#ifdef CRYPT_TCRYPT
	return CRYPT_TCRYPT ;
#else
	return "" ;
#endif
}

void * zuluCryptCryptsetupTCryptVCrypt( const open_struct_t * opt )
{
#ifdef CRYPT_TCRYPT

	struct crypt_params_tcrypt * m = malloc( sizeof( struct crypt_params_tcrypt ) ) ;

	memset( m,'\0',sizeof( struct crypt_params_tcrypt ) ) ;

	m->passphrase      = opt->key ;
	m->passphrase_size = opt->key_len ;
	m->keyfiles        = opt->tcrypt_keyfiles ;
	m->keyfiles_count  = ( unsigned int )opt->tcrypt_keyfiles_count ;

	m->flags = CRYPT_TCRYPT_LEGACY_MODES ;

	if( opt->system_volume ){

		m->flags |= CRYPT_TCRYPT_SYSTEM_HEADER ;
	}
	if( opt->use_backup_header ){

		m->flags |= CRYPT_TCRYPT_BACKUP_HEADER ;
	}
	if( opt->use_hidden_header ){

		m->flags |= CRYPT_TCRYPT_HIDDEN_HEADER ;
	}

#if SUPPORT_VERACRYPT_PIM

	m->veracrypt_pim   = ( unsigned int )  opt->iteration_count ;
#endif

#ifdef CRYPT_TCRYPT_VERA_MODES

	if( opt->veraCrypt_volume ){

		m->flags |= CRYPT_TCRYPT_VERA_MODES ;
	}
#endif
	return m ;
#else
	return NULL ;
#endif
}

string_t zuluCryptCreateKeyFile( const char * key,size_t key_len,const char * fileName )
{
	string_t st = StringVoid ;
	int fd ;
	const char * file ;

	struct stat statstr ;

	if( key == NULL || key_len == 0 || fileName == NULL ){
		return StringVoid ;
	}

	#define path_does_not_exist( x ) stat( x,&statstr ) != 0

	if( path_does_not_exist( "/run" ) ){

		mkdir( "/run",S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH | S_IROTH ) ;
		_chown( "/run",0,0 ) ;
	}
	if( path_does_not_exist( "/run/zuluCrypt" ) ){

		mkdir( "/run/zuluCrypt",S_IRWXU ) ;
		_chown( "/run/zuluCrypt",0,0 ) ;
	}

	st = String_1( "/run/zuluCrypt/",fileName,NULL ) ;
	file = StringAppendInt( st,(u_int64_t)syscall( SYS_gettid ) ) ;
	fd = open( file,O_WRONLY | O_CREAT,S_IRUSR | S_IWUSR | S_IRGRP |S_IROTH ) ;

	if( fd == -1 ){

		StringDelete( &st ) ;
	}else{
		_write( fd,key,key_len ) ;
		_close( fd ) ;
		_chown( file,0,0 ) ;
		_chmod( file,S_IRWXU ) ;
	}

	return st ;
}

string_t zuluCryptCreateKeyFile_1( string_t st,const char * fileName )
{
	return zuluCryptCreateKeyFile( StringContent( st ),StringLength( st ),fileName ) ;
}

static int zuluExit( int st,struct crypt_device * cd, void * m )
{
	free( m ) ;
	crypt_free( cd ) ;
	return st ;
}

static int _open_tcrypt_volume( const char * device,const open_struct_t * opt )
{
	struct crypt_device * cd ;
	uint32_t flags ;
	int st ;

	void * m ;

	if( crypt_init( &cd,device ) != 0 ){

		return 2 ;
	}

	m = zuluCryptCryptsetupTCryptVCrypt( opt ) ;

	if( crypt_load( cd,zuluCryptCryptsetupTCRYPTType(),m ) != 0 ){

		return zuluExit( 2,cd,m ) ;
	}
	if( StringHasComponent( opt->m_opts,"ro" ) ){

		flags = CRYPT_ACTIVATE_READONLY ;
	}else{
		flags = CRYPT_ACTIVATE_ALLOW_DISCARDS ;
	}

	st = crypt_activate_by_volume_key( cd,opt->mapper_name,NULL,0,flags ) ;

	if( st == 0 ){

		return zuluExit( 0,cd,m ) ;
	}else{
		return zuluExit( 1,cd,m ) ;
	}
}

static int _open_tcrypt_volume_1( const char * device,const resolve_path_t * opt )
{
	int r ;

	open_struct_t opts ;

	memcpy( &opts,opt->args,sizeof( opts ) ) ;

	if( opts.trueCrypt_volume ){

		r = _open_tcrypt_volume( device,&opts ) ;

		if( r == 0 ){

			return 0 ;
		}

		opts.use_backup_header = 1 ;

		r = _open_tcrypt_volume( device,&opts ) ;

		if( r == 0 ){

			return 0 ;
		}

		opts.use_backup_header = 0 ;
		opts.use_hidden_header = 1 ;

		r = _open_tcrypt_volume( device,&opts ) ;

		if( r == 0 ){

			return 0 ;
		}

		opts.use_backup_header = 0 ;
		opts.use_hidden_header = 0 ;
		opts.system_volume = 1 ;

		return _open_tcrypt_volume( device,&opts ) ;
	}else{
		if( opts.system_volume || opts.use_hidden_header ){

			return _open_tcrypt_volume( device,&opts ) ;
		}else{
			if( _open_tcrypt_volume( device,&opts ) == 0 ){

				return 0 ;
			}else{
				opts.use_hidden_header = 1 ;

				return _open_tcrypt_volume( device,&opts ) ;
			}
		}
	}
}

static int _open_tcrypt_0( const open_struct_t * opt )
{
	/*
	 * zuluCryptResolveDevicePath_0() is defined in resolve_path.c
	 */
	return zuluCryptResolveDevicePath_0( _open_tcrypt_volume_1,opt,1 ) ;
}

int zuluCryptOpenTcrypt( const char * device,const char * mapper,const char * key,size_t key_len,
			 int key_source,int volume_type,const char * m_point,
			 uid_t uid,unsigned long m_flags,const char * fs_opts )
{
	open_struct_t opts ;
	string_t st ;
	int r ;
	const char * keyfile ;

	memset( &opts,'\0',sizeof( open_struct_t ) ) ;

	opts.device      = device ;
	opts.mapper_name = mapper ;
	opts.volume_type = volume_type ;
	opts.m_point     = m_point ;
	opts.uid         = uid ;
	opts.m_flags     = m_flags ;
	opts.fs_opts     = fs_opts ;

	if( m_flags & MS_RDONLY ){
		opts.m_opts = "ro" ;
	}else{
		opts.m_opts = "rw" ;
	}

	if( key_source == TCRYPT_KEYFILE ){
		st = zuluCryptCreateKeyFile( key,key_len,"open_tcrypt-" ) ;
		if( st != StringVoid ){

			keyfile = StringContent( st ) ;

			opts.tcrypt_keyfiles_count = 1 ;
			opts.tcrypt_keyfiles       = &keyfile ;

			r = zuluCryptOpenTcrypt_1( &opts ) ;
			/*
			 * zuluCryptDeleteFile() is defined in open_path_security.c
			 */
			zuluCryptDeleteFile( keyfile ) ;
			StringDelete( &st ) ;
		}else{
			r = 1 ;
		}
	}else if( key_source == TCRYPT_KEYFILE_FILE ){

		opts.tcrypt_keyfiles_count = 1 ;
		opts.tcrypt_keyfiles       = &key ;

		r = zuluCryptOpenTcrypt_1( &opts ) ;
	}else{
		opts.key_len = key_len ;
		opts.key     = key ;
		r = zuluCryptOpenTcrypt_1( &opts ) ;
	}

	return r ;
}

int zuluCryptOpenTcrypt_1( const open_struct_t * opts )
{
	/*
	 * zuluCryptOpenVolume_0() is defined in open_volume.c
	 */
	return zuluCryptOpenVolume_0( _open_tcrypt_0,opts ) ;
}

/*
 * 1 is returned if a volume is a truecrypt volume.
 * 0 is returned if a volume is not a truecrypt volume or functionality is not supported
 */
int zuluCryptVolumeIsTcrypt( const char * device,const char * key,int key_source )
{
	void * m ;

	open_struct_t s ;

	struct crypt_device * cd = NULL;

	memset( &s,'\0',sizeof( open_struct_t ) ) ;

	if( key_source ){}

	if( crypt_init( &cd,device ) < 0 ){

		return 0 ;
	}else{
		s.key     = key ;
		s.key_len = StringSize( key ) ;

		m = zuluCryptCryptsetupTCryptVCrypt( &s ) ;

		if( m == NULL ){

			return 0 ;
		}

		if( crypt_load( cd,zuluCryptCryptsetupTCRYPTType(),m ) == 0 ){

			return zuluExit( 1,cd,m ) ;
		}else{
			return zuluExit( 0,cd,m ) ;
		}
	}
}
