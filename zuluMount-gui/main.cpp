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

#include <QApplication>
#include "zulumount.h"
#include "../zuluCrypt-gui/utility.h"

int main( int argc,char * argv[] )
{
	QSettings m( "zuluCrypt","zuluMount" ) ;

	utility::setSettingsObject( &m ) ;

	utility::setHDPI( "zuluMount" ) ;

	QApplication a( argc,argv ) ;

#if QT_VERSION >= QT_VERSION_CHECK( 5,7,0 )

	a.setDesktopFileName( "zuluMount" ) ;
#endif
	return utility::startApplication( "zuluMount",[ & ](){

		zuluMount e ;

		utility::invokeMethod( &e,&zuluMount::Show ) ;

		return a.exec() ;
	} ) ;
}
