/********************************************************************

src/database/Settings.cpp
-- holds settings of the app and accounts details

Copyright (c) 2013 Maciej Janiszewski

This file is part of Lightbulb.

Lightbulb is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*********************************************************************/

#include "Settings.h"

#include <QDir>
#include "AccountsListModel.h"
#include <QDebug>

QString Settings::appName = "Lightbulb";
QString Settings::confFolder = QDir::homePath() + QDir::separator() + ".config" + QDir::separator() + appName;
QString Settings::cacheFolder = confFolder + QDir::separator() + QString("cache");
QString Settings::confFile = confFolder + QDir::separator() + Settings::appName + ".conf";

Settings::Settings(QObject *parent) : QSettings(Settings::confFile, QSettings::NativeFormat , parent)
{
    alm = new AccountsListModel(this);
    this->initListOfAccounts();
}

/*************************** (generic settings) **************************/
bool Settings::gBool(QString group, QString key) {
    beginGroup( group );
    QVariant ret = value( key, false );
    endGroup();
    return ret.toBool();
}
void Settings::sBool(const bool isSet, QString group, QString key) {
    beginGroup( group );
    setValue( key, QVariant(isSet) );
    endGroup();
}
int Settings::gInt(QString group, QString key) {
    beginGroup( group );
    QVariant ret = value( key, false );
    endGroup();
    return ret.toInt();
}
void Settings::sInt(const int isSet, QString group, QString key) {
    beginGroup( group );
    setValue( key, QVariant(isSet) );
    endGroup();
}
QString Settings::gStr(QString group, QString key) {
    beginGroup( group );
    QVariant ret = value( key, false );
    endGroup();
    return ret.toString();
}
void Settings::sStr(const QString isSet, QString group, QString key) {
    beginGroup( group );
    setValue( key, QVariant(isSet) );
    endGroup();
}

/******** ACCOUNT RELATED SHIT *******/
QStringList Settings::getListAccounts()
{
    beginGroup( "accounts" );
    QStringList acc = value( "accounts", QStringList() ).toStringList();
    endGroup();
    return acc;
}
/*-------------------*/
void Settings::addAccount( const QString &acc )
{
    beginGroup( "accounts" );
    QVariant retList = value( "accounts", QStringList() );
    QStringList sl = retList.toStringList();
    if( sl.indexOf(acc) < 0 ) {
        sl.append(acc);
        setValue( "accounts", QVariant(sl) );
        //emit accountAdded(sl.indexOf(acc));
    }
    endGroup();
}
void Settings::removeAccount( const QString &acc )
{
    beginGroup( "accounts" );
    QVariant retList = value( "accounts", QStringList() );
    QStringList sl = retList.toStringList();
    int index = sl.indexOf(acc);
    if( index >= 0 ) {
        sl.removeOne(acc);
        setValue( "accounts", QVariant(sl) );
    }
    endGroup();
    emit accountsListChanged();
}

void Settings::initListOfAccounts() {
    QStringList listAcc = this->getListAccounts();

    qDebug() << "initializing list";

    alm->takeRows( 0, alm->count() );

    qDebug() << "rows removed";
    qDebug() << listAcc;
    qDebug() << alm->count();

    QStringList::const_iterator itr = listAcc.begin();
    int i = 0;
    while ( itr != listAcc.end() ) {
        QString jid = *itr;
        itr++;

        qDebug() << "initializing" << jid;

        QString passwd = gStr(jid,"passwd");

        QString host = gStr(jid,"host");
        int port = gInt(jid,"port");
        QString resource = gStr(jid,"resource");
        bool isManuallyHostPort = gBool(jid,"use_host_port");

        AccountsItemModel *aim = new AccountsItemModel( jid, passwd, resource, host, port, isManuallyHostPort, this );
        qDebug() << "account item model done";
        alm->append(aim);
        qDebug() << "account item model appended";
        i++;
    }

    qDebug() << "...and it's done";

    emit accountsListChanged();
}


void Settings::setAccount(
        QString _jid,
        QString _pass,
        bool _isDflt,
        QString _resource,
        QString _host,
        QString _port,
        bool manuallyHostPort) //Q_INVOKABLE
{
    bool isNew = false;
    beginGroup( "accounts" );

    QVariant retList = value( "accounts", QStringList() );
    QStringList sl = retList.toStringList();
    if( sl.indexOf(_jid) < 0 ) {
        sl.append(_jid);
        setValue( "accounts", QVariant(sl) );
        isNew = true;
    }
    endGroup();

    sStr(_pass,_jid,"passwd");

    sStr(_resource,_jid,"resource");
    sStr(_host,_jid,"host");
    sBool(manuallyHostPort,_jid,"use_host_port");

    bool ok = false;
    int p = _port.toInt(&ok);
    if( ok ) { sInt( p, _jid, "port" ); }

    initListOfAccounts();

    if (isNew) emit accountAdded(sl.indexOf(_jid));
      else emit accountEdited(sl.indexOf(_jid));
}

AccountsItemModel* Settings::getAccount(int index) {
    return (AccountsItemModel*)alm->getElementByID(index);
}

QString Settings::getJidByIndex(int index) {
    return getListAccounts().at(index);
}
