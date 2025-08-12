/*
 *
 *  Copyright ( c ) 2015
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


#ifndef ABOUT_H
#define ABOUT_H

#include <QDialog>
#include <QString>

class QCloseEvent ;
class QEvent ;
class QObject ;

namespace Ui {
class about;
}

class about : public QDialog
{
	Q_OBJECT
public:
	static void instance( QWidget * parent )
	{
		new about( parent ) ;
	}
	explicit about( QWidget * parent ) ;
	~about() ;
private slots:
	void pbClose( void ) ;
private:
	void closeEvent( QCloseEvent * ) ;
	bool eventFilter( QObject * watched,QEvent * event ) ;
	Ui::about * m_ui ;
};

#endif // ABOUT_H
