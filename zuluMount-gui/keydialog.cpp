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

#include "keydialog.h"
#include "ui_keydialog.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QDir>
#include <QTableWidget>
#include <QDebug>
#include <QFile>

#include "bin_path.h"
#include "../zuluCrypt-gui/dialogmsg.h"
#include "task.hpp"
#include "../zuluCrypt-cli/constants.h"
#include "plugin_path.h"
#include "../zuluCrypt-gui/utility.h"
#include "lxqt_wallet.h"
#include "zulumounttask.h"
#include "zulumounttask.h"
#include "siritask.h"
#include "veracryptpimdialog.h"
#include "../zuluCrypt-gui/favorites2.h"
#include "../zuluCrypt-gui/tcrypt.h"

keyDialog::keyDialog( QWidget * parent,
		      QTableWidget * table,
		      secrets& s,
		      const volumeProperty& e,
		      std::function< void() > p,
		      std::function< void( const QString& ) > q ) :
	QDialog( parent ),
	m_ui( new Ui::keyDialog ),
	m_secrets( s ),
	m_cancel( std::move( p ) ),
	m_success( std::move( q ) )
{
	m_ui->setupUi( this ) ;

	m_label.setOptions( m_ui->veraCryptWarning,m_ui->pushButton ) ;

	m_ui->lineEditKey->setMaxLength( 32767 ) ;

	m_ui->checkBoxShareMountPoint->setToolTip( utility::shareMountPointToolTip() ) ;
	m_table = table ;
	m_path = e.volumeName() ;
	m_working = false ;
	m_encryptedFolder = e.fileSystem() == "cryptfs" ;

	QString msg ;

	if( e.fileSystem() == "crypto_LUKS" ){

		msg = tr( "Mount A LUKS volume in \"%1\"").arg( m_path ) ;
	}else{
		msg = tr( "Mount An Encrypted Volume In \"%1\"").arg( m_path ) ;
	}

	m_ui->checkBoxVisibleKey->setToolTip( tr( "Check This Box To Make Password Visible" ) ) ;

	m_ui->checkBoxShareMountPoint->setChecked( utility::mountWithSharedMountPoint() ) ;

	this->setWindowTitle( msg ) ;

	m_ui->lineEditMountPoint->setText( m_path ) ;
	m_ui->pbOpenMountPoint->setIcon( QIcon( ":/folder.png" ) ) ;

	m_menu = new QMenu( this ) ;

	connect( m_menu,&QMenu::triggered,this,&keyDialog::pbPluginEntryClicked ) ;

	this->setFixedSize( this->size() ) ;
	this->setWindowFlags( Qt::Window | Qt::Dialog ) ;
	this->setFont( parent->font() ) ;

	m_ui->lineEditKey->setFocus() ;

	m_ui->checkBoxOpenReadOnly->setChecked( utility::getOpenVolumeReadOnlyOption( "zuluMount-gui" ) ) ;

	m_ui->pbkeyOption->setEnabled( false ) ;

	m_ui->lineEditKey->setEchoMode( QLineEdit::Password ) ;

	m_veraCryptVolumeType.setValues( m_ui->checkBoxVeraCryptVolume,
					 m_ui->checkBoxVeraCryptSystemVolume,
					 utility::autoSetVolumeAsVeraCrypt() ) ;
	if( m_encryptedFolder ){

		m_ui->checkBoxVeraCryptVolume->setEnabled( false ) ;
		m_ui->checkBoxVeraCryptSystemVolume->setEnabled( false ) ;
		m_ui->lineEditPIM->setEnabled( false ) ;
		m_ui->labelVeraCryptPIM->setEnabled( false ) ;
	}

	auto cc = static_cast< void( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ) ;

	connect( m_ui->pbCancel,&QPushButton::clicked,this,&keyDialog::pbCancel ) ;
	connect( m_ui->pbOpen,&QPushButton::clicked,this,&keyDialog::openVolume ) ;
	connect( m_ui->pbkeyOption,&QPushButton::clicked,this,&keyDialog::pbkeyOption ) ;
	connect( m_ui->pbOpenMountPoint,&QPushButton::clicked,this,&keyDialog::pbMountPointPath ) ;
	connect( m_ui->checkBoxOpenReadOnly,&QCheckBox::stateChanged,this,&keyDialog::cbMountReadOnlyStateChanged ) ;
	connect( m_ui->cbKeyType,cc,this,&keyDialog::cbActicated ) ;
	connect( m_ui->checkBoxVisibleKey,&QCheckBox::stateChanged,this,&keyDialog::cbVisibleKeyStateChanged ) ;

	m_ui->pbOpenMountPoint->setVisible( false ) ;

	connect( m_ui->checkBoxShareMountPoint,&QCheckBox::stateChanged,[]( int s ){

		utility::mountWithSharedMountPoint( s == Qt::Checked ) ;
	} ) ;

	const auto& m = e.mountPoint() ;

	if( m.isEmpty() || m == "Nil" ){

		m_point = utility::mountPathPostFix( m_path.split( "/" ).last() ) ;
	}else{
		m_point = utility::mountPathPostFix( m ) ;
	}

	m_ui->lineEditMountPoint->setText( m_point ) ;

	m_ui->pbOptions->setEnabled( !m_encryptedFolder ) ;

	m_ui->frame->setVisible( false ) ;

	connect( m_ui->pbOptions,&QPushButton::clicked,[ this ](){

		m_ui->lineEditVolumeOffset->setText( m_deviceOffSet ) ;

		m_ui->lineEditFsOptions->setText( utility::fileSystemOptions( m_path ) ) ;

		m_ui->frame->setVisible( true ) ;
	} ) ;

	connect( m_ui->pbSet,&QPushButton::clicked,[ this ](){

		m_ui->frame->setVisible( false ) ;

		auto e = m_ui->lineEditVolumeOffset->text() ;

		if( !e.isEmpty() ){

			m_deviceOffSet = QString( " -o %1" ).arg( e ) ;
		}

		m_options = m_ui->lineEditFsOptions->text() ;
	} ) ;

	connect( m_ui->pbCancelOptions,&QPushButton::clicked,[ this ](){

		m_ui->frame->setVisible( false ) ;
	} ) ;

	m_ui->cbKeyType->addItem( tr( "Key" ) ) ;
	m_ui->cbKeyType->addItem( tr( "KeyFile" ) ) ;
	m_ui->cbKeyType->addItem( tr( "Key+KeyFile" ) ) ;
	m_ui->cbKeyType->addItem( tr( "Plugin" ) ) ;

	if( m_encryptedFolder ){

		m_ui->checkBoxShareMountPoint->setEnabled( false ) ;
	}else{
		m_ui->cbKeyType->addItem( tr( "TrueCrypt/VeraCrypt Keys" ) ) ;
	}

	m_ui->cbKeyType->addItem( tr( "YubiKey Challenge/Response" ) ) ;

	m_veraCryptWarning.setWarningLabel( m_ui->veraCryptWarning ) ;

	m_ui->pbkeyOption->setVisible( false ) ;
	m_ui->checkBoxVisibleKey->setVisible( true ) ;
	this->installEventFilter( this ) ;
}

bool keyDialog::eventFilter( QObject * watched,QEvent * event )
{
	return utility::eventFilter( this,watched,event,[ this ](){ this->pbCancel() ; } ) ;
}

void keyDialog::cbMountReadOnlyStateChanged( int state )
{
	m_ui->checkBoxOpenReadOnly->setEnabled( false ) ;
	m_ui->checkBoxOpenReadOnly->setChecked( utility::setOpenVolumeReadOnly( this,state == Qt::Checked,QString( "zuluMount-gui" ) ) ) ;
	m_ui->checkBoxOpenReadOnly->setEnabled( true ) ;

	if( m_ui->lineEditKey->text().isEmpty() ){

		m_ui->lineEditKey->setFocus() ;

	}else if( m_ui->lineEditMountPoint->text().isEmpty() ){

		m_ui->lineEditMountPoint->setFocus() ;
	}else{
		m_ui->pbOpen->setFocus() ;
	}
}

void keyDialog::pbMountPointPath()
{
	auto msg = tr( "Select A Folder To Create A Mount Point In" ) ;
	auto Z = QFileDialog::getExistingDirectory( this,msg,utility::homePath(),QFileDialog::ShowDirsOnly ) ;

	if( !Z.isEmpty() ){

		while( true ){

			if( Z.endsWith( '/' ) ){

				Z.truncate( Z.length() - 1 ) ;
			}else{
				break ;
			}
		}

		Z = Z + "/" + m_ui->lineEditMountPoint->text().split( "/" ).last() ;
		m_ui->lineEditMountPoint->setText( Z ) ;
	}
}

void keyDialog::cbVisibleKeyStateChanged( int s )
{
	auto m = m_ui->cbKeyType->currentIndex() ;

	if( m == keyDialog::Key || m == keyDialog::yubikey ){

		if( s == Qt::Checked ){

			m_ui->lineEditKey->setEchoMode( QLineEdit::Normal ) ;
		}else{
			m_ui->lineEditKey->setEchoMode( QLineEdit::Password ) ;
		}

		m_ui->lineEditKey->setFocus() ;
	}
}

void keyDialog::enableAll()
{
	auto m = m_ui->cbKeyType->currentIndex() ;

	m_ui->checkBoxVisibleKey->setEnabled( m == keyDialog::Key || m == keyDialog::yubikey ) ;
	m_ui->checkBoxVeraCryptVolume->setEnabled( true ) ;
	m_ui->pbOptions->setEnabled( !m_encryptedFolder ) ;
	m_ui->label_2->setEnabled( true ) ;
	m_ui->lineEditMountPoint->setEnabled( true ) ;
	m_ui->pbOpenMountPoint->setEnabled( true ) ;
	m_ui->pbCancel->setEnabled( true ) ;
	m_ui->pbOpen->setEnabled( true ) ;
	m_ui->label->setEnabled( true ) ;
	m_ui->cbKeyType->setEnabled( true ) ;

	auto index = m_ui->cbKeyType->currentIndex() ;

	m_ui->lineEditKey->setEnabled( index == keyDialog::Key || index == keyDialog::yubikey ) ;

	m_ui->pbkeyOption->setEnabled( index == keyDialog::Key ||
				       index == keyDialog::yubikey ||
				       index == keyDialog::keyfile ) ;

	m_ui->checkBoxOpenReadOnly->setEnabled( true ) ;

	m_ui->checkBoxShareMountPoint->setEnabled( !m_encryptedFolder ) ;

	m_ui->checkBoxVeraCryptVolume->setEnabled( true ) ;
	m_ui->checkBoxVeraCryptSystemVolume->setEnabled( true ) ;
	m_ui->lineEditPIM->setEnabled( true ) ;
	m_ui->labelVeraCryptPIM->setEnabled( true ) ;
}

void keyDialog::disableAll()
{
	m_ui->checkBoxVisibleKey->setEnabled( false ) ;
	m_ui->checkBoxVeraCryptVolume->setEnabled( false ) ;
	m_ui->cbKeyType->setEnabled( false ) ;
	m_ui->pbOptions->setEnabled( false ) ;
	m_ui->pbkeyOption->setEnabled( false ) ;
	m_ui->label_2->setEnabled( false ) ;
	m_ui->lineEditMountPoint->setEnabled( false ) ;
	m_ui->pbOpenMountPoint->setEnabled( false ) ;
	m_ui->lineEditKey->setEnabled( false ) ;
	m_ui->pbCancel->setEnabled( false ) ;
	m_ui->pbOpen->setEnabled( false ) ;
	m_ui->label->setEnabled( false ) ;
	m_ui->checkBoxOpenReadOnly->setEnabled( false ) ;
	m_ui->checkBoxShareMountPoint->setEnabled( false ) ;
	m_ui->checkBoxVeraCryptVolume->setEnabled( false ) ;
	m_ui->checkBoxVeraCryptSystemVolume->setEnabled( false ) ;
	m_ui->lineEditPIM->setEnabled( false ) ;
	m_ui->labelVeraCryptPIM->setEnabled( false ) ;
}

void keyDialog::KeyFile()
{
	if( m_ui->cbKeyType->currentIndex() == keyDialog::keyfile ){

		auto msg = tr( "Select A File To Be Used As A Keyfile" ) ;
		auto Z = QFileDialog::getOpenFileName( this,msg,utility::homePath() ) ;

		if( !Z.isEmpty() ){

			m_ui->lineEditKey->setText( Z ) ;
		}
	}
}

void keyDialog::pbkeyOption()
{
	auto keyType = m_ui->cbKeyType->currentIndex() ;

	if( keyType == keyDialog::plugin ){

		this->Plugin() ;

	}else if( keyType == keyDialog::keyfile ){

		this->KeyFile() ;
	}
}

void keyDialog::Plugin()
{
	m_menu->clear() ;

	utility::addPluginsToMenu( *m_menu ) ;

	m_menu->setFont( this->font() ) ;

	m_menu->addSeparator() ;

	m_menu->addAction( tr( "Cancel" ) )->setObjectName( "Cancel" ) ;

	m_menu->exec( QCursor::pos() ) ;
}

void keyDialog::pbPluginEntryClicked( QAction * e )
{
	auto m = e->objectName() ;

	utility::setDefaultPlugin( m ) ;

	auto r = e->text() ;

	r.remove( "&" ) ;

	if( m != "Cancel" ){

		m_ui->lineEditKey->setText( r ) ;
	}
}

void keyDialog::closeEvent( QCloseEvent * e )
{
	e->ignore() ;
	this->pbCancel() ;
}

void keyDialog::encryptedFolderMount()
{
	if( m_key.isEmpty() ){

		m_label.show( tr( "Atleast one required field is empty" ) ) ;

		return this->enableAll() ;
	}

	auto m = utility::mountPath( utility::mountPathPostFix( m_ui->lineEditMountPoint->text() ) ) ;

	auto ro = m_ui->checkBoxOpenReadOnly->isChecked() ;

	auto e = siritask::encryptedFolderMount( { m_path,m,m_key,QString(),QString(),
						   QString(),ro,m_success } ).await() ;

	switch( e.status() ){

	case siritask::status::success :

		return this->HideUI() ;

	case siritask::status::cryfs :

		m_label.show( tr( "Failed to unlock a cryfs volume.\nWrong password entered" ) ) ;
		break;

	case siritask::status::encfs :

		m_label.show( tr( "Failed to unlock an encfs volume.\nWrong password entered" ) ) ;
		break;

	case siritask::status::gocryptfs :

		m_label.show( tr( "Failed to unlock a gocryptfs volume.\nWrong password entered" ) ) ;
		break;

	case siritask::status::ecryptfs :

		m_label.show( tr( "Failed to unlock an ecryptfs volume.\nWrong password entered" ) ) ;
		break;

	case siritask::status::ecryptfsIllegalPath :

		m_label.show( tr( "A Space Character Is Not Allowed In Paths When Using Ecryptfs Backend And Polkit" ) ) ;
		break;

	case siritask::status::securefs :

		m_label.show( tr( "Failed to unlock a securefs volume.\nWrong password entered" ) ) ;
		break;

	case siritask::status::cryfsNotFound :

		m_label.show( tr( "Failed to unlock a cryfs volume.\ncryfs executable could not be found" ) ) ;
		break;

	case siritask::status::securefsNotFound :

		m_label.show( tr( "Failed to unlock a securefs volume.\nsecurefs executable could not be found" ) ) ;
		break;

	case siritask::status::gocryptfsNotFound :

		m_label.show( tr( "Failed to unlock a gocryptfs volume.\ngocryptfs executable could not be found" ) ) ;
		break;

	case siritask::status::encfsNotFound :

		m_label.show( tr( "Failed to unlock an encfs volume.\nencfs executable could not be found" ) ) ;
		break;

	case siritask::status::ecryptfs_simpleNotFound :

		m_label.show( tr( "Failed to unlock an ecryptfs volume.\necryptfs-simple executable could not be found" ) ) ;
		break;

	case siritask::status::failedToCreateMountPoint :

		m_label.show( tr( "Failed to create mount point" ) ) ;
		break;

	case siritask::status::unknown :

		m_label.show( tr( "Failed to unlock the volume.\nNot supported volume encountered" ) ) ;
		break;

	case siritask::status::ecrypfsBadExePermissions :

		m_label.show( tr( "This backend requires root's privileges and an attempt to acquire them has failed." ) ) ;
		break;

	case siritask::status::cryfsMigrateFileSystem :

		m_label.show( tr( "zuluMount Can Not Unlock This Volume Because Its FileSystem Has To Manually Be Converted To The Version Of Cryfs That Is Currently In Use.\n\nRun Cryfs With This Volume To Manually Update This Volume's FileSystem." ) ) ;
		break;

	case siritask::status::backendFail :
	default:
		m_label.show( e.msg() ) ;
		break;
	}

	auto mm = m_ui->cbKeyType->currentIndex() ;

	if( mm == keyDialog::Key || mm == keyDialog::yubikey ){

		m_ui->lineEditKey->clear() ;
	}

	m_ui->lineEditKey->setFocus() ;

	this->enableAll() ;
}

bool keyDialog::errorNotFound( int r )
{
	DialogMsg msg( this ) ;

	switch ( r ){
		case 0 : break ;
		case 1 : m_label.show( tr( "Failed to mount ntfs/exfat file system using ntfs-3g,is ntfs-3g/exfat package installed?" ) ) ; break ;
		case 2 : m_label.show( tr( "There seem to be an open volume accociated with given address" ) ) ;				break ;
		case 3 : m_label.show( tr( "No file or device exist on given path" ) ) ; 						break ;
		case 4 : m_label.show( tr( "Volume could not be opened with the presented key" ) ) ;					break ;
		case 5 : m_label.show( tr( "Insufficient privilege to mount the device with given options" ) ) ;				break ;
		case 6 : m_label.show( tr( "Insufficient privilege to open device in read write mode or device does not exist" ) ) ;	break ;
		case 7 : m_label.show( tr( "Only root user can perform this operation" ) ) ;						break ;
		case 8 : m_label.show( tr( "-O and -m options can not be used together" ) ) ;						break ;
		case 9 : m_label.show( tr( "Could not create mount point, invalid path or path already taken" ) ) ;			break ;
		case 10: m_label.show( tr( "Shared mount point path aleady taken" ) ) ;							break ;
		case 11: m_label.show( tr( "There seem to be an opened mapper associated with the device" ) ) ;				break ;
		case 12: m_label.show( tr( "Could not get a passphrase from the module" ) ) ;						break ;
		case 13: m_label.show( tr( "Could not get passphrase in silent mode" ) ) ;						break ;
		case 14: m_label.show( tr( "Insufficient memory to hold passphrase" ) ) ;						break ;
		case 15: m_label.show( tr( "One or more required argument(s) for this operation is missing" ) ) ;			break ;
		case 16: m_label.show( tr( "Invalid path to key file" ) ) ;								break ;
		case 17: m_label.show( tr( "Could not get enought memory to hold the key file" ) ) ;					break ;
		case 18: m_label.show( tr( "Insufficient privilege to open key file for reading" ) ) ;					break ;
		case 19: m_label.show( tr( "Could not get a passphrase through a local socket" ) ) ;					break ;
		case 20: m_label.show( tr( "Failed to mount a filesystem:invalid/unsupported mount option or unsupported file system encountered" ) ) ;	break ;
		case 21: m_label.show( tr( "Could not create a lock on /etc/mtab" ) ) ;							break ;
		case 22: m_label.show( tr( "Insufficient privilege to open a system volume.\n\nConsult menu->help->permission for more informaion\n" ) ) ;					break ;
		case 113:m_label.show( tr( "A non supported device encountered,device is missing or permission denied\n\
Possible reasons for getting the error are:\n1.Device path is invalid.\n2.The device has LVM or MDRAID signature" ) ) ;					break ;
		default: return true ;
	}

	return false ;
}

void keyDialog::openVolume()
{
	this->disableAll() ;

	auto keyType = m_ui->cbKeyType->currentIndex() ;

	if( m_encryptedFolder ){

		if( keyType == keyDialog::Key ){

			m_key = m_ui->lineEditKey->text().toLatin1() ;

		}else if( keyType == keyDialog::keyfile ){

			QFile f( m_ui->lineEditKey->text() ) ;

			f.open( QIODevice::ReadOnly ) ;

			m_key = f.readAll() ;

		}else if( keyType == keyDialog::keyKeyFile ){

			if( utility::pluginKey( m_secrets.parent(),&m_key,"hmac" ) ){

				return this->enableAll() ;
			}

		}else if( keyType == keyDialog::plugin ){

			/*
			 * m_key is already set
			 */

		}else if( keyType + 1 == keyDialog::yubikey ){

			utility::debug() << "ssss" ;

			auto s = utility::yubiKey( m_ui->lineEditKey->text() ) ;

			if( s.has_value() ){

				m_key = s.value() ;
			}else{
				m_label.show( tr( "Failed To Locate Or Run Yubikey's \"ykchalresp\" Program." ) ) ;
				return this->enableAll() ;
			}
		}

		return this->encryptedFolderMount() ;
	}

	if( m_ui->lineEditKey->text().isEmpty() ){

		 if( keyType == keyDialog::plugin ){

			m_label.show( tr( "Plug in name field is empty" ) ) ;

			m_ui->lineEditKey->setFocus() ;

			return this->enableAll() ;

		}else if( keyType == keyDialog::keyfile ){

			m_label.show( tr( "Keyfile field is empty" ) ) ;

			m_ui->lineEditKey->setFocus() ;

			return this->enableAll() ;
		}
	}

	auto test_name = m_ui->lineEditMountPoint->text() ;

	if( test_name.contains( "/" ) ){

		m_label.show( tr( "\"/\" character is not allowed in the mount name field" ) ) ;

		m_ui->lineEditKey->setFocus() ;

		return this->enableAll() ;
	}

	QString m ;

	if( keyType == keyDialog::yubikey ){

		auto s = utility::yubiKey( m_ui->lineEditKey->text() ) ;

		if( s.has_value() ){

			auto addr = utility::keyPath() ;
			m = QString( "-f %1" ).arg( addr ) ;

			utility::keySend( addr,s.value() ) ;
		}else{
			m_label.show( tr( "Failed To Locate Or Run Yubikey's \"ykchalresp\" Program." ) ) ;
			return this->enableAll() ;
		}

	}else if( keyType == keyDialog::Key ){

		auto addr = utility::keyPath() ;
		m = QString( "-f %1" ).arg( addr ) ;

		utility::keySend( addr,m_ui->lineEditKey->text() ) ;

	}else if( keyType == keyDialog::keyKeyFile ){

		if( utility::pluginKey( m_secrets.parent(),&m_key,"hmac" ) ){

			return this->enableAll() ;
		}

		auto addr = utility::keyPath() ;
		m = QString( "-f %1" ).arg( addr ) ;

		utility::keySend( addr,m_key ) ;

	}else if( keyType == keyDialog::keyfile ){

		auto e = m_ui->lineEditKey->text().replace( "\"","\"\"\"" ) ;
		m = "-f \"" + utility::resolvePath( e ) + "\"" ;

	}else if( keyType == keyDialog::plugin ){

		auto r = m_ui->lineEditKey->text() ;

		if( r == "hmac" || r == "gpg" || r == "keykeyfile" ){

			if( utility::pluginKey( m_secrets.parent(),&m_key,r ) ){

				return this->enableAll() ;
			}
		}
		if( m_key.isEmpty() ){

			m = "-G " + m_ui->lineEditKey->text().replace( "\"","\"\"\"" ) ;
		}else{

			auto addr = utility::keyPath() ;
			m = QString( "-f %1" ).arg( addr ) ;

			utility::keySend( addr,m_key ) ;
		}

	}else if( keyType == keyDialog::tcryptKeys ){

		QEventLoop wait ;

		bool cancelled = false ;

		tcrypt::instance( this,false,[ this,&wait ]( const QString& key,
				  const QStringList& keyFiles ){

			m_key = key.toLatin1() ;
			m_keyFiles = keyFiles ;

			wait.exit() ;

		},[ this,&wait,&cancelled ](){

			cancelled = true ;
			m_key.clear() ;
			m_keyFiles.clear() ;

			wait.exit() ;
		} ) ;

		wait.exec() ;

		if( cancelled ){

			return this->enableAll() ;
		}

		auto addr = utility::keyPath() ;
		m = QString( "-f %1 " ).arg( addr ) ;

		utility::keySend( addr,m_key ) ;
	}else{
		qDebug() << "ERROR: Uncaught condition" ;
	}

	auto volume = m_path ;

	if( !volume.startsWith( "/dev/" ) ){

		auto m = utility::loopDevicePath( volume ) ;

		if( !m.isEmpty() ){

			volume = m ;
		}
	}

	volume.replace( "\"","\"\"\"" ) ;

	QString exe = zuluMountPath ;

	if( m_ui->checkBoxShareMountPoint->isChecked() ){

		exe += " -M -m -d \"" + volume + "\"" ;
	}else{
		exe += " -m -d \"" + volume + "\"" ;
	}

	if( m_ui->checkBoxOpenReadOnly->isChecked() ){

		exe += " -e ro" ;
	}else{
		exe += "  e rw" ;
	}

	auto mountPoint = m_ui->lineEditMountPoint->text() ;
	mountPoint.replace( "\"","\"\"\"" ) ;

	exe += " -z \"" + mountPoint + "\"" ;

	if( !m_deviceOffSet.isEmpty() ){

		exe += m_deviceOffSet ;
	}

	if( !m_keyFiles.isEmpty() ){

		for( const auto& it : m_keyFiles ){

			auto e = it ;
			e.replace( "\"","\"\"\"" ) ;

			exe += " -F \"" + e + "\"" ;
		}
	}

	if( m_veraCryptVolumeType.veraCrypt() ){

		auto pim = m_ui->lineEditPIM->text() ;

		if( !pim.isEmpty() ){

			exe += " -t vcrypt." + pim + " " + m ;
		}else{
			exe += " -t vcrypt " + m ;
		}

	}else if( m_veraCryptVolumeType.veraCryptSystem() ){

		auto pim = m_ui->lineEditPIM->text() ;

		if( !pim.isEmpty() ){

			exe += " -t vcrypt-sys." + pim + " " + m ;
		}else{
			exe += " -t vcrypt-sys " + m ;
		}
	}else{
		exe += " " + m ;
	}

	utility::setFileSystemOptions( exe,volume,mountPoint,m_options ) ;

	m_veraCryptWarning.show( m_veraCryptVolumeType.yes() ) ;

	m_working = true ;

	auto s = utility::Task::run( utility::appendUserUID( exe ) ).await() ;

	m_working = false ;

	m_veraCryptWarning.stopTimer() ;

	if( s.success() ){

		m_success( utility::mountPath( mountPoint ) ) ;

		this->HideUI() ;
	}else{
		m_veraCryptWarning.hide() ;

		auto keyType = m_ui->cbKeyType->currentIndex() ;

		int r = s.exitCode() ;

		if( r == 12 && keyType == keyDialog::plugin ){

			/*
			 * A user cancelled the plugin
			 */

			this->enableAll() ;
		}else{
			if( this->errorNotFound( r ) ){

				QString z = s.stdOut() ;

				z.replace( tr( "ERROR: " ),"" ) ;

				m_label.show( z ) ;
			}

			auto m = m_ui->cbKeyType->currentIndex() ;

			if( m == keyDialog::Key || m == keyDialog::yubikey ){

				if( utility::clearPassword() ){

					m_ui->lineEditKey->clear() ;
				}
			}

			this->enableAll() ;

			m_ui->lineEditKey->setFocus() ;

			if( keyType == keyDialog::keyKeyFile ){

				m_ui->cbKeyType->setCurrentIndex( 0 ) ;

				this->key() ;
			}
		}
	}
}

void keyDialog::cbActicated( int e )
{
	if( e == keyDialog::Key || e == keyDialog::yubikey ){

		m_ui->checkBoxVisibleKey->setVisible( true ) ;
		m_ui->pbkeyOption->setVisible( false ) ;
	}else {
		m_ui->checkBoxVisibleKey->setVisible( false ) ;
		m_ui->pbkeyOption->setVisible( true ) ;
	}

	auto _set_yubikey = [ this ](){

		this->plugIn() ;
		m_ui->pbkeyOption->setEnabled( !m_encryptedFolder ) ;
		m_ui->lineEditKey->setText( tr( "YubiKey Challenge/Response" ) ) ;
	} ;

	switch( e ){

		case keyDialog::Key        : return this->key() ;
		case keyDialog::yubikey    : return _set_yubikey() ;
		case keyDialog::keyfile    : return this->keyFile() ;
		case keyDialog::keyKeyFile : return this->keyAndKeyFile() ;
		case keyDialog::plugin     : return this->plugIn() ;
		case keyDialog::tcryptKeys : return this->tcryptGui() ;
	}
}

void keyDialog::keyAndKeyFile()
{
	m_ui->pbkeyOption->setIcon( QIcon( ":/module.png" ) ) ;
	m_ui->lineEditKey->setEchoMode( QLineEdit::Normal ) ;
	m_ui->label->setText( tr( "Plugin name" ) ) ;
	m_ui->pbkeyOption->setEnabled( false ) ;
	m_ui->lineEditKey->setEnabled( false ) ;
	m_ui->lineEditKey->setText( tr( "Key+KeyFile" ) ) ;
}

void keyDialog::plugIn()
{
	m_ui->pbkeyOption->setIcon( QIcon( ":/module.png" ) ) ;
	m_ui->lineEditKey->setEchoMode( QLineEdit::Normal ) ;
	m_ui->label->setText( tr( "Plugin name" ) ) ;
	m_ui->pbkeyOption->setEnabled( !m_encryptedFolder ) ;
	m_ui->lineEditKey->setEnabled( false ) ;

	if( m_encryptedFolder ){

		m_ui->pbkeyOption->setEnabled( false ) ;
		m_ui->lineEditKey->clear() ;
	}else{
		m_ui->pbkeyOption->setEnabled( true ) ;
		m_ui->lineEditKey->setText( utility::defaultPlugin() ) ;
	}
}

void keyDialog::key()
{
	m_ui->pbkeyOption->setIcon( QIcon( ":/passphrase.png" ) ) ;
	m_ui->pbkeyOption->setEnabled( false ) ;
	m_ui->label->setText( tr( "Key" ) ) ;
	m_ui->lineEditKey->setEchoMode( QLineEdit::Password ) ;
	m_ui->lineEditKey->clear() ;
	m_ui->lineEditKey->setEnabled( true ) ;
	m_ui->lineEditKey->setFocus() ;
}

void keyDialog::keyFile()
{
	m_ui->pbkeyOption->setIcon( QIcon( ":/keyfile.png" ) ) ;
	m_ui->lineEditKey->setEchoMode( QLineEdit::Normal ) ;
	m_ui->label->setText( tr( "Keyfile path" ) ) ;
	m_ui->pbkeyOption->setEnabled( true ) ;
	m_ui->lineEditKey->clear() ;
	m_ui->lineEditKey->setEnabled( true ) ;
	m_ui->lineEditKey->setFocus() ;
}

void keyDialog::tcryptGui()
{
	m_ui->pbkeyOption->setIcon( QIcon( ":/module.png" ) ) ;
	m_ui->lineEditKey->setEchoMode( QLineEdit::Normal ) ;
	m_ui->label->setText( tr( "Plugin name" ) ) ;
	m_ui->pbkeyOption->setEnabled( false ) ;
	m_ui->lineEditKey->setEnabled( false ) ;
	m_ui->lineEditKey->setText( tr( "TrueCrypt/VeraCrypt Keys" ) ) ;
}

void keyDialog::pbCancel()
{
	this->HideUI() ;
	m_cancel() ;
}

void keyDialog::ShowUI()
{
	auto m = favorites2::settings().autoMountBackEnd() ;

	if( m.isValid() ){

		auto e = utility::pathToUUID( m_path ) ;

		auto secret = m_secrets.walletBk( m.bk() ).getKey( e ) ;

		if( secret.notConfigured ){

			DialogMsg msg( this ) ;
			msg.ShowUIOK( tr( "ERROR!" ),tr( "Internal wallet is not configured" ) ) ;
		}else{
			m_ui->lineEditKey->setText( secret.key ) ;
		}
	}

	this->show() ;
}

void keyDialog::HideUI()
{
	if( !m_working ){

		this->hide() ;
		this->deleteLater() ;
	}
}

keyDialog::~keyDialog()
{
	m_menu->deleteLater() ;
	delete m_ui ;
}

void keyDialog::veraCryptVolumeType::setValues( QCheckBox * vc,QCheckBox * sys,const QString& e )
{
	m_veraCrypt = vc ;
	m_veraCryptSystem = sys ;

	if( e == "veraCrypt" ){

		m_veraCrypt->setChecked( true ) ;
		m_veraCryptSystem->setChecked( false ) ;

	}else if( e == "veraCryptSystem" ){

		m_veraCrypt->setChecked( false ) ;
		m_veraCryptSystem->setChecked( true ) ;
	}else{
		m_veraCrypt->setChecked( false ) ;
		m_veraCryptSystem->setChecked( false ) ;
	}

	connect( m_veraCrypt,&QCheckBox::stateChanged,[ this ]( int s ){

		if( s == Qt::Checked ){

			m_veraCryptSystem->setChecked( false ) ;
		}
	} ) ;

	connect( m_veraCryptSystem,&QCheckBox::stateChanged,[ this ]( int s ){

		if( s == Qt::Checked ){

			m_veraCrypt->setChecked( false ) ;
		}
	} ) ;
}

keyDialog::veraCryptVolumeType::~veraCryptVolumeType()
{
	if( m_veraCrypt->isChecked() ){

		utility::autoSetVolumeAsVeraCrypt( "veraCrypt" ) ;

	}else if( m_veraCryptSystem->isChecked() ){

		utility::autoSetVolumeAsVeraCrypt( "veraCryptSystem" ) ;
	}else{
		utility::autoSetVolumeAsVeraCrypt( "" ) ;
	}
}

bool keyDialog::veraCryptVolumeType::veraCrypt()
{
	return m_veraCrypt->isChecked() ;
}

bool keyDialog::veraCryptVolumeType::veraCryptSystem()
{
	return m_veraCryptSystem->isChecked() ;
}

bool keyDialog::veraCryptVolumeType::yes()
{
	return m_veraCryptSystem->isChecked() || m_veraCrypt->isChecked() ;
}
