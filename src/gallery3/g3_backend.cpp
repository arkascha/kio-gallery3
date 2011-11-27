/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

/*!
 * @file
 * @brief Implements all methods of class G3Backend
 * @see G3Backend
 * @author Christian Reiner
 */
#include <klocalizedstring.h>
#include "utility/exception.h"
#include "gallery3/g3_backend.h"
#include "gallery3/g3_request.h"
#include "entity/g3_file.h"
#include "entity/g3_item.h"

using namespace KIO;
using namespace KIO::Gallery3;

/*!
 * G3Backend* const G3Backend::instantiate ( QObject* parent, QHash<QString,G3Backend*>& backends, KUrl g3Url )
 * @brief Instantiates a new backend object
 * @param url url of requested base inside the galleries hierarchy
 * @return    pointer to a validated and unique backend object
 * Detects and returns the position (url) of the G3-API based on any given
 * Strategy: the REST API must be some base folder of the requested url
 *           so we test each breadcrump one by one (shortening the url) until we find the service
 * Note: this appears horrible, but there are two reasons for this:
 * 1.) our slave might be re-used to access more than one single gallery
 * 2.) there might be more than one gallery sharing the same start url
 * @see G3Backend
 * @author Christian Reiner
 */
G3Backend* const G3Backend::instantiate ( QObject* parent, QHash<QString,G3Backend*>& backends, KUrl g3Url )
{
  KDebug::Block block ( "G3Backend::instantiate" );
  kDebug() << "(<url>)" << g3Url;
  G3Backend* backend;
  // try if any existing backend is assiciated with a sub-URL of the requested one
  QHash<QString,G3Backend*>::const_iterator it;
  for ( it=backends.constBegin(); it!=backends.constEnd(); it++ )
  {
    backend = it.value();
    if ( backend->baseUrl().isParentOf(g3Url) )
    {
      kDebug() << QString("detected existing G3-API url '%1', reusing associated backend").arg(backend->restUrl().prettyUrl());
      return backend;
    } // if
  }
  // try all sub-urls downwards in an iterative manner until we either find an existing API or we have to give up
  do
  {
    backend = new G3Backend ( parent, g3Url );
    if ( G3Request::g3Check ( backend ) )
    {
      kDebug() << QString("detected existing G3-API url '%1', created fresh backend").arg(backend->restUrl().prettyUrl());
      backends.insert ( backend->baseUrl().url(), backend );
      return backend;
    }
    // no success, iterate 'downwards'
    kDebug() << "no G3-API available at url" << g3Url;
    delete backend;
    g3Url.setPath ( g3Url.directory() );
  }
  while ( ! g3Url.path().isEmpty() );
  // REST API _not_ detected
  throw Exception ( Error(ERR_SLAVE_DEFINED),i18n("No usable G3-API service found") );
} // G3Backend::instantiate

/*!
 * G3Backend::G3Backend ( QObject* parent, const KUrl& g3Url )
 * @brief Constructor
 * @param g3Url url of the galleries web root
 * Stores the derived REST API url and handles the requested protocol
 * @see G3Backend
 * @author Christian Reiner
 */
G3Backend::G3Backend ( QObject* parent, const KUrl& g3Url )
  : QObject ( parent )
  , m       ( new G3Backend::Members(g3Url) )
{
  KDebug::Block block ( "G3Backend::G3Backend" );
  m->restUrl = m->baseUrl;
  m->restUrl.setProtocol ( (QLatin1String("gallery3s")==m->baseUrl.protocol()) ? QLatin1String("https"):QLatin1String("http") );
  // authentication credentials dont make sense since the REST API does not use http basic authentication
  m->restUrl.setUser     ( QString() );
  m->restUrl.setPass     ( QString() );
  m->restUrl.addPath     ( "rest" );
  kDebug() << "{<base> <rest>}" << m->baseUrl << m->restUrl;
  // prepare AuthInfo for later authentication against the remote gallery3 system
  m->credentials.caption      = QLatin1String("Authentication required");
  m->credentials.prompt       = QLatin1String("Authentication required");
  m->credentials.commentLabel = QLatin1String("Login:");
  m->credentials.comment      = QString("Gallery3 at host '%1'").arg(m->baseUrl.host());
  m->credentials.realmValue   = QString("Gallery3 at host '%1'").arg(m->baseUrl.host());;
  m->credentials.keepPassword = TRUE;
  m->credentials.verifyPath   = TRUE;
  m->credentials.url          = m->baseUrl;
  m->credentials.password     = m->baseUrl.password ( ); // might be empty
  if ( ! m->baseUrl.userName().isEmpty() )
  {
    m->credentials.username = m->baseUrl.userName();
    m->credentials.readOnly = TRUE;
  }
}

/*!
 * G3Backend::~G3Backend ( )
 * @brief Desctructor
 * Recursively deletes all registered items associated to this backend by
 * deleting the base item
 * @see G3Backend
 * @author Christian Reiner
 */
G3Backend::~G3Backend ( )
{
  KDebug::Block block ( "G3Backend::~G3Backend" );
  kDebug() << "(<>)";
  // remove all items generated on-the-fly if the base item exists at all
  kDebug() << "deleting base item";
  if ( m->items.contains(1) )
  {
    delete itemBase ( );
    m->items.remove ( 1 );
  }
  // remove any orphaned items
  kDebug() << "removing " << m->items.count() << "orphaned items";
  while ( ! m->items.isEmpty() )
  {
    QHash<g3index,G3Item*>::const_iterator item = m->items.constBegin();
    kDebug() << "deleting item" << item.value()->toPrintout();
    delete item.value();
  } // while
  kDebug() << m->items.count() << "items left after removal of orphans";
  // delete private members
  delete m;
}

//==========

/*!
 * const UDSEntry G3Backend::toUDSEntry ( )
 * @brief Publishes a backend locally
 * @return UDS description of this backend as base folder
 * @see G3Backend
 * @author Christian Reiner
 */
const UDSEntry G3Backend::toUDSEntry ( )
{
  KDebug::Block block ( "G3Backend::toUDSEntry" );
  kDebug() << "(<>)";
  return itemBase()->toUDSEntry();
} // G3Backend::toUDSEntry

/*!
 * const UDSEntryList G3Backend::toUDSEntryList ( )
 * @brief Publishes a backends base item locally
 * @return a list holding the base item as UDS entry
 * @see G3Backend
 * @author Christian Reiner
 */
const UDSEntryList G3Backend::toUDSEntryList ( )
{
  KDebug::Block block ( "G3Backend::toUDSEntryList" );
  kDebug() << "(<>)";
  UDSEntryList list;
  list << itemBase()->toUDSEntry();
  return list;
} // G3Backend::toUDSEntryList

/*!
 * const QString G3Backend::toPrintout ( ) const
 * @brief Human readable representation of a backend
 * @return string description of the backend
 * A human readable description of the backend, mostly for debugging purposes
 * @see G3Backend
 * @author Christian Reiner
 */
const QString G3Backend::toPrintout ( ) const
{
  return QString("G3Backend [%1 items] (%2)").arg(m->items.count()).arg(m->baseUrl.prettyUrl());
} // G3Backend::toPrintout

//==========

/*!
 * G3Item* G3Backend::itemBase ( )
 * @brief Provides the base item inside a backends item hierarchy
 * @return the base (root) item of the assiciated gallery
 * Each gallery has only one and exactly one base item. 
 * This item acts as root of the items hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
G3Item* G3Backend::itemBase ( )
{
  KDebug::Block block ( "G3Backend::itemBase" );
  kDebug() << "(<>)";
  return itemById ( 1 );
} // G3Backend::itemBase

/*!
 * G3Item* G3Backend::itemById ( g3index id )
 * @brief Provides or creates an item inside the backends item hierarchy
 * @param  g3index numeric item id
 * @return         pointer to the item associated with the given id
 * The method blindly returns an existing, previously registered item. 
 * In case no such item exists it starts an attempt to retrieve the item. 
 * In that case the item is automatically registered and integrated into the
 * items hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
G3Item* G3Backend::itemById ( g3index id )
{
  KDebug::Block block ( "G3Backend::itemById" );
  kDebug() << "(<id>)" << id;
  if ( m->items.contains(id) )
    return m->items[id];
  // item not yet known, try to request it from the gallery server
  return item ( id );
} // G3Backend::itemById

/*!
 * G3Item* G3Backend::itemByUrl ( const KUrl& itemUrl )
 * @brief Provides or creates an item inside the backends item hierarchy
 * @param itemUrl local item url inside the local folder hierarchy
 * @return        pointer to the item associated with the given url
 * Walks through the folder hierarchy as presented by the set of registered
 * items, in case a subpath of the given url does not map onto previously
 * registered items. Those items will be retrieved, registered and integrated
 * into the local hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
G3Item* G3Backend::itemByUrl ( const KUrl& itemUrl )
{
  KDebug::Block block ( "G3Backend::itemByUrl" );
  kDebug() << "(<url>)" << itemUrl;
  const QString itemPath = KUrl::relativeUrl ( m->baseUrl, itemUrl );
  return itemByPath ( itemPath );
} // G3Backend::itemByUrl

/*!
 * G3Item* G3Backend::itemByPath ( const QString& path )
 * @brief Provides or creates an item inside a backends item hierarchy
 * @param path local item path inside the local folder hierarchy
 * @return     pointer to the item associated with the given url
 * Walks through the folder hierarchy as presented by the set of registered
 * items, in case a subpath of the given url does not map onto previously
 * registered items. Those items will be retrieved, registered and integrated
 * into the local hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
G3Item* G3Backend::itemByPath ( const QString& path )
{
  KDebug::Block block ( "G3Backend::itemByPath" );
  kDebug() << "(<path>)" << path;
  QStringList breadcrumbs = path.split(QLatin1String("/"));
  return itemByPath ( breadcrumbs );
} // G3Backend::itemByPath

/*!
 * G3Item* G3Backend::itemByPath ( const QStringList& breadcrumbs )
 * @brief Provides or creates an item inside a backends item hierarchy
 * @param breadcrumbs local item pathinside the folder hierarchy
 * @return            pointer to the item associated with the given url
 * Walks through the folder hierarchy as presented by the set of registered
 * items, in case a subpath of the given url does not map onto previously
 * registered items. Those items will be retrieved, registered and integrated
 * into the local hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
G3Item* G3Backend::itemByPath ( const QStringList& breadcrumbs )
{
  KDebug::Block block ( "G3Backend::itemByPath" );
  kDebug() << "(<breadcrumbs>)"<< breadcrumbs.join(QLatin1String("|"));
  // start at the 'root' album
  G3Item* item = itemBase ( );
  QList<QString>::const_iterator it;
  // descend into the album hierarchy one by one along the breadcrumbs path
  for ( it=breadcrumbs.constBegin(); it!=breadcrumbs.constEnd(); it++)
    // skip empty names, this might come from processing an absolute path or because of double slashes in paths
    if ( ! it->isEmpty() )
      item = item->member ( *it );
  kDebug() << "{<item>}" << item->toPrintout();
  return item;
} // G3Backend::itemByPath

/*!
 * QList<G3Item*> G3Backend::membersByItemId ( g3index id )
 * @brief Provides a list of all member items of an item inside the backends item hierarchy
 * @param  id numeric item id
 * @return    list of pointers to registered member item objects
 * Will return all members of a given parent item by looking at the 'members'
 * attribute inside the item description. 
 * Any members registered that are not listed in the attribute are deleted
 * Any members not (yet) registered but listed in the attribute are retrieved,
 * registered and integrated into the item hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
QList<G3Item*> G3Backend::membersByItemId ( g3index id )
{
  KDebug::Block block ( "G3Backend::membersByItemId" );
  kDebug() << "(<id>)" << id;
  G3Item* parent = itemById ( id );
  QList<G3Item*> items = G3Request::g3GetItems ( this, id );
  // process url list, generate items and sort them into this backends item cache
  foreach ( G3Item* item, items )
  {
    if ( parent->containsMember(item->id()) )
      parent->popMember ( item );
    parent->pushMember ( item );
  } // foreach
  return items;
} // G3Backend::membersByItemId

/*!
 * QList<G3Item*> G3Backend::membersByItemPath ( const QString& path )
 * @brief Provides a list of all member items of an item inside the backends item hierarchy
 * @param path path of item in local folder hierarchy
 * @return     list of pointers to registered member item objects
 * Will return all members of a parent item by looking at the 'members'
 * attribute inside the item description. The parent item is identified by its
 * path inside the local folder hierary. 
 * Any missing subfolders leading to the parent item will be retrieved,
 * registered and integrated ino the item hierarchy. 
 * Any members registered that are not listed in the attribute are deleted. 
 * Any members not (yet) registered but listed in the attribute are retrieved,
 * registered and integrated into the item hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
QList<G3Item*> G3Backend::membersByItemPath ( const QString& path )
{
  KDebug::Block block ( "G3Backend::membersByItemPath" );
  kDebug() << "(<path>)" << path;
  if ( QLatin1String("./")==path )
    return membersByItemPath ( QStringList() );
  else
    return membersByItemPath ( path.split(QLatin1String(QLatin1String("/"))) );
} // G3Backend::membersByItemPath

/*!
 * QList<G3Item*> G3Backend::membersByItemPath ( const QStringList& breadcrumbs )
 * @brief Provides a list of all member items of an item inside the backends item hierarchy
 * @param breadcrumbs path of item as breadcrumbs in local folder hierarchy
 * @return            list of pointers to registered member item objects
 * Will return all members of a parent item by looking at the 'members'
 * attribute inside the item description. The parent item is identified by its
 * path inside the local folder hierary. 
 * Any missing subfolders leading to the parent item will be retrieved,
 * registered and integrated ino the item hierarchy. 
 * Any members registered that are not listed in the attribute are deleted. 
 * Any members not (yet) registered but listed in the attribute are retrieved,
 * registered and integrated into the item hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
QList<G3Item*> G3Backend::membersByItemPath ( const QStringList& breadcrumbs )
{
  KDebug::Block block ( "G3Backend::membersByItemPath" );
  kDebug() << "(<breadcrumbs>)" << breadcrumbs.join(QLatin1String("|"));
  G3Item* parent = itemByPath ( breadcrumbs );
  QList<G3Item*> items = G3Request::g3GetItems ( this, QStringList(QString("%1/item/%2").arg(m->restUrl.url()).arg(parent->id())) );
  // process url list, generate items and sort them into this backends item cache
  foreach ( G3Item* item, items )
  {
    if ( parent->containsMember(item->id()) )
      parent->popMember ( item );
    parent->pushMember ( item );
  } // foreach
  return items;
} // G3Backend::membersByItemPath

//==========

/*!
 * G3Item* G3Backend::item ( g3index id )
 * @brief Provides an item inside the backends item hierarchy
 * @param id numeric item id
 * @return   pointer to the associated item object
 * Accepts and returns a previously registered item matching the id or attempts
 * to retrieve it. A retrieved item will be registered and integrated in to the
 * local item hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
G3Item* G3Backend::item ( g3index id )
{
  KDebug::Block block ( "G3Backend::item" );
  kDebug() << "(<id>)" << id;
  if ( m->items.contains(id) )
    return m->items[id];
  // item not found, retrieve it from the remote gallery
  G3Item* item = G3Request::g3GetItem ( this, id );
  kDebug() << "{<item>}" << item->toPrintout();
  return item;
} // G3Backend::item

/*!
 * QHash<g3index,G3Item*> G3Backend::members ( g3index id )
 * @brief Provides a list of member items of an item inside a backends item hierarchy
 * @param id numeric item id
 * @return   hash of pointers to items by their id
 * Consults a given item and requests the list of member items inside that item
 * @see G3Backend
 * @author Christian Reiner
 */
QHash<g3index,G3Item*> G3Backend::members ( g3index id )
{
  KDebug::Block block ( "G3Backend::members" );
  kDebug() << "(<id>)" << id;
  itemById(id)->members ( );
} // G3Backend::members

/*!
 * QHash<g3index,G3Item*> G3Backend::members ( G3Item* item )
 * @brief Provides a list of member items of an item inside a backends item hierarchy
 * @param item pointer to an item object
 * @return     hash of pointers to items by their id
 * Consults a given item and requests the list of member items inside that item
 * @see G3Backend
 * @author Christian Reiner
 */
QHash<g3index,G3Item*> G3Backend::members ( G3Item* item )
{
  KDebug::Block block ( "G3Backend::members" );
  kDebug() << "(<item>)" << item->toPrintout();
  return item->members ( );
} // G3Backend::members

//==========

/*!
 * int G3Backend::countItems  ( )
 * @brief Number of items that exist inside a backends item hierarchy
 * @return number of currently known items associated with this backend
 * Mainly for debugging / logging purposes
 * @see G3Backend
 * @author Christian Reiner
 */
int G3Backend::countItems  ( )
{
  kDebug() << "(<>)";
  return m->items.count();
} // G3Backend::countItems

//==========

/*!
 * bool G3Backend::login ( AuthInfo& credentials )
 * @brief Login to a remote Gallery3 system represented by the backend
 * @param credentials authentiction credentials
 * @return            TRUE if authentication succeeded, FALSE otherwise
 * Performs a G3 specific authentication request. 
 * Goal is to get back the 'remote access key' that is used by the G§ REST API as a kind of long-term-session-keys. 
 * Upon success that key is stored inside the authentication credential as 'digestInfo'. 
 * @see G3Backend
 * @author Christian Reiner
 */
bool G3Backend::login ( AuthInfo& credentials )
{
  kDebug() << "(<credentials>)" << credentials.caption;
  return G3Request::g3Login ( this, credentials );
} // G3Request::g3RemoteAccessKey

/*!
 * void G3Backend::pushItem ( G3Item* item )
 * @brief Pushes a given item into the backends item hierarchy
 * @param item pointer to an item object
 * Registeres a given item inside its associated backend. 
 * Note that the backend only holds an unordered list of items, not a hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
void G3Backend::pushItem ( G3Item* item )
{
  kDebug() << "(<item>)" << item->toPrintout();
  m->items.insert ( item->id(), item );
} // G3Backend::pushItem

/*!
 * G3Item* G3Backend::popItem ( g3index id )
 * @brief Pops an item from the backends item hierarchy
 * @param id numeric item id
 * @returns  pointer to the item holding the given id)
 * Deregisteres an existing item from its associated backend and returns a pointer to it. 
 * Note that this does not delete the item object nor does it remove it from the items hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
G3Item* G3Backend::popItem ( g3index id )
{
  kDebug() << "(<id>)" << id;
  if ( m->items.contains(id) )
    return m->items.take ( id );
  throw Exception ( Error(ERR_INTERNAL), i18n("attempt to remove non-existing item with id '%1'").arg(id) );
} // G3Backend::popItem

/*!
 * void G3Backend::removeItem ( G3Item* item )
 * @brief removes an item from the backends item hierarchy
 * @param item pointer to item object
 * Deregisteres a given item from its associated backend and deletes the object. 
 * Note that this includes the removal from the items hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
void G3Backend::removeItem ( G3Item* item )
{
  KDebug::Block block ( "G3Backend::removeItem" );
  kDebug() << "(<item>)" << item->toPrintout();
  // check for write permissions
  if ( ! item->canEdit() )
    throw Exception ( Error(ERR_WRITE_ACCESS_DENIED),item->toPrintout() );
  G3Request::g3DelItem ( this, item->id() );
  G3Item* parent = item->parent();
  if ( parent )
  {
    // we delete the parent folder to create a fresh state
    QStringList breadcrumbs = parent->path();
    delete parent;
    parent = NULL;
    // now try to get the fresh item, this will re-construct the parent on-the-fly
    parent = itemByPath ( breadcrumbs );
    kDebug() << "deleted item in album" << parent->toPrintout();
  }
} // G3Backend::removeItem

/*!
 * G3Item* const G3Backend::updateItem ( G3Item* item, const QHash<QString,QString>& attributes )
 * @brief Updates any attributes of an item inside the remote Gallery3 system represented by the backend
 * @param item       pointer to item object
 * @param attributes list of altered item attributes
 * @return           pointer to updated item object
 * Updates an existing item on the remote server side by altering some of its attributes. 
 * Note that this list might include the parent item, so in fact it can move the item inside the hierarchy. 
 * @see G3Backend
 * @author Christian Reiner
 */
G3Item* const G3Backend::updateItem ( G3Item* item, const QHash<QString,QString>& attributes )
{
  KDebug::Block block ( "G3Backend::updateItem" );
  kDebug() << "(<item> <attributes[keys]>)" << item->toPrintout() << QStringList(attributes.keys()).join(QLatin1String(","));
  // check for write permissions
  if ( ! item->canEdit() )
    throw Exception ( Error(ERR_WRITE_ACCESS_DENIED),item->toPrintout() );
  G3Request::g3PutItem ( this, item->id(), attributes );
  // refresh old parent item
  G3Item* parent = item->parent ( );
  QStringList breadcrumbs = parent->path();
  delete parent;
  parent = NULL;
  parent = itemByPath ( breadcrumbs );
  // refresh new parent item, if this was a move
  if ( attributes.contains(QLatin1String("parent")) )
  {
    g3index id = QVariant(KUrl(attributes[QLatin1String("parent")]).fileName()).toInt();
    parent = itemById ( id );
    QStringList breadcrumbs = parent->path();
    delete parent;
    parent = NULL;
    parent = itemByPath ( breadcrumbs );
  } // if
  return item;
} // G3Backend::updateItem

/*!
 * G3Item* const G3Backend::createItem  ( G3Item* parent, const QString& name, const G3File* const file )
 * @brief Creates a new item inside the remote Gallery3 system represented by the backend
 * @param parent parent (pointer to item object
 * @param name   readable name the item is created as
 * @param file   pointer to a G3File describing a file to be uploaded
 * @return       pointer to created item object
 * Creates an item inside the remote gallery system, be it an album, a photo or a movie. 
 * This creation consists of the node created, some attributes for its description and. 
 * In a local file in case of a photo or movie item. 
 * @see G3Backend
 * @author Christian Reiner
 */
G3Item* const G3Backend::createItem  ( G3Item* parent, const QString& name, const G3File* const file )
{
  KDebug::Block block ( "G3Backend::createItem" );
  kDebug() << "(<parent> <name> <file[name]>)" << parent->toPrintout() << name << ( file ? file->filename() : "-/-" );
  // check for write permissions
  if ( ! parent->canEdit() )
    throw Exception ( Error(ERR_WRITE_ACCESS_DENIED),parent->toPrintout() );
  // setup the attributes that describe to new entity
  QHash<QString,QString> attributes;
  attributes.insert ( QLatin1String("name"),      name );
  attributes.insert ( QLatin1String("title"),     name.left(name.lastIndexOf(QLatin1String("."))) ); // strip "file name extension", if contained
  if ( file )
  {
    attributes.insert ( QLatin1String("type"),      G3Type(file->mimetype()).toString() );
    attributes.insert ( QLatin1String("mime_type"), file->mimetype()->name() );
  }
  else
  {
    attributes.insert ( QLatin1String("type"),      G3Type(G3Type::ALBUM).toString() );
    attributes.insert ( QLatin1String("mime_type"), QLatin1String("inode/directory") );
  }
  // send request
  G3Request::g3PostItem ( this, parent->id(), attributes, file );
  // we delete the parent folder to create a fresh start including parent and new member
  QStringList breadcrumbs = parent->path() << name;
  delete parent;
  parent = NULL;
  // now try to get the fresh item, this will re-construct the parent on-the-fly
  G3Item* item = itemByPath ( breadcrumbs );
  kDebug() << "created item" << item->toPrintout();
  return item;
} // G3Backend::createItem

/*!
 * void G3Backend::fetchFile ( G3Item* item )
 * @brief Retrieves an unresized file represented by an item inside the remote Gallery3 system represented by the backend
 * @param item item holding the requested file
 * Retrieves the file represented by the given item inside the remote Gallery3 system. 
 * @see G3Backend
 * @author Christian Reiner
 */
void G3Backend::fetchFile ( G3Item* item )
{
  KDebug::Block block ( "G3Backend::fetchFile" );
  kDebug() << "(<item>>)" << item->toPrintout();
  G3Request::g3FetchObject ( this, item->fileUrl(TRUE) );
} // G3Backend::fetchFile

/*!
 * void G3Backend::fetchResize ( G3Item* item )
 * @brief Retrieves a resized file represented by an item inside the remote Gallery3 system represented by the backend
 * @param item item holding the requested resize file
 * Retrieves a resize represented by the given item inside the remote Gallery3 system. 
 * @see G3Backend
 * @author Christian Reiner
 */
void G3Backend::fetchResize ( G3Item* item )
{
  KDebug::Block block ( "G3Backend::fetchResize" );
  kDebug() << "(<item>>)" << item->toPrintout();
  G3Request::g3FetchObject ( this, item->resizeUrl(TRUE) );
} // G3Backend::fetchResize

/*!
 * void G3Backend::fetchThumb ( G3Item* item )
 * @brief Retrieves a thumbnailed file represented by an item inside the remote Gallery3 system represented by the backend
 * @param item item holding the requested thumbnail file
 * Retrieves a thumbnail represented by the given item inside the remote Gallery3 system.
 * @see G3Backend
 * @author Christian Reiner
 */
void G3Backend::fetchThumb ( G3Item* item )
{  
  KDebug::Block block ( "G3Backend::fetchThumb" );
  kDebug() << "(<item>>)" << item->toPrintout();
  G3Request::g3FetchObject ( this, item->thumbUrl(TRUE) );
} // G3Backend::fetchThumb

/*!
 * void G3Backend::fetchCover ( G3Item* item )
 * @brief Retrieves a thumbnailed file represented by an item inside the remote Gallery3 system represented by the backend
 * @param item item holding the requested cover thumbnail
 * Retrieves a cover thumbnail represented by the given item inside the remote Gallery3 system.
 * @see G3Backend
 * @author Christian Reiner
 */
void G3Backend::fetchCover ( G3Item* item )
{
  KDebug::Block block ( "G3Backend::fetchCover" );
  kDebug() << "(<item>>)" << item->toPrintout();
  G3Request::g3FetchObject ( this, item->coverUrl(TRUE) );
} // G3Backend::fetchCover

#include "gallery3/g3_backend.moc"
