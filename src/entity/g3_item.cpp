/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

#include <QObject>
#include <QCryptographicHash>
#include <QVariant>
#include <qjson/parser.h>
#include <qjson/serializer.h>
#include <qjson/qobjecthelper.h>
#include <kurl.h>
#include <kio/netaccess.h>
#include <klocalizedstring.h>
#include <klocale.h>
#include <kdatetime.h>
#include "utility/debug.h"
#include "utility/exception.h"
#include "kio_gallery3_protocol.h"
#include "gallery3/g3_backend.h"
#include "gallery3/g3_request.h"
#include "entity/g3_item.h"

using namespace KIO;
using namespace KIO::Gallery3;

G3Item* const G3Item::instantiate ( G3Backend* const backend, const QVariantMap& attributes )
{
  MY_KDEBUG_BLOCK ( "<G3Item::instantiate>" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(",");
  // find out the items type first
  QVariantMap    entity;
  Entity::G3Type type;
  if (    attributes.contains(QLatin1String("entity"))
       && attributes[QLatin1String("entity")].canConvert(QVariant::Map) )
    entity = attributes[QLatin1String("entity")].toMap();
  else
    throw Exception ( Error(ERR_INTERNAL),
                      QString("invalid response form gallery: no item entites specified") );
  if (    entity.contains(QLatin1String("type"))
       && entity[QLatin1String("type")].canConvert(QVariant::String) )
  {
    type = Entity::G3Type ( entity[QLatin1String("type")].toString() );
    kDebug() << QString("item type is '%1' [%2]").arg(type.toString()).arg(type.toInt());
  }
  else
    throw Exception ( Error(ERR_INTERNAL),
                      QString("invalid response form gallery: no item type specified") );
  // create an item object
  switch ( type.toInt() )
  {
    case Entity::G3Type::ALBUM:
      return new AlbumEntity   ( backend, attributes );
    case Entity::G3Type::MOVIE:
      return new MovieEntity   ( backend, attributes );
    case Entity::G3Type::PHOTO:
      return new PhotoEntity   ( backend, attributes );
    case Entity::G3Type::TAG:
      return new TagEntity     ( backend, attributes );
    case Entity::G3Type::COMMENT:
      return new CommentEntity ( backend, attributes );
    default:
      throw Exception ( Error(ERR_INTERNAL),
                        QString("failed to instantiate entity because of an unknown item type '%1'").arg(type.toString()) );
  } // // switch
} // G3Item::instantiate

/**
 * This constructs an item object that describes exactly one single node inside the gallery.
 */
G3Item::G3Item ( const Entity::G3Type type, G3Backend* const backend, const QVariantMap& attributes )
  : G3Entity     ( type, backend )
  , m_attributes ( attributes )
{
  MY_KDEBUG_BLOCK ( "<G3Item::G3Item>" );
  kDebug() << "(<type> <backend> <attributes>)" << type.toString() << backend->toPrintout() << QStringList(attributes.keys()).join(",");
  // store most important entity tokens directly as strings
  // a few values stored type-strict for later convenience
  m_id       = attributeMapToken ( QLatin1String("entity"), QLatin1String("id"),   QVariant::UInt,   TRUE  ).toUInt();
  m_name     = attributeMapToken ( QLatin1String("entity"), QLatin1String("name"), QVariant::String, FALSE ).toString();
  m_restUrl  = KUrl ( attributeToken    (                          QLatin1String("url"),       QVariant::String, FALSE ).toString() );
  m_webUrl   = KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("web_url"),   QVariant::String, FALSE ).toString() );
  m_fileUrl  = KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("file_url"),  QVariant::String, FALSE ).toString() );
  m_thumbUrl = KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("thumb_url"), QVariant::String, FALSE ).toString() );
  // set the items mimetype
  QString mimetype_name = attributeMapToken ( QLatin1String("entity"), QLatin1String("mime_type"), QVariant::String, FALSE ).toString();
  if ( ! mimetype_name.isEmpty() )
    m_mimetype = KMimeType::mimeType(mimetype_name);
  else
    switch ( type.toInt() )
    {
      case Entity::G3Type::ALBUM:
        m_mimetype = KMimeType::mimeType ( QLatin1String("inode/directory") );
        break;
      default:
        m_mimetype = KMimeType::defaultMimeTypePtr ( );
    } // switch type
  // set parent and pushd into parent
  QString parent_url = attributeMapToken ( QLatin1String("entity"), QLatin1String("parent"), QVariant::String, FALSE ).toString();
  if ( parent_url.isEmpty() )
  {
    m_parent = NULL;
    m_backend->pushItem ( this );
  }
  else
  {
    KUrl url = KUrl ( parent_url );
    g3index id = QVariant(url.fileName()).toInt();
    m_parent = m_backend->item ( id );
    kDebug() << "caching item" << this->toPrintout() << "in parent item" << m_parent->toPrintout();
    m_parent->pushMember ( this );
    m_backend->pushItem ( this );
  } // else
} // G3Item::G3Item

/**
 * Delete all members as registered inside this object
 * Deregister this item as child in its parent
 */
G3Item::~G3Item()
{
  MY_KDEBUG_BLOCK ( "<G3Item::~G3Item>" );
  kDebug() << "(<>)";
  // remove this node from parents list of members
  if ( NULL!=m_parent )
  {
    kDebug() << "removing item" << toPrintout() << "from parents member list";
    m_parent->popMember ( this );
  }
  // delete all members registered inside this item
  QHash<g3index,G3Item*>::iterator member;
  while ( ! m_members.isEmpty() )
  {
    member = m_members.begin ( );
    kDebug() << "deleting member" << member.value()->toPrintout();
    delete member.value();
  }
  // remove this item from the backends catalog
  m_backend->popItem ( m_id );
} // G3Item::~G3Item

//==========

const QVariant G3Item::attributeToken ( const QString& attribute, QVariant::Type type, bool strict ) const
{
  kDebug() << "(<attribute> <type> <strict>)" << attribute << type << strict;
  if (   m_attributes.contains (attribute)
      && m_attributes[attribute].canConvert(type) )
    // value exists and is convertable
    return m_attributes[attribute];
  else if ( ! strict )
    // value does not exist but an empty instance will be accepted anyway
    return QVariant(type);
  else
    // value does not exist but is required
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("mandatory item attribute '%1' does not exist or does not have requested type").arg(attribute) );
} // G3Item::attributeToken

const QVariant G3Item::attributeMap ( const QString& attribute, bool strict ) const
{
  return attributeToken ( attribute, QVariant::Map, strict );
} // G3Item::attributeMap

const QVariant G3Item::attributeList ( const QString& attribute, bool strict ) const
{
  return attributeToken ( attribute, QVariant::List, strict );
} // G3Item::attributeMap

const QVariant G3Item::attributeString ( const QString& attribute, bool strict ) const
{
  return attributeToken ( attribute, QVariant::String, strict );
} // G3Item::attributeMap

const QVariant G3Item::attributeMapToken ( const QString& attribute, const QString& token, QVariant::Type type, bool strict ) const
{
  kDebug() << "(<attribute> <token> <type> <strict>)" << attribute << token << type << strict;
  // attributes MUST contain an entry "entity"
  QVariantMap map = attributeToken(attribute,QVariant::Map,TRUE).toMap();
  // inside that token "entity" look for the requested token
  if (   map.contains (token)
      && map[token].canConvert(type) )
    // value exists and is convertable
    return map[token];
  else if ( ! strict )
    // value does not exist but an empty instance will be accepted anyway
    return QVariant(type);
  else
    // value does not exist but is required
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("mandatory attribute token '%1|%2' does not exist or does not have requested type").arg(attribute).arg(token) );
} // G3Item::attributeMapToken

//==========

void G3Item::pushMember ( G3Item* item )
{
  kDebug() << "(<this> <item>)" << toPrintout() << item->toPrintout();
  if ( m_members.contains(item->id()) )
    throw Exception ( Error(ERR_INTERNAL), QString("attempt to register item with id '%1' that already exists").arg(item->id()) );
  // all fine, store items
  m_members.insert ( item->id(), item );
  item->m_parent = this;
} // G3Item::pushMember

/**
 * G3Item::popMember
 * returns: G3Item*: pointer to item object that has been removed
 * param: G3Item* item: pointer to an item object to be removed
 * description: shortcut & convenience method to remove a given object from the list of members
 */
G3Item* G3Item::popMember ( G3Item* item )
{
  kDebug() << "(<this> <item>)" << toPrintout() << item->toPrintout();
  return popMember ( item->id() );
} // G3Item::popMember

/**
 * G3Item::popMember
 * return: G3Item*: pointer to the item object that has been removed
 * param: g3index id: id of item object to be removed
 * description: removes a specific item object from the list of members. Not, that the object is NOT deleted,
 *              it is merely removed from the list and a pointer to that object is returned
 */
G3Item* G3Item::popMember ( g3index id )
{
  kDebug() << "(<this> <id>)" << toPrintout() << id;
  if ( m_members.contains(id) )
  {
    G3Item* item = m_members[id];
    m_members.remove ( id );
    return item;
  }
  throw Exception ( Error(ERR_INTERNAL), QString("attempt to remove non-existing member item with id '%1'").arg(id) );
} // G3Item::popMember

void G3Item::setParent ( G3Item* parent )
{
  kDebug() << "(<parent>)" << parent->toPrintout();
  m_parent = parent;
} // G3Item::setParent

//==========

G3Item* G3Item::member ( const QString& name )
{
  MY_KDEBUG_BLOCK ( "<G3Item::member>" );
  kDebug() << "(<this> <name>)" << toPrintout() << name;
  // make sure members have been retrieved
  buildMemberItems ( );
  // look for requested member
  QHash<g3index,G3Item*>::const_iterator member;
  for ( member=m_members.constBegin(); member!=m_members.constEnd(); member++ )
    if ( name==member.value()->name() )
      return member.value();
  // no success, no matching member found
  throw Exception ( Error(ERR_DOES_NOT_EXIST), name );
} // G3Item::getMember

G3Item* G3Item::member ( g3index id )
{
  MY_KDEBUG_BLOCK ( "<G3Item::member>" );
  kDebug() << "(<this> <id>)" << toPrintout() << id;
  // make sure members have been retrieved
  buildMemberItems ( );
  // look for requested member
  if ( m_members.contains(id) )
  {
    kDebug() << QString("found member '%1' [%2]").arg(m_members[id]->toPrintout()).arg(m_members[id]->id());
    return m_members[id];
  }
  else
    throw Exception ( Error(ERR_DOES_NOT_EXIST), QString("item with id '%1'").arg(id) );
} // G3Item::member

QHash<g3index,G3Item*> G3Item::members ( )
{
  MY_KDEBUG_BLOCK ( "<G3Item::members>" );
  kDebug() << "(<this>)" << toPrintout();
  buildMemberItems ( );
  return m_members;
} // G3Item::members

bool G3Item::containsMember ( const QString& name )
{
  MY_KDEBUG_BLOCK ( "<G3Item::containsMember>" );
  kDebug() << "(<this> <name>)" << toPrintout() << name;
  buildMemberItems ( );
  try
  {
    member ( name );
    return TRUE;
  } // try
  catch ( Exception e )
  {
    if ( ERR_DOES_NOT_EXIST==e.getCode() )
    {
      kDebug() << e.getText();
      return FALSE;
    }
    else
      throw e;
  } // catch
}

bool G3Item::containsMember ( g3index id )
{
  MY_KDEBUG_BLOCK ( "<G3Item::containsMember>" );
  kDebug() << "(<this> <id>)" << toPrintout() << id;
  buildMemberItems ( );
  return m_members.contains ( id );
}

int G3Item::countMembers ( )
{
  MY_KDEBUG_BLOCK ( "<G3Item::countMembers>" );
  kDebug() << "(<this>)" << toPrintout();
  buildMemberItems ( );
  return m_members.count();
} // G3Item::countMembers

void G3Item::buildMemberItems ( )
{
  MY_KDEBUG_BLOCK ( "<G3Item::buildMemberItems>" );
  kDebug() << "(<this>)" << toPrintout();
  QVariantList list = attributeList("members",TRUE).toList();
  // members list oout of sync ?
  if ( list.count()!=m_members.count() )
  {
    // note: we do NOT construct a list of KUrls, since we need to specify the urls as strings in the request url anyway
    QHash<g3index,QString> urls;
    foreach ( QVariant entry, list )
    {
      // entry is a "url string", the 'filename' is the items id
      // e.g. http://gallery.some.server/rest/item/666
      g3index id = QVariant(KUrl(entry.toString()).fileName()).toInt();
      urls.insert ( id, entry.toString() );
    } // foreach
    // remove all 'stale members', members that are not mentioned in attribute 'members'
    foreach ( G3Item* member, m_members )
      if ( ! urls.contains(member->id()) )
      {
        kDebug() << "removing stale member" << member->toPrintout();
        m_members.remove ( member->id() );
        delete member;
      } // if
      else
        kDebug() << "keeping existing member" << member->toPrintout();
    // identify all members not present but mentioned in attribute 'members'
    foreach ( g3index id, urls.keys() )
      if ( ! m_members.contains(id) )
        // keep id in list of urls, will be used further down
        kDebug() << "keeping id" << id << " in list of missing members";
      else
      {
        kDebug() << "removing id" << id << "from list of missing members";
        urls.remove ( id );
      }
    // construct the required items
    if ( 0<urls.count() )
    {
      kDebug() << "constructing" << urls.count() << "missing member items";
      QList<G3Item*> items = G3Request::g3GetItems ( m_backend, urls.values() );
    }
  } // if
} // G3Item::getMembers

//==========

QStringList G3Item::path ( ) const
{
  MY_KDEBUG_BLOCK ( "<G3Item::path>" );
  kDebug() << "(<this>)" << toPrintout();
  if ( NULL!=m_parent )
  {
    QStringList path = m_parent->path();
    path << name();
    return path;
  }
  return QStringList();
} // G3Item::path

G3Item* G3Item::parent ( ) const
{
  kDebug() << "(<>)";
  return m_parent;
} // G3Item::parent

//==========

const G3Item& G3Item::operator<< ( G3Item& member )
{
  kDebug() << "(<this> <item>)" << toPrintout() << "<<" << member.toPrintout();
  pushMember ( &member );
  return *this;
} // operator<<
const G3Item& G3Item::operator<< ( G3Item* member )
{
  kDebug() << "(<this> <item>)" << toPrintout() << "<<" << member->toPrintout();
  pushMember ( member );
  return *this;
} // operator<<

//==========

const QString G3Item::toPrintout ( ) const
{
  return QString("G3Item ('%1' [%2])").arg(name()).arg(id());
}

/**
 */
const UDSEntry G3Item::toUDSEntry ( ) const
{
  MY_KDEBUG_BLOCK ( "<G3Item::toUDSEntry>" );
  kDebug() << "(<this>)" << toPrintout();
  UDSEntry entry;
  entry.insert( UDSEntry::UDS_FILE_TYPE,          m_type.toUDSFileType() );
  entry.insert( UDSEntry::UDS_NAME,               QString("%1").arg(m_name) );
//  entry.insert( UDSEntry::UDS_DISPLAY_NAME,       QString("[%1] %2").arg(m_id).arg(attributeMapToken("entity","title",QVariant::String).toString()) );
  entry.insert( UDSEntry::UDS_MIME_TYPE,          m_mimetype->name() );
  entry.insert( UDSEntry::UDS_DISPLAY_TYPE,       m_type.toString() );
  entry.insert( UDSEntry::UDS_SIZE,               attributeMapToken(QLatin1String("entity"),QLatin1String("file_size"),QVariant::Int).toInt() );
  entry.insert( UDSEntry::UDS_ACCESS,             0600 );
  entry.insert( UDSEntry::UDS_CREATION_TIME,      attributeMapToken(QLatin1String("entity"),QLatin1String("created"),QVariant::Int).toInt() );
  entry.insert( UDSEntry::UDS_MODIFICATION_TIME,  attributeMapToken(QLatin1String("entity"),QLatin1String("updated"),QVariant::Int).toInt() );
//  entry.insert( UDSEntry::UDS_LOCAL_PATH,         m_fileUrl.path() );
//  entry.insert( UDSEntry::UDS_TARGET_URL,         m_fileUrl.url() );
//  entry.insert( UDSEntry::UDS_LINK_DEST,          m_fileUrl.url() );
//  if ( ! m_attributes["entity"]Icon.isEmpty() )
//    entry.insert( UDSEntry::UDS_ICON_NAME,          m_attributes["entity"]Icon );
//  if ( ! m_attributes["entity"]Overlays.isEmpty() )
//    entry.insert( UDSEntry::UDS_ICON_OVERLAY_NAMES, m_attributes["entity"]Overlays.join(",") );

  // some intense debugging output...
  QList<uint> _tags = entry.listFields ( );
  kDebug() << "list of defined UDS entry tags for entry" << toPrintout() << ":";
  foreach ( uint _tag, _tags )
    switch ( _tag )
    {
      case UDSEntry::UDS_NAME:               kDebug() << "UDS_NAME:"               << entry.stringValue(_tag); break;
      case UDSEntry::UDS_DISPLAY_NAME:       kDebug() << "UDS_DISPLAY_NAME:"       << entry.stringValue(_tag); break;
      case UDSEntry::UDS_FILE_TYPE:          kDebug() << "UDS_FILE_TYPE:"          << entry.numberValue(_tag); break;
      case UDSEntry::UDS_MIME_TYPE:          kDebug() << "UDS_MIME_TYPE:"          << entry.stringValue(_tag); break;
      case UDSEntry::UDS_MODIFICATION_TIME:  kDebug() << "UDS_MODIFICATION_TIME:"  << entry.numberValue(_tag); break;
      case UDSEntry::UDS_CREATION_TIME:      kDebug() << "UDS_CREATION_TIME:"      << entry.numberValue(_tag); break;
      case UDSEntry::UDS_DISPLAY_TYPE:       kDebug() << "UDS_DISPLAY_TYPE:"       << entry.stringValue(_tag); break;
      case UDSEntry::UDS_LOCAL_PATH:         kDebug() << "UDS_LOCAL_PATH:"         << entry.stringValue(_tag); break;
      case UDSEntry::UDS_URL:                kDebug() << "UDS_URL:"                << entry.stringValue(_tag); break;
      case UDSEntry::UDS_TARGET_URL:         kDebug() << "UDS_TARGET_URL:"         << entry.stringValue(_tag); break;
      case UDSEntry::UDS_LINK_DEST:          kDebug() << "UDS_LINK_DEST:"          << entry.stringValue(_tag); break;
      case UDSEntry::UDS_SIZE:               kDebug() << "UDS_SIZE:"               << entry.numberValue(_tag); break;
      case UDSEntry::UDS_GUESSED_MIME_TYPE:  kDebug() << "UDS_GUESSED_MIME_TYPE:"  << entry.stringValue(_tag); break;
      case UDSEntry::UDS_ACCESS:             kDebug() << "UDS_ACCESS:"             << entry.numberValue(_tag); break;
      case UDSEntry::UDS_ICON_NAME:          kDebug() << "UDS_ICON_NAME:"          << entry.stringValue(_tag); break;
      case UDSEntry::UDS_ICON_OVERLAY_NAMES: kDebug() << "UDS_ICON_OVERLAY_NAMES:" << entry.stringValue(_tag); break;
      default:                               kDebug() << "UDS_<UNKNOWN>:"          << _tag;
    } // switch
  return entry;
} // G3Item::toUDSEntry

/**
 */
const UDSEntryList G3Item::toUDSEntryList ( bool signalEntries ) const
{
  MY_KDEBUG_BLOCK ( "<G3Item::toUDSEntryList>" );
  kDebug() << "(<this>)" << toPrintout();
  // NOTE: the CALLING func has to make sure the members array is complete and up2date
  // generate and return final list
  UDSEntryList list;
  kDebug() << "listing" << m_members.count() << "item members";
  foreach ( G3Item* member, m_members )
    if ( signalEntries )
      emit signalUDSEntry ( member->toUDSEntry() );
    else
      list << member->toUDSEntry();
  kDebug() << "{<UDSEntryList[count]>}" << list.count();
  return list;
} // G3Item::toUDSEntryList

const QHash<QString,QString> G3Item::toAttributes ( ) const
{
  MY_KDEBUG_BLOCK ( "<G3Item::toAttributes>" );
  kDebug() << "(<this>)" << toPrintout();
  QHash<QString,QString> attributes;
  attributes.insert ( QLatin1String("id"),       QString("%1").arg(m_id) );
  attributes.insert ( QLatin1String("name"),     m_name );
  attributes.insert ( QLatin1String("type"),     m_type.toString() );
  return attributes;
} // G3Item::toAttributes

//==========

AlbumEntity::AlbumEntity ( G3Backend* const backend, const QVariantMap& attributes )
  : G3Item ( Entity::G3Type::ALBUM, backend, attributes )
{
  MY_KDEBUG_BLOCK ( "<AlbumEntity::AlbumEntity>" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(",");
} // AlbumEntity::AlbumEntity

MovieEntity::MovieEntity ( G3Backend* const backend, const QVariantMap& attributes )
  : G3Item ( Entity::G3Type::MOVIE, backend, attributes )
{
  MY_KDEBUG_BLOCK ( "<MovieEntity::MovieEntity>" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(",");
} // MovieEntity::MovieEntity

PhotoEntity::PhotoEntity ( G3Backend* const backend, const QVariantMap& attributes )
  : G3Item ( Entity::G3Type::PHOTO, backend, attributes )
{
  MY_KDEBUG_BLOCK ( "<PhotoEntity::PhotoEntity>" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(",");
} // PhotoEntity::PhotoEntity

TagEntity::TagEntity ( G3Backend* const backend, const QVariantMap& attributes )
  : G3Item ( Entity::G3Type::TAG, backend, attributes )
{
  MY_KDEBUG_BLOCK ( "<TagEntity::TagEntity>" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(",");
} // TagEntity::TagEntity

CommentEntity::CommentEntity ( G3Backend* const backend, const QVariantMap& attributes )
  : G3Item ( Entity::G3Type::COMMENT, backend, attributes )
{
  MY_KDEBUG_BLOCK ( "<CommentEntity::CommentEntity>" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(",");
} // CommentEntity::CommentEntity

#include "entity/g3_item.moc"
