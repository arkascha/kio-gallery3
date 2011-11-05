/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

#include <klocalizedstring.h>
#include "utility/debug.h"
#include "utility/exception.h"
#include "gallery3/g3_backend.h"
#include "gallery3/g3_request.h"

using namespace KIO;
using namespace KIO::Gallery3;

/**
 * const KUrl G3Backend::detectBase ( const KUrl& url )
 *
 * detects and returns the position (url) of the G3-API based on any given
 * strategy: the REST API must be some base folder of the requested url
 *           so we test each breadcrump one by one (shortening the url) until we find the service
 * note: this appears horrible, but there are two reasons for this:
 * 1.) our slave might be re-used to access more than one single gallery
 * 2.) there might be more than one gallery sharing the same start url
 */
G3Backend* const G3Backend::instantiate ( QHash<QString,G3Backend*>& backends, KUrl g3Url )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::instantiate>" );
  kDebug() << "(<url>)" << g3Url.prettyUrl();
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
    backend = new G3Backend ( g3Url );
    if ( G3Request::g3Check ( backend ) )
    {
      kDebug() << QString("detected existing G3-API url '%1', created fresh backend").arg(backend->restUrl().prettyUrl());
      backends.insert ( backend->baseUrl().url(), backend );
      return backend;
    }
    // no success, iterate 'downwards'
    kDebug() << "no G3-API available at url" << g3Url.prettyUrl();
    delete backend;
    g3Url.setPath ( g3Url.directory() );
  }
  while ( ! g3Url.path().isEmpty() );
  // REST API _not_ detected
  throw Exception ( Error(ERR_SLAVE_DEFINED),
                    QString("No usable G3-API service found") );
} // G3Backend::instantiate

G3Backend::G3Backend ( const KUrl& g3Url )
  : m_base_url   ( g3Url.url(KUrl::RemoveTrailingSlash) )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::G3Backend>" );
  m_rest_url = m_base_url;
  m_rest_url.setProtocol ( ("gallery3s"==m_base_url.protocol()) ? "https":"http" );
  m_rest_url.addPath     ( "rest" );
  kDebug() << "{<base> <rest>}" << m_base_url.prettyUrl() << m_rest_url.prettyUrl();
}

G3Backend::~G3Backend ( )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::~G3Backend>" );
  kDebug() << "(<>)";
  // remove all items generated on-the-fly
  kDebug() << "deleting member items first";
  delete itemBase ( );
  // remove any left-over items (stale)
  kDebug() << m_items.count() << "stale items left, deleting them";
  QHash<g3index,G3Item*>::const_iterator item;
  for ( item=m_items.constBegin(); item!=m_items.constEnd(); item++ )
    delete item.value();
}

//==========

const UDSEntry G3Backend::toUDSEntry ( )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::toUDSEntry>" );
  kDebug() << "(<>)";
  return itemBase()->toUDSEntry();
} // G3Backend::toUDSEntry

const UDSEntryList G3Backend::toUDSEntryList ( )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::toUDSEntryList>" );
  kDebug() << "(<>)";
  UDSEntryList list;
  list << itemBase()->toUDSEntry();
  return list;
} // G3Backend::toUDSEntryList

const QString G3Backend::toPrintout ( ) const
{
  return QString("G3Backend [%1 items] (%2)").arg(m_items.count()).arg(m_base_url.prettyUrl());
} // G3Backend::toPrintout

//==========

G3Item* G3Backend::itemBase ( )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::itemBase>" );
  kDebug() << "(<>)";
  return itemById ( 1 );
} // G3Backend::itemBase

G3Item* G3Backend::itemById ( g3index id )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::itemById>" );
  kDebug() << "(<id>)" << id;
  if ( m_items.contains(id) )
    return m_items[id];
  // item not yet known, try to request it from the gallery server
  return item ( id );
} // G3Backend::itemById

G3Item* G3Backend::itemByUrl ( const KUrl& itemUrl )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::itemByUrl>" );
  kDebug() << "(<url>)" << itemUrl.prettyUrl();
  const QString itemPath = KUrl::relativeUrl ( m_base_url, itemUrl );
  return itemByPath ( itemPath );
} // G3Backend::itemByUrl

G3Item* G3Backend::itemByPath ( const QString& path )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::itemByPath>" );
  kDebug() << "(<path>)" << path;
  QStringList breadcrumbs = path.split("/");
  return itemByPath ( breadcrumbs );
} // G3Backend::itemByPath

G3Item* G3Backend::itemByPath ( const QStringList& breadcrumbs )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::itemByPath>" );
  kDebug() << "(<breadcrumbs>)"<< breadcrumbs.join("|");
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

QList<G3Item*> G3Backend::membersByItemId ( g3index id )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::membersByItemId>" );
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

QList<G3Item*> G3Backend::membersByItemPath ( const QString& path )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::membersByItemPath>" );
  kDebug() << "(<path>)" << path;
  if ( "./"==path )
    return membersByItemPath ( QStringList() );
  else
    return membersByItemPath ( path.split("/") );
} // G3Backend::membersByItemPath

QList<G3Item*> G3Backend::membersByItemPath ( const QStringList& breadcrumbs )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::membersByItemPath>" );
  kDebug() << "(<breadcrumbs>)" << breadcrumbs.join("|");
  G3Item* parent = itemByPath ( breadcrumbs );
  QList<G3Item*> items = G3Request::g3GetItems ( this, QStringList(QString("%1/item/%2").arg(m_rest_url.url()).arg(parent->id())) );
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

G3Item* G3Backend::item ( g3index id )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::item>" );
  kDebug() << "(<id>)" << id;
kDebug() << "XOXOX" << m_items.keys()  << "XOXOX";
  if ( m_items.contains(id) )
    return m_items[id];
  // item not found, retrieve it from the remote gallery
  G3Item* item = G3Request::g3GetItem ( this, id );
  kDebug() << "{<item>}" << item->toPrintout();
  return item;
} // G3Backend::item

QHash<g3index,G3Item*> G3Backend::members ( g3index id )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::members>" );
  kDebug() << "(<id>)" << id;
  itemById(id)->members ( );
} // G3Backend::members

QHash<g3index,G3Item*> G3Backend::members ( G3Item* item )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::members>" );
  kDebug() << "(<item>)" << item->toPrintout();
  return item->members ( );
} // G3Backend::members

//==========

int G3Backend::countItems  ( )
{
  kDebug() << "(<>)";
  return m_items.count();
} // G3Backend::countItems

void G3Backend::pushItem ( G3Item* item )
{
  kDebug() << "(<item>)" << item->toPrintout();
  m_items.insert ( item->id(), item );
} // G3Backend::pushItem

G3Item* G3Backend::popItem ( g3index id )
{
  kDebug() << "(<id>)" << id;
  if ( m_items.contains(id) )
    return m_items.take ( id );
  throw Exception ( Error(ERR_INTERNAL), QString("attempt to remove non-existing item with id '%1'").arg(id) );
} // G3Backend::popItem

void G3Backend::removeItem ( G3Item* item )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::removeItem>" );
  kDebug() << "(<item>)" << item->toPrintout();
  G3Request::g3DelItem ( this, item->id() );
  G3Item* parent = item->parent();
  if ( parent )
  {
    // we delete the parent folder to create a fresh state
    QStringList breadcrumbs = parent->path();
    delete parent;
    // now try to get the fresh item, this will re-construct the parent on-the-fly
    parent = itemByPath ( breadcrumbs );
    kDebug() << "deleted item in album" << parent->toPrintout();
  }
} // G3Backend::removeItem

void G3Backend::updateItem ( G3Item* item, QHash<QString,QString>& attributes )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::updateItem>" );
  kDebug() << "(<item> <attributes>)" << item->toPrintout() << QStringList(attributes.keys()).join(",");
  G3Request::g3PutItem ( this, item->id(), item->toAttributes(), item->type() );
} // G3Backend::updateItem

G3Item* const G3Backend::createItem  ( const KTemporaryFile& file, G3Item* parent, Entity::G3Type type, const QString& name, QString title )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::createItem>" );
  kDebug() << "(<file> <parent> <type> <name> <title>)" << "-/-" << parent->toPrintout() << type.toString() << name << title;
  if ( title.isEmpty() )
    title = name;
  QHash<QString,QString> attributes;
  attributes.insert ( "type",  type.toString() );
  attributes.insert ( "name",  name );
  attributes.insert ( "title", title );
  G3Request::g3PostItem ( this, parent->id(), attributes, &file );
  // we delete the parent folder to create a fresh start including parent and new member
  QStringList breadcrumbs = parent->path() << name;
  delete parent;
  // now try to get the fresh item, this will re-construct the parent on-the-fly
  G3Item* item = itemByPath ( breadcrumbs );
  kDebug() << "created item" << item->toPrintout();
  return item;
} // G3Backend::createItem

G3Item* const G3Backend::createAlbum ( G3Item* parent, const QString& name, QString title )
{
  MY_KDEBUG_BLOCK ( "<G3Backend::createAlbum>" );
  kDebug() << "(<parent> <name> <title>)" << parent->toPrintout() << name << title;
  if ( title.isEmpty() )
    title = name;
  QHash<QString,QString> attributes;
//  attributes.insert ( "type",  "album" );
  Entity::G3Type type(Entity::G3Type::ALBUM);
  attributes.insert ( "type",  type.toString() );
  attributes.insert ( "name",  name );
  attributes.insert ( "title", title );
  // TODO: further attributes ? => permissions
  G3Request::g3PostItem ( this, parent->id(), attributes );
  // we delete the parent folder to create a fresh start including parent and new member
  QStringList breadcrumbs = parent->path() << name;
  delete parent;
  // now try to get the fresh item, this will re-construct the parent on-the-fly
  G3Item* item = itemByPath ( breadcrumbs );
  kDebug() << "created item" << item->toPrintout();
  return item;
} // G3Backend::createAlbum
