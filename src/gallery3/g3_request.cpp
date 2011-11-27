/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 81 $
 * $Date: 2011-08-13 13:08:50 +0200 (Sat, 13 Aug 2011) $
 */

/*!
 * @file
 * @brief Implements the methods of class G3Request
 * @see G3Request
 * @author Christian Reiner
 */

#include <QBuffer>
#include <QDataStream>
#include <QByteArray>
#include <krandom.h>
#include <kio/global.h>
#include <kdeversion.h>
#include <kio/netaccess.h>
#include <algorithm>
#include "utility/exception.h"
#include "gallery3/g3_request.h"
#include "gallery3/g3_backend.h"
#include "entity/g3_file.h"
#include "entity/g3_item.h"

using namespace KIO;
using namespace KIO::Gallery3;

/*!
 * G3Request::Members::Members ( G3Backend* const backend, KIO::HTTP_METHOD method, const QString& service, const G3File* const file )
 * @brief Constructor
 * Constructor for the private members class for G3Request
 * @see G3Request
 * @see G3Reqeust::Members
 * @author Christian Reiner
 */
G3Request::Members::Members ( G3Backend* const backend, KIO::HTTP_METHOD method, const QString& service, const G3File* const file )
  : backend ( backend )
  , method  ( method )
  , service ( service )
  , file    ( file )
  , payload ( NULL )
  , result  ( QVariant() )
  , meta    ( QMap<QString,QString>() )
  , query   ( QHash<QString,QString>() )
  , status  ( 0 )
  , job     ( NULL )
{
  kDebug();
  requestUrl = backend->restUrl();
  requestUrl.adjustPath ( KUrl::AddTrailingSlash );
  requestUrl.addPath ( service );
  boundary = KRandom::randomString(42+13).toAscii();
} // G3Request::Members

/*!
 * G3Request::G3Request ( G3Backend* const backend, KIO::HTTP_METHOD method, const QString& service, const G3File* const file )
 * @brief Constructor
 * @param backend backend associated to the requested g3 system
 * @param method  http method to be used
 * @param service service to be called, relative to the REST url
 * @param file    internal file ressource description, might hold a file for upload
 * Offers a generic object holding all aspects and data required while processing a http request to the remote gallery3 system. 
 * Some members are initialized, some basic settings done and a connection to the controlling slave base is established for authentication purposes.
 * @see G3Request
 * @author Christian Reiner
 */
G3Request::G3Request ( G3Backend* const backend, KIO::HTTP_METHOD method, const QString& service, const G3File* const file )
  : m ( new G3Request::Members(backend,method,service,file) )
{
  KDebug::Block block ( "G3Request::G3Request" );
  kDebug() << "(<backend> <method> <service> <file[name]>)" << backend->toPrintout() << method << service << ( file ? file->filename() : "-/-" );
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

/*!
 * G3Request::~G3Request ( )
 *@brief Destructor
 * Destructor of class G3Request
 * @see G3Request
 * @author Christian Reiner
 */
G3Request::~G3Request ( )
{
  kDebug();
  // delete private members
  delete m;
/*
it appears the jobs are somehow removed by some controller
trying to delete a job here often leads to a segfault
  if ( NULL!=m->job )
  {
    kDebug() << "deleting background job";
    //! @todo fixme: deleting the job after it has been executed reproduceably crashes the slave with a segfault
    delete m->job;
    m->job = NULL;
  }
*/
} // G3Request::~G3Request

//==========

/*!
 * int G3Request::httpStatusCode ( )
 * @brief HTTP status code of request
 * @return    http status code as defined by the RFCs
 * @exception ERR_SLAVE_DEFINED in case no valid http status code could be extracted
 * Extracts a standard http status code from any request result after processing a job. 
 * Note that a job may well return without error when being run, since those errors only refer to processing problems. 
 * http status codes refer to the protocol and content level, this is not handled by the method return value
 * @see G3Request
 * @author Christian Reiner
 */
int G3Request::httpStatusCode ( )
{
  kDebug() << "(<>)";
  QVariant httpStatusCode = QVariant(m->meta[QLatin1String("responsecode")]);
  if ( httpStatusCode.canConvert(QVariant::Int) )
  {
    int httpStatus = QVariant(m->meta[QLatin1String("responsecode")]).toInt();
    kDebug() << httpStatus;
    return httpStatus;
  }
  else
    throw Exception ( Error(ERR_SLAVE_DEFINED), i18n("No http status provided in response") );
} // G3Request::httpStatusCode

/*!
 * bool G3Request::retryWithChangedCredentials ( int attempt )
 * @brief Decide about retry after an error 403: authentication required
 * @param  attempt insicates how many attempts of this authentication request have been made before
 * @return         TRUE if credentials have been modified, FALSE otherwise
 * Wraps the execution of an authentication attempt against the remote Gallery3 system.
 * Signals if it makes sense to retry the request because of successfully changed credentials. 
 * @see G3Request
 * @author Christian Reiner
 */
bool G3Request::retryWithChangedCredentials ( int attempt )
{
  kDebug() << "(<attempt>)" << attempt;
  emit signalRequestAuthInfo ( m->backend, m->backend->credentials(), attempt );
  kDebug() << ( m->backend->credentials().isModified() ? "credentials changed" : "credentials unchanged" );
  return m->backend->credentials().isModified();
} // G3Request::retryWithChangedCredentials

/*!
 * void G3Request::addHeaderItem ( const QString& key, const QString& value )
 * @brief Adds a single header item to a request
 * @param key   http header key
 * @param value http header value
 * Adds a single key/value pair to the header item collection in the request object. 
 * Each pair collected by calls to this method will be specified as http header entries later during job setup. 
 * @see G3Request
 * @author Christian Reiner
 */
void G3Request::addHeaderItem ( const QString& key, const QString& value )
{
  kDebug() << "(<key> <value>)" << key << value;
  // add value to an existing header if one already exists, do NOT overwrite the existing one
  // we need this for the customHTTPHeaders as required by the G3 API
  QString content;
  if ( m->header.contains(key) )
    content = QString("%1\r\n%2").arg(m->header[key]).arg(value.trimmed());
  else
    content = value;
  //! @todo: some plausibility checks might be good here...
  m->header.insert ( key, content );
  kDebug() << "{<>}";
} // G3Request::addHeaderItem

/*!
 * void G3Request::addQueryItem ( const QString& key, const QString& value, bool skipIfEmpty )
 * @brief Adds a string as single query item to the request
 * @param key         http query item key
 * @param http        query item value
 * @param skipIfEmpty controls if this item will be silently skipped in case of an empty value
 * Adds a single key/value pair to the query item collection in the request object. 
 * Each pair collected by calls to this method will be added as query item later during job setup. 
 * Note, that these items will be specified as part of the request url or its form body, depending on the request method. 
 * @see G3Request
 * @author Christian Reiner
 */
void G3Request::addQueryItem ( const QString& key, const QString& value, bool skipIfEmpty )
{
  kDebug() << "(<key> <value> <bool>)" << key << value << skipIfEmpty;
  if ( m->query.contains(key) )
    m->query.remove ( key ); //! @todo: throw an error instead ?!?
  if ( value.isEmpty() )
    kDebug() << QString("skipping query item '%1'").arg(key);
  else
    m->query.insert ( key, value );
  kDebug() << "{<>}";
} // G3Request::addQueryItem

/*!
 * void G3Request::addQueryItem ( const QString& key, G3Type value, bool skipIfEmpty )
 * @brief Add G3Type as single query item to the request
 * @param key         http query item key
 * @param value       http query item value
 * @param skipIfEmpty controls if this item will be silently skipped in case of an empty value
 * Adds a single key/value pair to the query item collection in the request object. 
 * Each pair collected by calls to this method will be added as query item later during job setup. 
 * Note, that these items will be specified as part of the request url or its form body, depending on the request method. 
 * @see G3Request
 * @see G3Type
 * @author Christian Reiner
 */
void G3Request::addQueryItem ( const QString& key, G3Type value, bool skipIfEmpty )
{
  kDebug() << "(<key> <value> <bool>)" << key << value.toString() << skipIfEmpty;
  if ( m->query.contains(key) )
    m->query.remove ( key ); //! @todo: throw an error instead ?!?
  if ( value==G3Type::NONE )
    kDebug() << QString("skipping query item '%1' holding 'NONE' as entity type").arg(key);
  else
  {
    kDebug() << value.toString();
    addQueryItem ( key, value.toString() );
  };
  kDebug() << "{<>}";
} // G3Request::addQueryItem

/*!
 * void G3Request::addQueryItem ( const QString& key, const QStringList& values, bool skipIfEmpty )
 * @brief Adds a list of strings as a single query item to the request
 * @param key         http query item key
 * @param values      list of http query item values
 * @param skipIfEmpty controls if this item will be silently skipped in case of an empty value
 * Adds a list of key/value pairs to the query item collection in the request object. 
 * All values will be added in form of a multi-line value as a single item identified by the key (http protocol standard). 
 * Each pair collected by calls to this method will be added as query item later during job setup. 
 * Note, that these items will be specified as part of the request url or its form body, depending on the request method. 
 * @see G3Request
 * @author Christian Reiner
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

/*!
 * KUrl G3Request::webUrlWithQueryItems ( KUrl url, const QHash<QString,QString>& query )
 * @brief Creates the final http get url including all query items
 * @param  url   plain request url
 * @param  query collection of query items
 * @return       final request url including the query part 
 * constructs the final url to be requested in case of a http get or head method
 * this is the plain request url enriched by all query items being specified in the part of the url
 * @see G3Request
 * @author Christian Reiner
 */
KUrl G3Request::webUrlWithQueryItems ( KUrl url, const QHash<QString,QString>& query )
{
  kDebug() << "(<url> <query [count]>)" << url << query.count();
    for ( QHash<QString,QString>::const_iterator it=m->query.constBegin(); it!=m->query.constEnd(); it++ )
      url.addQueryItem ( it.key(), it.value() );
  kDebug() << "{<url>}" << url;
  return url;
} // G3Request::webUrlWithQueryItems

/*!
 * QByteArray G3Request::webFormPostPayload ( const QHash<QString,QString>& query )
 * @brief Contructs a http post body holding all query items
 * @param  queryItems dictionary of query items
 * @return            http post body
 * Constructs a complete http post body holding the content of a html form to be sent to a http server
 * @see G3Request
 * @author Christian Reiner
 */
QByteArray G3Request::webFormPostPayload ( const QHash<QString,QString>& query )
{
  kDebug() << "(<query[count]>)" << query.count();
  QStringList queryItems;
  // construct an encoded form payload
  for ( QHash<QString,QString>::const_iterator it=m->query.constBegin(); it!=m->query.constEnd(); it++ )
    queryItems << QString("%1=%2").arg(it.key()).arg(it.value());
  QByteArray buffer;
  buffer = queryItems.join(QLatin1String("&")).toAscii();
  kDebug() << "{<buffer[size]>}" << buffer.size();
  return buffer;
} // G3Request::webFormPostPayload

/*!
 * QByteArray G3Request::webFileFormPostPayload ( const QHash<QString,QString>& query, const G3File* const file )
 * @brief Constructs a multipart http post body holding all query items and a file to be uploaded
 * @param query set of query items to be sent inside the request
 * @param file  internal file description specifying a file to be uploaded during the request
 * @return      http post body
 * Constructs a complete http post body holding multi-part form to be sent to a http server. 
 * This for includes several query items as separate parts and one final file. 
 * @see G3Request
 * @author Christian Reiner
 */
QByteArray G3Request::webFileFormPostPayload ( const QHash<QString,QString>& query, const G3File* const file )
{
  kDebug() << "(<query> <file[name]>)" << query << ( file ? file->filename() : "-/-" );
  // write the form data
  QFile binary ( file->filepath() );
  binary.open ( QIODevice::ReadOnly );
  QByteArray buffer;
  // one part for each query items
  for ( QHash<QString,QString>::const_iterator it=m->query.constBegin(); it!=m->query.constEnd(); it++ )
  {
    buffer.append ( QString("--%1\r\n").arg(m->boundary).toAscii() );
    buffer.append ( QString("Content-Disposition: form-data; name=\"%1\"\r\n").arg(it.key()).toAscii() );
    buffer.append ( QString("Content-Type: text/plain; charset=UTF-8\r\n").toAscii() );
    buffer.append ( QString("Content-Transfer-Encoding: 8bit\r\n\r\n").toAscii() );
    buffer.append ( it.value().toAscii() );
  } // for
  // the file to be uploaded
  buffer.append ( QString("\r\n--%1\r\n").arg(m->boundary).toAscii() );
  buffer.append ( QString("Content-Disposition: form-data; name=\"file\"; filename=\"%1\"\r\n").arg(file->filename()).toAscii() );
//  buffer.append ( QString("Content-Type: application/octet-stream\r\n").toAscii() );
  buffer.append ( QString("Content-Type: %1\r\n\r\n").arg(file->mimetype()->name()).toAscii() );
//  buffer.append ( QString("Content-Type: %1\r\n").arg(file->mimetype()->name()).toAscii() );
//  buffer.append ( QString("Content-Transfer-Encoding: binary\r\n\r\n").toAscii() );
  buffer.append ( binary.readAll() );
  // terminating boundary marker (note the trailing "--")
  buffer.append ( QString("\r\n--%1--").arg(m->boundary).toAscii() );
  binary.close ( );
  kDebug() << "{<buffer[size]>}" << buffer.size();
  return buffer;
} // G3Request::webFileFormPostPayload

//==========

/*!
 * void G3Request::setup ( )
 * @brief Prepares a request by constructing the http job
 * Constructs a KIO::TransferJob as required for the specific request to the remote gallery3 system. 
 * The job is enriched with all query items, files and header entries as required for the final processing of the job. 
 * @see G3Request
 * @author Christian Reiner
 */ 
void G3Request::setup ( )
{
  KDebug::Block block ( "G3Request::setup" );
  kDebug() << "(<>)";
  // reset / initialize the members
  m->header.clear();
  m->meta.clear();
  m->payload = NULL;
  m->result  = QVariant();
  m->status  = 0;
  // G3 uses 'RemoteAccesKeys' for authentication purposes (see API documentation)
  // this key is locally stored by this slave, we specify it if it exists
  if ( ! m->backend->credentials().digestInfo.isEmpty() )
    addHeaderItem ( QLatin1String("customHTTPHeader"), QString("X-Gallery-Request-Key: %1" ).arg(m->backend->credentials().digestInfo) );
  // setup the actual http job
  switch ( m->method )
  {
    case KIO::HTTP_DELETE:
      m->job = KIO::http_post ( m->requestUrl, QByteArray(), KIO::DefaultFlags );
      addHeaderItem ( QLatin1String("content-type"),     QLatin1String("Content-Type: application/x-www-form-urlencoded") );
      addHeaderItem ( QLatin1String("customHTTPHeader"), QLatin1String("X-Gallery-Request-Method: delete") );
      break;
    case KIO::HTTP_GET:
      m->job = KIO::get ( webUrlWithQueryItems(m->requestUrl,m->query), KIO::Reload, KIO::DefaultFlags );
      addHeaderItem ( QLatin1String("customHTTPHeader"), QLatin1String("X-Gallery-Request-Method: get") );
      break;
    case KIO::HTTP_HEAD:
//      m->job = KIO::get ( webUrlWithQueryItems(m->requestUrl,m->query), KIO::Reload, KIO::DefaultFlags );
      m->job = KIO::mimetype ( webUrlWithQueryItems(m->requestUrl,m->query), KIO::DefaultFlags );
      addHeaderItem ( QLatin1String("customHTTPHeader"), QLatin1String("X-Gallery-Request-Method: head") );
      break;
    case KIO::HTTP_POST:
      if ( m->file )
      {
        m->job = KIO::http_post ( m->requestUrl, webFileFormPostPayload(m->query,m->file), KIO::DefaultFlags );
        addHeaderItem ( QLatin1String("content-type"), QString("Content-Type: multipart/form-data; boundary=%1").arg(m->boundary) );
      }
      else
      {
        m->job = KIO::http_post ( m->requestUrl, webFormPostPayload(m->query), KIO::DefaultFlags );
        addHeaderItem ( QLatin1String("content-type"), QLatin1String("Content-Type: application/x-www-form-urlencoded") );
      }
      addHeaderItem ( QLatin1String("customHTTPHeader"), QLatin1String("X-Gallery-Request-Method: post") );
      break;
    case KIO::HTTP_PUT:
      m->job = KIO::http_post ( m->requestUrl, webFormPostPayload(m->query), KIO::DefaultFlags );
      addHeaderItem ( QLatin1String("content-type"), QLatin1String("Content-Type: application/x-www-form-urlencoded") );
      addHeaderItem ( QLatin1String("customHTTPHeader"), QLatin1String("X-Gallery-Request-Method: put") );
      break;
  } // switch request method
  m->job->removeOnHold ( );
  // add header items if specified
  QHash<QString,QString>::const_iterator it;
  for ( it=m->header.constBegin(); it!=m->header.constEnd(); it++ )
    m->job->addMetaData ( it.key(), it.value() );
  kDebug() << "{<>}";
} // G3Request::setup

/*!
 * void G3Request::process ( )
 * @brief Processes a prepared request
 * @exception ERR_SLAVE_DEFINED in case of a failure on method level (NOT protocol level)
 * Processes a jbo that has been setup completely before. 
 * Attempts to retry the job after requesting authentication information in case of a http-403 from the server. 
 * @see G3Request
 * @author Christian Reiner
 */
void G3Request::process ( )
{
  KDebug::Block block ( "G3Request::process" );
  kDebug() << "(<>)";
  // prepare handling of authentication info
  // run the job
  kDebug() << "sending request to url" << m->job->url();
  int attempt = 0;
  do
  {
    // if status is 403 this is a retry after a failed attempt
    // we simply construct a fresh job by calling setup again...
    //! @todo: required at all ? or can a job simply be run several times ?
    if ( 403==m->status )
    {
      kDebug() << "resetting job for a new trial";
      setup ( );
    }
    if ( ! NetAccess::synchronousRun ( m->job, NULL, &m->payload, &m->finalUrl, &m->meta ) )
      throw Exception ( Error(ERR_SLAVE_DEFINED),
                        i18n("request failed: %2 [%1]").arg(m->job->error()).arg(m->job->errorString()) );
    // check for problems on protocol level
    if ( m->job->error() )
      throw Exception ( Error(ERR_SLAVE_DEFINED),
                        i18n("Runtime error processing job: %2 [%1]").arg(m->job->error()).arg(m->job->errorString()) );
    // extract and store http status code from reply
    m->status = httpStatusCode();
  } while (    (m->job->url().fileName()!=QLatin1String("rest")) // exception: g3Check: looking for REST API
            && (403==m->status)                   // repeat only in this case
//            && retryWithChangedCredentials(++attempt) );  // retry makes sense if credentials have changed
            && retryWithChangedCredentials(attempt) );  // retry makes sense if credentials have changed
  kDebug() << "{<>}"; 
} // G3Request::process

/*!
 * void G3Request::evaluate ( )
 * @brief Evaluates the reply received after a request
 * @exception ERR_SLAVE_DEFINED in case of unexpected content syntax, that is non-json-encoded content
 * Evaluates the received result payload of a preceding processed job on a generic level. 
 * Provides the json-parsed payload in form of a QVariant. 
 * @see G3Request
 * @author Christian Reiner
 */
void G3Request::evaluate ( )
{
  KDebug::Block block ( "G3Request::evaluate" );
  kDebug() << "(<>)";
  // check for success on protocol & content level (headers and so on)
  switch ( m->status )
  {
    case 0:   break; // no error
    case 200: kDebug() << QString("HTTP %1 OK"                           ).arg(QVariant(m->meta[QLatin1String("responsecode")]).toInt()); break;
    case 201: kDebug() << QString("HTTP %1 Created"                      ).arg(QVariant(m->meta[QLatin1String("responsecode")]).toInt()); break;
    case 202: kDebug() << QString("HTTP %1 Accepted"                     ).arg(QVariant(m->meta[QLatin1String("responsecode")]).toInt()); break;
    case 203: kDebug() << QString("HTTP %1 Non-Authoritative Information").arg(QVariant(m->meta[QLatin1String("responsecode")]).toInt()); break;
    case 204: kDebug() << QString("HTTP %1 No Content"                   ).arg(QVariant(m->meta[QLatin1String("responsecode")]).toInt()); break;
    case 205: kDebug() << QString("HTTP %1 Reset Content"                ).arg(QVariant(m->meta[QLatin1String("responsecode")]).toInt()); break;
    case 206: kDebug() << QString("HTTP %1 Partial Content"              ).arg(QVariant(m->meta[QLatin1String("responsecode")]).toInt()); break;
    case 400: throw Exception ( Error(ERR_INTERNAL_SERVER),        i18n("HTTP 400: Bad Request") );
    case 401: throw Exception ( Error(ERR_ACCESS_DENIED),          i18n("HTTP 401: Unauthorized") );
    case 403: throw Exception ( Error(ERR_COULD_NOT_AUTHENTICATE), i18n("HTTP 403: No Authorization") ); // login failed
    case 404: throw Exception ( Error(ERR_SERVICE_NOT_AVAILABLE),  i18n("HTTP 404: Not Found") );
    default:  throw Exception ( Error(ERR_SLAVE_DEFINED),          i18n("Unexpected http error %1").arg(QVariant(m->meta[QLatin1String("responsecode")]).toInt()) );
  } // switch
  kDebug() << QString ("request processed [ headers size: %2 / payload size: %1]")
                      .arg(m->meta.size())
                      .arg(m->payload.size());
  if ( QLatin1String("application/json")!=m->meta[QLatin1String("content-type")] )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      i18n("unexpected content type in response: %1").arg(m->meta[QLatin1String("content-type")]) );
  kDebug() << QString("response has expected content type '%1'").arg(m->meta[QLatin1String("content-type")]);
  // SUCCESS, convert result content (payload) into a usable object structure
  // NOTE: there is a bug in the G3 API implementation, it returns 'null' instead of an empty json structure in certain cases (DELETE)
  if ( "null"==m->payload )
    // gallery3 sends the literal string 'null' when serializing an empty set or object
    m->result = QVariant();
  else if ( '"'==m->payload.trimmed()[0] )
    // QJson crashes when decoding a simple json encoded string ("string")
    // workaround: we wrap and dewrap it as a single array element
    m->result = g3parse(QString("["+m->payload+"]").toUtf8()).toList().first();
  else
    m->result = g3parse ( m->payload );
  kDebug() << "{<>}";
} // G3Request::process

//==========

/*!
 * QString G3Request::toString ( )
 * @brief Converts the requests reply into a string
 * @return the string as extracted from the result payload
 * @exception ERR_SLAVE_DEFINED in case the payload did not carry a plain string as expected
 * Converts the request result payload into a valid QString. 
 * @see G3Request
 * @author Christian Reiner
 */
QString G3Request::toString ( )
{
  if ( ! m->result.canConvert(QVariant::String) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      i18n("gallery response did not hold a valid remote access key") );
  QString string = m->result.toString();
  kDebug() << "{<string>}" << string;
  return string;
} // G3Request::toString

/*!
 * G3Item* G3Request::toItem ( QVariant& entry )
 * @brief Converts a requests reply into a local item object
 * @param entry a single entry as extracted from a result payload holding multiple entries
 * @return pointer to an internal item object
 * @exception ERR_SLAVE_DEFINED in case the entry does not hold a valid item description as expected
 * Converts the specified entry into an internal G3Item object
 * @see G3Request
 * @author Christian Reiner
 */
G3Item* G3Request::toItem ( QVariant& entry )
{
  KDebug::Block block ( "G3Request::toItem" );
  if ( ! entry.canConvert(QVariant::Map) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      i18n("gallery response did not hold a valid item description") );
  QVariantMap attributes = entry.toMap();
  G3Item* item = G3Item::instantiate ( m->backend, attributes );
  kDebug() << "{<item>}" << item->toPrintout();
  return item;
} // G3Request::toItem

/*!
 * QList<G3Item*> G3Request::toItems ( )
 * @brief Converts a requests reply into a list of local item objects
 * @return list of pointers to item objects
 * @exception ERR_SLAVE_DEFINED in case the result payload did not hold a list of entries as expected
 * Converts the result payload into a list of internal G3Item objects. 
 * @see G3Request
 * @author Christian Reiner
 */
QList<G3Item*> G3Request::toItems ( )
{
  KDebug::Block block ( "G3Request::toItems" );
  kDebug() << "(<>)";
  QList<G3Item*> items;
  // expected result syntax ?
  if ( ! m->result.canConvert(QVariant::List) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      i18n("gallery response did not hold a valid list of item descriptions") );
  QList<QVariant> entries = m->result.toList();
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

/*!
 * g3index G3Request::toItemId ( QVariant& entry )
 * @brief Converts a requests reply into a numeric item id
 * @param     entry             technical description that should contain a single item entity
 * @return                      g3index numeric item id
 * @exception ERR_SLAVE_DEFINED in case the result payload does not hold a valid item id as expected
 * Converts the result payload into a valid internal numerical G3Item id. 
 * @see G3Request
 * @author Christian Reiner
 */
g3index G3Request::toItemId ( QVariant& entry )
{
  KDebug::Block block ( "G3Request::toItemId" );
  kDebug() << "(<entry>)";
  if ( ! entry.canConvert(QVariant::Map) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      i18n("gallery response did not hold a valid item description") );
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
      throw Exception ( Error(ERR_INTERNAL), i18n("gallery response did not hold a valid item description") );
  } // if
  else
    throw Exception ( Error(ERR_INTERNAL), i18n("gallery response did not hold valid return content") );
} // G3Request::toItemId

//==========

/*!
 * QList<g3index> G3Request::toItemIds ( )
 * @brief Converts a requests reply into a list of numeric item ids
 * @return list of item ids
 * @exception ERR_SLAVE_DEFINED in case the result payload does not hold a list of item ids as expected
 * Converts the result payload into a list of internal numeric item ids. 
 * @see G3Request
 * @author Christian Reiner
 */
QList<g3index> G3Request::toItemIds ( )
{
  KDebug::Block block ( "G3Request::toItemIds" );
  kDebug() << "(<>)";
  QList<g3index> ids;
  // expected result syntax ?
  if ( ! m->result.canConvert(QVariant::List) )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      i18n("gallery response did not hold a valid list of item descriptions") );
  QList<QVariant> entries = m->result.toList();
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

/*!
 * bool G3Request::g3Check ( G3Backend* const backend )
 * @brief Checks the existance of the RESTful API of a remote Gallery3 system
 * @param backend backend used for this request
 * @return        indication, if the given url does in fast exist and is usable as a REST API url
 * Tests if the backend refers to an existing REST API urlencoded. 
 * This is done by issuing a http head request to the url and evaluating the received return code. 
 * No reply payload is received or evaluated except the http headers carrying the response code. 
 * @see G3Request
 * @author Christian Reiner
 */
bool G3Request::g3Check ( G3Backend* const backend )
{
  KDebug::Block block ( "G3Request::g3Check" );
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

/*!
 * bool G3Request::g3Login ( G3Backend* const backend, AuthInfo& credentials )
 * @brief performs a login to a remote Gallery3 system
 * @param backend     backend used for this request
 * @param credentials authentication credentials as prepared, cached or requested before
 * @return            indicates if the login attempt has succeeded 
 * Performs a login to the remote gallery3 system by issuing a single login request. 
 * In case of a success the result payload holds a valid and user specific 'remote access key' stored and cached in the credentials structure. 
 * This key acts as an authentication key (kind of a long-term session key) for all subsequent requests to the gallery3 system. 
 * @see G3Request
 * @author Christian Reiner
 */
bool G3Request::g3Login ( G3Backend* const backend, AuthInfo& credentials )
{
  KDebug::Block block ( "G3Request::g3Login" );
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

/*!
 * QList<G3Item*> G3Request::g3GetItems ( G3Backend* const backend, const QStringList& urls, G3Type type )
 * @brief Retrieves a list of specified items from a remote Gallery3 system
 * @param backend backend used for this request
 * @param urls    list of rest urls pointing to the requested items
 * @param type    filters result to items of a specific type if specified
 * @return        list of pointers to valid local item objects
 * @see G3Request
 * @author Christian Reiner
 */
QList<G3Item*> G3Request::g3GetItems ( G3Backend* const backend, const QStringList& urls, G3Type type )
{
  KDebug::Block block ( "G3Request::g3GetItems" );
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

/*!
 * QList<G3Item*> G3Request::g3GetItems ( G3Backend* const backend, g3index id, G3Type type )
 * @brief Retrieves a list of items being members of a given item from a remote Gallery3 system
 * @param backend G3 backend used for this request
 * @param id      numeric id of parent item whos members are requested
 * @param type    filters resulting item set to items of a specific type if specified
 * @return        list of valid internal item objects
 * Retrieves all members contained in a parent item specified by a numeric id. 
 * The resulting set of items may be filtered to a specific item type. 
 * @see G3Request
 * @author Christian Reiner
 */
QList<G3Item*> G3Request::g3GetItems ( G3Backend* const backend, g3index id, G3Type type )
{
  KDebug::Block block ( "G3Request::g3GetItems" );
  kDebug() << "(<backend> <id> <type>)" << backend->toPrintout() << id << type.toString();
  KUrl url = backend->restUrl ( );
  url.addPath ( QString("item/%1").arg(id) );
  return g3GetItems ( backend, QStringList(url.url()), type );
} // G3Request::g3GetItems

/*!
 * QList<g3index> G3Request::g3GetAncestors ( G3Backend* const backend, G3Item* item )
 * @brief Retrieves the path of anchestors of a specified item from a remote Gallery3 system
 * @param backend backend used for this request
 * @param item    item whos ancestors are requested
 * @return        list of numeric item ids of ancestors
 * Retrieves a list of numeric item ids for all items being an ancestor to the requested item. 
 * @see G3Request
 * @author Christian Reiner
 */
QList<g3index> G3Request::g3GetAncestors ( G3Backend* const backend, G3Item* item )
{
  KDebug::Block block ( "G3Request::g3GetAncestors" );
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

/*!
 * g3index G3Request::g3GetAncestors ( G3Backend* const backend, G3Item* item )
 * @brief Retrieves the path of anchestors of a specified item from a remote Gallery3 system
 * @param  backend backend used for this request
 * @param  item    item whos ancestors are requested
 * @return         numeric item id of ancestor (g3index)
 * Retrieves the numeric item id for the item being the direct ancestor to the requested item. 
 * @see G3Request
 * @author Christian Reiner
 */
g3index G3Request::g3GetAncestor ( G3Backend* const backend, G3Item* item )
{
  KDebug::Block block ( "G3Request::g3GetAncestor" );
  kDebug() << "(<backend> <item>)" << backend->toPrintout() << item->toPrintout();
  QList<g3index> ancestors = g3GetAncestors ( backend, item );
  // second last one in the row should be the direct ancestor, the 'parent', IF it exists
  g3index parent;
  switch ( ancestors.count() )
  {
    case 0:
      throw Exception ( Error(ERR_INTERNAL), i18n("requested item appears not to be part of its own ancestors list ?!?") );
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

/*!
 * G3Item* G3Request::g3GetItem ( G3Backend* const backend, g3index id, const QString& scope, const QString& name, bool random, G3Type type )
 * @brief Retrieves a single item specified by its numerical id
 * @param   backend backend used for this request
 * @param   id      numeric item id
 * @param   scope   scope the request is to be interpreted in
 * @param   name    name acting as a filter for the matching items names
 * @param   random  flag indicating if a random item is returned
 * @param   type    filters retrieved item to a certain type
 * @returns         internal item object
 * Retrieves a single item from the remote gallery3 system. 
 * Which item is picked can be controlled by the additional query items.
 * A typical request spcifies a 'direct' scope resulting in the retrieval of the item directly specified by the given numerical item id. 
 * @see G3Request
 * @author Christian Reiner
 */
G3Item* G3Request::g3GetItem ( G3Backend* const backend, g3index id, const QString& scope, const QString& name, bool random, G3Type type )
{
  KDebug::Block block ( "G3Request::g3GetItem" );
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

/*!
 * void G3Request::g3PostItem ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes, const G3File* file )
 * @brief Creates a new item inside the remote Gallery3 system
 * @param backend    backend used for this request
 * @param id         numeric item id
 * @param attributes list of item attributes to be changed
 * @param file       file to be uploaded as item
 * Creates a single item described by a list of attributes and a file uploaded as altered content in case of a photo or movie item. 
 * @see G3Request
 * @author Christian Reiner
 */
void G3Request::g3PostItem ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes, const G3File* file )
{
  KDebug::Block block ( "G3Request::g3PostItem" );
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

/*!
 * void G3Request::g3PutItem ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes )
 * @brief Updates an existing item inside a remote Gallery3 system
 * @param backend    backend used for this request
 * @param id         numeric item id
 * @param attributes list of attributes describing the item
 * Updates a single item that already exists in the remote gallery3 system. 
 * A list of attributes holds the changes to be altered. 
 * @see G3Request
 * @author Christian Reiner
 */
void G3Request::g3PutItem ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes )
{
  KDebug::Block block ( "G3Request::g3PutItem" );
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

/*!
 * void G3Request::g3DelItem ( G3Backend* const backend, g3index id )
 * @brief Deletes an item inside a remote Gallery3 system
 * @param backend backend used for this request
 * @param id      numeric item id
 * Permanently deletes an existing item in the remote gallery3 system. 
 * @see G3Request
 * @author Christian Reiner
 */
void G3Request::g3DelItem ( G3Backend* const backend, g3index id )
{
  KDebug::Block block ( "G3Request::g3DelItem" );
  kDebug() << "(<backend> <id>)" << backend->toPrintout() << id;
  G3Request request ( backend, KIO::HTTP_DELETE, QString("item/%1").arg(id) );
  request.setup        ( );
  request.process      ( );
  request.evaluate     ( );
  kDebug() << "{<>}";
} // G3Request::g3DelItem

/*!
 * g3index G3Request::g3SetItem ( G3Backend* const backend, g3index id, const QString& name, G3Type type, const QByteArray& file )
 * @brief Creates a new item inside a remote Gallery3 system
 * @param backend backend used for this request
 * @param id      numeric item id
 * @param name    name of item to create
 * @param type    type of item to create
 * @param file    file to be uploaded as required for the item creation
 * @return        numeric item id (g3index)
 * Creates a new item inside the remote Gallery3 system
 * @see G3Request
 * @author Christian Reiner
 */
g3index G3Request::g3SetItem ( G3Backend* const backend, g3index id, const QString& name, G3Type type, const QByteArray& file )
{
  KDebug::Block block ( "G3Request::g3SetItem" );
  kDebug() << "(<backend> <id> <name> <type> <file>)" << backend->toPrintout() << id << name << type.toString();
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

/*!
 * void G3Request::g3FetchObject ( G3Backend* const backend, const KUrl& url )
 * @brief Retrieves the file represented by an item inside a remote Gallery3 system
 * @param backend backend used for this request
 * @param url     url of the object represented by the remote item, a file
 * @see G3Request
 * @author Christian Reiner
 */
void G3Request::g3FetchObject ( G3Backend* const backend, const KUrl& url )
{
  KDebug::Block block ( "G3Request::g3FetchObject" );
  kDebug() << "(<backend> <url>)" << backend->toPrintout() << url;
  // we strip the leading "/rest" from the path to gain the 'service' we require here
  G3Request request ( backend, KIO::HTTP_GET, url.path().mid(5) );
  QMap<QString,QString> queryItems = url.queryItems ( );
  for ( QMap<QString,QString>::const_iterator it=queryItems.constBegin(); it!=queryItems.constEnd(); it++ )
    request.addQueryItem ( it.key(), it.value() );
  request.setup    ( );
  connect ( request.m->job, SIGNAL(    data(KIO::Job*,const QByteArray&)), backend->parent(), SLOT(    slotData(KIO::Job*,const QByteArray&)) );
  request.process  ( );
  kDebug() << "{<>}";
} // G3Request::g3FetchObject

#include "gallery3/g3_request.moc"
