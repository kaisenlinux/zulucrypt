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

#ifndef KEYDIALOG_H
#define KEYDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>

#include <QMenu>

class QCloseEvent ;
class QAction ;
class QTableWidgetItem ;
class QTableWidget ;

#include "zulumount.h"
#include "volumeproperty.h"
#include "../zuluCrypt-gui/utility.h"
#include "../zuluCrypt-gui/secrets.h"
#include <functional>
#include <memory>

#include <QCheckBox>

namespace Ui {
class keyDialog;
}

class keyDialog : public QDialog
{
	Q_OBJECT
public:
	static keyDialog& instance( QWidget * parent,
				    QTableWidget * table,
				    secrets& s,
				    const volumeProperty& v,
				    std::function< void() > cancel,
				    std::function< void( const QString& ) > success )
	{
		return *( new keyDialog( parent,table,s,v,std::move( cancel ),std::move( success ) ) ) ;
	}
	keyDialog( QWidget * parent,
		   QTableWidget *,
		   secrets&,
		   const volumeProperty&,
		   std::function< void() >,
		   std::function< void( const QString& ) > ) ;
	~keyDialog() ;
	void ShowUI() ;
	void HideUI() ;
signals:
	void mounted( QString ) ;
	void cryptoOpen( QString ) ;
private:
	void cbVisibleKeyStateChanged( int ) ;
	void cbActicated( int ) ;
	void pbkeyOption() ;
	void pbMountPointPath() ;
	void pbPluginEntryClicked( QAction * ) ;
	void plugIn() ;
	void key() ;
	void keyFile() ;
	void openVolume() ;
	void pbCancel() ;
	void Plugin() ;
	void KeyFile() ;
	void cbMountReadOnlyStateChanged( int ) ;
	void encryptedFolderMount() ;
	class veraCryptVolumeType{
	public:
		void setValues( QCheckBox * vc,QCheckBox * sys,const QString& opt ) ;
		~veraCryptVolumeType() ;
		bool veraCrypt() ;
		bool veraCryptSystem() ;
		bool yes() ;
	private:
		QCheckBox * m_veraCrypt ;
		QCheckBox * m_veraCryptSystem ;
	} m_veraCryptVolumeType ;

	bool errorNotFound( int ) ;
	void keyAndKeyFile() ;
	void tcryptGui() ;
	void enableAll() ;
	void disableAll() ;
	void closeEvent( QCloseEvent * ) ;
	bool eventFilter( QObject * watched,QEvent * event ) ;
	Ui::keyDialog * m_ui ;
	secrets& m_secrets ;
	QString m_path ;
	QString m_point ;
	QByteArray m_key ;
	QStringList m_keyFiles ;
	QString m_deviceOffSet ;
	QString m_options ;
	QMenu * m_menu ;
	QTableWidget * m_table ;
	utility::veraCryptWarning m_veraCryptWarning ;	
	bool m_working ;
	bool m_encryptedFolder = false ;
	enum{ Key = 0,keyfile = 1,keyKeyFile = 2,plugin = 3,tcryptKeys = 4,yubikey = 5 } ;
	std::function< void() > m_cancel ;
	std::function< void( const QString& ) > m_success ;
	utility::label m_label ;
};

#endif // KEYDIALOG_H
