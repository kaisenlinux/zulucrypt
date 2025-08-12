/*
 *
 *  Copyright (c) 2015
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

#include "readonlywarning.h"
#include "ui_readonlywarning.h"

#include "utility.h"

#include <QDir>

#include <QCloseEvent>
#include <QEvent>

bool readOnlyWarning::getOpenVolumeReadOnlyOption( const QString& app )
{
	Q_UNUSED( app )
	return utility::readOnlyOption() ;
}

readOnlyWarning::readOnlyWarning( QWidget * parent,bool checked,const QString& app ) :
	QDialog( parent ),m_ui( new Ui::readOnlyWarning ),m_checked( checked ),m_app( app )
{
	m_ui->setupUi( this ) ;

	this->setFont( parent->font() ) ;
	this->setFixedSize( this->size() ) ;

	connect( m_ui->PbOK,SIGNAL( clicked() ),this,SLOT( pbOK() ) ) ;
	connect( m_ui->checkBox,SIGNAL( clicked( bool ) ),this,SLOT( checkBoxChecked( bool ) ) ) ;

	this->installEventFilter( this ) ;

	this->setReadOnlyOption( m_checked ) ;
}

void readOnlyWarning::pbOK()
{
	this->HideUI() ;
}

void readOnlyWarning::checkBoxChecked( bool checked )
{
	utility::readOnlyWarning( !checked ) ;
}

void readOnlyWarning::setReadOnlyOption( bool readOnly )
{
	utility::readOnlyOption( readOnly ) ;
}

bool readOnlyWarning::showUIwarning()
{
	return utility::readOnlyWarning() ;
}

bool readOnlyWarning::eventFilter( QObject * watched,QEvent * event )
{
	return utility::eventFilter( this,watched,event,[ this ](){ this->HideUI() ; } ) ;
}

void readOnlyWarning::closeEvent( QCloseEvent * e )
{
	e->ignore() ;
	this->HideUI() ;
}

void readOnlyWarning::ShowUI()
{
	if( m_checked && this->showUIwarning() ){

		this->show() ;
	}else{
		this->deleteLater() ;
	}
}

void readOnlyWarning::HideUI()
{
	this->hide() ;
	this->deleteLater() ;
}

readOnlyWarning::~readOnlyWarning()
{
	delete m_ui ;
}
