/********************************************************************

src/xmpp/MyXmppClient.cpp
-- wrapper between qxmpp library and XmppConnectivity

Copyright (c) 2013 Maciej Janiszewski
heavily based on the work by Anatoliy Kozlov

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

#include "MyXmppClient.h"

#include "QXmppRosterManager.h"
#include "QXmppVersionManager.h"
#include "QXmppConfiguration.h"
#include "QXmppClient.h"
#include "DatabaseManager.h"
#include "QXmppMessage.h"

#include <QDebug>

#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QSqlRecord>
#include <QThread>
#include <QStringList>

#include "RosterListModel.h"
#include "RosterItemModel.h"

QString MyXmppClient::myVersion = "0.3";
QString MyXmppClient::getBareJidByJid( const QString &jid ) { if (jid.indexOf('/') >= 0) return jid.split('/')[0]; else return jid; }

MyXmppClient::MyXmppClient() : QObject(0) {
    msgWrapper = new MessageWrapper(this);

    xmppClient = new QXmppClient( this );
    QObject::connect( xmppClient, SIGNAL(stateChanged(QXmppClient::State)), this, SLOT(clientStateChanged(QXmppClient::State)) );
    QObject::connect( xmppClient, SIGNAL(messageReceived(QXmppMessage)), this, SLOT(messageReceivedSlot(QXmppMessage)) );
    QObject::connect( xmppClient, SIGNAL(presenceReceived(QXmppPresence)), this, SLOT(presenceReceived(QXmppPresence)) );
    QObject::connect( xmppClient, SIGNAL(error(QXmppClient::Error)), this, SLOT(error(QXmppClient::Error)) );

    m_bareJidLastMessage = "";
    m_resourceLastMessage = "";
    m_stateConnect = Disconnect;
    m_status = Offline;
    m_statusText = "";
    m_flTyping = false;
    m_myjid = "";
    m_password = "";
    m_host = "";
    m_port = 5222;
    m_resource = "";
    m_keepAlive = 60;

    flVCardRequest = "";
    qmlVCard = new QMLVCard();

    this->initXmppClient();

    rosterManager = 0;
    cacheIM = new MyCache();

    cachedRoster = new RosterListModel( this );

    flSetPresenceWithoutAck = true;

    vCardManager = &xmppClient->vCardManager();
    QObject::connect( vCardManager, SIGNAL(vCardReceived(const QXmppVCardIq &)),
                      this, SLOT(initVCard(const QXmppVCardIq &)),
                      Qt::UniqueConnection  );
}

MyXmppClient::~MyXmppClient() {
    if( msgWrapper != NULL) delete msgWrapper; }

void MyXmppClient::initXmppClient() {
    xmppClient->versionManager().setClientName("Lightbulb");
    xmppClient->versionManager().setClientVersion( MyXmppClient::myVersion );
}


void MyXmppClient::clientStateChanged(QXmppClient::State state) {
    if( state == QXmppClient::ConnectingState ) m_stateConnect = Connecting;
    else if( state == QXmppClient::ConnectedState ) {
        m_stateConnect = Connected;

        if( !rosterManager ) {
            rosterManager = &xmppClient->rosterManager();

            qDebug() << Q_FUNC_INFO << " QObject::connect( rosterManager, SIGNAL(........).....)";

            QObject::connect( rosterManager, SIGNAL(presenceChanged(QString,QString)), this, SLOT(initPresence(const QString, const QString)), Qt::UniqueConnection );
            QObject::connect( rosterManager, SIGNAL(rosterReceived()), this, SLOT(initRoster()), Qt::UniqueConnection );
            QObject::connect( rosterManager, SIGNAL(subscriptionReceived(QString)), this, SIGNAL(subscriptionReceived(QString)), Qt::UniqueConnection );
            QObject::connect( rosterManager, SIGNAL(itemAdded(QString)), this, SLOT(itemAdded(QString)), Qt::UniqueConnection );
            QObject::connect( rosterManager, SIGNAL(itemRemoved(QString)), this, SLOT(itemRemoved(QString)), Qt::UniqueConnection );
            QObject::connect( rosterManager, SIGNAL(itemChanged(QString)), this, SLOT(itemChanged(QString)), Qt::UniqueConnection );
        }

        QXmppPresence pr = xmppClient->clientPresence();
        this->presenceReceived( pr );
    }
    else if( state == QXmppClient::DisconnectedState ) {
        m_stateConnect = Disconnect;
        this->setMyPresence( Offline, m_statusText );
    }
    emit connectingChanged();
}

void MyXmppClient::connectToXmppServer() {
    QXmppConfiguration xmppConfig;

    xmppConfig.setJid( m_myjid );
    xmppConfig.setPassword( m_password );
    xmppConfig.setKeepAliveInterval( m_keepAlive );
    xmppConfig.setAutoAcceptSubscriptions(false);
    xmppConfig.setSaslAuthMechanism("DIGEST-MD5");
    xmppConfig.setUseSASLAuthentication(true);
    xmppConfig.setStreamSecurityMode(QXmppConfiguration::TLSEnabled);

    /*******************/

    if( m_resource.isEmpty() || m_resource.isNull() ) xmppConfig.setResource( "Lightbulb" ); else xmppConfig.setResource( m_resource );

    if( !m_host.isEmpty() ) xmppConfig.setHost( m_host );
    if( m_port != 0 ) xmppConfig.setPort( m_port );

    xmppClient->connectToServer( xmppConfig );
}

void MyXmppClient::disconnectFromXmppServer() { xmppClient->disconnectFromServer(); }

void MyXmppClient::initRoster() {
    qDebug() << "MyXmppClient::initRoster() called";
    if( ! rosterManager->isRosterReceived() ) {
        qDebug() << "MyXmppClient::initRoster(): roster not available yet";
        return;
    }

    cachedRoster->cleanList();

    QStringList listBareJids = rosterManager->getRosterBareJids();

    for( int j=0; j < listBareJids.length(); j++ )
    {
        QString bareJid = listBareJids.at(j);

        cacheIM->addCacheJid( bareJid );

        QXmppRosterIq::Item itemRoster = rosterManager->getRosterEntry( bareJid );
        QString name = itemRoster.name();
        vCardData vCdata = cacheIM->getVCard( bareJid );

        if ( vCdata.isEmpty() ) {
            qDebug() << "MyXmppClient::initRoster():" << bareJid << "has no VCard. Requesting.";
            vCardManager->requestVCard( bareJid );
        }
        RosterItemModel *itemExists = (RosterItemModel*)cachedRoster->find(bareJid);
        if (itemExists == 0) {
          RosterItemModel *itemModel = new RosterItemModel( );
          itemModel->setPresence( this->getPicPresence( QXmppPresence::Unavailable ) );
          itemModel->setContactName( name );
          itemModel->setJid( bareJid );
          itemModel->setUnreadMsg( 0 );
          itemModel->setStatusText( "");
          itemModel->setAvatar(cacheIM->getAvatarCache(bareJid));
          cachedRoster->append(itemModel);
          itemModel = 0;
          delete itemModel;
        } else if (itemExists->name() != name) {
          itemExists->setContactName(name);
          emit contactRenamed(bareJid,name);
        }
        itemExists = 0; delete itemExists;
    }
    emit rosterChanged();
}

void MyXmppClient::initPresence(const QString& bareJid, const QString& resource)
{
    int indxItem = -1;
    RosterItemModel *item = (RosterItemModel*)cachedRoster->find( bareJid, indxItem );

    if( item == 0 ) {
        return;
    }

    QXmppPresence xmppPresence = rosterManager->getPresence( bareJid, resource );
    QXmppPresence::Type statusJid = xmppPresence.type();

    QStringList _listResources = this->getResourcesByJid( bareJid );
    if( (_listResources.count() > 0) && (!_listResources.contains(resource)) )
    {
        qDebug() << bareJid << "/" << resource << " ****************[" <<_listResources<<"]" ;
        if( statusJid == QXmppPresence::Unavailable ) {
            return;
        }
    }

    item->setResource( resource );

    QString picStatus = this->getPicPresence( xmppPresence );
    item->setPresence( picStatus );

    QString txtStatus = this->getTextStatus( xmppPresence.statusText(), xmppPresence );
    item->setStatusText( txtStatus );

    RosterItemModel *itemExists = (RosterItemModel*)cachedRoster->find( bareJid, indxItem );

    if( itemExists != 0 ) {
        itemExists->copy( item );
        QString picStatusPrev = itemExists->presence();
        if( picStatusPrev != picStatus )
        {
            //emit presenceJidChanged( bareJid, txtStatus, picStatus );
            emit rosterChanged();
        }
    }
    item = 0; itemExists = 0;
    delete item; delete itemExists;
}

QString MyXmppClient::getPicPresence( const QXmppPresence &presence ) const
{
    QString picPresenceName;
    QXmppPresence::Type status = presence.type();
    if( status != QXmppPresence::Available ) picPresenceName = "qrc:/presence/offline";
    else
    {
        QXmppPresence::AvailableStatusType availableStatus = presence.availableStatusType();
        if( availableStatus == QXmppPresence::Online ) picPresenceName = "qrc:/presence/online";
        else if ( availableStatus == QXmppPresence::Chat ) picPresenceName = "qrc:/presence/chatty";
        else if ( availableStatus == QXmppPresence::Away ) picPresenceName = "qrc:/presence/away";
        else if ( availableStatus == QXmppPresence::XA ) picPresenceName = "qrc:/presence/xa";
        else if ( availableStatus == QXmppPresence::DND ) picPresenceName = "qrc:/presence/busy";
    }

    return picPresenceName;
}

QString MyXmppClient::getTextStatus(const QString &textStatus, const QXmppPresence &presence ) const
{
  if( (!textStatus.isEmpty()) && (!textStatus.isNull()) ) return textStatus; else return "";
}



/* SLOT: it will be called when the vCardReceived signal will be received */
void MyXmppClient::initVCard(const QXmppVCardIq &vCard)
{
    QString bareJid = vCard.from();

    RosterItemModel *item = (RosterItemModel*)cachedRoster->find( bareJid );

    vCardData dataVCard;

    if( item != 0 )
    {
        /* set nickname */
        QXmppRosterIq::Item itemRoster = rosterManager->getRosterEntry( bareJid );
        QString nickName = vCard.nickName();
        if( (!nickName.isEmpty()) && (!nickName.isNull()) && (itemRoster.name().isEmpty()) ) {
            qDebug() << "MyXmppClient::initPresence: updating name for"<< bareJid;
            item->setContactName( nickName );
        }

        /* avatar */
        bool isAvatarCreated = true;
        QString avatarFile = cacheIM->getAvatarCache( bareJid );
        if( (avatarFile.isEmpty() || avatarFile == "qrc:/avatar" || (flVCardRequest != "")) && vCard.photo() != "" ) {
            isAvatarCreated =  cacheIM->setAvatarCache( bareJid, vCard.photo() );
        }
        item->setAvatar(cacheIM->getAvatarCache(bareJid));

        dataVCard.nickName = nickName;
        dataVCard.firstName = vCard.firstName();
        dataVCard.fullName = vCard.fullName();;
        dataVCard.middleName = vCard.middleName();
        dataVCard.lastName = vCard.lastName();
        dataVCard.url = vCard.url();
        dataVCard.eMail = vCard.email();

        if( flVCardRequest == bareJid ) {
            qmlVCard->setPhoto( avatarFile );
            qmlVCard->setNickName( vCard.nickName() );
            qmlVCard->setMiddleName( vCard.middleName() );
            qmlVCard->setLastName( vCard.lastName() );
            qmlVCard->setFullName( vCard.fullName() );
            qmlVCard->setName( vCard.firstName() );
            qmlVCard->setBirthday( vCard.birthday().toString("dd.MM.yyyy") );
            qmlVCard->setEMail( vCard.email() );
            qmlVCard->setUrl( vCard.url() );
            qmlVCard->setJid( bareJid );
            flVCardRequest = "";
            emit vCardChanged();
        }

        cacheIM->setVCard( bareJid, dataVCard );
    }

}


void MyXmppClient::setStatusText( const QString &__statusText )
{
    if( __statusText != m_statusText )
    {
        m_statusText=__statusText;

        QXmppPresence myPresence = xmppClient->clientPresence();
        myPresence.setStatusText( __statusText );
        xmppClient->setClientPresence( myPresence );

        emit statusTextChanged();
    }
}


void MyXmppClient::setStatus( StatusXmpp __status)
{
    if( __status != m_status )
    {
        QXmppPresence myPresence = xmppClient->clientPresence();

        if( __status == Online ) {
            myPresence.setType( QXmppPresence::Available );
            myPresence.setAvailableStatusType( QXmppPresence::Online );
        } else if( __status ==  Chat ) {
            myPresence.setType( QXmppPresence::Available );
            myPresence.setAvailableStatusType( QXmppPresence::Chat );
        } else if ( __status == Away ) {
            myPresence.setType( QXmppPresence::Available );
            myPresence.setAvailableStatusType( QXmppPresence::Away );
        } else if ( __status == XA ) {
            myPresence.setType( QXmppPresence::Available );
            myPresence.setAvailableStatusType( QXmppPresence::XA );
        } else if( __status == DND ) {
            myPresence.setType( QXmppPresence::Available );
            myPresence.setAvailableStatusType( QXmppPresence::DND );
        } else if( __status == Offline ) {
            myPresence.setType( QXmppPresence::Unavailable );
            m_status = __status;
        }

        xmppClient->setClientPresence( myPresence );
        this->presenceReceived( myPresence );
    }
}



void MyXmppClient::setMyPresence( StatusXmpp status, QString textStatus ) //Q_INVOKABLE
{
    qDebug() << Q_FUNC_INFO;
    if( textStatus != m_statusText ) {
        m_statusText =textStatus;
        emit statusTextChanged();
    }

    QXmppPresence myPresence;

    if( status == Online ) {
        if( xmppClient->state()  == QXmppClient::DisconnectedState ) this->connectToXmppServer();
        myPresence.setType( QXmppPresence::Available );
        myPresence.setStatusText( textStatus );
        myPresence.setAvailableStatusType( QXmppPresence::Online );
    } else if( status == Chat ) {
        if( xmppClient->state()  == QXmppClient::DisconnectedState ) this->connectToXmppServer();
        myPresence.setType( QXmppPresence::Available );
        myPresence.setAvailableStatusType( QXmppPresence::Chat );
        myPresence.setStatusText( textStatus );
    } else if( status == Away ) {
        if( xmppClient->state()  == QXmppClient::DisconnectedState ) this->connectToXmppServer();
        myPresence.setType( QXmppPresence::Available );
        myPresence.setAvailableStatusType( QXmppPresence::Away );
        myPresence.setStatusText( textStatus );
    } else if( status == XA ) {
        if( xmppClient->state()  == QXmppClient::DisconnectedState ) this->connectToXmppServer();
        myPresence.setType( QXmppPresence::Available );
        myPresence.setAvailableStatusType( QXmppPresence::XA );
        myPresence.setStatusText( textStatus );
    } else if( status == DND ) {
        if( xmppClient->state()  == QXmppClient::DisconnectedState ) this->connectToXmppServer();
        myPresence.setType( QXmppPresence::Available );
        myPresence.setAvailableStatusType( QXmppPresence::DND );
        myPresence.setStatusText( textStatus );
    } else if( status == Offline ) {
        if( (xmppClient->state()  == QXmppClient::ConnectedState)  || (xmppClient->state()  == QXmppClient::ConnectingState) ) xmppClient->disconnectFromServer();
        myPresence.setType( QXmppPresence::Unavailable );
    }

    xmppClient->setClientPresence( myPresence  );
    this->presenceReceived( myPresence );
}

/* it sends information about typing : typing is started */
void MyXmppClient::typingStart(QString bareJid, QString resource) {
    qDebug() << bareJid << " " << "start typing...";
    QXmppMessage xmppMsg;

    QString jid_to = bareJid;
    if( resource == "" ) jid_to += "/resource"; else jid_to += "/" + resource;
    xmppMsg.setTo( jid_to );

    QString jid_from = m_myjid + "/" + xmppClient->configuration().resource();
    xmppMsg.setFrom( jid_from );

    xmppMsg.setReceiptRequested( false );

    QDateTime currTime = QDateTime::currentDateTime();
    xmppMsg.setStamp( currTime );

    xmppMsg.setState( QXmppMessage::Composing );

    xmppClient->sendPacket( xmppMsg );
}


/* it sends information about typing : typing is stoped */
void MyXmppClient::typingStop(QString bareJid, QString resource) {
    qDebug() << bareJid << " " << "stop typing...";
    QXmppMessage xmppMsg;

    QString jid_to = bareJid;
    if( resource == "" ) jid_to += "/resource"; else jid_to += "/" + resource;
    xmppMsg.setTo( jid_to );

    QString jid_from = m_myjid + "/" + xmppClient->configuration().resource();
    xmppMsg.setFrom( jid_from );

    xmppMsg.setReceiptRequested( false );

    QDateTime currTime = QDateTime::currentDateTime();
    xmppMsg.setStamp( currTime );

    xmppMsg.setState( QXmppMessage::Paused );

    xmppClient->sendPacket( xmppMsg );
}

void MyXmppClient::itemAdded(const QString &bareJid ) {
    qDebug() << "MyXmppClient::itemAdded(): " << bareJid;
    QStringList resourcesList = rosterManager->getResources( bareJid );

    RosterItemModel *itemExists = (RosterItemModel*)cachedRoster->find(bareJid);

    if (itemExists == 0) {
      RosterItemModel *itemModel = new RosterItemModel( );
      itemModel->setPresence( this->getPicPresence(QXmppPresence::Unavailable) );
      itemModel->setContactName("");
      itemModel->setJid( bareJid );
      itemModel->setUnreadMsg( 0 );
      itemModel->setAvatar(cacheIM->getAvatarCache(bareJid));
      cachedRoster->append( itemModel );
      itemModel = 0; delete itemModel;
    };
    itemExists = 0; delete itemExists;

    for( int L = 0; L<resourcesList.length(); L++ ) {
        QString resource = resourcesList.at(L);
        this->initPresence( bareJid, resource );
    }
}



void MyXmppClient::itemChanged(const QString &bareJid ) {
    qDebug() << "MyXmppClient::itemChanged(): " << bareJid;

    QXmppRosterIq::Item rosterEntry = rosterManager->getRosterEntry( bareJid );
    if (rosterEntry.name() != "") {
        RosterItemModel *item = (RosterItemModel*)cachedRoster->find( bareJid );
        if( item != 0 ) item->setContactName( rosterEntry.name() );
        emit contactRenamed(bareJid,rosterEntry.name());
        item = 0; delete item;
    }
}


void MyXmppClient::itemRemoved(const QString &bareJid ) {
    qDebug() << "MyXmppClient::itemRemoved(): " << bareJid;

    int indxItem = -1;
    RosterItemModel *itemExists = (RosterItemModel*)cachedRoster->find( bareJid, indxItem );
    if( itemExists ) if( indxItem >= 0 ) cachedRoster->takeRow( indxItem );
}

void MyXmppClient::requestVCard(QString bareJid) //Q_INVOKABLE
{
    qDebug() << "MyXmppClient::requestVCard(" + bareJid + ") called";
    if (vCardManager && (flVCardRequest == "") ) {
        vCardManager->requestVCard( bareJid );
        flVCardRequest = bareJid;
    }
}


void MyXmppClient::messageReceivedSlot( const QXmppMessage &xmppMsg )
{
    QString bareJid_from = MyXmppClient::getBareJidByJid( xmppMsg.from() );
    QString bareJid_to = MyXmppClient::getBareJidByJid( xmppMsg.to() );

    if( xmppMsg.state() == QXmppMessage::Active ) qDebug() << "Msg state is QXmppMessage::Active";
    else if( xmppMsg.state() == QXmppMessage::Inactive ) qDebug() << "Msg state is QXmppMessage::Inactive";
    else if( xmppMsg.state() == QXmppMessage::Gone ) qDebug() << "Msg state is QXmppMessage::Gone";
    else if( xmppMsg.state() == QXmppMessage::Composing ) {
        if (bareJid_from != "") {
            m_flTyping = true;
            emit typingChanged( bareJid_from, true);
            qDebug() << bareJid_from << " is composing.";
        }
    }
    else if( xmppMsg.state() == QXmppMessage::Paused ) {
        if (bareJid_from != "") {
            m_flTyping = false;
            emit typingChanged( bareJid_from, false);
            qDebug() << bareJid_from << " paused.";
        }
    } else {
        if( xmppMsg.isAttentionRequested() )
        {
            //qDebug() << "ZZZ: attentionRequest !!! from:" <<xmppMsg.from();
            //msgWrapper->attention( bareJid_from, false );
        }
        qDebug() << "MessageWrapper::messageReceived(): xmppMsg.state():" << xmppMsg.state();
    }
    if ( !( xmppMsg.body().isEmpty() || xmppMsg.body().isNull() || bareJid_from == m_myjid ) ) {
        QString jid = xmppMsg.from();
        if( jid.indexOf('/') >= 0 ) {
            QStringList sl =  jid.split('/');
            m_bareJidLastMessage = sl[0];
            if( sl.count() > 1 ) m_resourceLastMessage = sl[1];
        } else m_bareJidLastMessage = xmppMsg.from();

        this->openChat( bareJid_from );

        RosterItemModel *item = (RosterItemModel*)cachedRoster->find( bareJid_from );
        if( item != 0 ) { int cnt = item->unreadMsg(); item->setUnreadMsg( ++cnt ); } else {
          RosterItemModel *itemModel = new RosterItemModel( );
          itemModel->setPresence( this->getPicPresence( QXmppPresence::Unavailable ) );
          itemModel->setContactName( bareJid_from );
          itemModel->setJid( bareJid_from );
          itemModel->setUnreadMsg( 1 );
          itemModel->setStatusText( "");
          cachedRoster->append(itemModel);
          itemModel = 0;
          delete itemModel;
        }
        item = 0; delete item;

        QString body = xmppMsg.body();
        body = body.replace(">", "&gt;");  //fix for > stuff
        body = body.replace("<", "&lt;");  //and < stuff too ^^
        body = msgWrapper->parseMsgOnLink(body);

        emit insertMessage(m_accountId,this->getBareJidByJid(xmppMsg.from()),body,QDateTime::currentDateTime().toString("dd-MM-yy hh:mm"),0);
    }
}

bool MyXmppClient::sendMyMessage(QString bareJid, QString resource, QString msgBody) //Q_INVOKABLE
{
    if( msgBody == "" ) { return false; }

    QXmppMessage xmppMsg;

    QString jid_from = bareJid;
    if( resource == "" ) jid_from += "/resource"; else jid_from += "/" + resource;

    xmppMsg.setTo( jid_from );
    QString jid_to = m_myjid + "/" + xmppClient->configuration().resource();
    xmppMsg.setFrom( jid_to );

    xmppMsg.setBody( msgBody );

    xmppMsg.setState( QXmppMessage::Active );

    xmppClient->sendPacket( xmppMsg );

    this->messageReceivedSlot( xmppMsg );

    QString body = msgBody.replace(">", "&gt;"); //fix for > stuff
    body = body.replace("<", "&lt;");  //and < stuff too ^^
    body = msgWrapper->parseMsgOnLink(body);

    emit insertMessage(m_accountId,this->getBareJidByJid(xmppMsg.to()),body,QDateTime::currentDateTime().toString("dd-MM-yy hh:mm"),1);

    return true;
}

void MyXmppClient::presenceReceived( const QXmppPresence & presence )
{
    QString jid = presence.from();
    QString bareJid = jid;
    QString resource = "";
    if( jid.indexOf('/') >= 0 ) {
        bareJid = jid.split('/')[0];
        resource = jid.split('/')[1];
    }
    QString myResource = xmppClient->configuration().resource();

    if( (((presence.from()).indexOf( m_myjid ) >= 0) && (resource == myResource)) || ((bareJid == "") && (resource == "")) ) {
        QXmppPresence::Type __type = presence.type();
        if( __type == QXmppPresence::Unavailable ) m_status = Offline;
        else {
            QXmppPresence::AvailableStatusType __status = presence.availableStatusType();
            if( __status == QXmppPresence::Online ) m_status = Online;
            else if( __status ==  QXmppPresence::Chat ) m_status = Chat;
            else if ( __status == QXmppPresence::Away ) m_status = Away;
            else if ( __status == QXmppPresence::XA ) m_status = XA;
            else if( __status == QXmppPresence::DND ) m_status = DND;
        }
        emit statusChanged();
    }
}


void MyXmppClient::error(QXmppClient::Error e) {
    QString errString;
    if( e == QXmppClient::SocketError ) errString = "SOCKET_ERROR";
    else if( e == QXmppClient::KeepAliveError ) errString = "KEEP_ALIVE_ERROR";
    else if( e == QXmppClient::XmppStreamError ) errString = "XMPP_STREAM_ERROR";

    if( !errString.isNull() ) {
        QXmppPresence pr = xmppClient->clientPresence();
        this->presenceReceived( pr );
        QXmppPresence presence( QXmppPresence::Unavailable );
        xmppClient->setClientPresence( presence );

        emit errorHappened( errString );
    }
}

/*--- add/remove contact ---*/
void MyXmppClient::addContact( QString bareJid, QString nick, QString group, bool sendSubscribe ) {
    if( rosterManager ) {
        QSet<QString> gr;
        QString n;
        if( !(group.isEmpty() || group.isNull()) )  { gr.insert( group ); }
        if( !(nick.isEmpty() || nick.isNull()) )  { n = nick; }
        rosterManager->addItem(bareJid, n, gr );

        if( sendSubscribe ) rosterManager->subscribe( bareJid );
    }
}

void MyXmppClient::removeContact( QString bareJid ) { if( rosterManager ) rosterManager->removeItem( bareJid ); }

void MyXmppClient::renameContact(QString bareJid, QString name) { if( rosterManager ) rosterManager->renameItem( bareJid, name ); }

bool MyXmppClient::subscribe(const QString bareJid) //Q_INVOKABLE
{
    qDebug() << "MyXmppClient::subscribe(" << bareJid << ")" ;
    bool res = false;
    if( rosterManager && (!bareJid.isEmpty()) && (!bareJid.isNull()) ) {
        res = rosterManager->subscribe( bareJid );
    }
    return res;
}

bool MyXmppClient::unsubscribe(const QString bareJid) //Q_INVOKABLE
{
    qDebug() << "MyXmppClient::unsubscribe(" << bareJid << ")" ;
    bool res = false;
    if( rosterManager && (!bareJid.isEmpty()) && (!bareJid.isNull()) ) {
        res = rosterManager->unsubscribe( bareJid );
    }
    return res;
}

bool MyXmppClient::acceptSubscribtion(const QString bareJid) //Q_INVOKABLE
{
    //qDebug() << "MyXmppClient::acceptSubscribtion(" << bareJid << ")" ;
    bool res = false;
    if( rosterManager && (!bareJid.isEmpty()) && (!bareJid.isNull()) ) {
        res = rosterManager->acceptSubscription( bareJid );
    }
    return res;
}

bool MyXmppClient::rejectSubscribtion(const QString bareJid) //Q_INVOKABLE
{
    bool res = false;
    if( rosterManager && (!bareJid.isEmpty()) && (!bareJid.isNull()) ) res = rosterManager->refuseSubscription( bareJid );
    return res;
}

void MyXmppClient::attentionSend( QString bareJid, QString resource )
{
    qDebug() << Q_FUNC_INFO;
    QXmppMessage xmppMsg;

    QString jid_to = bareJid;
    if( resource == "" ) {
        jid_to += "/resource";
    } else {
        jid_to += "/" + resource;
    }
    xmppMsg.setTo( jid_to );

    QString jid_from = m_myjid + "/" + xmppClient->configuration().resource();
    xmppMsg.setFrom( jid_from );

    xmppMsg.setReceiptRequested( false );

    xmppMsg.setState( QXmppMessage::None );
    xmppMsg.setType( QXmppMessage::Headline );
    xmppMsg.setAttentionRequested( true );

    xmppClient->sendPacket( xmppMsg );
}
