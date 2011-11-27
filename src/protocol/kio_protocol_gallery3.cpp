/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

/*!
 * @file
 * Implementation of the methods of class KIOGallery3Protocol
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */

#include <stdlib.h>
#include <unistd.h>
#include <KUrl>
#include <KMimeType>
#include <klocalizedstring.h>
#include <kio/netaccess.h>
#include <kio/job.h>
#include <kio/filejob.h>
#include <ktemporaryfile.h>
#include <kstandarddirs.h>
#include "utility/exception.h"
#include "gallery3/g3_backend.h"
#include "protocol/kio_protocol_gallery3.h"
#include "entity/g3_item.h"
#include "entity/g3_file.h"

using namespace KIO;
using namespace KIO::Gallery3;

/*!
 * void KIOGallery3Protocol::selectConnection ( const QString& host, g3index port, const QString& user, const QString& pass )
 * @brief Static method to set active remote connection
 * @param host host to connect to
 * @param port tcp port to connect to
 * @param user user to authenticate as
 * @param pass password to authenticate with
 * Accepts the connection details from the controlling slave interface and stores them in a persistant manner
 * But using this method as a switch we can jump between different remote Galelry3 systems. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::selectConnection ( const QString& host, qint16 port, const QString& user, const QString& pass )
{
  KDebug::Block block ( "KIOGallery3Protocol::selectConnection" );
  kDebug() << "(<host> <port> <user> <pass>)" << host << port << user << ( pass.isEmpty() ? "" : "<hidden password>" );
  m->connection.host = host;
  m->connection.port = port;
  m->connection.user = user;
  m->connection.pass = pass;
} // KIOGallery3Protocol::selectConnection

/*!
 * G3Backend* KIOGallery3Protocol::selectBackend ( const KUrl& targetUrl )
 * @brief Select backend responsible for a request
 * @param targetUrl requested url
 * @return          existing or freshly created backend associated with the given targetUrl
 * Standardizes the given target url to form a clean and reliable base url used
 * to identify the associated backend to used.
 * Note that a change of the user name (authentication) creates a different url
 * and by this a different backend. This is desired, the tree of items might
 * look completely different so we want to create a fresh backend.
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
G3Backend* KIOGallery3Protocol::selectBackend ( const KUrl& targetUrl )
{
  KDebug::Block block ( "KIOGallery3Protocol::selectBackend" );
  kDebug() << "(<url>)" << targetUrl;
  // enhance the url with the current (updated) connection details
  KUrl itemUrl;
  itemUrl = KUrl ( QString("%1://%2%3%4%5")
                          .arg( targetUrl.scheme() )
                          .arg( m->connection.user.isEmpty() ? "" : QString("%1@").arg(m->connection.user) )
                          .arg( m->connection.host )
                          .arg( 0==m->connection.port ? "" : QString(":%1").arg(m->connection.port) )
                          .arg( targetUrl.path() ) );
  itemUrl.adjustPath ( KUrl::RemoveTrailingSlash );
  kDebug() << "corrected url:" << itemUrl;
  G3Backend* backend = G3Backend::instantiate ( this, m->backends, itemUrl );
  return backend;
} // KIOGallery3Protocol::selectBackend

/*!
 * G3Item* KIOGallery3Protocol::itemBase ( const KUrl& itemUrl )
 * @brief Get the base item object
 * @param itemUrl item url required to identify the responsible backend
 * @return        the base item as existing inside the responsible backend
 * The base item is typically the entry piont for all searches inside the item hierarchy.
 * It is predefined inside the remote Gallery3, has the numerical id 1 and cannot be altered.
 * Usually it is invisible as such, it simply acts as a starting point inside the hierarchy.
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
G3Item* KIOGallery3Protocol::itemBase ( const KUrl& itemUrl )
{
  KDebug::Block block ( "KIOGallery3Protocol::itemBase" );
  kDebug() << "(<url>)" << itemUrl;
  G3Backend* backend = selectBackend ( itemUrl );
  return backend->itemBase ( );
} // KIOGallery3Protocol::itemBase

/*!
 * G3Item* KIOGallery3Protocol::itemByUrl ( const KUrl& itemUrl )
 * @brief Get an item as referenced by its url
 * @param itemUrl item url required to identify the responsible backend
 * @return        the requested item object as existing inside the responsible backend
 * This method offers a convenient way to get an item object inside the local
 * name based item hierarchy. It automatically selects the responsible backend
 * and creates all parent items inside the items path as requried.
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
G3Item* KIOGallery3Protocol::itemByUrl ( const KUrl& itemUrl )
{
  KDebug::Block block ( "KIOGallery3Protocol::itemByUrl" );
  kDebug() << "(<url>)" << itemUrl;
  G3Backend* backend = selectBackend ( itemUrl );
  QString path = KUrl::relativeUrl ( backend->baseUrl(), itemUrl );
  return backend->itemByPath ( path );
} // KIOGallery3Protocol::itemByUrl

/*!
 * QList<G3Item*> KIOGallery3Protocol::itemsByUrl ( const KUrl& itemUrl )
 * @brief Get list of member items of an album referenced by its url
 * @param itemUrl item url required to identify the responsible backend
 * @return        complete list of items contained in the requested item
 * Convenient way to get the whole list of member items that exist inside an
 * album. automatically selects or constructs the responsible backend and
 * all parent items inside the albums path as requried.
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
QList<G3Item*> KIOGallery3Protocol::itemsByUrl ( const KUrl& itemUrl )
{
  KDebug::Block block ( "KIOGallery3Protocol::itemsByUrl" );
  kDebug() << "(<url>)" << itemUrl;
  G3Backend* backend = selectBackend ( itemUrl );
  QString path = KUrl::relativeUrl ( backend->baseUrl(), itemUrl );
  return backend->membersByItemPath ( path );
} // KIOGallery3Protocol::itemsByUrl

//==========

/*!
 * KIOGallery3Protocol::KIOGallery3Protocol ( const QByteArray &pool, const QByteArray &app, QObject* parent )
 * @brief Constructor
 * @param pool
 * @param app
 * @param parent
 * The standard constructor, nothing special here.
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
KIOGallery3Protocol::KIOGallery3Protocol ( const QByteArray &pool, const QByteArray &app, QObject* parent )
  : QObject     ( parent )
  , KIOProtocol ( pool, app, protocol() )
  , m           ( new KIOGallery3Protocol::Members )
{
  KDebug::Block block ( "KIOGallery3Protocol::KIOGallery3Protocol" );
  try
  {
    // initialize the wrappers nodes
    //m_galleries->refreshNodes ( );
  }
  catch ( Exception &e ) { error ( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::KIOGallery3Protocol

/*!
 * KIOGallery3Protocol::~KIOGallery3Protocol ( )
 * @brief: Destructor
 * Standard descructor, removes all backends registered by this instance.
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
KIOGallery3Protocol::~KIOGallery3Protocol ( )
{
  KDebug::Block block ( "KIOGallery3Protocol::~KIOGallery3Protocol" );
  try
  {
    kDebug() << "deleting existing backends";
    while ( ! m->backends.isEmpty() )
    {
      QHash<QString,G3Backend*>::const_iterator backend = m->backends.constBegin();
      kDebug() << "removing backend" << backend.value()->toPrintout();
      delete m->backends.take ( backend.key() );
    } // while
    // delete private members
    delete m;
  }
  catch ( Exception &e )
  {
    error ( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::~KIOGallery3Protocol

//======================

/*!
 * void KIOGallery3Protocol::slotRequestAuthInfo ( G3Backend* backend, AuthInfo& credentials, int attempt )
 * @brief Interactive authentication service
 * @param backend     the backend an authentication is required against
 * @param credentials the credentials as tehy currently exist to be enhanced
 * @param attempt     the number of different authentication attempts done so far
 * This slot offers an authentication service to G3Requests that experience an
 * error 403: authentication required. This way we can do without a mandatory
 * authentication when starting and only identify against the remote Gallery3
 * system if requested to do so. This means that a guest access works if
 * granted by the remote Gallery3 system.
 * The slot performs a complete authentication request against the remote
 * Gallery3 system by itself and modifies the authentication credentials as
 * taken from the local cache or gathered in previous attempts.
 * IN case of a successful authentication the Gallery3-typical "remote access
 * key" is stored inside the credentials as AuthInfo::digestInfo. That key acts
 * as a long term session key for all subsequent requests to the system.
 * @see KIOGallery3Protocol
 * @see G3Request
 * @see G3Request::signalRequestAuthInfo
 * @see G3Request::g3Login
 * @author Christian Reiner
 */
void KIOGallery3Protocol::slotRequestAuthInfo ( G3Backend* backend, AuthInfo& credentials, int attempt )
{
  KDebug::Block block ( "KIOGallery3Protocol::slotRequestAuthInfo" );
  kDebug() << "(<AuthInfo>)" << credentials.url << credentials.caption << credentials.comment;
  // check if there is a matching entry in the cache
  AuthInfo cached = credentials;
  // NOTE: there are a few different situations we have to take into account...

  if ( 1==attempt && ! credentials.username.isEmpty() && ! credentials.password.isEmpty() )
  {
    if ( checkCachedAuthentication(credentials) )
    return;
  }
  else if (   ( 1==attempt && checkCachedAuthentication(cached) )
           && ( cached.username==credentials.username ) )
  {
    kDebug() << "attempt" << attempt << ": re-using cached credentials";
    credentials.password   = cached.password;
    credentials.digestInfo = cached.digestInfo;
    if ( ! credentials.digestInfo.isEmpty() )
      return;
  }

  // no way, we have to proceed interactively
  kDebug() << "asking user for authentication credentials";
  QString message = QString();
  while ( openPasswordDialog(credentials, message) )
  {
    kDebug() << "attempt" << attempt << "user provided authentication credentials";
    credentials.digestInfo = "";
    if (backend->login ( credentials ) )
    {
      kDebug() << "authentiaction succeeded";
      if (credentials.keepPassword)
      {
        kDebug() << "caching credentials";
        cacheAuthentication(credentials);
      }
      return;
    }
    else
    {
      kDebug() << "authentiaction failed, retrying";
      message = QLatin1String("Authentication failed");
    }
  } // while
  kDebug() << "user cancelled authentication";
  throw Exception ( Error(ERR_ABORTED), i18n("Authentication cancelled to '%1'").arg(credentials.url.prettyUrl()) );
} // KIOGallery3Protocol::slotRequireAuthInfo

/*!
 * void KIOGallery3Protocol::slotMessageBox ( int& result, SlaveBase::MessageBoxType type, const QString &text, const QString &caption, const QString &buttonYes, const QString &buttonNo )
 * @brief Interactive message box service
 * @param result    as defined in KIO::SlaveBase::messageBox
 * @param types     as defined in KIO::SlaveBase::messageBox
 * @param text      as defined in KIO::SlaveBase::messageBox
 * @param caption   as defined in KIO::SlaveBase::messageBox
 * @param buttonYes as defined in KIO::SlaveBase::messageBox
 * @param buttonNo  as defined in KIO::SlaveBase::messageBox
 * Offers an interactive message box to request decisions from the user.
 * This is only a wrapper that makes the KIO::SlaveBase::messageBox available
 * for other objects inside this slave. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::slotMessageBox ( int& result, SlaveBase::MessageBoxType type, const QString &text, const QString &caption, const QString &buttonYes, const QString &buttonNo )
{
  result = messageBox ( type, text, caption, buttonYes, buttonNo );
  if ( 0==result )
    throw Exception ( Error(ERR_INTERNAL), i18n("Communication error during user feedback") );
  kDebug() << text << caption << ">>" << result;
} // KIOGallery3Protocol::slotMessageBox

/*!
 * void KIOGallery3Protocol::slotMessageBox ( int& result, const QString &text, SlaveBase::MessageBoxType type, const QString &caption, const QString &buttonYes, const QString &buttonNo, const QString &dontAskAgainName )
 * @brief Interactive message box service
 * @param result    as defined in KIO::SlaveBase::messageBox
 * @param types     as defined in KIO::SlaveBase::messageBox
 * @param text      as defined in KIO::SlaveBase::messageBox
 * @param caption   as defined in KIO::SlaveBase::messageBox
 * @param buttonYes as defined in KIO::SlaveBase::messageBox
 * @param buttonNo  as defined in KIO::SlaveBase::messageBox
 * Offers an interactive message box to request decisions from the user.
 * This is only a wrapper that makes the KIO::SlaveBase::messageBox available
 * for other objects inside this slave.
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::slotMessageBox ( int& result, const QString &text, SlaveBase::MessageBoxType type, const QString &caption, const QString &buttonYes, const QString &buttonNo, const QString &dontAskAgainName )
{
  result = messageBox ( text, type, caption, buttonYes, buttonNo, dontAskAgainName );
  if ( 0==result )
    throw Exception ( Error(ERR_INTERNAL), i18n("Communication error during user feedback") );
  kDebug() << text << caption << ">>" << result << dontAskAgainName;
} // KIOGallery3Protocol::slotMessageBox

/*!
 * void KIOGallery3Protocol::slotListUDSEntries ( const UDSEntryList entries )
 * @brief Publish list of items
 * @param entries list of UDS entries
 * Accepts a list of UDSEntries describing items inside the item hierarchy.
 * Typically used to publish parts of a large set of members inside an album. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::slotListUDSEntries ( const UDSEntryList entries )
{
  kDebug() << "(<UDSEntries[count]>)" << entries.count();
  listEntries ( entries );
} // KIOGallery3Protocol::slotListUDSEntries

/*!
 * void KIOGallery3Protocol::slotListUDSEntry ( const UDSEntry entry )
 * @brief Publish single item as part of a list
 * @param entry single UDS entry
 * Accepts a single UDSEntry describing an item inside the item hierarchy.
 * Typically used to publish an item being part of the set of members of an album. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::slotListUDSEntry ( const UDSEntry entry )
{
  kDebug() << "(<UDSEntry>)" << entry.stringValue ( UDSEntry::UDS_NAME );
  listEntry ( entry, FALSE );
} // KIOGallery3Protocol::slotListUDSEntry

/*!
 * void KIOGallery3Protocol::slotStatUDSEntry ( const UDSEntry entry )
 * @brief Publish single item
 * @param entry single UDS entry
 * Accepts a single UDSEntry describing an item inside the item hierarchy.
 * Typically used to stat an item as requested by the calling scope. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::slotStatUDSEntry ( const UDSEntry entry )
{
  kDebug() << "(<UDSEntry>)" << entry.stringValue ( UDSEntry::UDS_NAME );
  statEntry ( entry );
} // KIOGallery3Protocol::slotStatUDSEntry

/*!
 * void KIOGallery3Protocol::slotData ( KIO::Job* job, const QByteArray& payload )
 * @brief Publish requested data
 * @param job     identifies the job this data was requested by
 * @param payload the payload as requested from the job
 * Accepts and forwards arbitrary data as requested by a job.
 * Typically used when downloading a file from the remote Gallery3 system. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::slotData ( KIO::Job* job, const QByteArray& payload )
{
  data ( payload );
} // KIOGallery3Protocol::slotData

/*!
 * void KIOGallery3Protocol::slotMimetype ( KIO::Job* job, const QString& type )
 * @brief Publish mimetype of an item
 * @param job  identifies the job this data was requested by
 * @param type the mimetype of an object as requested by the job
 * Accepts and forwards the mimetype of an object as requested by the calling scope.
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::slotMimetype ( KIO::Job* job, const QString& type )
{
  kDebug() << "(<mimetype>)" << type;
  mimeType ( type );
} // KIOGallery3Protocol::slotMimetype

//======================

/*!
 * void KIOGallery3Protocol::setHost ( const QString& host, g3index port, const QString& user, const QString& pass )
 * @brief Allows the calling scope to set connection details
 * @param host host where to contact a remote Gallery3 system
 * @param port tcp port where to contact a remote Gallery3 system
 * @param user the username used to contact a remote Gallery3 system
 * @param pass the pasword used to contact a remote Gallery3 system
 * Called when the target changes, the remote Gallery3 system.
 * This might be because a different system is to contacted or because the
 * authentication credentails change, typically the username.
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::setHost ( const QString& host, g3index port, const QString& user, const QString& pass )
{
  KDebug::Block block ( "KIOGallery3Protocol::setHost" );
  kDebug() << "(<host> <port> <user> <pass>)" << host << port << user << ( pass.isEmpty() ? "" : "<hidden password>" );
  try
  {
    selectConnection ( host, port, user, pass );
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::setHost

/*!
 * void KIOGallery3Protocol::copy ( const KUrl& src, const KUrl& dest, int permissions, JobFlags flags )
 * @brief Copies an item
 * @param src         url of the source item to be copied
 * @param dest        destiniy url where to copy the item to
 * @param permissions file permissions as to be set for the copied item
 * @param flags       flags describing the copy process
 * Called to copy an item in a direct manner.
 * Currently not implemented, so we return a predefined error.
 * This results in the calling scope performing a get&put combination instead. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::copy ( const KUrl& src, const KUrl& dest, int permissions, JobFlags flags )
{
  KDebug::Block block ( "KIOGallery3Protocol::copy" );
  kDebug() << "(<src url> <dest url> <permissions> <flags>)" << src << dest << permissions << flags;
  try
  {
    throw Exception ( Error(ERR_UNSUPPORTED_ACTION), i18n("copy action not supported") );
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::copy

/*!
 * void KIOGallery3Protocol::del ( const KUrl& targetUrl, bool isfile )
 * @brief Deletes an item
 * @param targetUrl url of the item to be deleted
 * @param isfile    signals if the itemis a photo/movie (file) or an directory (album)
 * Called to delete an item from within the hierarchy. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::del ( const KUrl& targetUrl, bool isfile )
{
  // note: isfile signals if a directory or a file is meant to be deleted
  KDebug::Block block ( "KIOGallery3Protocol::del" );
  kDebug() << "(<url> <isfile>)" << targetUrl << isfile;
  try
  {
    // remove item from remote gallery3
    G3Backend* backend = selectBackend ( targetUrl );
    G3Item* item = backend->itemByUrl ( targetUrl );
    backend->removeItem ( item );
    finished();
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::del

/*!
 * void KIOGallery3Protocol::get ( const KUrl& targetUrl )
 * @brief Get the content of an item
 * @param targetUrl url of the item holding the file to be retrieved
 * Called to retrieve the file represented by the referenced item.
 * The type of file retrieved depends on the type of item referenced. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::get ( const KUrl& targetUrl )
{
  KDebug::Block block ( "KIOGallery3Protocol::get" );
  kDebug() << "(<url>)" << targetUrl;
  try
  {
    G3Backend* backend = selectBackend ( targetUrl );
    G3Item* item = itemByUrl ( targetUrl );
    mimeType  ( item->mimetype()->name() );
    totalSize ( item->size() );
    switch ( item->type().toInt() )
    {
      case G3Type::ALBUM:
        backend->fetchCover ( item );
        finished ( );
        break;
      case G3Type::PHOTO:
      case G3Type::MOVIE:
        backend->fetchFile ( item );
        finished ( );
        break;
      case G3Type::TAG:
      case G3Type::COMMENT:
        backend->fetchFile ( item );
        finished ( );
        break;
      default:
      case G3Type::NONE:
        throw Exception ( Error(ERR_SLAVE_DEFINED),i18n("unknown item type in action 'get'") );
    } // switch type
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::get

/*!
 * void KIOGallery3Protocol::listDir ( const KUrl& targetUrl )
 * @brief Lists the members of an album
 * @param targetUrl url of the item (album) referenced
 * Called to list all member items as contained inside referenced item (album).
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::listDir ( const KUrl& targetUrl )
{
  KDebug::Block block ( "KIOGallery3Protocol::listDir" );
  kDebug() << "(<url>)" << targetUrl << targetUrl.scheme() << targetUrl.host() << targetUrl.path();
  try
  {
    if ( targetUrl.path().isEmpty() )
    {
      KUrl redirectUrl = targetUrl;
      redirectUrl.adjustPath ( KUrl::AddTrailingSlash );
      kDebug() << "redirecting to" << redirectUrl;
      redirection ( redirectUrl );
      finished ( );
    } // if
    else if ( QLatin1String("/")==targetUrl.path() )
    {
      G3Item* item = itemBase ( targetUrl );
      kDebug() << "listing base entries members";
      totalSize   ( item->countMembers() );
      connect ( item, SIGNAL(signalUDSEntry(const UDSEntry)),
                this, SLOT(slotListUDSEntry(const UDSEntry)) );
//      listEntries ( item->toUDSEntryList(TRUE) );
      item->toUDSEntryList(TRUE);
      listEntry ( UDSEntry(), TRUE );
      disconnect ( item, SIGNAL(signalUDSEntry(const UDSEntry)),
                   this, SLOT(slotListUDSEntry(const UDSEntry)) );
      finished ( );
    } // else if
    else
    {
      G3Item* item = itemByUrl ( targetUrl );
      kDebug() << "listing items members";
      totalSize   ( item->countMembers() );
      connect ( item, SIGNAL(signalUDSEntry(const UDSEntry)),
                this, SLOT(slotListUDSEntry(const UDSEntry)) );
      listEntries ( item->toUDSEntryList(TRUE) );
      listEntry ( UDSEntry(), TRUE );
      disconnect ( item, SIGNAL(signalUDSEntry(const UDSEntry)),
                   this, SLOT(slotListUDSEntry(const UDSEntry)) );
      finished ( );
    } // else
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::listDir

/*!
 * void KIOGallery3Protocol::mimetype ( const KUrl& targetUrl )
 * @brief Names the mimetype of a referenced item
 * @param targetUrl url of the referenced item
 * Called to request the mimetype of the referenced item.
 * The mimetype depends on the type of item:
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::mimetype ( const KUrl& targetUrl )
{
  KDebug::Block block ( "KIOGallery3Protocol::mimetype" );
  kDebug() << "(<url>)" << targetUrl;
  try
  {
    G3Item* item = itemByUrl ( targetUrl );
    mimeType ( item->mimetype()->name() );
    finished ( );
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::mimetype

/*!
 * void KIOGallery3Protocol::mkdir ( const KUrl& targetUrl, int permissions )
 * @brief Creates a new directory (album)
 * @param targetUrl   url of the item (album) to be created
 * @param permissions file permissions to be set for the created item
 * Called to create a new album item inside the hierarchy.
 * Also used when moving or copying trees of items. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::mkdir ( const KUrl& targetUrl, int permissions )
{
  KDebug::Block block ( "KIOGallery3Protocol::mkdir" );
  kDebug() << "(<url> <permissions>)" << targetUrl << permissions;
  try
  {
    G3Backend* backend = selectBackend ( targetUrl );
//    KUrl    parent = target.directory();
    QString filename = targetUrl.fileName();
    G3Item* parent   = backend->itemByPath ( targetUrl.directory() );
    kDebug() << QString("creating new album '%1' in album '%2'").arg(filename).arg(targetUrl.directory());
    backend->createItem ( parent, filename );
    finished ( );
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::mkdir

/*!
 * void KIOGallery3Protocol::put ( const KUrl& targetUrl, int permissions, KIO::JobFlags flags )
 * @brief Creates a new item according to additional data provided
 * @param targetUrl   url of item to be created inside the hierarchy
 * @param permissions file permissions to be set for the created item
 * @param flags       flags controlling the creation action
 * Called to create a new item inside the remote Gallery3 system.
 * Also uploads a file provided by the calling scope in case of photos and movies. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::put ( const KUrl& targetUrl, int permissions, KIO::JobFlags flags )
{
  KDebug::Block block ( "KIOGallery3Protocol::put" );
  kDebug() << "(<url, <permissions> <flags>)" << targetUrl << permissions << flags;
  try
  {
    // strategy: save data stream to temp file and http-post that
    QByteArray     buffer;
    KTemporaryFile file;
//    if ( ! file.open(QIODevice::WriteOnly) )
    if ( ! file.open() )
      throw Exception ( Error(ERR_COULD_NOT_WRITE),i18n("failed to generate temporary file '%1'").arg(file.fileName()) );
    kDebug() << "using temporary file" << file.fileName() << "to upload content";
    int read_count=0, write_count=0;
    do
    {
      dataReq();
      buffer.clear ( );
      read_count  = readData ( buffer );
      write_count = file.write ( buffer );
      if  ( read_count<0 || write_count<0)
        throw Exception ( Error(ERR_SLAVE_DEFINED),i18n("failed to buffer data in temporary file").arg(targetUrl.prettyUrl()) );
    } while ( read_count>0 ); // a return value of 0 (zero) means: no more data
    file.close ( );
    // upload stream as new file to remote gallery
    G3Backend* backend = selectBackend ( targetUrl );
    // upload temporary file to server
    QString source = KStandardDirs::locate ( "tmp", file.fileName() );
    if ( source.isEmpty() )
      throw Exception ( Error(ERR_INTERNAL),i18n("failure while handling temporary file '%1'").arg(file.fileName()) );
    KMimeType::Ptr mimetype = KMimeType::findByPath ( source );
    QString        filename = targetUrl.fileName ( );
    G3Item* parent = backend->itemByPath ( targetUrl.directory() );
    // the backend part handles the upload request
    G3File g3file ( filename, mimetype, source );
    backend->createItem ( parent, filename, &g3file );
    // cleanup
    file.deleteLater ( );
    finished ( );
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::put

/*!
 * void KIOGallery3Protocol::rename ( const KUrl& srcUrl, const KUrl& destUrl, KIO::JobFlags flags )
 * @brief Rename / move an item
 * @param srcUrl  url of the item to be renamed/moved
 * @param destUrl url describing where the item should be renamed/moved to
 * @param flags   flags controlling the action
 * Called to rename / move an item inside the items hierarchy. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::rename ( const KUrl& srcUrl, const KUrl& destUrl, KIO::JobFlags flags )
{
  KDebug::Block block ( "KIOGallery3Protocol::rename" );
  kDebug() << "(<src> <dest> <flags>)" << srcUrl << destUrl;
  try
  {
    // we support only few types of "renaming" / "moving", deny the rest
    if ( srcUrl.scheme()!=destUrl.scheme() )
      throw Exception ( Error(ERR_UNSUPPORTED_ACTION), i18n("moving of entities between different protocol schemes not supported") );
    if ( srcUrl.host()!=destUrl.host() )
      throw Exception ( Error(ERR_UNSUPPORTED_ACTION), i18n("moving of entities between different gallery hosts not supported") );
    if (   (srcUrl.directory()!=destUrl.directory())
        && (srcUrl.isParentOf(destUrl)||destUrl.isParentOf(srcUrl)) )
      throw Exception ( Error(ERR_UNSUPPORTED_ACTION), i18n("moving of entities between different galleries not supported") );
    G3Backend* backend = selectBackend ( srcUrl );
    G3Item* item   = backend->itemByUrl ( srcUrl );
    QHash<QString,QString> attributes;
    // the items parent is meant to be changed, when the paths differ ("move")
    if ( srcUrl.directory()!=destUrl.directory() )
    {
      G3Item* parent = backend->itemByPath ( destUrl.directory() );
      kDebug() << "moving item" << item->toPrintout() << "to new parent" << parent->toPrintout();
      attributes.insert ( "parent", parent->restUrl().url() );
    } // if
    // the items name is meant to be changed, if the filenames differ ("rename")
    if (  srcUrl.fileName()!=destUrl.fileName()  )
    {
      attributes.insert ( "name", destUrl.fileName() );
      kDebug() << "updating item name of" << item->toPrintout() << "to" << attributes["name"];
    }
    backend->updateItem ( item, attributes );
    finished ( );
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::rename

/*!
 * void KIOGallery3Protocol::stat ( const KUrl& targetUrl )
 * @brief Gives information about an existing item
 * @param targetUrl url of the item referenced
 * Called to request information describing the referenced item.
 * Typically called to prepare the visualization of the item. 
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::stat ( const KUrl& targetUrl )
{
  KDebug::Block block ( "KIOGallery3Protocol::stat" );
  kDebug() << "(<url>)" << targetUrl;
  try
  {
    if ( targetUrl.path().isEmpty() )
    {
      KUrl _new_url = targetUrl;
      _new_url.setPath(QLatin1String("/"));
      kDebug() << "redirecting to:" << _new_url;
      redirection ( _new_url );
      finished ( );
    }
    else if ( QLatin1String("/")==targetUrl.path() )
    {
      const G3Item* item = itemBase ( targetUrl );
      mimeType  ( item->mimetype()->name() );
      statEntry ( item->toUDSEntry() );
      finished  ( );
    }
    else
    {
      // non-root element
      const G3Item* item = itemByUrl ( targetUrl );
      mimeType  ( item->mimetype()->name() );
      statEntry ( item->toUDSEntry() );
      finished  ( );
    }
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::stat

/*!
 * void KIOGallery3Protocol::symlink ( const QString& target, const KUrl& dest, KIO::JobFlags flags )
 * @brief Creates a symlink to an existing item
 * @param target the name of the symlink to be created
 * @param dest   url of the item to be symlinked
 * @param flags  flags controlling the action
 * Called to create a symlink pointing to an item inside the hierarchy.
 * @todo do-something!
 * (sorry, not really a clue how this works currently)
 * there actually is something like a link in G3, but only provided by an additional module
 * might make sense to have a try with this...
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::symlink ( const QString& target, const KUrl& dest, KIO::JobFlags flags )
{
  KDebug::Block block ( "KIOGallery3Protocol::symlink" );
  kDebug() << "(<target> <dest> <flags>)" << target << dest << flags;
  try
  {
    throw Exception ( Error(ERR_UNSUPPORTED_ACTION),i18n("sorry, currently not implemented...") );
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::symlink

/*!
 * void KIOGallery3Protocol::special ( const QByteArray& data )
 * @brief Extends the set of methods of the slave
 * @param data arbitrary data required to perform a certain action
 * Called to perform a 'special' action, an extention of the standard actions defined for kio slaves.
 * in future this will be used to perform G3 specific actions like reordering of items, changing
 * item attributes beside name and position and so on.
 * The structure of the data is implementation specific, thus it is up to the implementing
 * slave to define it and to the calling scope to know about that :-)
 * @see KIOGallery3Protocol
 * @author Christian Reiner
 */
void KIOGallery3Protocol::special ( const QByteArray& data )
{
//! @todo: use this in combination with a service menu to control all aspects of an item: description and so on, maybe even the order of items (priority) ?
/*
Used for any command that is specific to this slave (protocol) Examples are : HTTP POST, mount and unmount (kio_file)
Parameters:
data  packed data; the meaning is completely dependent on the slave, but usually starts with an int for the command number. Document your slave's commands, at least in its header file.
Reimplemented in FileProtocol, and HTTPProtocol.
*/
  KDebug::Block block ( "KIOGallery3Protocol::special" );
  kDebug() << "(<data>)";
  try
  {
    throw Exception ( Error(ERR_UNSUPPORTED_ACTION),i18n("sorry, currently not implemented...") );
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::special

#include "kio_protocol_gallery3.moc"
