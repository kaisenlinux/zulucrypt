﻿/*
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

#include "zulucrypt.h"
#include "ui_zulucrypt.h"
#include "utility.h"

#include <QProcess>
#include <QStringList>
#include <QMenu>
#include <QCursor>
#include <QByteArray>
#include <QColor>
#include <QBrush>
#include <iostream>
#include <QMessageBox>
#include <QFontDialog>
#include <QMetaType>
#include <QDebug>
#include <QKeySequence>
#include <QTranslator>
#include <QFileDialog>
#include <QTableWidgetItem>
#include <QFont>
#include <QWidget>
#include <QList>
#include <QMessageBox>
#include <QCloseEvent>
#include <QUrl>
#include <QMimeData>
#include <QShortcut>

#include "../zuluCrypt-cli/constants.h"
#include "../zuluMount-gui/oneinstance.h"
#include "password_dialog.h"
#include "openvolume.h"
#include "luksaddkey.h"
#include "luksdeletekey.h"
#include "createvolume.h"
#include "createfile.h"
#include "filemanager.h"
#include "createkeyfile.h"
#include "favorites2.h"
#include "cryptoinfo.h"
#include "erasedevice.h"
#include "managevolumeheader.h"
#include "cryptfiles.h"
#include "dialogmsg.h"
#include "managesystemvolumes.h"
#include "tablewidget.h"
#include "utility.h"
#include "task.hpp"
#include "about.h"
#include "help.h"
#include "createvolumeinexistingfile.h"
#include "pdf_path.h"
#include "warnwhenextendingcontainerfile.h"
#include "showluksslots.h"

#include <memory>

zuluCrypt::zuluCrypt( QWidget * parent ) :
	QMainWindow( parent ),
	m_secrets( this ),
	m_mountInfo( this,false,[ this ](){ this->quitApplication() ; } ),
	m_signalHandler( this )
{
	utility::mainWindowWidget( this ) ;

	m_signalHandler.setAction( [ this ]( systemSignalHandler::signal s ){

		Q_UNUSED( s )

		this->emergencyQuitApplication() ;
	} ) ;
}

void zuluCrypt::setLocalizationLanguage( bool translate )
{
	if( translate ){

		utility::setLocalizationLanguage( translate,nullptr,"zuluCrypt-gui" ) ;
	}else{
		utility::setLocalizationLanguage( translate,m_language_menu,"zuluCrypt-gui" ) ;
	}
}

void zuluCrypt::languageMenu( QAction * ac )
{
	utility::languageMenu( this,m_language_menu,ac,"zuluCrypt-gui" ) ;

	m_ui->retranslateUi( this ) ;
}

void zuluCrypt::helperStarted( bool e,const QString& volume )
{
	if( e ){

		this->setLocalizationLanguage( true ) ;

		m_ui = new Ui::zuluCrypt ;

		this->setupUIElements() ;
		this->setupConnections() ;
		this->initFont() ;
		this->initKeyCombo() ;
		this->initTray( m_startHidden ) ;
		this->info() ;
		this->setLocalizationLanguage( false ) ;
		this->updateVolumeList( volume ) ;

		m_mountInfo.start() ;

		if( !m_startHidden ){

			this->setVisible( true ) ;
			this->show() ;
			this->raise() ;
			this->setWindowState( Qt::WindowActive ) ;
		}
	}else{
		DialogMsg( this ).ShowUIOK( tr( "ERROR" ),utility::failedToStartzuluPolkit() ) ;

		this->closeApplication0() ;
	}
}

void zuluCrypt::updateVolumeList( const QString& volume )
{
	m_ui->tableWidget->setEnabled( false ) ;

	Task::run( [ this ](){

		utility::Task::waitForOneSecond() ;

		auto r = utility::Task( QString( "%1 -L" ).arg( ZULUCRYPTzuluCrypt ) ) ;

		if( r.success() ){

			this->updateVolumeList0( r.stdOut() ) ;
		}else{
			emit updateUi( {} ) ;
		}

	} ).then( [ this,volume ](){

		if( !volume.isEmpty() ){

			this->ShowPasswordDialog( volume,volume.split( "/" ).last() ) ;
		}
	} ) ;
}

void zuluCrypt::updateVolumeList0( const QString& r )
{
	QVector< QStringList > volumes ;

	for( const auto& it : utility::split( r,'\n' ) ){

		auto z = utility::split( it,'\t' ) ;

		if( z.size() >= 3 ){

			const auto& q = z.at( 2 ) ;

			if( q.startsWith( "crypto_LUKS" ) ){

				auto s = q ;

				z.replace( 2,s.replace( "crypto_","" ).toLower()  ) ;
			}else{
				auto e = q ;

				e.remove( "crypto_" ) ;

				z.replace( 2,e.toLower() ) ;
			}

			volumes.append( z ) ;
		}
	}

	emit updateUi( volumes ) ;
}

void zuluCrypt::updateUi0( QVector< QStringList > s )
{
	tablewidget::clearTable( m_ui->tableWidget ) ;

	for( int i = 0 ; i < s.size() ; i++ ){

		tablewidget::addRow( m_ui->tableWidget,s[ i ] ) ;
	}

	m_ui->tableWidget->setEnabled( true ) ;
	m_ui->tableWidget->setFocus() ;
}

void zuluCrypt::initKeyCombo()
{
}

void zuluCrypt::initFont()
{
	this->setUserFont( utility::getFont( this ) ) ;
}

void zuluCrypt::raiseWindow( const QString& device )
{
	this->setVisible( true ) ;
	this->show() ;
	this->raise() ;
	this->setWindowState( Qt::WindowActive ) ;

	if( !device.isEmpty() ){

		this->ShowPasswordDialog( device,device.split( "/" ).last() ) ;
	}
}

void zuluCrypt::polkitFailedWarning()
{
	DialogMsg( this ).ShowUIOK( tr( "ERROR" ),tr( "zuluCrypt Failed To Connect To zuluPolkit.\nPlease Report This Serious Bug." ) ) ;
}

void zuluCrypt::start()
{
	/*
	 * Entry point is here, the "instance" class checks if there is already a running zuluCrypt-gui process.
	 * If yes,this instance tells the existing instance to get focus and then it exits.
	 *
	 * If no,then it knowns it is the only running instance and it calls "setUpApp() to set up the GUI and
	 * runs.
	 */

	const auto l   = QCoreApplication::arguments() ;

	m_openPath     = utility::cmdArgumentValue( l,"-m",utility::fileManager() ) ;
	m_startHidden  = l.contains( "-e" ) ;

	auto s = utility::socketPath() ;

	if( utility::configDirectoriesAreNotWritable( this ) ){

		return this->closeApplication0() ;
	}

	utility::polkitFailedWarning( [ this ](){

		utility::invokeMethod( this,&zuluCrypt::polkitFailedWarning ) ;
	} ) ;

	if( utility::libCryptSetupLibraryNotFound() ){

		auto a = tr( "Cryptsetup library could not be found and zuluCrypt will most likely not work as expected." ) ;
		auto b = tr( "\n\nPlease recompile zuluCrypt to force it to re-discover the new library" ) ;

		DialogMsg( this ).ShowUIOK( tr( "ERROR" ),a + b ) ;
	}

	utility::startHelperStatus st ;

	st.success = [ this ]( bool e,QString m ){

		this->helperStarted( e,m ) ;
	} ;

	st.error = [ this ](){

		this->closeApplication0() ;
	} ;

	auto a = s + "/zuluCrypt-gui.socket" ;
	auto b = utility::cmdArgumentValue( l,"-d" ) ;
	auto c = [ this,st ]( const QString& e ){ utility::startHelper( this,e,"zuluCrypt",st ) ; } ;
	auto d = [ this ]( int s ){ this->closeApplication( s ) ; } ;
	auto e = [ this ]( const QString& e ){ this->raiseWindow( e ) ; } ;

	oneinstance::instance( this,a,b,c,d,e ) ;
}

void zuluCrypt::initTray( bool e )
{
	utility::setIconMenu( "zuluCrypt",m_ui->actionSelect_Icons,this,[ this ]( const QString& e ){

		utility::setIcons( "zuluCrypt",e ) ;

		this->setIcons() ;
	} ) ;

	this->setIcons() ;

	utility::showTrayIcon( m_ui->actionTray_icon,m_trayIcon,e ) ;
}

void zuluCrypt::showTrayIcon( bool e )
{
	if( e ){
		m_trayIcon.show() ;
	}else{
		m_trayIcon.hide() ;
	}
}

void zuluCrypt::trayProperty()
{
	m_ui->actionTray_icon->setEnabled( false ) ;

	utility::trayProperty( &m_trayIcon ) ;

	m_ui->actionTray_icon->setEnabled( true ) ;
}

void zuluCrypt::setupUIElements()
{
	m_ui->setupUi( this ) ;

	//m_secrets.setParent( this ) ;

	m_trayIcon.setParent( this ) ;

	const auto f = utility::getWindowDimensions( "zuluCrypt" ) ;

	const auto e = f.data() ;

	this->window()->setGeometry( *( e + 0 ),*( e + 1 ),*( e + 2 ),*( e + 3 ) ) ;

	const auto table = m_ui->tableWidget ;

	table->setColumnWidth( 0,*( e + 4 ) ) ;
	table->setColumnWidth( 1,*( e + 5 ) ) ;
	table->setColumnWidth( 2,*( e + 6 ) ) ;
}

void zuluCrypt::setIcons()
{
	const auto& icon = utility::getIcon( "zuluCrypt" ) ;

	m_trayIcon.setIcon( icon ) ;

	this->setWindowIcon( icon ) ;
}

void zuluCrypt::itemEntered( QTableWidgetItem * item )
{
	auto m_point = item->tableWidget()->item( item->row(),1 )->text() ;

	if( !m_point.isEmpty() ){

		item->setToolTip( utility::shareMountPointToolTip( m_point ) ) ;
	}
}

void zuluCrypt::createVolumeInFile()
{
	auto e = QFileDialog::getOpenFileName( this,tr( "Path To A File" ),QDir::homePath() ) ;

	if( !e.isEmpty() ){

		createvolume::instance( this ).ShowFile( e ) ;
	}
}

void zuluCrypt::setupConnections()
{
	m_ui->tableWidget->setMouseTracking( true ) ;

	m_ui->tableWidget->setContextMenuPolicy( Qt::CustomContextMenu ) ;

	//m_ui->tableWidget->horizontalHeader()->setStretchLastSection( true ) ;

	connect( m_ui->tableWidget,&QTableWidget::customContextMenuRequested,[ this ]( QPoint s ){

		Q_UNUSED( s )

		auto item = m_ui->tableWidget->currentItem() ;

		if( item ){

			this->itemClicked( item,QCursor::pos() ) ;
		}
	} ) ;

	connect( m_ui->actionClear_Dead_Mount_Points,&QAction::triggered,[](){

		utility::Task::run( ZULUCRYPTzuluCrypt" --clear-dead-mount-points" ).await() ;
	} ) ;

	connect( m_ui->tableWidget,&QTableWidget::currentItemChanged,this,&zuluCrypt::currentItemChanged ) ;

	auto qc = Qt::QueuedConnection ;

	connect( this,&zuluCrypt::updateUi,this,&zuluCrypt::updateUi0,qc ) ;
	connect( &m_mountInfo,&monitor_mountinfo::gotEvent,this,&zuluCrypt::updateVolumeList1 ) ;

	connect( m_ui->action_update_volume_list,&QAction::triggered,this,&zuluCrypt::updateVolumeList1 ) ;
	connect( m_ui->actionFileOpen,&QAction::triggered,this,&zuluCrypt::ShowPasswordDialog0 ) ;
	connect( m_ui->tableWidget,&QTableWidget::itemClicked,this,&zuluCrypt::itemClicked0 ) ;
	connect( m_ui->action_close,&QAction::triggered,this,&zuluCrypt::closeApplication0 ) ;
	connect( m_ui->actionDecrypt_file,&QAction::triggered,this,&zuluCrypt::decryptFile0 ) ;
	connect( m_ui->actionEncrypted_Container_In_A_File,&QAction::triggered,this,&zuluCrypt::createVolumeInFile ) ;
	connect( m_ui->tableWidget,&QTableWidget::itemEntered,this,&zuluCrypt::itemEntered ) ;
	connect( m_ui->actionErase_data_on_device,&QAction::triggered,this,&zuluCrypt::ShowEraseDataDialog ) ;
	connect( m_ui->actionPartitionOpen,&QAction::triggered,this,&zuluCrypt::ShowOpenPartition ) ;
	connect( m_ui->actionFileCreate,&QAction::triggered,this,&zuluCrypt::ShowCreateFile ) ;
	connect( m_ui->actionEncrypted_Container_In_An_Existing_FIle,&QAction::triggered,this,&zuluCrypt::createVolumeInExistingFile ) ;
	connect( m_ui->actionManage_names,&QAction::triggered,this,&zuluCrypt::ShowFavoritesEntries ) ;
	connect( m_ui->actionCreatekeyFile,&QAction::triggered,this,&zuluCrypt::ShowCreateKeyFile ) ;
	connect( m_ui->actionAbout,&QAction::triggered,this,&zuluCrypt::aboutMenuOption ) ;
	connect( m_ui->actionAddKey,&QAction::triggered,this,&zuluCrypt::ShowAddKey ) ;
	connect( m_ui->actionDeleteKey,&QAction::triggered,this,&zuluCrypt::ShowDeleteKey ) ;
	connect( m_ui->actionPartitionCreate,&QAction::triggered,this,&zuluCrypt::ShowNonSystemPartitions ) ;
	connect( m_ui->actionFonts,&QAction::triggered,this,&zuluCrypt::fonts ) ;
	connect( m_ui->menuFavorites,&QMenu::aboutToShow,this,&zuluCrypt::readFavorites ) ;
	connect( m_ui->menuFavorites,&QMenu::aboutToHide,this,&zuluCrypt::favAboutToHide ) ;
	connect( m_ui->actionTray_icon,&QAction::triggered,this,&zuluCrypt::trayProperty ) ;
	connect( &m_trayIcon,&QSystemTrayIcon::activated,this,&zuluCrypt::trayClicked ) ;
	connect( m_ui->menuFavorites,&QMenu::triggered,this,&zuluCrypt::favClicked ) ;
	connect( m_ui->actionMinimize_to_tray,&QAction::triggered,this,&zuluCrypt::minimizeToTray ) ;
	connect( m_ui->actionClose_all_opened_volumes,&QAction::triggered,this,&zuluCrypt::closeAllVolumes ) ;
	connect( m_ui->actionEncrypt_file,&QAction::triggered,this,&zuluCrypt::encryptFile ) ;
	connect( m_ui->actionManage_system_partitions,&QAction::triggered,this,&zuluCrypt::ShowManageSystemPartitions ) ;
	connect( m_ui->actionManage_non_system_partitions,&QAction::triggered,this,&zuluCrypt::ShowManageNonSystemPartitions ) ;
	connect( m_ui->actionOpen_zuluCrypt_pdf,&QAction::triggered,this,&zuluCrypt::openpdf ) ;
	connect( m_ui->actionSet_File_Manager,&QAction::triggered,this,&zuluCrypt::setFileManager ) ;

	utility::connect( this,&zuluCrypt::closeVolume,this,&zuluCrypt::closeAll ) ;

	m_ui->actionDo_not_minimize_to_tray->setChecked( utility::doNotMinimizeToTray() ) ;

	connect( m_ui->actionDo_not_minimize_to_tray,&QAction::triggered,[ this ](){

		auto s = !utility::doNotMinimizeToTray() ;

		m_ui->actionDo_not_minimize_to_tray->setChecked( s ) ;
		utility::setDoNotMinimizeToTray( s ) ;
	} ) ;

	m_autoOpenMountPoint = utility::autoOpenFolderOnMount( "zuluCrypt-gui" ) ;

	m_ui->actionAuto_Open_Mount_Point->setCheckable( true ) ;
	m_ui->actionAuto_Open_Mount_Point->setChecked( m_autoOpenMountPoint ) ;

	connect( m_ui->actionAuto_Open_Mount_Point,&QAction::toggled,this,&zuluCrypt::autoOpenMountPoint ) ;

	m_ui->actionRestore_header->setText( tr( "Restore Volume Header" ) ) ;
	m_ui->actionBackup_header->setText( tr( "Backup Volume Header" ) ) ;

	utility::connect( m_ui->actionBackup_header,&QAction::triggered,this,&zuluCrypt::volumeHeaderBackUp ) ;
	utility::connect( m_ui->actionRestore_header,&QAction::triggered,this,&zuluCrypt::volumeRestoreHeader ) ;

	utility::connect( m_ui->actionView_LUKS_Key_Slots,&QAction::triggered,this,&zuluCrypt::showLUKSSlotsData ) ;

	m_ui->actionView_LUKS_Key_Slots->setEnabled( utility::canShowKeySlotProperties() ) ;

	m_ui->actionManage_volumes_in_gnome_wallet->setEnabled( false ) ;
	m_ui->actionManage_volumes_in_kde_wallet->setEnabled( false ) ;

	utility::connect( m_ui->menuOptions,&QMenu::aboutToShow,this,&zuluCrypt::optionMenuAboutToShow ) ;

	m_language_menu = [ this ](){

		auto m = new QMenu( tr( "Select Language" ),this ) ;

		utility::connect( m,&QMenu::triggered,this,&zuluCrypt::languageMenu ) ;

		m_ui->actionSelect_Language->setMenu( m ) ;

		return m ;
	}() ;

	m_ui->actionVeracrypt_container_in_a_file->setEnabled( true ) ;
	m_ui->actionVeracrypt_container_in_a_partition->setEnabled( true ) ;

	connect( m_ui->actionShow_Debug_Window,&QAction::triggered,[ this ](){

		m_debugWindow.Show() ;
	} ) ;

	this->setAcceptDrops( true ) ;
	this->updateTrayContextMenu() ;
}

void zuluCrypt::showLUKSSlotsData()
{
	showLUKSSlots::Show( this,{},[](){} ) ;
}

void zuluCrypt::showLUKSSlotsInfo()
{
	auto item = m_ui->tableWidget->currentItem() ;

	if( item ){

		auto path = m_ui->tableWidget->item( item->row(),0 )->text() ;

		showLUKSSlots::Show( this,path,[](){} ) ;
	}
}

void zuluCrypt::updateTrayContextMenu()
{
	m_trayIconMenu.clear() ;

	m_trayIconMenu.addMenu( m_ui->menu_zc ) ;

	m_trayIconMenu.addMenu( m_ui->menuOpen ) ;

	m_trayIconMenu.addMenu( m_ui->menuCreate ) ;

	//m_trayIconMenu.addMenu( m_ui->menuFavorites ) ;

	m_trayIconMenu.setFont( this->font() ) ;

	m_trayIconMenu.addAction( tr( "Show/Hide" ),this,&zuluCrypt::showTrayGUI ) ;

	m_trayIconMenu.addAction( tr( "Quit" ),this,&zuluCrypt::closeApplication0 ) ;

	m_trayIcon.setContextMenu( &m_trayIconMenu ) ;
}

void zuluCrypt::showTrayGUI()
{
	this->trayClicked( QSystemTrayIcon::Trigger ) ;
}

void zuluCrypt::autoOpenMountPoint( bool e )
{
	m_autoOpenMountPoint = e ;

	utility::autoOpenFolderOnMount( "zuluCrypt-gui",e ) ;
}

void zuluCrypt::optionMenuAboutToShow()
{
	auto a = utility::walletName() ;
	auto b = utility::applicationName() ;
	auto c = LXQt::Wallet::walletExists( LXQt::Wallet::BackEnd::internal,a,b ) ;

	m_ui->actionChange_internal_wallet_password->setEnabled( c ) ;
}

void zuluCrypt::cinfo()
{
}

void zuluCrypt::updateVolumeList1()
{
	this->updateVolumeList( QString() ) ;
}

void zuluCrypt::info()
{
	//cryptoinfo::instance( this,utility::homePath() + "/.zuluCrypt/doNotshowWarning.option",QString() ) ;
}

void zuluCrypt::createVolumeInExistingFile()
{
	warnWhenExtendingContainerFile::Show( this,[ this ](){

		createVolumeInExistingFIle::instance( this ) ;
	} ) ;
}

void zuluCrypt::failedToOpenWallet()
{
	//DialogMsg msg( this ) ;
	//msg.ShowUIOK( tr( "ERROR" ),tr( "could not open selected wallet" ) ) ;
}

void zuluCrypt::permissionExplanation()
{
	DialogMsg m( this ) ;
	m.ShowPermissionProblem( QString() ) ;
}

void zuluCrypt::ShowManageSystemPartitions()
{
	manageSystemVolumes::instance( this,"/etc/zuluCrypt/system_volumes.list" ) ;
}

void zuluCrypt::ShowManageNonSystemPartitions()
{
	manageSystemVolumes::instance( this,"/etc/zuluCrypt/nonsystem_volumes.list" ) ;
}

void zuluCrypt::currentItemChanged( QTableWidgetItem * current,QTableWidgetItem * previous )
{
	tablewidget::selectRow( current,previous ) ;

	if( m_ui->tableWidget->rowCount() > 12 ){

		m_ui->tableWidget->setColumnWidth( 2,70 ) ;
	}else{
		m_ui->tableWidget->setColumnWidth( 2,95 ) ;
	}
}

void zuluCrypt::emergencyQuitApplication()
{
	this->hide() ;

	m_mountInfo.announceEvents( false ) ;

	Task::await( [ this ](){

		auto table = m_ui->tableWidget ;

		auto volumeCount = table->rowCount() ;

		if( volumeCount > 0 ){

			QVector< QTableWidgetItem * > tableItems( volumeCount ) ;

			auto it = tableItems.data() ;

			for( int i = 0 ; i < volumeCount ; i++ ){

				*( it + i ) = table->item( i,0 ) ;
			}

			auto exe = utility::appendUserUID( "%1 -q -d \"%2\"" ) ;

			for( int i = tableItems.count() - 1 ; i >= 0 ; i-- ){

				auto e = *( it + i ) ;

				auto device = e->text().replace( "\"","\"\"\"" ) ;

				utility::Task( exe.arg( ZULUCRYPTzuluCrypt,device ) ) ;
			}
		}

	} ) ;

	this->quitApplication() ;
}

void zuluCrypt::closeAllVolumes()
{
	m_ui->tableWidget->setEnabled( false ) ;

	m_mountInfo.announceEvents( false ) ;

	Task::await( [ this ](){

		utility::Task::waitForOneSecond() ; // for ui effect

		auto table = m_ui->tableWidget ;

		auto volumeCount = table->rowCount() ;

		if( volumeCount > 0 ){

			QVector< QTableWidgetItem * > tableItems( volumeCount ) ;

			auto it = tableItems.data() ;

			for( int i = 0 ; i < volumeCount ; i++ ){

				*( it + i ) = table->item( i,0 ) ;
			}

			auto exe = utility::appendUserUID( "%1 -q -d \"%2\"" ) ;

			for( int i = tableItems.count() - 1 ; i >= 0 ; i-- ){

				auto e = *( it + i ) ;

				auto device = e->text().replace( "\"","\"\"\"" ) ;

				auto r = utility::Task( exe.arg( ZULUCRYPTzuluCrypt,device ) ) ;

				emit closeVolume( e,r.exitCode() ) ;

				utility::Task::waitForOneSecond() ; // for ui effect
			}
		}

	} ) ;

	m_mountInfo.announceEvents( true ) ;

	m_ui->tableWidget->setEnabled( true ) ;
}

void zuluCrypt::closeAll( QTableWidgetItem * item,int st )
{
	if( st ){

		this->closeStatusErrorMessage( st ) ;
	}else{
		this->removeRowFromTable( item->row() ) ;
	}
}

void zuluCrypt::removeRowFromTable( int x )
{
	tablewidget::deleteRow( m_ui->tableWidget,x ) ;
}

void zuluCrypt::minimizeToTray()
{
	if( m_ui->actionTray_icon->isChecked() ){

		this->hide() ;
	}else{
		m_ui->actionTray_icon->setChecked( true ) ;
		this->trayProperty() ;
		this->hide() ;
	}
}

void zuluCrypt::closeEvent( QCloseEvent * e )
{
	e->ignore() ;

	if( utility::doNotMinimizeToTray() ){

		this->hide() ;

		this->closeApplication0() ;

	}else if( m_trayIcon.isVisible() ){

		this->hide() ;
	}else{
		this->hide() ;

		this->closeApplication0() ;
	}
}

void zuluCrypt::dragEnterEvent( QDragEnterEvent * e )
{
	e->accept() ;
}

void zuluCrypt::dropEvent( QDropEvent * e )
{
	for( const auto& it : e->mimeData()->urls() ){

		const auto& e = it.path() ;

		if( utility::pathPointsToAFile( e ) ){

			this->ShowPasswordDialog( e,e.split( "/" ).last() ) ;
		}
	}
}

void zuluCrypt::quitApplication()
{
	m_trayIcon.hide() ;
	this->hide() ;
	QCoreApplication::quit() ;
}

void zuluCrypt::closeApplication0()
{
	m_secrets.close() ;
	utility::quitHelper() ;
	m_mountInfo.stop()() ;
}

void zuluCrypt::closeApplication( int )
{
	m_secrets.close() ;
	utility::quitHelper() ;
	m_mountInfo.stop()() ;
}

void zuluCrypt::trayClicked( QSystemTrayIcon::ActivationReason e )
{
	if( e == QSystemTrayIcon::Trigger ){

		if( this->isVisible() ){

			this->hide() ;
		}else{
			this->show() ;
		}
	}
}

void zuluCrypt::fonts()
{
	int size = 11 ;
	bool ok ;
	QFont Font = QFontDialog::getFont( &ok,this->font(),this ) ;

	if( ok ){

		int k = Font.pointSize() ;

		if( k > size ){

			k = size ;
			Font.setPointSize( k ) ;
			UIMessage( tr( "INFO" ),tr( "Resetting font size to %1 because larger font sizes do not fit" ).arg( QString::number( size ) ) ) ;
		}

		this->setUserFont( Font ) ;

		utility::saveFont( Font ) ;
	}
}

void zuluCrypt::setUserFont( QFont Font )
{
	this->setFont( Font ) ;

	m_ui->tableWidget->horizontalHeaderItem( 0 )->setFont( Font ) ;
	m_ui->tableWidget->horizontalHeaderItem( 1 )->setFont( Font ) ;
	m_ui->tableWidget->horizontalHeaderItem( 2 )->setFont( Font ) ;

	m_ui->actionEncrypted_Container_In_An_Existing_FIle->setFont( Font ) ;
	m_ui->actionAbout->setFont( Font ) ;
	m_ui->actionAddKey->setFont( Font ) ;
	m_ui->actionCreatekeyFile->setFont( Font ) ;
	m_ui->actionDeleteKey->setFont( Font ) ;
	m_ui->actionFavorite_volumes->setFont( Font ) ;
	m_ui->actionFileCreate->setFont( Font ) ;
	m_ui->actionFileOpen->setFont( Font ) ;
	m_ui->actionFonts->setFont( Font ) ;
	m_ui->actionInfo->setFont( Font ) ;
	m_ui->actionManage_favorites->setFont( Font ) ;
	m_ui->actionPartitionCreate->setFont( Font ) ;
	m_ui->actionPartitionOpen->setFont( Font ) ;
	m_ui->actionEncrypted_Container_In_A_File->setFont( Font ) ;
	m_ui->actionSelect_random_number_generator->setFont( Font ) ;
	m_ui->actionTray_icon->setFont( Font ) ;
	m_ui->menuFavorites->setFont( Font ) ;
	m_ui->actionManage_names->setFont( Font ) ;
	m_ui->actionBackup_header->setFont( Font ) ;
	m_ui->actionRestore_header->setFont( Font ) ;
	m_ui->actionEncrypt_file->setFont( Font ) ;
	m_ui->actionDecrypt_file->setFont( Font ) ;
	m_ui->menu_zc->setFont( Font ) ;
	m_ui->actionPermission_problems->setFont( Font ) ;
	m_ui->actionLuks_header_backup->setFont( Font ) ;
	m_ui->actionManage_system_partitions->setFont( Font ) ;
	m_ui->actionManage_non_system_partitions->setFont( Font ) ;
	m_ui->actionManage_volumes_in_gnome_wallet->setFont( Font ) ;
	m_ui->actionManage_volumes_in_internal_wallet->setFont( Font ) ;
	m_ui->actionManage_volumes_in_kde_wallet->setFont( Font ) ;
	m_ui->actionUse_kde_default_wallet->setFont( Font ) ;
	m_ui->actionChange_internal_wallet_password->setFont( Font ) ;
	m_ui->actionVeracrypt_container_in_a_file->setFont( Font ) ;
	m_ui->actionVeracrypt_container_in_a_partition->setFont( Font ) ;
	m_ui->actionOpen_zuluCrypt_pdf->setFont( Font ) ;
	m_ui->actionSelect_Language->setFont( Font ) ;
	m_ui->actionAuto_Open_Mount_Point->setFont( Font ) ;
	m_ui->actionSelect_Icons->setFont( Font ) ;
}

void zuluCrypt::aboutMenuOption( void )
{
	about::instance( this ) ;
}

void zuluCrypt::HelpLuksHeaderBackUp()
{
	QString msg = tr( "\nLUKS,TrueCrypt and VeraCrypt based encrypted volumes have what is called a \"volume header\".\n\n\
A volume header is responsible for storing information necessary to open a header using encrypted volume and any damage \
to it will makes it impossible to open the volume causing permanent loss of encrypted data.\n\n\
The damage to the header is usually caused by accidental formatting of the device or use of \
some buggy partitioning tools or wrongly reassembled logical volumes.\n\n\
Having a backup of the volume header is strongly advised because it is the only way the encrypted data will be accessible \
again after the header is restored if the header on the volume get corrupted.\n\n" ) ;

	DialogMsg m( this ) ;
	m.ShowUIInfo( tr( "Important Information On Volume Header Backup" ),false,msg ) ;
}

void zuluCrypt::volume_property()
{
	m_ui->tableWidget->setEnabled( false ) ;

	auto item = m_ui->tableWidget->currentItem() ;

	if( item == nullptr ){

		return ;
	}

	auto x = m_ui->tableWidget->item( item->row(),0 )->text() ;

	x.replace( "\"","\"\"\"" ) ;

	Task::run( [ x ](){

		auto e = utility::appendUserUID( "%1 -s -d \"%2\"" ) ;

		auto r = utility::Task( e.arg( ZULUCRYPTzuluCrypt,x ) ) ;

		if( r.success() ){

			auto data = r.stdOut() ;

			return QString( " %1" ).arg( QString( data.mid( data.indexOf( '\n' ) + 2 ) ) ) ;
		}else{
			return QString() ;
		}

	} ).then( [ this ]( const QString& r ){

		DialogMsg msg( this ) ;

		if( r.isEmpty() ){

			msg.ShowUIOK( tr( "ERROR!" ),tr( "Volume is not open or was opened by a different user" ) ) ;
		}else{
			msg.ShowUIVolumeProperties( tr( "Volume Properties" ),r ) ;
		}

		m_ui->tableWidget->setEnabled( true ) ;
	} ) ;
}

void zuluCrypt::favAboutToHide()
{
}

void zuluCrypt::favClicked( QAction * ac )
{
	auto r = ac->text() ;

	r.remove( "&" ) ;

	auto _show_dialog = [ this ]( const QString& v,const QString& m ){

		if( !utility::pathPointsToAFolder( v ) ){

			this->ShowPasswordDialog( v,m ) ;
		}
	} ;

	auto e = utility::favoriteClickedOption( r ) ;

	if( e == 1 ){

		this->ShowFavoritesEntries() ;

	}else if( e == 2 ){

		favorites::instance().entries( [ & ]( const favorites::entry& e ){

			_show_dialog( e.volumePath,e.mountPointPath ) ;
		} ) ;
	}else{
		auto m = utility::split( r,'\t' ) ;
		_show_dialog( m.at( 0 ),m.at( 1 ) ) ;
	}
}

void zuluCrypt::readFavorites()
{
	utility::readFavorites( m_ui->menuFavorites,false,false ) ;
	this->updateTrayContextMenu() ;
}

void zuluCrypt::addToFavorite()
{
	auto item = m_ui->tableWidget->currentItem() ;
	auto x = m_ui->tableWidget->item( item->row(),0 )->text() ;
	auto y = x.split( "/" ).last() ;
	utility::addToFavorite( x,y ) ;
}

void zuluCrypt::menuKeyPressed()
{
	auto table = m_ui->tableWidget ;

	if( table->rowCount() > 0 ){

		this->itemClicked( m_ui->tableWidget->currentItem(),false ) ;
	}
}

void zuluCrypt::openFolder()
{
	auto table = m_ui->tableWidget ;

	if( table->rowCount() > 0 ){

		auto item = table->currentItem() ;
		this->openFolder( table->item( item->row(),1 )->text() ) ;
	}
}

void zuluCrypt::openSharedFolder()
{
	this->openFolder( m_sharedMountPoint ) ;
}

void zuluCrypt::openFolder( const QString& path )
{
	auto x = tr( "WARNING!" ) ;
	auto y = tr( "Could not open mount point because \"%1\" tool does not appear to be working correctly").arg( m_openPath ) ;

	utility::openPath( path,m_openPath,this,x,y ) ;
}

void zuluCrypt::openpdf()
{
	help::instance( this,m_openPath ) ;
}

void zuluCrypt::itemClicked0( QTableWidgetItem * it )
{
	this->itemClicked( it,true ) ;
}

void zuluCrypt::itemClicked( QTableWidgetItem * item,QPoint point )
{
	QMenu m ;

	m.setFont( this->font() ) ;

	auto m_point = item->tableWidget()->item( item->row(),1 )->text() ;

	m_sharedMountPoint = utility::sharedMountPointPath( m_point ) ;

	if( m_sharedMountPoint.isEmpty() ){

		auto aa = static_cast< void( zuluCrypt::* )() >( &zuluCrypt::openFolder ) ;

		connect( m.addAction( tr( "Open Folder" ) ),&QAction::triggered,this,aa ) ;
	}else{
		auto aa = static_cast< void( zuluCrypt::* )() >( &zuluCrypt::openFolder ) ;

		connect( m.addAction( tr( "Open Private Folder" ) ),&QAction::triggered,
			 this,aa ) ;
		connect( m.addAction( tr( "Open Shared Folder" ) ),&QAction::triggered,
			 this,&zuluCrypt::openSharedFolder ) ;
	}

	m.addSeparator() ;

	auto ac = m.addAction( tr( "Properties" ) ) ;
	//ac->setEnabled( !m_ui->tableWidget->item( item->row(),2 )->text().startsWith( "bitlocker" ) ) ;
	connect( ac,&QAction::triggered,this,&zuluCrypt::volume_property ) ;

	m.addSeparator() ;

	if( m_ui->tableWidget->item( item->row(),2 )->text().startsWith( "luks" ) ){

		m.addSeparator() ;

		connect( m.addAction( tr( "Add Key" ) ),&QAction::triggered,this,&zuluCrypt::luksAddKeyContextMenu ) ;
		connect( m.addAction( tr( "Remove Key" ) ),&QAction::triggered,this,&zuluCrypt::luksDeleteKeyContextMenu ) ;

		auto ac = m.addAction( tr( "Show Key Slots Information" ) ) ;

		ac->setEnabled( utility::canShowKeySlotProperties() ) ;

		connect( ac,&QAction::triggered,this,&zuluCrypt::showLUKSSlotsInfo ) ;

		m.addSeparator() ;

		connect( m.addAction( tr( "Backup LUKS Header" ) ),&QAction::triggered,this,&zuluCrypt::luksHeaderBackUpContextMenu ) ;
	}

	m.addSeparator() ;

	auto volume_id = m_ui->tableWidget->item( item->row(),0 )->text() + "\t" ;

	bool has_favorite = false ;

	favorites::instance().entries( [ & ]( const favorites::entry& e ){

		if( e.volumePath.startsWith( volume_id ) ){

			has_favorite = true ;

			return true ;
		}

		return false ;
	} ) ;

	ac = m.addAction( tr( "Add To Favorite" ) ) ;

	if( has_favorite ){

		ac->setEnabled( false ) ;
	}else{
		ac->setEnabled( true ) ;
		ac->connect( ac,&QAction::triggered,this,&zuluCrypt::addToFavorite ) ;
	}

	m.addSeparator() ;

	connect( m.addAction( tr( "Unmount" ) ),&QAction::triggered,this,&zuluCrypt::close ) ;

	m.addSeparator() ;
	m.addAction( tr( "Cancel" ) ) ;

	m.exec( point ) ;
}

void zuluCrypt::itemClicked( QTableWidgetItem * item,bool clicked )
{
	if( clicked ){

		this->itemClicked( item,QCursor::pos() ) ;
	}else{
		int x = m_ui->tableWidget->columnWidth( 0 ) ;
		int y = m_ui->tableWidget->rowHeight( item->row() ) * item->row() + 20 ;

		this->itemClicked( item,m_ui->tableWidget->mapToGlobal( QPoint( x,y ) ) ) ;
	}
}

void zuluCrypt::setDefaultWallet()
{
	m_ui->actionUse_kde_default_wallet->setEnabled( false ) ;
	m_ui->actionUse_kde_default_wallet->setEnabled( true ) ;
}

void zuluCrypt::luksAddKeyContextMenu( void )
{
	auto item = m_ui->tableWidget->currentItem() ;

	this->ShowAddKeyContextMenu( m_ui->tableWidget->item( item->row(),0 )->text() ) ;
}

void zuluCrypt::luksDeleteKeyContextMenu( void )
{
	auto item = m_ui->tableWidget->currentItem() ;

	this->ShowDeleteKeyContextMenu( m_ui->tableWidget->item( item->row(),0 )->text() ) ;
}

void zuluCrypt::UIMessage( QString title,QString message )
{
	DialogMsg msg( this ) ;
	msg.ShowUIOK( title,message ) ;
}

void zuluCrypt::closeStatusErrorMessage( int st )
{
	switch ( st ) {

		case 0 :break ;
		case 1 :UIMessage( tr( "ERROR!" ),tr( "Close failed, volume is not open or was opened by a different user" ) ) ;			break ;
		case 2 :UIMessage( tr( "ERROR!" ),tr( "Close failed, one or more files in the volume are in use." ) ) ;					break ;
		case 3 :UIMessage( tr( "ERROR!" ),tr( "Close failed, volume does not have an entry in /etc/mtab" ) ) ;					break ;
		case 4 :UIMessage( tr( "ERROR!" ),tr( "Close failed, could not get a lock on /etc/mtab~" ) ) ;						break ;
		case 5 :UIMessage( tr( "ERROR!" ),tr( "Close failed, volume is unmounted but could not close mapper,advice to close it manually" ) ) ;	break ;
		case 6 :UIMessage( tr( "ERROR!" ),tr( "Close failed, could not resolve full path of device\n" ) ) ;               			break ;
		case 7 :UIMessage( tr( "ERROR!" ),tr( "Close failed, shared mount point appear to be busy\n" ) ) ;					break ;
		case 8 :UIMessage( tr( "ERROR!" ),tr( "Close failed, shared mount point appear to belong to a different user or multiple mount points detected\n" ) ) ;			break ;
		case 9 :UIMessage( tr( "ERROR!" ),tr( "Close failed, shared mount point appear to be in an ambiguous state,advice to unmount manually" ) ) ;break ;
		case 10:UIMessage( tr( "ERROR!" ),tr( "Close failed, multiple mount points for the volume detected" ) ) ;                               break ;

		case 110:UIMessage( tr( "ERROR!" ),tr( "Close failed, could not find any partition with the presented UUID" ) ) ;			break ;
		default:UIMessage( tr( "ERROR!" ),tr( "Unrecognized error with status number %1 encountered" ).arg( st ) ) ;
	}
}

void zuluCrypt::close()
{
	m_ui->tableWidget->setEnabled( false ) ;

	auto item = m_ui->tableWidget->currentItem() ;

	auto path = m_ui->tableWidget->item( item->row(),0 )->text().replace( "\"","\"\"\"" ) ;

	Task::run( [ path ](){

		auto exe = utility::appendUserUID( "%1 -q -d \"%2\"" ).arg( ZULUCRYPTzuluCrypt,path ) ;

		return utility::Task( exe ).exitCode() ;

	} ).then( [ this ]( int r ){

		this->closeStatusErrorMessage( r ) ;
	} ) ;
}

void zuluCrypt::setFileManager()
{
	fileManager::instance( this,[ this ]( const QString& e ){ m_openPath = e ; } ) ;
}

void zuluCrypt::volumeRestoreHeader()
{
	managevolumeheader::instance( this ).restoreHeader() ;
}

void zuluCrypt::volumeHeaderBackUp()
{
	managevolumeheader::instance( this ).backUpHeader() ;
}

void zuluCrypt::luksHeaderBackUpContextMenu()
{
	auto item = m_ui->tableWidget->currentItem() ;

	auto device = m_ui->tableWidget->item( item->row(),0 )->text() ;

	managevolumeheader::instance( this ).backUpHeader( device ) ;
}

void zuluCrypt::ShowAddKeyContextMenu( QString key )
{
	luksaddkey::instance( this ).ShowUI( key ) ;
}

void zuluCrypt::ShowAddKey()
{
	luksaddkey::instance( this ).ShowUI() ;
}

void zuluCrypt::ShowDeleteKeyContextMenu( QString key )
{
	luksdeletekey::instance( this ).ShowUI( key ) ;
}

void zuluCrypt::ShowDeleteKey()
{
	luksdeletekey::instance( this ).ShowUI() ;
}

void zuluCrypt::ShowCreateKeyFile()
{
	createkeyfile::instance( this ) ;
}

void zuluCrypt::ShowFavoritesEntries()
{
	favorites2::instance( this,m_secrets,[](){


	} ) ;
}

void zuluCrypt::ShowCreateFile()
{
	createfile::instance( this,[ this ]( const QString& file ){

		if( utility::pathExists( file ) ){

			createvolume::instance( this ).ShowFile( file ) ;
		}
	} ) ;
}

void zuluCrypt::ShowNonSystemPartitions()
{
	openvolume::instance( this,false ).ShowNonSystemPartitions( [ this ]( const QString& e ){

		createvolume::instance( this ).ShowPartition( e ) ;
	} ) ;
}

void zuluCrypt::ShowOpenPartition()
{
	openvolume::instance( this,true ).showEncryptedOnly().ShowAllPartitions( [ this ]( const QString& e ){

		this->setUpPasswordDialog().ShowUI( e ) ;
	} ) ;
}

passwordDialog& zuluCrypt::setUpPasswordDialog()
{
	return passwordDialog::instance( m_ui->tableWidget,this,m_secrets,[ this ]( const QString& path ){

		if( m_autoOpenMountPoint ){

			this->openFolder( path ) ;
		}
	 } ) ;
}

void zuluCrypt::ShowPasswordDialog0()
{
	this->setUpPasswordDialog().ShowUI() ;
}

void zuluCrypt::ShowPasswordDialog( QString x,QString y )
{
	if( x.endsWith( ".zc" ) || x.endsWith( ".zC" ) ){

		this->decryptFile( x ) ;
	}else{
		this->setUpPasswordDialog().ShowUI( x,y ) ;
	}
}

void zuluCrypt::ShowEraseDataDialog()
{
	erasedevice::instance( this ).ShowUI() ;
}

void zuluCrypt::encryptFile()
{
	cryptfiles::instance( this ).encrypt() ;
}

void zuluCrypt::decryptFile0()
{
	cryptfiles::instance( this ).decrypt() ;
}

void zuluCrypt::decryptFile( const QString& e )
{
	cryptfiles::instance( this ).decrypt( e ) ;
}

zuluCrypt::~zuluCrypt()
{
	if( !m_ui ){

		return ;
	}

	auto q = m_ui->tableWidget ;

	const auto& r = this->window()->geometry() ;

	utility::setWindowDimensions( "zuluCrypt",{ r.x(),
						    r.y(),
						    r.width(),
						    r.height(),
						    q->columnWidth( 0 ),
						    q->columnWidth( 1 ),
						    q->columnWidth( 2 ) } ) ;

	delete m_ui ;
}
