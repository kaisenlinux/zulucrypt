
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

#include "oneinstance.h"
#include <QDebug>
#include "../zuluCrypt-gui/utility.h"
#include <memory>
#include <utility>

oneinstance::oneinstance( QObject * parent,
			  const QString& socketPath,
			  const QString& argument,
			  std::function< void( const QString& ) > start,
			  std::function< void( int ) > exit,
			  std::function< void( const QString& ) > event ) :
	QObject( parent ),
	m_serverPath( socketPath ),
	m_argument( argument ),
	m_start( std::move( start ) ),
	m_exit( std::move( exit ) ),
	m_event( std::move( event ) )
{
	if( QFile::exists( m_serverPath ) ){

		connect( &m_localSocket,&QLocalSocket::connected,this,&oneinstance::connected ) ;

		#if QT_VERSION < QT_VERSION_CHECK( 5,15,0 )
			using cs = void( QLocalSocket::* )( QLocalSocket::LocalSocketError ) ;
			QObject::connect( &m_localSocket,
					  static_cast< cs >( &QLocalSocket::error ),
					  this,
					  &oneinstance::errorOnConnect ) ;
		#else
			QObject::connect( &m_localSocket,
					  &QLocalSocket::errorOccurred,
					  this,
					  &oneinstance::errorOnConnect ) ;
		#endif

		m_localSocket.connectToServer( m_serverPath ) ;
	}else{
		this->start() ;
	}
}

void oneinstance::start()
{
	m_start( m_argument ) ;

	connect( &m_localServer,&QLocalServer::newConnection,this,&oneinstance::gotConnection ) ;

	m_localServer.listen( m_serverPath ) ;
}

void oneinstance::gotConnection()
{
	std::unique_ptr< QLocalSocket > s( m_localServer.nextPendingConnection() ) ;

	s->waitForReadyRead() ;

	m_event( s->readAll() ) ;
}

void oneinstance::errorOnConnect( QLocalSocket::LocalSocketError e )
{
	Q_UNUSED( e )
	utility::debug() << tr( "Previous instance seem to have crashed,trying to clean up before starting" ) ;
	QFile::remove( m_serverPath ) ;
	this->start() ;
}

void oneinstance::connected()
{
	utility::debug() << tr( "There seem to be another instance running,exiting this one" ) ;

	if( !m_argument.isEmpty() ){

		m_localSocket.write( m_argument.toLatin1() ) ;
		m_localSocket.waitForBytesWritten() ;
	}

	m_localSocket.close() ;

	m_exit( 255 ) ;
}

oneinstance::~oneinstance()
{
	if( m_localServer.isListening() ){

		m_localServer.close() ;
		QFile::remove( m_serverPath ) ;
	}
}
