/********************************************************************

src/RosterItemModel.h
-- implements item model for roster

Copyright (c) 2012 Anatoliy Kozlov

This file is part of Lightbulb and was derived from MeegIM.

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

#ifndef ROSTERITEMMODEL_H
#define ROSTERITEMMODEL_H

#include "listmodel.h"

#define ROSTER_ITEM_CONTACT 0
#define ROSTER_ITEM_MUC 1

class RosterItemModel : public ListItem
{
    Q_OBJECT

public:
    enum Roles {
        roleName = Qt::UserRole+1,
        roleJid,
        roleResource,
        rolePresence,
        roleStatusText,
        roleUnreadMsg,
        roleAvatar
      };

public:
      RosterItemModel(QObject *parent = 0): ListItem(parent) {
          contactName = "";
          contactJid = "";
          contactResource = "";
          contactPresence = "";
          contactStatusText = "";
          contactUnreadMsg = 0;
          contactAvatar = "qrc:/avatar";
      }
      explicit RosterItemModel( const QString &_contactName,
                                       const QString &_contactJid,
                                       const QString &_contactResource,
                                       const QString &_contactPresence,
                                       const QString &_contactStatusText,
                                       const int _contactUnreadMsg,
                                       QObject *parent = 0 ) : ListItem(parent),
          contactName(_contactName),
          contactJid(_contactJid),
          contactResource(_contactResource),
          contactStatusText(_contactStatusText),
          contactPresence(_contactPresence),
          contactUnreadMsg(_contactUnreadMsg)
      {
      }

      virtual QVariant data(int role) const {
        switch(role) {
        case roleName:
          return name();
        case roleJid:
          return jid();
        case roleResource:
          return resource();
        case roleStatusText:
          return statusText();
        case rolePresence:
          return presence();
        case roleUnreadMsg:
          return unreadMsg();
        case roleAvatar:
          return avatar();
        default:
          return QVariant();
        }
      }
      virtual QHash<int, QByteArray> roleNames() const {
          QHash<int, QByteArray> names;
          names[roleName] = "name";
          names[roleJid] = "jid";
          names[roleResource] = "resource";
          names[rolePresence] = "presence";
          names[roleStatusText] = "statusText";
          names[roleUnreadMsg] = "unreadMsg";
          names[roleAvatar] = "avatar";
          return names;
        }


      virtual QString id() const { return contactJid; }

      void setPresence( const QString &_contactPresence ) {
          if(contactPresence != _contactPresence) {
            contactPresence = _contactPresence;
            emit dataChanged();
          }
      }

      void setContactName( const QString &_contactName ) {
          if(contactName != _contactName) {
            contactName = _contactName;
            emit dataChanged();
          }
      }

      void setJid( const QString &_contactJid ) {
          if(contactJid != _contactJid) {
            contactJid = _contactJid;
            emit dataChanged();
          }
      }

      void setResource( const QString &_contactResource ) {
          if(contactResource != _contactResource) {
            contactResource = _contactResource;
            emit dataChanged();
          }
      }

      void setStatusText( const QString &_contactStatusText )  {
          if(contactStatusText != _contactStatusText) {
            contactStatusText = _contactStatusText;
            emit dataChanged();
          }
      }

      void setUnreadMsg( const int _contactUnreadMsg )  {
          if(contactUnreadMsg != _contactUnreadMsg) {
            contactUnreadMsg = _contactUnreadMsg;
            emit dataChanged();
          }
      }

      void setAvatar( const QString &_contactAvatar )  {
          if(contactAvatar != _contactAvatar) {
            contactAvatar = _contactAvatar;
            emit dataChanged();
          }
      }

      inline QString presence() const { return contactPresence; }
      inline QString name() const { return contactName; }
      inline QString jid() const { return contactJid; }
      inline QString resource() const { return contactResource; }
      inline QString statusText() const { return contactStatusText; }
      inline int unreadMsg() const { return contactUnreadMsg; }
      inline QString avatar() const { return contactAvatar; }

      void copy( const RosterItemModel* item ) {
          contactName = item->name();
          contactPresence = item->presence();
          contactName = item->name();
          contactJid = item->jid();
          contactResource = item->resource();
          contactStatusText = item->statusText();
          contactUnreadMsg = item->unreadMsg();
          contactAvatar = item->avatar();
      }

    private:
      QString contactName;
      QString contactJid;
      QString contactResource;
      QString contactPresence;
      QString contactStatusText;
      QString contactAvatar;
      int contactUnreadMsg;
};

#endif // ROSTERITEMMODEL_H

