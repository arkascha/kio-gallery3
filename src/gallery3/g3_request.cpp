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

/**
 * G3Request::G3Request ( G3Backend* const backend, KIO::HTTP_METHOD method, const QString& service, const Entity::G3File* const file )
 *
 * param: G3Backend* const backend (backend associated to the requested g3 system)
 * param: KIO::HTTP_METHOD method (http method to be used)
 * param: const QString& service (service to be called, relative to the REST url)
 * param: const Entity::G3File* file (internal file ressource description, might hold a file for upload)
 * description:
 * offers a generic object holding all aspects and data required while processing a http request to the remote gallery3 system
 * some members are initialized, some basic settings done and a connection to the controlling slave base is established for authentication purposes
 */
G3Request::G3Request ( G3Backend* const backend, KIO::HTTP_METHOD method, const QString& service, const Entity::G3File* const file )
  : m_backend ( backend )
  , m_method  ( method )
  , m_service ( service )
  , m_file    ( file )
  , m_payload ( NULL )
  , m_result  ( QVariant() )
  , m_meta    ( QMap<QString,QString>() )
  , m_query   ( QHash<QString,QString>() )
  , m_status  ( 0 )
  , m_job     ( NULL )
{
  kDebug() << "(<backend> <method> <service> <file[name]>)" << backend->toPrintout() << method << service << ( file ? file->filename() : "-/-" );
  m_requestUrl = backend->restUrl();
  m_requestUrl.adjustPath ( KUrl::AddTrailingSlash );
  m_requestUrl.addPath ( m_service );
  m_boundary = KRandom::randomString(42+13).toAscii();
  // an agent string we can recognize
  addHeaderItem ( QLatin1String("User-Agent"), QString("kio-gallery3 (X11; Linux x86_64) KDE/%1.%2.%3")
  // NOTE: opensuse uses a complete release string instead of the version for KDE_VERSION and KDE::versionString()
  //       so we construct a clean version string by hand:
                .arg(KDE::versionMajor()).arg(KDE::versionMinor()).arg(KDE::versionRelease()) );
  kDebug() << "{<>}";
  // prepare authentication requests
  connect ( this,              SIGNAL(signalRequestAuthInfo(G3Backend*,AuthInfo&,int)),
            backend->parent(), SLOT(slotRequestAuthInfo(G3Backend*,AuthInfo&,int)) );
} // G3Request::G3Request


G3Request::~G3Request ( )
{
  kDebug();
  if ( m_job )
  {
    kDebug() << "deleting background job";
    // FIXME: deleting the job after it has been executed reproduceably crashes the slave with a segfault
    // delete m_job;
  }
} // G3Request::~G3Request

//==========

/**
 * int G3Request::httpStatusCode ( )
 *
 * returns: int ( http status code as defined by the RFCs )
 * exception: ERR_SLAVE_DEFINED ( in case no valid http status code could be extracted )
 * description:
 * extracts a standard http status code from any request result after processing a job
 * note that a job may well return without error when being run, since those errors only refer to processing problems
 * http status codes refer to the protocol and content level, this is not handled by the method return value
 */
int G3Request::httpStatusCode ( )
{
  QVariant httpStatusCode = QVariant(m_meta[QLatin1String("responsecode")]);
  if ( httpStatusCode.canConvert(QVariant::Int) )
  {
    int httpStatus = QVariant(m_meta[QLatin1String("responsecode")]).toInt();
    kDebug() << httpStatus;
    return httpStatus;
  }
  else
    throw Exception ( Error(ERR_SLAVE_DEFINED), QString("No http status provided in response") );
} // G3Request::httpStatusCode


bool G3Request::retryWithChangedCredentials ( int attempt )
{
  kDebug();
  emit signalRequestAuthInfo ( m_backend, m_backend->credentials(), attempt );
  kDebug() << ( m_backend->credentials().isModified() ? "credentials changed" : "credentials unchanged" );
  return m_backend->credentials().isModified();
} // G3Request::retryWithChangedCredentials

/**
 * void G3Request::addHeaderItem ( const QString& key, const QString& value )
 *
 * param: const QString& key ( http header key )
 * param: const QString& value ( http header value )
 * description:
 * adds a single key/value pair to the header item collection in the request object
 * each pair collected by calls to this method will be specified as http header entries later during job setup
 */
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

/**
 * void G3Request::addQueryItem ( const QString& key, const QString& value, bool skipIfEmpty )
 *
 * param: const QString& key ( http query item key )
 * param: const QString& value ( http query item value )
 * param: bool skipIfEmpty ( controls if this item will be silently skipped in case of an empty value )
 * escription:
 * adds a single key/value pair to the query item collection in the request object
 * each pair collected by calls to this method will be added as query item later during job setup
 * note, that these items will be specified as part of the request url or its form body, depending on the request method
 */
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

/**
 * void G3Request::addQueryItem ( const QString& key, Entity::G3Type value, bool skipIfEmpty )
 *
 * param: const QString& key ( http query item key )
 * param: Entity::G3Type value ( http query item value )
 * param: bool skipIfEmpty ( controls if this item will be silently skipped in case of an empty value )
 * escription:
 * adds a single key/value pair to the query item collection in the request object
 * each pair collected by calls to this method will be added as query item later during job setup
 * note, that these items will be specified as part of the request url or its form body, depending on the request method
 */
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

/**
 * void G3Request::addQueryItem ( const QString& key, const QStringList& values, bool skipIfEmpty )
 *
 * param: const QString& key ( http query item key )
 * param: const QStringList& values ( list of http query item values )
 * param: bool skipIfEmpty ( controls if this item will be silently skipped in case of an empty value )
 * escription:
 * adds a list of key/value pairs to the query item collection in the request object
 * all values will be added in form of a multi-line value as a single item identified by the key (http protocol standard)
 * each pair collected by calls to this method will be added as query item later during job setup
 * note, that these items will be specified as part of the request url or its form body, depending on the request method
 */
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

//==========

/**
 * KUrl G3Request::webUrlWithQueryItems ( KUrl url, const QHash<QString,QString>& query )
 *
 * param: KUrl url ( plain request url )
 * param: const QHash<QString,QString>& query ( collection of query items )
 * return: KUrl ( final request url including the query part )
 * description:
 * constructs the final url to be requested in case of a http get or head method
 * this is the plain request url enriched by all query items being specified in the part of the url
 */
KUrl G3Request::webUrlWithQueryItems ( KUrl url, const QHash<QString,QString>& query )
{
  kDebug() << "(<url> <query [count]>)" << url << query.count();
    for ( QHash<QString,QString>::const_iterator it=m_query.constBegin(); it!=m_query.constEnd(); it++ )
      url.addQueryItem ( it.key(), it.value() );
  kDebug() << "{<url>}" << url;
  return url;
} // G3Request::webUrlWithQueryItems

/**
 * QByteArray G3Request::webFormPostPayload ( const QHash<QString,QString>& query )
 *
 * param: const QHash<QString,QString>& queryItems
 * return: QByteArray ( http post body )
 * description:
 * constructs a complete http post body holding the content of a html form to be sent to a http server
 */
QByteArray G3Request::webFormPostPayload ( const QHash<QString,QString>& query )
{
  kDebug() << "(<query[count]>)" << query.count();
  QStringList queryItems;
  // construct an encoded form payload
  for ( QHash<QString,QString>::const_iterator it=m_query.constBegin(); it!=m_query.constEnd(); it++ )
    queryItems << QString("%1=%2").arg(it.key()).arg(it.value());
  QByteArray buffer;
  buffer = queryItems.join("&").toAscii();
  kDebug() << "{<buffer[size]>}" << buffer.size();
  return buffer;
} // G3Request::webFormPostPayload

/**
 * QByteArray G3Request::webFileFormPostPayload ( const QHash<QString,QString>& query, const Entity::G3File* const file )
 *
 * param: const QHash<QString,QString>& query ( set of query items to be sent inside the request )
 * param: const Entity::G3File* const file ( internal file description specifying a file to be uploaded during the request )
 * return: QByteArray ( http post body )
 * description:
 * constructs a complete http post body holding multi-part form to be sent to a http server
 * this for includes several query items as separate parts and one final file
 */
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

/**
 * void G3Request::setup ( )
 *
 * description:
 * constructs a KIO::TransferJob as required for the specific request to the remote gallery3 system
 * the job is enriched with all query items, files and header entries as required for the final processing of the job
 */ 
void G3Request::setup ( )
{
  MY_KDEBUG_BLOCK ( "<G3Request::setup>" );
  kDebug() << "(<>)";
  // reset / initialize the members
  m_header.clear();
  m_meta.clear();
  m_payload = NULL;
  m_result  = QVariant();
  m_status  = 0;
  // G3 uses 'RemoteAccesKeys' for authentication purposes (see API documentation)
  // this key is locally stored by this slave, we specify it if it exists
  if ( ! m_backend->credentials().digestInfo.isEmpty() )
    addHeaderItem ( QLatin1String("customHTTPHeader"), QString("X-Gallery-Request-Key: %1" ).arg(m_backend->credentials().digestInfo) );
  // setup the actual http job
  switch ( m_method )
  {
    case KIO::HTTP_DELETE:
      m_job = KIO::http_post ( m_requestUrl, QByteArray(), KIO::DefaultFlags );
      addHeaderItem ( QLatin1String("content-type"),     QLatin1String("Content-Type: application/x-www-form-urlencoded") );
      addHeaderItem ( QLatin1String("customHTTPHeader"), QLatin1String("X-Gallery-Request-Method: delete") );
      break;
    case KIO::HTTP_GET:
      m_job = KIO::get ( webUrlWithQueryItems(m_requestUrl,m_query), KIO::Reload, KIO::DefaultFlags );
      addHeaderItem ( QLatin1String("customHTTPHeader"), QLatin1String("X-Gallery-Request-Method: get") );
      break;
    case KIO::HTTP_HEAD:
//      m_job = KIO::get ( webUrlWithQueryItems(m_requestUrl,m_query), KIO::Reload, KIO::DefaultFlags );
      m_job = KIO::mimetype ( webUrlWithQueryItems(m_requestUrl,m_query), KIO::DefaultFlags );
      addHeaderItem ( QLatin1String("customHTTPHeader"), QLatin1String("X-Gallery-Request-Method: head") );
      break;
    case KIO::HTTP_POST:
      if ( m_file )
      {
        m_job = KIO::http_post ( m_requestUrl, webFileFormPostPayload(m_query,m_file), KIO::DefaultFlags );
        addHeaderItem ( QLatin1String("content-type"), QString("Content-Type: multipart/form-data; boundary=%1").arg(m_boundary) );
      }
      else
      {
        m_job = KIO::http_post ( m_requestUrl, webFormPostPayload(m_query), KIO::DefaultFlags );
        addHeaderItem ( QLatin1String("content-type"), QLatin1String("Content-Type: application/x-www-form-urlencoded") );
      }
      addHeaderItem ( QLatin1String("customHTTPHeader"), QLatin1String("X-Gallery-Request-Method: post") );
      break;
    case KIO::HTTP_PUT:
      m_job = KIO::http_post ( m_requestUrl, webFormPostPayload(m_query), KIO::DefaultFlags );
      addHeaderItem ( QLatin1String("content-type"), QLatin1String("Content-Type: application/x-www-form-urlencoded") );
      addHeaderItem ( QLatin1String("customHTTPHeader"), QLatin1String("X-Gallery-Request-Method: put") );
      break;
  } // switch request method
  // add header items if specified
  QHash<QString,QString>::const_iterator it;
  for ( it=m_header.constBegin(); it!=m_header.constEnd(); it++ )
    m_job->addMetaData ( it.key(), it.value() );
  kDebug() << "{<>}";
} // G3Request::setup

/**
 * void G3Request::process ( )
 *
 * exception: ERR_SLAVE_DEFINED ( in case of a failure on method level (NOT protocol level) )
 * description:
 * processes a jbo that has been setup completely before
 * attempts to retry the job after requesting authentication information in case of a http-403 from the server
 */
void G3Request::process ( )
{
  MY_KDEBUG_BLOCK ( "<G3Request::process>" );
  kDebug() << "(<>)";
  // prepare handling of authentication info
  // run the job
  kDebug() << "sending request to url" << m_job->url();
  int attempt = 0;
  do
  {
    // if status is 403 this is a retry after a failed attempt
    // we simply construct a fresh job by calling setup again...
    // TODO: required at all ? or can a job simply be run several times ?
    if ( 403==m_status )
    {
      kDebug() << "resetting job for a new trial";
      setup ( );
    }
    if ( ! NetAccess::synchronousRun ( m_job, NULL, &m_payload, &m_finalUrl, &m_meta ) )
      throw Exception ( Error(ERR_SLAVE_DEFINED),
                        QString("request failed: %2 [ %1 ]").arg(m_job->error()).arg(m_job->errorString()) );
    // check for problems on protocol level
    if ( m_job->error() )
      throw Exception ( Error(ERR_SLAVE_DEFINED),
                        QString("Unexcepted processing error %1: %2").arg(m_job->error()).arg(m_job->errorString()) );
    // extract and store http status code from reply
    m_status = httpStatusCode();
  } while (    (m_job->url().fileName()!=QLatin1String("rest")) // exception: g3Check: looking for REST API
            && (403==m_status)                   // repeat only in this case
//            && retryWithChangedCredentials(++attempt) );  // retry makes sense if credentials have changed
            && retryWithChangedCredentials(attempt) );  // retry makes sense if credentials have changed
  kDebug() << "{<>}"; 
} // G3Request::process

/**
 * void G3Request::evaluate ( )
 *
 * exception: ERR_SLAVE_DEFINED ( in case of unexpected content syntax, that is non-json-encoded content )
 * description:
 * evaluates the received result payload of a preceding processed job on a generic level
 * provides the json-parsed payload in form of a QVariant
 */
void G3Request::evaluate ( )
{
  MY_KDEBUG_BLOCK ( "<G3Request::evaluate>" );
  kDebug() << "(<>)";
  // check for success on protocol & content level (headers and so on)
  switch ( m_status )
  {
    case 0:   break; // no error
    case 200: kDebug() << QString("HTTP %1 OK"                           ).arg(QVariant(m_meta[QLatin1String("responsecode")]).toInt()); break;
    case 201: kDebug() << QString("HTTP %1 Created"                      ).arg(QVariant(m_meta[QLatin1String("responsecode")]).toInt()); break;
    case 202: kDebug() << QString("HTTP %1 Accepted"                     ).arg(QVariant(m_meta[QLatin1String("responsecode")]).toInt()); break;
    case 203: kDebug() << QString("HTTP %1 Non-Authoritative Information").arg(QVariant(m_meta[QLatin1String("responsecode")]).toInt()); break;
    case 204: kDebug() << QString("HTTP %1 No Content"                   ).arg(QVariant(m_meta[QLatin1String("responsecode")]).toInt()); break;
    case 205: kDebug() << QString("HTTP %1 Reset Content"                ).arg(QVariant(m_meta[QLatin1String("responsecode")]).toInt()); break;
    case 206: kDebug() << QString("HTTP %1 Partial Content"              ).arg(QVariant(m_meta[QLatin1String("responsecode")]).toInt()); break;
    case 400: throw Exception ( Error(ERR_INTERNAL_SERVER),        QString("HTTP 400: Bad Request") );
    case 401: throw Exception ( Error(ERR_ACCESS_DENIED),          QString("HTTP 401: Unauthorized") );
    case 403: throw Exception ( Error(ERR_COULD_NOT_AUTHENTICATE), QString("HTTP 403: No Authorization") ); // login failed
    case 404: throw Exception ( Error(ERR_SERVICE_NOT_AVAILABLE),  QString("HTTP 404: Not Found") );
    default:  throw Exception ( Error(ERR_SLAVE_DEFINED),          QString("Unexpected http error %1").arg(QVariant(m_meta[QLatin1String("responsecode")]).toInt()) );
  } // switch
  kDebug() << QString ("request processed [ headers size: %2 / payload size: %1]")
                      .arg(m_meta.size())
                      .arg(m_payload.size());
  if ( QLatin1String("application/json")!=m_meta[QLatin1String("content-type")] )
    throw Exception ( Error(ERR_SLAVE_DEFINED),QString("unexpected content type in response: %1").arg(m_meta[QLatin1String("content-type")]) );
  kDebug() << QString("response has expected content type '%1'").arg(m_meta[QLatin1String("content-type")]);
  // SUCCESS, convert result content (payload) into a usable object structure
  // NOTE: there is a bug in the G3 API implementation, it returns 'null' instead of an empty json structure in certain cases (DELETE)
  if ( "null"==m_payload )
    // gallery3 sends the literal string 'null' when serializing an empty set or object
    m_result = QVariant();
  else if ( '"'==m_payload.trimmed()[0] )
    // QJson crashes when decoding a simple json encoded string ("string")
    // workaround: we wrap and dewrap it as a single array element
    m_result = g3parse(QString("["+m_payload+"]").toUtf8()).toList().first();
  else
    m_result = g3parse ( m_payload );
  kDebug() << "{<>}";
} // G3Request::process

//==========

/**
 * QString G3Request::toString ( )
 *
 * returns: QString ( the string as extracted from the result payload )
 * exception: ERR_SLAVE_DEFINED ( in case the payload did not carry a plain string as expected )
 * description: 
 * converts the request result payload into a valid QString
 */
QString G3Request::toString ( )
{
  if ( ! m_result.canConvert(QVariant::String) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("gallery response did not hold a valid remote access key") );
  QString string = m_result.toString();
  kDebug() << "{<string>}" << string;
  return string;
} // G3Request::toString

/**
 * G3Item* G3Request::toItem ( QVariant& entry )
 *
 * param: QVariant& entry ( a single entry as extracted from a result payload holding multiple entries )
 * returns: G3Item* ( pointer to an internal item object )
 * exception: ERR_SLAVE_DEFINED ( in case the entry does not hold a valid item description as expected )
 * description:
 * converts the specified entry into an internal G3Item object
 */
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

/**
 * QList<G3Item*> G3Request::toItems ( )
 *
 * returns: QList<G3Item*> ( list of pointers to item objects )
 * exception: ERR_SLAVE_DEFINED ( in case the result payload did not hold a list of entries as expected )
 * description:
 * converts the result payload into a list of internal G3Item objects
 */
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

/**
 * g3index G3Request::toItemId ( QVariant& entry )
 *
 * param: QVariant& entry
 * returns: g3index ( numeric item id )
 * exception: ERR_SLAVE_DEFINED ( in case the result payload does not hold a valid item id as expected )
 * description:
 * converts the result payload into a valid internal numerical G3Item id
 */
g3index G3Request::toItemId ( QVariant& entry )
{
  MY_KDEBUG_BLOCK ( "<G3Request::toItemId>" );
  kDebug() << "(<entry>)";
  if ( ! entry.canConvert(QVariant::Map) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("gallery response did not hold a valid item description") );
  QVariantMap attributes = entry.toMap();
  // extract token 'id' from attribute 'entity' (IF it exists)
  if (    attributes.contains(QLatin1String("entity"))
       && attributes[QLatin1String("entity")].canConvert(QVariant::Map) )
  {
    QMap<QString,QVariant> entity = attributes["entity"].toMap();
    if (    entity.contains("id")
         && entity[QLatin1String("id")].canConvert(QVariant::Int) )
    {
      kDebug() << "{<id>}" << entity["id"].toInt();
      return entity[QLatin1String("id")].toInt();
    }
    else
      throw Exception ( Error(ERR_INTERNAL), QString("gallery response did not hold a valid item description") );
  } // if
  else
    throw Exception ( Error(ERR_INTERNAL), QString("gallery response did not hold valid return content") );
} // G3Request::toItemId

//==========

/**
 * QList<g3index> G3Request::toItemIds ( )
 *
 * returns: QList<g3index> ( list of item ids )
 * exception: ERR_SLAVE_DEFINED ( in case the result payload does not hold a list of item ids as expected )
 * description:
 * converts the result payload into a list of internal numeric item ids
 */
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

/**
 * bool G3Request::g3Check ( G3Backend* const backend )
 *
 * param: G3Backend* const backend ( G3 backend used for this request )
 * returns: bool ( indication, if the given url does in fast exist and is usable as a REST API url )
 * description:
 * tests if the backend refers to an existing REST API urlencoded
 * this is done by issuing a http head request to the url and evaluating the received return code
 * no reply payload is received or evaluated except the http headers carrying the response code
 */
bool G3Request::g3Check ( G3Backend* const backend )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3Check>" );
  kDebug() << "(<backend>)" << backend->toPrintout();
  try
  {
    G3Request request ( backend, KIO::HTTP_HEAD );
    request.setup    ( );
    request.process  ( );
    request.evaluate ( );
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
      case Error(ERR_COULD_NOT_AUTHENTICATE): // http 403
        // the 'rest-url' DOES exist, it typically returns a 403 when called without parameters
        kDebug() << "{<bool>} TRUE";
        return TRUE;
    } // switch
    // re-throw everything else
    throw e;
  } // catch
} // G3Request::g3Check

/**
 * bool G3Request::g3Login ( G3Backend* const backend, AuthInfo& credentials )
 *
 * param: G3Backend* const backend ( G3 backend used for this request )
 * param: AuthInfo& credentials ( authentication credentials as prepared, cached or requested before )
 * returns: bool ( indicates if the login attempt has succeeded )
 * description:
 * performs a login to the remote gallery3 system by issuing a single login request
 * in case of a success the result payload holds a valid and user specific 'remote access key' stored and cached in the credentials structure
 * this key acts as an authentication key (kind of a long-term session key) for all subsequent requests to the gallery3 system
 */
bool G3Request::g3Login ( G3Backend* const backend, AuthInfo& credentials )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3Login>" );
  kDebug() << "(<backend> <credentials>)" << backend->toPrintout() << credentials.username;
  G3Request request ( backend, KIO::HTTP_POST );
  request.addQueryItem ( QLatin1String("user"),     credentials.username);
  request.addQueryItem ( QLatin1String("password"), credentials.password);
  request.setup    ( );
  try
  {
    request.process  ( );
    request.evaluate ( );
  }
  catch ( Exception e )
  {
    switch ( e.getCode() )
    {
      case Error(ERR_COULD_NOT_AUTHENTICATE):
        credentials.digestInfo = QLatin1String("");
        kDebug() << "{<authenticated>}" << "FALSE";
        return FALSE;
      default: throw e;
    } // switch
  }
  credentials.digestInfo = request.toString ( );
  kDebug() << "{<authenticated>}" << "TRUE";
  return TRUE;
} // G3Request::g3Login

/**
 * QList<G3Item*> G3Request::g3GetItems ( G3Backend* const backend, const QStringList& urls, Entity::G3Type type )
 *
 * param: G3Backend* const backend ( G3 backend used for this request )
 * param: const QStringList& urls ( list of rest urls pointing to the requested items )
 * param: Entity::G3Type type ( filters result to items of a specific type if specified )
 * returns: QList<G3Item*> ( list of pointers to valid local item objects )
 */
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
    G3Request request ( backend, KIO::HTTP_GET, QLatin1String("items") );
    request.addQueryItem ( QLatin1String("urls"), urls_chunk );
    request.addQueryItem ( QLatin1String("type"), type );
    request.setup        ( );
    request.process      ( );
    request.evaluate     ( );
    items << request.toItems ( );
  } while ( (++chunk*ITEM_LIST_CHUNK_SIZE)<urls.count() );
  kDebug() << "{<items [count]>}" << items.count();
  return items;
} // G3Request::g3GetItems

/**
 * QList<G3Item*> G3Request::g3GetItems ( G3Backend* const backend, g3index id, Entity::G3Type type )
 *
 * param: G3Backend* const backend ( G3 backend used for this request )
 * param: g3index id ( numeric id of parent item whos members are requested )
 * param: Entity::G3Type type ( filters resulting item set to items of a specific type if specified )
 * returns: QList<G3Item*> ( list of valid internal item objects )
 * description:
 * retrieves all members contained in a parent item specified by a numeric id
 * the resulting set of items may be filtered to a specific item type
 */
QList<G3Item*> G3Request::g3GetItems ( G3Backend* const backend, g3index id, Entity::G3Type type )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3GetItems>" );
  kDebug() << "(<backend> <id> <type>)" << backend->toPrintout() << id << type.toString();
  KUrl url = backend->restUrl ( );
  url.addPath ( QString("item/%1").arg(id) );
  return g3GetItems ( backend, QStringList(url.url()), type );
} // G3Request::g3GetItems

/**
 * QList<g3index> G3Request::g3GetAncestors ( G3Backend* const backend, G3Item* item )
 *
 * param: G3Backend* const backend ( G3 backend used for this request )
 * param: G3Item* item ( item whos ancestors are requested )
 * returns: QList<g3index> ( list of numeric item ids of ancestors )
 * description:
 * retrieves a list of numeric item ids for all items being an ancestor to the requested item
 */
QList<g3index> G3Request::g3GetAncestors ( G3Backend* const backend, G3Item* item )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3GetAncestors>" );
  kDebug() << "(<backend> <item>)" << backend->toPrintout() << item->toPrintout();
  G3Request request ( backend, KIO::HTTP_GET, QLatin1String("items") );
  request.addQueryItem ( QLatin1String("ancestors_for"), item->restUrl().url() );
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
  QList<g3index> list = request.toItemIds ( );
  kDebug() << "{<items [count]>}" << list.count();
  return list;
} // G3Request::g3GetAncestors

/**
 * g3index G3Request::g3GetAncestors ( G3Backend* const backend, G3Item* item )
 *
 * param: G3Backend* const backend ( G3 backend used for this request )
 * param: G3Item* item ( item whos ancestors are requested )
 * returns: g3index ( numeric item id of ancestor )
 * description:
 * retrieves the numeric item id for the item being the direct ancestor to the requested item
 */
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

/**
 * G3Item* G3Request::g3GetItem ( G3Backend* const backend, g3index id, const QString& scope, const QString& name, bool random, Entity::G3Type type )
 *
 * param: G3Backend* const backend ( G3 backend used for this request )
 * param: g3index id ( numeric item id )
 * param: const QString& scope ( scope the request is to be interpreted in )
 * param: const QString& name ( name acting as a filter for the matching items names )
 * param: bool random ( flag indicating if a random item is returned )
 * param: Entity::G3Type type ( filters retrieved item to a certain type )
 * returns: G3Item* ( internal item object )
 * description:
 * retrieves a single item from the remote gallery3 system
 * which item is picked can be controlled by the additional query items
 * a typical request spcifies a 'direct' scope resulting in the retrieval of the item directly specified by the given numerical item id
 */
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

/**
 * void G3Request::g3PostItem ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes, const Entity::G3File* file )
 *
 * param: G3Backend* const backend ( G3 backend used for this request )
 * param: g3index id ( numeric item id )
 * param: const QHash<QString,QString>& attributes ( list of item attributes to be changed )
 * param: const Entity::G3File* file ( file to be uploaded as item )
 * description:
 * creates a single item described by a list of attributes and a file uploaded as altered content in case of a photo or movie item
 */
void G3Request::g3PostItem ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes, const Entity::G3File* file )
{
  // TODO: implement file to post a file as item, if file is specified (not NULL)
  MY_KDEBUG_BLOCK ( "<G3Request::g3PostItem>" );
  kDebug() << "(<backend> <id> <attributes[count]> <file>)" << backend->toPrintout() << id << attributes.count()
                                                            << ( file ? file->filename() : "-/-" );
  G3Request request ( backend, KIO::HTTP_POST, QString("item/%1").arg(id), file );
  // add attributes as 'entity' structure
  QMap<QString,QVariant> entity; // QMap, since QJson silently fails to encode a QHash !
  for ( QHash<QString,QString>::const_iterator it=attributes.constBegin(); it!=attributes.constEnd(); it++ )
    entity.insert ( it.key(), QVariant(it.value()) );
  // specify entity description as a single, serialized query attribute
  request.addQueryItem ( QLatin1String("entity"), request.g3serialize(QVariant(entity)) );
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
} // G3Request::g3PostItem

/**
 * void G3Request::g3PutItem ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes )
 *
 * param: G3Backend* const backend ( G3 backend used for this request )
 * param: g3index id ( numeric item id )
 * param: const QHash<QString,QString>& attributes ( list of attributes describing the item )
 * description:
 * updates a single item that already exists in the remote gallery3 system
 * a list of attributes holds the changes to be altered
 */
void G3Request::g3PutItem ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3PutItem>" );
  kDebug() << "(<backend> <id> <attributes[keys]> <type>)" << backend->toPrintout() << attributes.keys();
  G3Request request ( backend, KIO::HTTP_PUT, QString("item/%1").arg(id) );
  // add attributes as 'entity' structure
  QMap<QString,QVariant> entity; // QMap, since QJson silently fails to encode a QHash !
  for ( QHash<QString,QString>::const_iterator it=attributes.constBegin(); it!=attributes.constEnd(); it++ )
    entity.insert ( it.key(), it.value() );
  // specify entity description as a single, serialized query attribute
  request.addQueryItem ( QLatin1String("entity"), request.g3serialize(QVariant(entity)) );
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
} // G3Request::g3PutItem

/**
 * void G3Request::g3DelItem ( G3Backend* const backend, g3index id )
 *
 * param: G3Backend* const backend ( G3 backend used for this request )
 * param: g3index id ( numeric item id )
 * description:
 * permanently deletes an existing item in the remote gallery3 system
 */
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

/**
 * g3index G3Request::g3SetItem ( G3Backend* const backend, g3index id, const QString& name, Entity::G3Type type, const QByteArray& file )
 *
 * param: G3Backend* const backend ( G3 backend used for this request )
 * param: g3index id ( numeric item id )
 * param: const QString& name
 * param: Entity::G3Type type
 * param: const QByteArray& file
 * returns: g3index (numeric item id )
 * description:
 * ???
 */
g3index G3Request::g3SetItem ( G3Backend* const backend, g3index id, const QString& name, Entity::G3Type type, const QByteArray& file )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3SetItem>" );
  kDebug() << "(<backend> <id> <name> <type> <file>)" << backend->toPrintout() << name << type.toString();
  G3Request request ( backend, KIO::HTTP_POST, QString("item/%1").arg(id) );
  request.addQueryItem ( QLatin1String("name"),  name, TRUE );
  request.addQueryItem ( QLatin1String("type"),  type, TRUE );
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
  g3index index = request.toItemId ( );
  kDebug() << "{<item[id]>}" << index;
  return index;
} // G3Request::g3SetItem

void G3Request::g3FetchObject ( G3Backend* const backend, const KUrl& url )
{
  MY_KDEBUG_BLOCK ( "<G3Request::g3FetchObject>" );
  kDebug() << "(<backend> <url>)" << backend->toPrintout() << url;
  // we strip the leading "/rest" from the path to gain the 'service' we require here
  G3Request request ( backend, KIO::HTTP_GET, url.path().mid(5) );
  QMap<QString,QString> queryItems = url.queryItems ( );
  for ( QMap<QString,QString>::const_iterator it=queryItems.constBegin(); it!=queryItems.constEnd(); it++ )
    request.addQueryItem ( it.key(), it.value() );
  request.setup    ( );
  connect ( request.m_job, SIGNAL(    data(KIO::Job*,const QByteArray&)), backend->parent(), SLOT(    slotData(KIO::Job*,const QByteArray&)) );
  connect ( request.m_job, SIGNAL(mimetype(KIO::Job*,const QByteArray&)), backend->parent(), SLOT(slotMimetype(KIO::Job*,const QString&)) );
  request.process  ( );
  kDebug() << "{<>}";
} // G3Request::g3FetchObject

#include "gallery3/g3_request.moc"
