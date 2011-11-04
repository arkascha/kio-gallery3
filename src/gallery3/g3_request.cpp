/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 81 $
 * $Date: 2011-08-13 13:08:50 +0200 (Sat, 13 Aug 2011) $
 */

#include <QBuffer>
#include <QDataStream>
#include <QByteArray>
#include <krandom.h>
#include <kio/global.h>
#include <kio/netaccess.h>
#include <algorithm>
#include "utility/debug.h"
#include "utility/exception.h"
#include "gallery3/g3_request.h"

using namespace KIO;
using namespace KIO::Gallery3;

G3Request::G3Request ( G3Backend* const backend, KIO::HTTP_METHOD method, const QString& service, QIODevice* const file )
  : m_backend ( backend )
  , m_method  ( method )
  , m_service ( service )
  , m_payload ( NULL )
  , m_job     ( NULL )
  , m_file    ( file )
{
  MY_KDEBUG_BLOCK ( "<G3Request::G3Request>" );
  kDebug() << "(<backend> <method> <service>)" << backend->toPrintout() << method << service;
  m_requestUrl = backend->restUrl();
  m_requestUrl.adjustPath ( KUrl::AddTrailingSlash );
  m_requestUrl.addPath ( m_service );
  kDebug() << "{<>}";
} // G3Request::G3Request

void G3Request::addHeaderItem ( const QString& key, const QString& value )
{
  MY_KDEBUG_BLOCK ( "<G3Request::addHeaderItem>" );
  kDebug() << "(<key> <value>)" << key << value;
  if ( m_header.contains(key) )
    m_header.remove ( key ); // TODO: throw an error instead ?!?
  // TODO: some plausibility checks might be good here...
  m_header.insert ( key, value );
  kDebug() << "{<>}";
} // G3Request::addHeaderItem

void G3Request::addQueryItem ( const QString& key, const QString& value, bool skipIfEmpty )
{
  MY_KDEBUG_BLOCK ( "<G3Request::addQueryItem>" );
  kDebug() << "(<key> <value> <bool>)" << key << value << skipIfEmpty;
  if ( m_query.contains(key) )
    m_query.remove ( key ); // TODO: throw an error instead ?!?
  if ( value.isEmpty() )
    kDebug() << QString("skipping query item '%1'").arg(key);
  else
    m_query.insert ( key, value );
  kDebug() << "{<>}";
} // G3Request::addQueryItem

void G3Request::addQueryItem ( const QString& key, Entity::G3Type value, bool skipIfEmpty )
{
  MY_KDEBUG_BLOCK ( "<G3Request::addQueryItem>" );
  kDebug() << "(<key> <value> <bool>)" << key << value.toString() << skipIfEmpty;
  if ( m_query.contains(key) )
    m_query.remove ( key ); // TODO: throw an error instead ?!?
  if ( value==Entity::G3Type::NONE )
    kDebug() << QString("skipping query item '%1' holding 'NONE' as entity type").arg(key);
  else
  {
    kDebug() << value.toString();
    addQueryItem ( key, value.toString() );
  };
  kDebug() << "{<>}";
} // G3Request::addQueryItem

void G3Request::addQueryItem ( const QString& key, const QStringList& values, bool skipIfEmpty )
{
  MY_KDEBUG_BLOCK ( "<G3Request::addQueryItem>" );
  kDebug() << "(<key> <values [count]> <bool>)" << key << values.count() << skipIfEmpty;
  if ( 0==values.count() )
    kDebug() << "skipping query item holding an empty list of values";
  else
  {
    QVariantList items;
    foreach ( const QString& value, values )
      items << QVariant ( value );
    addQueryItem ( key, QString(g3serialize(items)) );
  }
  kDebug() << "{<>}";
} // G3Request::addQueryItem

KUrl G3Request::webUrlWithQueryItems ( KUrl url, const QHash<QString,QString>& query )
{
  MY_KDEBUG_BLOCK ( "<G3Request::webUrlWithQueryItems>" );
  kDebug() << "(<url> <query [count]>)" << url.prettyUrl() << query.count();
  QHash<QString,QString>::const_iterator it;
    for ( it=m_query.constBegin(); it!=m_query.constEnd(); it++ )
      url.addQueryItem ( it.key(), it.value() );
  kDebug() << "{<url>}" << url.prettyUrl();
  return url;
} // G3Request::webUrlWithQueryItems

QByteArray G3Request::webFormPostPayload ( const QHash<QString,QString>& query )
{
  MY_KDEBUG_BLOCK ( "<G3Request::webFormPostUpload>" );
  kDebug() << "(<query[count]>)" << query.count();
  QHash<QString,QString>::const_iterator it;
  QStringList queryItems;
  // construct an encoded form payload
  for ( it=m_query.constBegin(); it!=m_query.constEnd(); it++ )
    queryItems << QString("%1=%2").arg(it.key()).arg(it.value());
  QByteArray payload = queryItems.join("&").toUtf8();
  kDebug() << "{<payload[size]>}" << payload.size();
kDebug() << "#+++" <<  payload  << "+++#";
  return payload;
} // G3Request::webFormPostPayload

QByteArray G3Request::webFileFormPostPayload ( const QHash<QString,QString>& query, QIODevice* file )
{
  MY_KDEBUG_BLOCK ( "<G3Request::webFilePostUpload>" );
  kDebug() << "(<query> <file[size]>)" << query << ( file ? QString("%1").arg(file->size()) : "NULL" );
  QHash<QString,QString>::const_iterator it;
  QString     boundary;
  QStringList queryItems;
  QByteArray  postData;
  // construct a multi part form reply as payload
  boundary = "----------" + KRandom::randomString(42 + 13).toAscii();
  // add query attributes one-by-one
  postData = webFormPostPayload ( query );
/*  
      for ( it=m_query.constBegin(); it!=m_query.constEnd(); it++ )
      {
        postData << QString("--%1\r\nContent-Disposition: form-data; name=\"%2\"\r\n\r\n%3\r\n")
                            .arg(boundary).arg(it.key()).arg(it.value());
      } // for
*/
  // add file to be uploaded as part of the form
/*
      postData << QString("--%1\r\nContent-Disposition: form-data; name=\"%2\";filename=\"%3\"\r\nContent-Type: %4\r\n\r\n")
                         .arg(boundary).arg("name").arg("filename").arg(mimetype.name().toUtf8());
      postData << file_content;
      postData << "\r\n";
*/
  kDebug() << "{<postData[size]>}" << postData.size();
  return postData;
} // G3Request::webFileFormPostPayload

//==========

void G3Request::authenticate ( )
{
  MY_KDEBUG_BLOCK ( "<G3Request::authenticate>" );
  kDebug() << "(<>)";
  // TODO: add cached key (from AuthInfo), unless "is login" or "not cached"
  // TODO: somewhere else: upon "auth denied" remove key from cache and try again here
  // FIXME: THIS IS TEMPORARY: instead the key must be retrieved from either a config file or by an explicit login
  addHeaderItem ( "X-Gallery-Request-Key", "efc9547401bdd05625bee9dc49f3c52b" );
  /*
bool SlaveBase::openPasswordDialog  ( KIO::AuthInfo &   info,
                                      const QString &   errorMsg = QString() )
   */
  kDebug() << "{<>}";
}

void G3Request::setup ( )
{
  MY_KDEBUG_BLOCK ( "<G3Request::setup>" );
  kDebug() << "(<>)";
  // setup the actual http job
  switch ( m_method )
  {
    case KIO::HTTP_DELETE:
      m_job = KIO::http_post ( m_requestUrl, QByteArray(), KIO::DefaultFlags );
      addHeaderItem ( "content-type", "Content-Type: application/x-www-form-urlencoded" );
      addHeaderItem ( "X-Gallery-Request-Method", "DELETE" );
// FIXME: required ??      addHeaderItem ( "Content-Type", "application/x-www-form-urlencoded" );
      break;
    case KIO::HTTP_GET:
      m_job = KIO::get ( webUrlWithQueryItems(m_requestUrl,m_query), KIO::Reload, KIO::DefaultFlags );
      addHeaderItem ( "X-Gallery-Request-Method", "GET" );
      break;
    case KIO::HTTP_HEAD:
      m_job = KIO::get ( webUrlWithQueryItems(m_requestUrl,m_query), KIO::Reload, KIO::DefaultFlags );
      addHeaderItem ( "X-Gallery-Request-Method", "HEAD" );
      break;
    case KIO::HTTP_POST:
      if ( m_file )
        m_job = KIO::http_post ( m_requestUrl, webFileFormPostPayload(m_query,m_file), KIO::DefaultFlags );
      else
        m_job = KIO::http_post ( m_requestUrl, webFormPostPayload(m_query), KIO::DefaultFlags );
      addHeaderItem ( "content-type", "Content-Type: application/x-www-form-urlencoded" );
      addHeaderItem ( "X-Gallery-Request-Method", "POST" );
      // FIXME: below this should be something like multi part form, I guess
//      addHeaderItem ( "content-type", "Content-Type: application/x-www-form-urlencoded" );
      break;
    case KIO::HTTP_PUT:
      m_job = KIO::http_post ( m_requestUrl, webFormPostPayload(m_query), KIO::DefaultFlags );
      addHeaderItem ( "content-type", "Content-Type: application/x-www-form-urlencoded" );
      addHeaderItem ( "X-Gallery-Request-Method", "PUT" );
      break;
  } // switch request method
  // add header items if specified
  QHash<QString,QString>::const_iterator it;
  for ( it=m_header.constBegin(); it!=m_header.constEnd(); it++ )
    m_job->addMetaData ( it.key(), it.value() );
  kDebug() << "{<>}";
} // G3Request::setup

void G3Request::process ( )
{
  MY_KDEBUG_BLOCK ( "<G3Request::process>" );
  kDebug() << "(<>)";
  // run the job
  if ( ! NetAccess::synchronousRun ( m_job, 0, &m_payload, &m_finalUrl, &m_meta ) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("request failed: %2 [ %1 ]").arg(m_job->error()).arg(m_job->errorString()) );
  kDebug() << "sending request to url" << m_job->url();
  // check for success on protocol level
  if ( m_job->error() )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("Unexcepted processing error %1: %2").arg(m_job->error()).arg(m_job->errorString()) );
  switch ( QVariant(m_meta["responsecode"]).toInt() )
  {
    case 0:   break; // no error
    case 200: break; // http code for : all fine
    case 403: throw Exception ( Error(ERR_ACCESS_DENIED),         QString("403: No Authorization") );
    case 404: throw Exception ( Error(ERR_SERVICE_NOT_AVAILABLE), QString("404: Not Found") );
    default:  throw Exception ( Error(ERR_SLAVE_DEFINED),         QString("Unexcepted http error %1").arg(QVariant(m_meta["responsecode"]).toInt()) );
  } // switch
  kDebug() << "{<>}";
} // G3Request::process

void G3Request::evaluate ( )
{
  MY_KDEBUG_BLOCK ( "<G3Request::evaluate>" );
  kDebug() << "(<>)";
  // check for success on content level (headers and so on)
  if ( "200"!=m_meta["responsecode"] )
    throw Exception ( Error(ERR_SLAVE_DEFINED),QString("unexpected content type in response: %1").arg(m_meta["content-type"]) );
  kDebug() << QString ("request processed [ headers size: %2 / payload size: %1]")
                      .arg(m_meta.size())
                      .arg(m_payload.size());
  if ( "application/json"!=m_meta["content-type"] )
    throw Exception ( Error(ERR_SLAVE_DEFINED),QString("unexpected content type in response: %1").arg(m_meta["content-type"]) );
  kDebug() << QString("response has content type '%1'").arg(m_meta["content-type"]);
  // SUCCESS, convert result content (payload) into a usable object structure
  m_result = g3parse ( m_payload );
  kDebug() << "{<>}";
} // G3Request::process

G3Item* G3Request::toItem ( QVariant& entry )
{
  MY_KDEBUG_BLOCK ( "<G3Request::toItem>" );
  if ( ! entry.canConvert(QVariant::Map) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("gallery response did not hold a valid item description") );
  QVariantMap attributes = entry.toMap();
  G3Item* item = G3Item::instantiate ( m_backend, attributes );
  kDebug() << "{<item>}" << item->toPrintout();
  return item;
} // G3Request::toItem

QList<G3Item*> G3Request::toItems ( )
{
  MY_KDEBUG_BLOCK ( "<G3Request::toItems>" );
  kDebug() << "(<>)";
  QList<G3Item*> items;
  // expected result syntax ?
  if ( ! m_result.canConvert(QVariant::List) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("gallery response did not hold a valid list of item descriptions") );
  QList<QVariant> entries = m_result.toList();
  kDebug() << "result holds" << entries.count() << "entries";
  int i=0;
  foreach ( QVariant entry, entries )
  {
    try
    {
      kDebug() << "extracting entry" << ++i << "from response list";
      items << toItem ( entry );
    } // try
    catch ( Exception e )
    { // swallow exception
      kDebug() << "failed to extract item from gallery response:" << e.getText();
    } // catch
  } // foreach
  kDebug() << "extracted" << items.count() << "items from gallery response";
  kDebug() << "{<items[count]>}" << items.count();
  return items;
} // G3Request::toItems

g3index G3Request::toItemId ( QVariant& entry )
{
  MY_KDEBUG_BLOCK ( "<G3Request::toItemId>" );
  kDebug() << "(<>)";
  if ( ! entry.canConvert(QVariant::Map) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("gallery response did not hold a valid item description") );
  QVariantMap attributes = entry.toMap();
  // extract token 'id' from attribute 'entity' (IF it exists)
  if (    attributes.contains("entity")
       && attributes["entity"].canConvert(QVariant::Map) )
  {
    QMap<QString,QVariant> entity = attributes["entity"].toMap();
    if (    entity.contains("id")
         && entity["id"].canConvert(QVariant::Int) )
    {
      kDebug() << "{<id>}" << entity["id"].toInt();
      return entity["id"].toInt();
    }
    else
      throw Exception ( Error(ERR_INTERNAL), QString("gallery response did not hold a valid item description") );
  } // if
  else
    throw Exception ( Error(ERR_INTERNAL), QString("gallery response did not hold a valid item description") );
} // G3Request::toItemId

QList<g3index> G3Request::toItemIds ( )
{
  MY_KDEBUG_BLOCK ( "<G3Request::toItemIds>" );
  kDebug() << "(<>)";
  QList<g3index> ids;
  // expected result syntax ?
  if ( ! m_result.canConvert(QVariant::List) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("gallery response did not hold a valid list of item descriptions") );
  QList<QVariant> entries = m_result.toList();
  kDebug() << "result holds" << entries.count() << "entries";
  int i=0;
  foreach ( QVariant entry, entries )
  {
    try
    {
      kDebug() << "extracting entry" << ++i << "from response list";
      ids << toItemId ( entry );
    } // try
    catch ( Exception e )
    { // swallow exception
      kDebug() << "failed to extract item from gallery response:" << e.getText();
    } // catch
  } // foreach
  kDebug() << "extracted" << ids.count() << "items from gallery response";
  kDebug() << "{<ids[count]>}" << ids.count();
  return ids;
} // G3Request::toItemIds

//==========

bool G3Request::g3Check ( G3Backend* const backend )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3Check>" );
  kDebug() << "(<backend>)" << backend->toPrintout();
  try
  {
    G3Request request ( backend, KIO::HTTP_HEAD, "" );
    request.setup    ( );
    request.process  ( );
    kDebug() << "{<bool>} TRUE";
    return TRUE;
  } // try
  catch ( Exception e )
  {
    switch ( e.getCode() )
    {
      case Error(ERR_SERVICE_NOT_AVAILABLE):
        // we kind of expected this: we tried a guessed url and guess what: it does not exist!
        kDebug() << "{<bool>} FALSE";
        return FALSE;
      case Error(ERR_ACCESS_DENIED):
        // the 'rest-url' DOES exist, it typically returns a 403 when called without parameters
        kDebug() << "{<bool>} TRUE";
        return TRUE;
    } // switch
    // re-throw everything else
    throw e;
  } // catch
} // G3Request::g3Check

void G3Request::g3Login ( G3Backend* const backend, const AuthInfo& credentials )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3Login>" );
  kDebug() << "(<backend> <credentials>)" << backend->toPrintout() << credentials.username;
  G3Request request ( backend, KIO::HTTP_GET, "" );
  // special format as described in the API doc
  QByteArray payload = QString("user=%1&password=%2")
                              .arg(credentials.username)
                              .arg(credentials.password).toUtf8();
//  job->addHeaderItem ( "content-type", "quoted-printable" );
  request.setup   ( );
  request.process ( );
  request.evaluate ( );
  kDebug() << "{<>}";
  // TODO: store session key
} // G3Request::g3Login

QList<G3Item*> G3Request::g3GetItems ( G3Backend* const backend, const QStringList& urls, Entity::G3Type type )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3GetItems>" );
  kDebug() << "(<backend> <urls [count]> <type>)" << backend->toPrintout() << urls.count() << type.toString();
  QList<G3Item*> items;
  QStringList urls_chunk;
  int chunk=0;
  do
  {
    urls_chunk = urls.mid ( (chunk*ITEM_LIST_CHUNK_SIZE), ITEM_LIST_CHUNK_SIZE );
    kDebug() << QString("retrieving chunk %1 (items %2-%3)").arg(chunk+1).arg(chunk*ITEM_LIST_CHUNK_SIZE)
//                                                           .arg(MIN((((chunk+1)*ITEM_LIST_CHUNK_SIZE)-1),(urls.count()-1)));
                                                           .arg(std::min((((chunk+1)*ITEM_LIST_CHUNK_SIZE)-1),(urls.count()-1)));
    G3Request request ( backend, KIO::HTTP_GET, "items" );
    request.addQueryItem ( "urls", urls_chunk );
    request.addQueryItem ( "type", type );
    request.authenticate ( );
    request.setup        ( );
    request.process      ( );
    request.evaluate     ( );
    items << request.toItems ( );
  } while ( (++chunk*ITEM_LIST_CHUNK_SIZE)<urls.count() );
  kDebug() << "{<items [count]>}" << items.count();
  return items;
} // G3Request::g3GetItems

QList<G3Item*> G3Request::g3GetItems ( G3Backend* const backend, g3index id, Entity::G3Type type )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3GetItems>" );
  kDebug() << "(<backend> <id> <type>)" << backend->toPrintout() << id << type.toString();
  KUrl url = backend->restUrl ( );
  url.addPath ( QString("item/%1").arg(id) );
  return g3GetItems ( backend, QStringList(url.url()), type );
} // G3Request::g3GetItems

QList<g3index> G3Request::g3GetAncestors ( G3Backend* const backend, G3Item* item )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3GetAncestors>" );
  kDebug() << "(<backend> <item>)" << backend->toPrintout() << item->toPrintout();
  G3Request request ( backend, KIO::HTTP_GET, "items" );
  request.addQueryItem ( "ancestors_for", item->restUrl().url() );
  request.authenticate ( );
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
  QList<g3index> list = request.toItemIds ( );
  kDebug() << "{<items [count]>}" << list.count();
  return list;
} // G3Request::g3GetAncestors

g3index G3Request::g3GetAncestor ( G3Backend* const backend, G3Item* item )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3GetAncestor>" );
  kDebug() << "(<backend> <item>)" << backend->toPrintout() << item->toPrintout();
  QList<g3index> ancestors = g3GetAncestors ( backend, item );
  // second last one in the row should be the direct ancestor, the 'parent', IF it exists
  g3index parent;
  switch ( ancestors.count() )
  {
    case 0:
      throw Exception ( Error(ERR_INTERNAL), QString("requested item appears not to be part of its own ancestors list ?!?") );
    case 1:
      // no further entry in the ancestors list, except the item itself. Therefore there is NO parent item (NULL pinter)
      kDebug() << "item has no parent, this appears to be the base item";
      return 0;
    default:
      int pos = ancestors.size()-2; // second but last
      kDebug() << "items parent has id" << ancestors.at ( pos );
      return ancestors.at ( pos ); // last-but-one, the direct parent
  } // switch
} // G3Request::g3GetAncestor


G3Item* G3Request::g3GetItem ( G3Backend* const backend, g3index id, const QString& scope, const QString& name, bool random, Entity::G3Type type )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3GetItem>" );
  kDebug() << "(<backend> <id> <scope> <name> <random> <type>)" << backend->toPrintout() << scope << name << random << type.toString();
  G3Request request ( backend, KIO::HTTP_GET, QString("item/%1").arg(id) );
  request.addQueryItem ( "scope", scope );
  request.addQueryItem ( "name",  name, TRUE );
  request.addQueryItem ( "type",  type, TRUE );
  request.authenticate ( );
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
  G3Item* item = request.toItem ( );
  kDebug() << "{<item>}" << item->toPrintout();
  return item;
} // G3Request::g3GetItem

g3index G3Request::g3PostItem ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes, const KTemporaryFile* file )
{
  // TODO: implement file to post a file as item, if file is specified (not NULL)
  MY_KDEBUG_BLOCK ( "<G3Request::g3PostItem>" );
  kDebug() << "(<backend> <id> <attributes[count]> <file>)" << backend->toPrintout() << id << attributes.count()
                                                            << ( file ? file->fileName() : "NULL" );
  G3Request request ( backend, KIO::HTTP_POST, QString("item/%1").arg(id) );
  // add attributes as 'entity' structure
  QHash<QString,QVariant> entity;
  QHash<QString,QString>::const_iterator it;
  for ( it=attributes.constBegin(); it!=attributes.constEnd(); it++ )
    entity.insert ( it.key(), QVariant(it.value()) );
  // specify entity description as a single query attribute
kDebug() << "#***" << QVariant(entity) << "***#";
kDebug() << "#***" << request.g3serialize(QVariant(entity)) << "***#";
  request.addQueryItem ( "entity", request.g3serialize(QVariant(entity)) );
  // add file to be uploaded
  request.authenticate ( );
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
  g3index index = request.toItemId ( );
  kDebug() << "{<item[id]>}" << index;
  return index;
} // G3Request::g3PostItem

g3index G3Request::g3PutItem ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes, Entity::G3Type type )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3PutItem>" );
  kDebug() << "(<backend> <id> <attributes> <type>)" << backend->toPrintout() << attributes.count() << type.toString();
  G3Request request ( backend, KIO::HTTP_PUT, QString("item/%1").arg(id) );
  // add attributes one-by-one
  QHash<QString,QString>::const_iterator it;
  for ( it=attributes.constBegin(); it!=attributes.constEnd(); it++ )
    request.addQueryItem ( it.key(), it.value() );
  request.addQueryItem ( "type", type, TRUE );
  request.authenticate ( );
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
  g3index index = request.toItemId ( );
  kDebug() << "{<item[id]>}" << index;
  return index;
} // G3Request::g3PutItem

void G3Request::g3DelItem ( G3Backend* const backend, g3index id )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3DelItem>" );
  kDebug() << "(<backend> <id>)" << backend->toPrintout() << id;
  G3Request request ( backend, KIO::HTTP_DELETE, QString("item/%1").arg(id) );
  request.authenticate ( );
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
  kDebug() << "{<>}";
} // G3Request::g3DelItem

g3index G3Request::g3SetItem ( G3Backend* const backend, g3index id, const QString& name, Entity::G3Type type, const QByteArray& file )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3SetItem>" );
  kDebug() << "(<backend> <id> <name> <type> <file>)" << backend->toPrintout() << name << type.toString();
  G3Request request ( backend, KIO::HTTP_POST, QString("item/%1").arg(id) );
  request.addQueryItem ( "name",  name, TRUE );
  request.addQueryItem ( "type",  type, TRUE );
  request.authenticate ( );
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
  g3index index = request.toItemId ( );
  kDebug() << "{<item[id]>}" << index;
  return index;
} // G3Request::g3SetItem
