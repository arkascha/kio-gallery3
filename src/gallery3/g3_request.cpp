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

G3Request::G3Request ( G3Backend* const backend, KIO::HTTP_METHOD method, const QString& service, const Entity::G3File* const file )
  : m_backend ( backend )
  , m_method  ( method )
  , m_service ( service )
  , m_file    ( file )
  , m_payload ( NULL )
  , m_job     ( NULL )
{
  kDebug() << "(<backend> <method> <service> <file[name]>)" << backend->toPrintout() << method << service << ( file ? file->filename() : "-/-" );
  m_requestUrl = backend->restUrl();
  m_requestUrl.adjustPath ( KUrl::AddTrailingSlash );
  m_requestUrl.addPath ( m_service );
  m_boundary = KRandom::randomString(42 + 13).toAscii();
  // an (cached) authentication key as defined by the G3 API
  addHeaderItem ( "customHTTPHeader", QString("X-Gallery-Request-Key: %1" ).arg(g3RequestKey()) );
  // an agent string we can recognize
  addHeaderItem ( "UserAgent", QString("Mozilla/5.0 (X11; Linux x86_64) (KIO-gallery3) KDE/%1.%2.%3")
  // NOTE: opensuse uses a complete release string instead of the version for KDE_VERSION and KDE::versionString()
  //       so we construct a clean version string by hand:
                .arg(KDE::versionMajor()).arg(KDE::versionMinor()).arg(KDE::versionRelease()));
  kDebug() << "{<>}";
} // G3Request::G3Request

void G3Request::addHeaderItem ( const QString& key, const QString& value )
{
  kDebug() << "(<key> <value>)" << key << value;
  // add value to an existing header if one already exists, do NOT overwrite the existing one
  // we need this for the customHTTPHeaders as required by the G3 API
  QString content;
  if ( m_header.contains(key) )
    content = QString("%1\r\n%2").arg(m_header[key]).arg(value.trimmed());
  else
    content = value;
  // TODO: some plausibility checks might be good here...
  m_header.insert ( key, content );
  kDebug() << "{<>}";
} // G3Request::addHeaderItem

void G3Request::addQueryItem ( const QString& key, const QString& value, bool skipIfEmpty )
{
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
  kDebug() << "(<url> <query [count]>)" << url.prettyUrl() << query.count();
  QHash<QString,QString>::const_iterator it;
    for ( it=m_query.constBegin(); it!=m_query.constEnd(); it++ )
      url.addQueryItem ( it.key(), it.value() );
  kDebug() << "{<url>}" << url.prettyUrl();
  return url;
} // G3Request::webUrlWithQueryItems

QByteArray G3Request::webFormPostPayload ( const QHash<QString,QString>& query )
{
  kDebug() << "(<query[count]>)" << query.count();
  QHash<QString,QString>::const_iterator it;
  QStringList queryItems;
  // construct an encoded form payload
  for ( it=m_query.constBegin(); it!=m_query.constEnd(); it++ )
    queryItems << QString("%1=%2").arg(it.key()).arg(it.value());
  QByteArray buffer;
  buffer = queryItems.join("&").toAscii();
  kDebug() << "{<buffer[size]>}" << buffer.size();
  return buffer;
} // G3Request::webFormPostPayload

QByteArray G3Request::webFileFormPostPayload ( const QHash<QString,QString>& query, const Entity::G3File* const file )
{
  kDebug() << "(<query> <file[name]>)" << query << ( file ? file->filename() : "-/-" );
  // write the form data
  QFile binary ( file->filepath() );
  binary.open ( QIODevice::ReadOnly );
  QByteArray buffer;
  // one part for each query items
  for ( QHash<QString,QString>::const_iterator it=m_query.constBegin(); it!=m_query.constEnd(); it++ )
  {
    buffer.append ( QString("--%1\r\n").arg(m_boundary).toAscii() );
    buffer.append ( QString("Content-Disposition: form-data; name=\"%1\"\r\n").arg(it.key()).toAscii() );
    buffer.append ( QString("Content-Type: text/plain; charset=UTF-8\r\n").toAscii() );
    buffer.append ( QString("Content-Transfer-Encoding: 8bit\r\n\r\n").toAscii() );
    buffer.append ( it.value().toAscii() );
  } // for
  // the file to be uploaded
  buffer.append ( QString("\r\n--%1\r\n").arg(m_boundary).toAscii() );
  buffer.append ( QString("Content-Disposition: form-data; name=\"file\"; filename=\"%1\"\r\n").arg(file->filename()).toAscii() );
//  buffer.append ( QString("Content-Type: application/octet-stream\r\n").toAscii() );
  buffer.append ( QString("Content-Type: %1\r\n\r\n").arg(file->mimetype()->name()).toAscii() );
//  buffer.append ( QString("Content-Type: %1\r\n").arg(file->mimetype()->name()).toAscii() );
//  buffer.append ( QString("Content-Transfer-Encoding: binary\r\n\r\n").toAscii() );
  buffer.append ( binary.readAll() );
  // terminating boundary marker (note the trailing "--")
  buffer.append ( QString("\r\n--%1--").arg(m_boundary).toAscii() );
  binary.close ( );
  kDebug() << "{<buffer[size]>}" << buffer.size();
  return buffer;
} // G3Request::webFileFormPostPayload

//==========

const QString G3Request::g3RequestKey ( )
{
  kDebug() << "(<>)";
  // TODO: add cached key (from AuthInfo), unless "is login" or "not cached"
  // TODO: somewhere else: upon "auth denied" remove key from cache and try again here
  // FIXME: THIS IS TEMPORARY: instead the key must be retrieved from either a config file or by an explicit login
  return "efc9547401bdd05625bee9dc49f3c52b";
  /*
bool SlaveBase::openPasswordDialog  ( KIO::AuthInfo &   info,
                                      const QString &   errorMsg = QString() )
   */
} // G3Request::g3RequestKey

void G3Request::setup ( )
{
  MY_KDEBUG_BLOCK ( "<G3Request::setup>" );
  kDebug() << "(<>)";
  // setup the actual http job
  switch ( m_method )
  {
    case KIO::HTTP_DELETE:
      m_job = KIO::http_post ( m_requestUrl, QByteArray(), KIO::DefaultFlags );
      addHeaderItem ( "content-type",     "Content-Type: application/x-www-form-urlencoded" );
      addHeaderItem ( "customHTTPHeader", "X-Gallery-Request-Method: delete" );
// FIXME: required ??      addHeaderItem ( "Content-Type", "application/x-www-form-urlencoded" );
      break;
    case KIO::HTTP_GET:
      m_job = KIO::get ( webUrlWithQueryItems(m_requestUrl,m_query), KIO::Reload, KIO::DefaultFlags );
      addHeaderItem ( "customHTTPHeader", "X-Gallery-Request-Method: get" );
      break;
    case KIO::HTTP_HEAD:
      m_job = KIO::get ( webUrlWithQueryItems(m_requestUrl,m_query), KIO::Reload, KIO::DefaultFlags );
      addHeaderItem ( "customHTTPHeader", "X-Gallery-Request-Method: head" );
      break;
    case KIO::HTTP_POST:
      if ( m_file )
      {
        m_job = KIO::http_post ( m_requestUrl, webFileFormPostPayload(m_query,m_file), KIO::DefaultFlags );
        addHeaderItem ( "content-type", QString("Content-Type: multipart/form-data; boundary=%1").arg(m_boundary) );
      }
      else
      {
        m_job = KIO::http_post ( m_requestUrl, webFormPostPayload(m_query), KIO::DefaultFlags );
        addHeaderItem ( "content-type", "Content-Type: application/x-www-form-urlencoded" );
      }
      addHeaderItem ( "customHTTPHeader", "X-Gallery-Request-Method: post" );
      break;
    case KIO::HTTP_PUT:
      m_job = KIO::http_post ( m_requestUrl, webFormPostPayload(m_query), KIO::DefaultFlags );
      addHeaderItem ( "content-type", "Content-Type: application/x-www-form-urlencoded" );
      addHeaderItem ( "customHTTPHeader", "X-Gallery-Request-Method: put" );
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
  kDebug() << "sending request to url" << m_job->url();
  if ( ! NetAccess::synchronousRun ( m_job, 0, &m_payload, &m_finalUrl, &m_meta ) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("request failed: %2 [ %1 ]").arg(m_job->error()).arg(m_job->errorString()) );
  // check for success on protocol level
  if ( m_job->error() )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("Unexcepted processing error %1: %2").arg(m_job->error()).arg(m_job->errorString()) );
  switch ( QVariant(m_meta["responsecode"]).toInt() )
  {
    case 0:   break; // no error
    case 200: kDebug() << QString("HTTP %1 OK").arg(QVariant(m_meta["responsecode"]).toInt());       break;
    case 201: kDebug() << QString("HTTP %1 Created").arg(QVariant(m_meta["responsecode"]).toInt());  break;
    case 202: kDebug() << QString("HTTP %1 Accepted").arg(QVariant(m_meta["responsecode"]).toInt()); break;
    case 400: throw Exception ( Error(ERR_ACCESS_DENIED),         QString("HTTP 400: Bad Request") );
    case 401: throw Exception ( Error(ERR_ACCESS_DENIED),         QString("HTTP 401: Unauthorized") );
    case 403: throw Exception ( Error(ERR_ACCESS_DENIED),         QString("HTTP 403: No Authorization") );
    case 404: throw Exception ( Error(ERR_SERVICE_NOT_AVAILABLE), QString("HTTP 404: Not Found") );
    default:  throw Exception ( Error(ERR_SLAVE_DEFINED),         QString("Unexpected http error %1").arg(QVariant(m_meta["responsecode"]).toInt()) );
  } // switch
  kDebug() << "{<>}";
} // G3Request::process

void G3Request::evaluate ( )
{
  MY_KDEBUG_BLOCK ( "<G3Request::evaluate>" );
  kDebug() << "(<>)";
  // check for success on content level (headers and so on)
  if ( "2"!=m_meta["responsecode"].at(0) ) // 2XX is ok in general
    throw Exception ( Error(ERR_SLAVE_DEFINED),QString("unexpected response code in response: %1").arg(m_meta["responsecode"]) );
  kDebug() << QString ("request processed [ headers size: %2 / payload size: %1]")
                      .arg(m_meta.size())
                      .arg(m_payload.size());
  if ( "application/json"!=m_meta["content-type"] )
    throw Exception ( Error(ERR_SLAVE_DEFINED),QString("unexpected content type in response: %1").arg(m_meta["content-type"]) );
  kDebug() << QString("response has content type '%1'").arg(m_meta["content-type"]);
  // SUCCESS, convert result content (payload) into a usable object structure
  // NOTE: there is a bug in the G3 API implementation, it returns 'null' instead of an empty json structure in certain cases (DELETE)
  if ( "null"==m_payload )
    m_result = QVariant();
  else
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
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
  G3Item* item = request.toItem ( );
  kDebug() << "{<item>}" << item->toPrintout();
  return item;
} // G3Request::g3GetItem

void G3Request::g3PostItem ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes, const Entity::G3File* file )
{
  // TODO: implement file to post a file as item, if file is specified (not NULL)
  MY_KDEBUG_BLOCK ( "<G3Request::g3PostItem>" );
  kDebug() << "(<backend> <id> <attributes[count]> <file>)" << backend->toPrintout() << id << attributes.count()
                                                            << ( file ? file->filename() : "-/-" );
  G3Request request ( backend, KIO::HTTP_POST, QString("item/%1").arg(id), file );
  // add attributes as 'entity' structure
  QMap<QString,QVariant> entity; // QMap, since QJson silently fails to encode a QHash !
  QHash<QString,QString>::const_iterator it;
  for ( it=attributes.constBegin(); it!=attributes.constEnd(); it++ )
    entity.insert ( it.key(), QVariant(it.value()) );
  // specify entity description as a single, serialized query attribute
  request.addQueryItem ( "entity", request.g3serialize(QVariant(entity)) );
  // add file to be uploaded
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
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
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
  g3index index = request.toItemId ( );
  kDebug() << "{<item[id]>}" << index;
  return index;
} // G3Request::g3SetItem
