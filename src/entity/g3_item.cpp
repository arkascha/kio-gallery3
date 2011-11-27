/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

/*!
 * @file
 * Implements the methods of class G3Item and the classes derived from that
 * Such an item mirrors an item contained inside the remote Gallery3 system
 * in the local hierarchical object structure
 * @see G3Item
 * @author Christian Reiner
 */

#include <QObject>
#include <QCryptographicHash>
#include <QVariant>
#include <qjson/parser.h>
#include <qjson/serializer.h>
#include <qjson/qobjecthelper.h>
#include <KUser>
#include <KUrl>
#include <kio/netaccess.h>
#include <klocalizedstring.h>
#include <klocale.h>
#include <kdatetime.h>
#include "utility/exception.h"
#include "protocol/kio_protocol_gallery3.h"
#include "gallery3/g3_backend.h"
#include "gallery3/g3_request.h"
#include "entity/g3_item.h"

using namespace KIO;
using namespace KIO::Gallery3;

/*!
 * G3Item* const G3Item::instantiate ( G3Backend* const backend, const QVariantMap& attributes )
 * @brief Instantiates an item based in the attributes provided 
 * @param  backend    pointer to a validated backend required for communication with the remote Gallery3 system
 * @param  attributes the items technical description as retrieved from the remote Gallery3 system
 * @return
 * the created item object that is already integrated into the items hierarchy in backend and parent item
 * Creates a fresh item object by interpreting the attributes as retrieed from the remote Gallery3 system
 * The object is automatically integrated into the locally cached items hierarchy (backend and parent item)
 * note that it is always an object of a class derived from G3Item that is instanciated, not a G3Item object itself
 * @see G3Item
 * @author Christian Reiner
 */
G3Item* const G3Item::instantiate ( G3Backend* const backend, const QVariantMap& attributes )
{
  KDebug::Block block ( "G3Item::instantiate" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(QLatin1String(","));
  // find out the items type first
  QVariantMap    entity;
  G3Type type;
  if (    attributes.contains(QLatin1String("entity"))
       && attributes[QLatin1String("entity")].canConvert(QVariant::Map) )
    entity = attributes[QLatin1String("entity")].toMap();
  else
    throw Exception ( Error(ERR_INTERNAL),i18n("invalid response form gallery: no item entites specified") );
  if (    entity.contains(QLatin1String("type"))
       && entity[QLatin1String("type")].canConvert(QVariant::String) )
  {
    type = G3Type ( entity[QLatin1String("type")].toString() );
    kDebug() << QString("item type is '%1' [%2]").arg(type.toString()).arg(type.toInt());
  }
  else
    throw Exception ( Error(ERR_INTERNAL),i18n("invalid response form gallery: no item type specified") );
  // create an item object
  switch ( type.toInt() )
  {
    case G3Type::ALBUM:
      return new G3AlbumItem   ( backend, attributes );
    case G3Type::MOVIE:
      return new G3MovieItem   ( backend, attributes );
    case G3Type::PHOTO:
      return new G3PhotoItem   ( backend, attributes );
    case G3Type::TAG:
      return new G3TagItem     ( backend, attributes );
    case G3Type::COMMENT:
      return new G3CommentItem ( backend, attributes );
    default:
      throw Exception ( Error(ERR_INTERNAL),
                        i18n("failed to instantiate entity because of an unknown item type '%1'").arg(type.toString()) );
  } // // switch
} // G3Item::instantiate

/*!
 * G3Item::G3Item ( const G3Type type, G3Backend* const backend, const QVariantMap& attributes )
 * @brief Constructor
 * @param type       the type of object, since it is always a derived class that an object is generated from
 * @param backend    the object will be integrated into the items hierarchy of this backend
 * @param attributes the items technical description as retrieved from the remote Gallery3 system
 * This constructs an item object that describes exactly one single node inside the gallery.
 * @see G3Item
 * @author Christian Reiner
 */
G3Item::G3Item ( const G3Type type, G3Backend* const backend, const QVariantMap& attributes )
  : m ( new G3Item::Members(type,backend,attributes) )
{
  KDebug::Block block ( "G3Item::G3Item" );
  kDebug() << "(<type> <backend> <attributes>)" << type.toString() << backend->toPrintout() << QStringList(attributes.keys()).join(QLatin1String(","));
  // store most important entity tokens directly as strings
  // a few values stored type-strict for later convenience
  m->id         = attributeMapToken ( QLatin1String("entity"), QLatin1String("id"),   QVariant::UInt,   TRUE  ).toUInt();
  m->name       = attributeMapToken ( QLatin1String("entity"), QLatin1String("name"), QVariant::String, FALSE ).toString();
  // set the items mimetype
  QString mimetype_name = attributeMapToken ( QLatin1String("entity"), QLatin1String("mime_type"), QVariant::String, FALSE ).toString();
  if ( ! mimetype_name.isEmpty() )
    m->mimetype = KMimeType::mimeType(mimetype_name);
  else
    switch ( type.toInt() )
    {
      case G3Type::ALBUM:
        m->mimetype = KMimeType::mimeType ( QLatin1String("inode/directory") );
        break;
      default:
        m->mimetype = KMimeType::defaultMimeTypePtr ( );
    } // switch type
  // set parent and pushd into parent
  QString parent_url = attributeMapToken ( QLatin1String("entity"), QLatin1String("parent"), QVariant::String, FALSE ).toString();
  if ( parent_url.isEmpty() )
  {
    m->parent = NULL;
    m->backend->pushItem ( this );
  }
  else
  {
    KUrl url = KUrl ( parent_url );
    g3index id = QVariant(url.fileName()).toInt();
    m->parent = m->backend->item ( id );
    kDebug() << "caching item" << this->toPrintout() << "in parent item" << m->parent->toPrintout();
    m->parent->pushMember ( this );
    m->backend->pushItem ( this );
  } // else
} // G3Item::G3Item

/*!
 * G3Item::~G3Item()
 * @brief Destructor
 * Delete all members as registered inside this object
 * Deregister this item as child in its parent
 * @see G3Item
 * @author Christian Reiner
 */
G3Item::~G3Item()
{
  KDebug::Block block ( "G3Item::~G3Item" );
  kDebug() << "(<>)";
  // remove this node from parents list of members
  if ( NULL!=m->parent )
  {
    kDebug() << "removing item" << toPrintout() << "from parents member list";
    m->parent->popMember ( this );
  }
  // delete all members registered inside this item
  while ( ! m->members.isEmpty() )
  {
    QHash<g3index,G3Item*>::const_iterator member = m->members.constBegin ( );
    kDebug() << "deleting member" << member.value()->toPrintout();
    delete member.value ( );
  }
  // remove this item from the backends catalog
  m->backend->popItem ( m->id );
  // delete private members
  delete m;
} // G3Item::~G3Item

//==========

/*!
 * const QVariant G3Item::attributeToken ( const QString& attribute, QVariant::Type type, bool strict ) const
 * @brief Extracts an attributes from the technical item description
 * @param  attribute name of attribute to be extracted
 * @param  type      type of the attribute to be extracted
 * @param  strict    insist on existance or throw exception instead
 * @return           the extracted attribute in form of a QVariant
 * Extracts a single attribute of the items description, the attribute being
 * identified by its name. In addition the the existance pf the requrested
 * attribute the methods tests if the attribute can be converted to the
 * requested type. In case the attribute does not exist an empty value is
 * returned, except if the strict parameter was set to TRUE, in that case an
 * exception is thrown. 
 * @see G3Item
 * @author Christian Reiner
 */
const QVariant G3Item::attributeToken ( const QString& attribute, QVariant::Type type, bool strict ) const
{
  kDebug() << "(<attribute> <type> <strict>)" << attribute << type << strict;
  if (   m->attributes.contains (attribute)
      && m->attributes[attribute].canConvert(type) )
    // value exists and is convertable
    return m->attributes[attribute];
  else if ( ! strict )
    // value does not exist but an empty instance will be accepted anyway
    return QVariant(type);
  else
    // value does not exist but is required
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      i18n("mandatory item attribute '%1' does not exist or does not have requested type").arg(attribute) );
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

/*!
 * const QVariant G3Item::attributeMapToken ( const QString& attribute, const QString& token, QVariant::Type type, bool strict ) const
 * @brief Extracts a singel item from an attribute of the technical item description
 * @param  attribute name of attribute to be extracted
 * @param  token     name of token inside attribute
 * @param  type      type of the attribute to be extracted
 * @param  strict    insist on existance or throw exception instead
 * @return           the extracted attribute in form of a QVariant
 * Extracts a singel troken from inside an attribute contained in the items
 * technical description as retrieved from the remote Gallery3 system. In
 * addition to the existance of attribute and token the method checks if the
 * value can be converted to the requested type. If the attribute or token
 * does not exist an empty value will be returned, except if the strict
 * parameter was set to TRUE, in that case an exception is thrown.
 * @see G3Item
 * @author Christian Reiner
 */
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
                      i18n("mandatory attribute token '%1|%2' does not exist or does not have requested type").arg(attribute).arg(token) );
} // G3Item::attributeMapToken

//==========

/*!
 * void G3Item::pushMember ( G3Item* item )
 * @brief Accepts a new member item
 * @param item: item to be accepted as a new member inside this item
 * Accepts a new item as member inside the current album. 
 * @see G3Item
 * @author Christian Reiner
 */
void G3Item::pushMember ( G3Item* item )
{
  kDebug() << "(<this> <item>)" << toPrintout() << item->toPrintout();
  if ( m->members.contains(item->id()) )
    throw Exception ( Error(ERR_INTERNAL),
                      i18n("attempt to register item with id '%1' that already exists").arg(item->id()) );
  // all fine, store items
  m->members.insert ( item->id(), item );
  item->m->parent = this;
} // G3Item::pushMember

/*!
 * G3Item* G3Item::popMember ( G3Item* item )
 * @brief Removes a member item
 * @param  item pointer to an item object to be removed
 * @return      pointer to item object that has been removed
 * Shortcut & convenience method to remove a given object from the members list
 * @see G3Item
 * @author Christian Reiner
 */
G3Item* G3Item::popMember ( G3Item* item )
{
  kDebug() << "(<this> <item>)" << toPrintout() << item->toPrintout();
  return popMember ( item->id() );
} // G3Item::popMember

/*!
 * G3Item* G3Item::popMember ( g3index id )
 * @brief Removes a member item
 * @param  id numeric id of item object to be removed
 * @return    pointer to the item object that has been removed
 * Removes a specific item object from the list of members. Not, that the
 * object is NOT deleted, it is merely removed from the list and a pointer to
 * that object is returned
 * @see G3Item
 * @author Christian Reiner
 */
G3Item* G3Item::popMember ( g3index id )
{
  kDebug() << "(<this> <id>)" << toPrintout() << id;
  if ( m->members.contains(id) )
  {
    G3Item* item = m->members[id];
    m->members.remove ( id );
    return item;
  }
  throw Exception ( Error(ERR_INTERNAL),
                    i18n("attempt to remove non-existing member item with id '%1'").arg(id) );
} // G3Item::popMember

/*!
 * void G3Item::setParent ( G3Item* parent )
 * @brief Sets an items parent (album)
 * @param parent the parent item the item is to be associate with
 * Associates the item to a new parent item. 
 * @see G3Item
 * @author Christian Reiner
 */
void G3Item::setParent ( G3Item* parent )
{
  kDebug() << "(<parent>)" << parent->toPrintout();
  m->parent = parent;
} // G3Item::setParent

//==========

/*!
 * G3Item* G3Item::member ( const QString& name )
 * @brief Provides a member item
 * @param  name name of the requested item
 * @return      pointer to the requested member item
 * Requests an item object specified by its unique name. The item will be
 * created (retrieved) if it does not (yet) exist locally or an exception will
 * be thrown in case the item does not exist inside the remote Gallery3 system.
 * @see G3Item
 * @author Christian Reiner
 */
G3Item* G3Item::member ( const QString& name )
{
  KDebug::Block block ( "G3Item::member" );
  kDebug() << "(<this> <name>)" << toPrintout() << name;
  // make sure members have been retrieved
  buildMemberItems ( );
  // look for requested member
  QHash<g3index,G3Item*>::const_iterator member;
  for ( member=m->members.constBegin(); member!=m->members.constEnd(); member++ )
    if ( name==member.value()->name() )
      return member.value();
  // no success, no matching member found
  throw Exception ( Error(ERR_DOES_NOT_EXIST), name );
} // G3Item::getMember

/*!
 * G3Item* G3Item::member ( g3index id )
 * @brief Provides a member item
 * @param  id numerical id of the requested item
 * @return    pointer to the requested member items
 * Requests an item object specified by its numerical id. The item will be
 * created (retrieved) if it does not (yet) exist locally or an exception will
 * be thrown in case the item does not exist inside the remote Gallery3 system.
 * @see G3Item
 * @author Christian Reiner
 */
G3Item* G3Item::member ( g3index id )
{
  KDebug::Block block ( "G3Item::member" );
  kDebug() << "(<this> <id>)" << toPrintout() << id;
  // make sure members have been retrieved
  buildMemberItems ( );
  // look for requested member
  if ( m->members.contains(id) )
  {
    kDebug() << QString("found member '%1' [%2]").arg(m->members[id]->toPrintout()).arg(m->members[id]->id());
    return m->members[id];
  }
  else
    throw Exception ( Error(ERR_DOES_NOT_EXIST), i18n("item with id '%1'").arg(id) );
} // G3Item::member

/*!
 * QHash<g3index,G3Item*> G3Item::members ( )
 * @brief Provides a list of all member items
 * @return dictionary of member items
 * Returns a copy of the dictionary of member items associated with this items
 * The member item objects will be created or re-created as required
 * @see G3Item
 * @author Christian Reiner
 */
QHash<g3index,G3Item*> G3Item::members ( )
{
  KDebug::Block block ( "G3Item::members" );
  kDebug() << "(<this>)" << toPrintout();
  buildMemberItems ( );
  return m->members;
} // G3Item::members

/*!
 * bool G3Item::containsMember ( const QString& name )
 * @brief Decides if a member item exists
 * @param  name name of the requested item
 * @return      wether the requested item does exist or not
 * Returns of the requested item does exist inside the parent item (album)
 * Note that the list of member items will be refreshed as required
 * @see G3Item
 * @author Christian Reiner
 */
bool G3Item::containsMember ( const QString& name )
{
  KDebug::Block block ( "G3Item::containsMember" );
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

/*!
 * bool G3Item::containsMember ( g3index id )
 * @brief Decides if a member item exists
 * @param  id numeric if of the requested item
 * @return    wether the requested item does exist or not
 * Returns of the requested item does exist inside the parent item (album). 
 * Note that the list of member items will be refreshed as required. 
 * @see G3Item
 * @author Christian Reiner
 */
bool G3Item::containsMember ( g3index id )
{
  KDebug::Block block ( "G3Item::containsMember" );
  kDebug() << "(<this> <id>)" << toPrintout() << id;
  buildMemberItems ( );
  return m->members.contains ( id );
}

/*!
 * int G3Item::countMembers ( )
 * @brief Counts the member items
 * @return number of member items inside the parent item (album)
 * Returns the number of items contained inside a parent item (album). 
 * note that the list of items is refreshed and all items are instanciated. 
 * @see G3Item
 * @author Christian Reiner
 */
int G3Item::countMembers ( )
{
  KDebug::Block block ( "G3Item::countMembers" );
  kDebug() << "(<this>)" << toPrintout();
  buildMemberItems ( );
  return m->members.count();
} // G3Item::countMembers

/*!
 * void G3Item::buildMemberItems ( )
 * @brief Creates all member items as specified in the items description
 * Instanciates all member items contained inside a parent item (album).
 * Note that items already existing will not be re-created.
 * @see G3Item
 * @author Christian Reiner
 */
void G3Item::buildMemberItems ( )
{
  KDebug::Block block ( "G3Item::buildMemberItems" );
  kDebug() << "(<this>)" << toPrintout();
  QVariantList list = attributeList("members",TRUE).toList();
  // members list oout of sync ?
  if ( list.count()!=m->members.count() )
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
    foreach ( G3Item* member, m->members )
      if ( ! urls.contains(member->id()) )
      {
        kDebug() << "removing stale member" << member->toPrintout();
        m->members.remove ( member->id() );
        delete member;
      } // if
      else
        kDebug() << "keeping existing member" << member->toPrintout();
    // identify all members not present but mentioned in attribute 'members'
    foreach ( g3index id, urls.keys() )
      if ( ! m->members.contains(id) )
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
      QList<G3Item*> items = G3Request::g3GetItems ( m->backend, urls.values() );
    }
  } // if
} // G3Item::getMembers

//==========

/*!
 * QStringList G3Item::path ( ) const
 * @brief Provides an items path
 * @return path in form of a list of breadcrumbs
 * Returns an items path inside the items hierarchy in form of a breadcrumb list. 
 * @see G3Item
 * @author Christian Reiner
 */
QStringList G3Item::path ( ) const
{
  KDebug::Block block ( "G3Item::path" );
  kDebug() << "(<this>)" << toPrintout();
  if ( NULL!=m->parent )
  {
    QStringList path = m->parent->path();
    path << name();
    return path;
  }
  return QStringList();
} // G3Item::path

/*!
 * G3Item* G3Item::parent ( ) const
 * @brief Provides an items parent item
 * @return pointer to parent item
 * Returns the parent item that contains this item
 * @see G3Item
 * @author Christian Reiner
 */
G3Item* G3Item::parent ( ) const
{
  kDebug() << "(<>)";
  return m->parent;
} // G3Item::parent

//==========

/*!
 * const G3Item& G3Item::operator<< ( G3Item& member )
 * @brief Accepts a new member item
 * @param  member member item to be accepted
 * @return        item accepting the new member item
 * Accepts a new item as new member item inside a given item (album)
 * @see G3Item
 * @author Christian Reiner
 */
const G3Item& G3Item::operator<< ( G3Item& member )
{
  kDebug() << "(<this> <item>)" << toPrintout() << "<<" << member.toPrintout();
  pushMember ( &member );
  return *this;
} // operator<<

/*!
 * const G3Item& G3Item::operator<< ( G3Item* member )
 * @brief Accepts a new member item
 * @param  member member item to be accepted
 * @return        item accepting the new member item
 * Accepts a new item as new member item inside a given item (album)
 * @see G3Item
 * @author Christian Reiner
 */
const G3Item& G3Item::operator<< ( G3Item* member )
{
  kDebug() << "(<this> <item>)" << toPrintout() << "<<" << member->toPrintout();
  pushMember ( member );
  return *this;
} // operator<<

//==========

/*!
 * const QString G3Item::toPrintout ( ) const
 * @brief Human readable representation of an item
 * @return readable presentation of the item
 * Returns a human readable presentation of the item, typically used to be
 * cited in debug outputs or log files. 
 * @see G3Item
 * @author Christian Reiner
 */
const QString G3Item::toPrintout ( ) const
{
  return QString("G3Item ('%1' [%2])").arg(name()).arg(id());
}

/*!
 * const UDSEntry G3Item::toUDSEntry ( ) const
 * @brief Publish item as UDS entry
 * @return UDSEntry describing the item object
 * Generates and returns a UDSEntry that describes the item object
 * @see G3Item
 * @author Christian Reiner
 */
const UDSEntry G3Item::toUDSEntry ( ) const
{
  KDebug::Block block ( "G3Item::toUDSEntry" );
  kDebug() << "(<this>)" << toPrintout();
  UDSEntry entry;
  entry.insert( UDSEntry::UDS_NAME,               QString("%1").arg(m->name) );
//  entry.insert( UDSEntry::UDS_DISPLAY_NAME,       QString("[%1] %2").arg(m->id).arg(attributeMapToken("entity","title",QVariant::String).toString()) );
  entry.insert( UDSEntry::UDS_COMMENT,            attributeMapToken("entity","description",QVariant::String).toString() );
  entry.insert( UDSEntry::UDS_FILE_TYPE,          m->type.toUDSFileType() );
  entry.insert( UDSEntry::UDS_MIME_TYPE,          m->mimetype->name() );
  entry.insert( UDSEntry::UDS_DISPLAY_TYPE,       m->type.toString() );
  entry.insert( UDSEntry::UDS_SIZE,               (G3Type::ALBUM==m->type.toInt()) ? 0L : attributeMapToken(QLatin1String("entity"),QLatin1String("file_size"),QVariant::Int).toInt() );
  entry.insert( UDSEntry::UDS_ACCESS,             canEdit() ? 0600 : 0400 );
  entry.insert( UDSEntry::UDS_CREATION_TIME,      attributeMapToken(QLatin1String("entity"),QLatin1String("created"),QVariant::Int).toInt() );
  entry.insert( UDSEntry::UDS_MODIFICATION_TIME,  attributeMapToken(QLatin1String("entity"),QLatin1String("updated"),QVariant::Int).toInt() );
//  entry.insert( UDSEntry::UDS_LOCAL_PATH,         m_fileUrl.path() );
//  entry.insert( UDSEntry::UDS_URL,                m_fileUrl.url() );
//  entry.insert( UDSEntry::UDS_TARGET_URL,         m_fileUrl.url() );
//  entry.insert( UDSEntry::UDS_LINK_DEST,          m_fileUrl.url() );
/*
  KUser user ( KUser::UseEffectiveUID );
  entry.insert( UDSEntry::UDS_USER,               user.loginName() );
  entry.insert( UDSEntry::UDS_GROUP,              user.groupNames().first() );
*/

  // some intense debugging output...
  QList<uint> _tags = entry.listFields ( );
  kDebug() << "list of defined UDS entry tags for entry" << toPrintout() << QLatin1String(":");
  foreach ( uint _tag, _tags )
    switch ( _tag )
    {
      case UDSEntry::UDS_NAME:               kDebug() << "UDS_NAME:"               << entry.stringValue(_tag); break;
      case UDSEntry::UDS_DISPLAY_NAME:       kDebug() << "UDS_DISPLAY_NAME:"       << entry.stringValue(_tag); break;
      case UDSEntry::UDS_COMMENT:            kDebug() << "UDS_COMMENT:"            << entry.stringValue(_tag); break;
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

/*!
 * const UDSEntryList G3Item::toUDSEntryList ( bool signalEntries ) const
 * @brief Publish items members as UDS entries
 * @param signalEntries output entries via signal or collected at the end
 * Generates a list of UDSEntries of all member items contained.
 * The flag 'signalEntries' results in single UDSentries being emitted via the
 * signal 'signalUDSEntry'. If not set all entries will be returned in form of
 * a UDSEntrylist at the end of the method. 
 * @see G3Item
 * @author Christian Reiner
 */
const UDSEntryList G3Item::toUDSEntryList ( bool signalEntries ) const
{
  KDebug::Block block ( "G3Item::toUDSEntryList" );
  kDebug() << "(<this>)" << toPrintout();
  // NOTE: the CALLING func has to make sure the members array is complete and up2date
  // generate and return final list
  UDSEntryList list;
  kDebug() << "listing" << m->members.count() << "item members";
  foreach ( G3Item* member, m->members )
    if ( signalEntries )
      emit signalUDSEntry ( member->toUDSEntry() );
    else
      list << member->toUDSEntry();
  kDebug() << "{<UDSEntryList[count]>}" << list.count();
  return list;
} // G3Item::toUDSEntryList

/*!
 * const QHash<QString,QString> G3Item::toAttributes ( ) const
 * @brief Provides the most basic attributes of the item
 * @return dictionary of items attributes
 * Returns a dictionary (hash) of the basic attributes that identify an item, 
 * typically used during the creation of a new item inside the remote Gallery3
 * system.
 * @see G3Item
 * @author Christian Reiner
 */
const QHash<QString,QString> G3Item::toAttributes ( ) const
{
  KDebug::Block block ( "G3Item::toAttributes" );
  kDebug() << "(<this>)" << toPrintout();
  QHash<QString,QString> attributes;
  attributes.insert ( QLatin1String("id"),       QString("%1").arg(m->id) );
  attributes.insert ( QLatin1String("name"),     m->name );
  attributes.insert ( QLatin1String("type"),     m->type.toString() );
  return attributes;
} // G3Item::toAttributes

//==========

/*!
 * G3AlbumItem::G3AlbumItem ( G3Backend* const backend, const QVariantMap& attributes )
 * @brief Constructor
 * @param backend    backend that contains this item
 * @param attributes attributes describing the item
 * Constructs a new item of type 'album' described by the specified attributes
 * @see G3Item
 * @author Christian Reiner
 */
G3AlbumItem::G3AlbumItem ( G3Backend* const backend, const QVariantMap& attributes )
  : G3Item ( G3Type::ALBUM, backend, attributes )
{
  KDebug::Block block ( "G3AlbumItem::G3AlbumItem" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(QLatin1String(","));
} // G3AlbumItem::G3AlbumItem

/*!
 * G3MovieItem::G3MovieItem ( G3Backend* const backend, const QVariantMap& attributes )
 * @brief Constructor
 * @param backend backend that contains this item
 * @param attributes attributes describing the item
 * Constructs a new item of type 'movie' described by the specified attributes
 * @see G3Item
 * @author Christian Reiner
 */
G3MovieItem::G3MovieItem ( G3Backend* const backend, const QVariantMap& attributes )
  : G3Item ( G3Type::MOVIE, backend, attributes )
{
  KDebug::Block block ( "G3MovieItem::G3MovieItem" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(QLatin1String(","));
} // G3MovieItem::G3MovieItem

/*!
 * G3PhotoItem::G3PhotoItem ( G3Backend* const backend, const QVariantMap& attributes )
 * @brief Constructor
 * @param backend backend that contains this item
 * @param attributes attributes describing the item
 * Constructs a new item of type 'photo' described by the specified attributes
 * @see G3Item
 * @author Christian Reiner
 */
G3PhotoItem::G3PhotoItem ( G3Backend* const backend, const QVariantMap& attributes )
  : G3Item ( G3Type::PHOTO, backend, attributes )
{
  KDebug::Block block ( "G3PhotoItem::G3PhotoItem" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(QLatin1String(","));
} // G3PhotoItem::G3PhotoItem

/*!
 * G3TagItem::G3TagItem ( G3Backend* const backend, const QVariantMap& attributes )
 * @brief Constructor
 * @param backend backend that contains this item
 * @param attributes attributes describing the item
 * Constructs a new item of type 'tag' described by the specified attributes
 * @see G3Item
 * @author Christian Reiner
 */
G3TagItem::G3TagItem ( G3Backend* const backend, const QVariantMap& attributes )
  : G3Item ( G3Type::TAG, backend, attributes )
{
  KDebug::Block block ( "G3TagItem::G3TagItem" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(QLatin1String(","));
} // G3TagItem::G3TagItem

/*!
 * G3CommentItem::G3CommentItem ( G3Backend* const backend, const QVariantMap& attributes )
 * @brief Constructor
 * @param backend backend that contains this item
 * @param attributes attributes describing the item
 * Constructs a new item of type 'comment' described by the specified attributes
 * @see G3Item
 * @author Christian Reiner
 */
G3CommentItem::G3CommentItem ( G3Backend* const backend, const QVariantMap& attributes )
  : G3Item ( G3Type::COMMENT, backend, attributes )
{
  KDebug::Block block ( "G3CommentItem::G3CommentItem" );
  kDebug() << "(<backend> <attributes>)" << backend->toPrintout() << QStringList(attributes.keys()).join(QLatin1String(","));
} // G3CommentItem::G3CommentItem

#include "entity/g3_item.moc"
