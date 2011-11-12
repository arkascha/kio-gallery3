/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
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
#include "utility/debug.h"
#include "utility/exception.h"
#include "gallery3/g3_backend.h"
#include "kio_gallery3_protocol.h"
#include "entity/g3_item.h"

using namespace KIO;
using namespace KIO::Gallery3;

/**
 * void KIOGallery3Protocol::selectConnection ( const QString& host, g3index port, const QString& user, const QString& pass )
 *
 * param: const QString& host ( host to connect to )
 * param: g3index port ( tcp port to connect to )
 * param: const QString& user ( user to authenticate as )
 * param: const QString& pass ( password to authenticate with )
 * description:
 * accepts the connection details from the controlling slave interface and stores them in a persistant manner
 */
void KIOGallery3Protocol::selectConnection ( const QString& host, qint16 port, const QString& user, const QString& pass )
{
  kDebug() << "(<host> <port> <user> <pass>)" << host << port << user << ( pass.isEmpty() ? "" : "<hidden password>" );
  m_connection.host = host;
  m_connection.port = port;
  m_connection.user = user;
  m_connection.pass = pass;
} // KIOGallery3Protocol::selectConnection

/**
 * G3Backend* KIOGallery3Protocol::selectBackend ( const KUrl& targetUrl )
 *
 * param: const KUrl& targetUrl ( requested url )
 * returns: G3Backend* ( existing or freshly created backend associated with the given targetUrl )
 * description:
 * standardizes the given target url to form a clean and reliable base url used to identify the associated backend to used
 * note that a change of the user name (authentication) creates a different url and by this a different backend
 * this is desired, the tree of items might look completely different so we want to create a fresh backend
 */
G3Backend* KIOGallery3Protocol::selectBackend ( const KUrl& targetUrl )
{
  MY_KDEBUG_BLOCK ( "<KIOGallery3Protocol::selectBackend>" );
  kDebug() << "(<url>)" << targetUrl;
  // enhance the url with the current (updated) connection details
  KUrl itemUrl;
  itemUrl = KUrl ( QString("%1://%2%3%4%5")
                          .arg( targetUrl.scheme() )
                          .arg( m_connection.user.isEmpty() ? "" : QString("%1@").arg(m_connection.user) )
                          .arg( m_connection.host )
                          .arg( 0==m_connection.port ? "" : QString(":%1").arg(m_connection.port) )
                          .arg( targetUrl.path() ) );
  itemUrl.adjustPath ( KUrl::RemoveTrailingSlash );
  kDebug() << "corrected url:" << itemUrl;
  G3Backend* backend = G3Backend::instantiate ( this, m_backends, itemUrl );
  return backend;
} // KIOGallery3Protocol::selectBackend

G3Item* KIOGallery3Protocol::itemBase ( const KUrl& itemUrl )
{
  MY_KDEBUG_BLOCK ( "<KIOGallery3Protocol::itemBase>" );
  kDebug() << "(<url>)" << itemUrl;
  G3Backend* backend = selectBackend ( itemUrl );
  return backend->itemBase ( );
} // KIOGallery3Protocol::itemBase

G3Item* KIOGallery3Protocol::itemByUrl ( const KUrl& itemUrl )
{
  MY_KDEBUG_BLOCK ( "<KIOGallery3Protocol::itemByUrl>" );
  kDebug() << "(<url>)" << itemUrl;
  G3Backend* backend = selectBackend ( itemUrl );
  QString path = KUrl::relativeUrl ( backend->baseUrl(), itemUrl );
  return backend->itemByPath ( path );
} // KIOGallery3Protocol::itemByUrl

QList<G3Item*> KIOGallery3Protocol::itemsByUrl ( const KUrl& itemUrl )
{
  MY_KDEBUG_BLOCK ( "<KIOGallery3Protocol::itemsByUrl>" );
  kDebug() << "(<url>)" << itemUrl;
  G3Backend* backend = selectBackend ( itemUrl );
  QString path = KUrl::relativeUrl ( backend->baseUrl(), itemUrl );
  return backend->membersByItemPath ( path );
} // KIOGallery3Protocol::itemsByUrl

//==========

/**
 * The standard constructor, nothing special here.
 * A fresh object is handled to the generic interface class this class derives from.
 * That object is initialized by refreshing it's nodes collection and destroyed again locally in the destructor. 
 */
KIOGallery3Protocol::KIOGallery3Protocol ( const QByteArray &pool, const QByteArray &app, QObject* parent )
  : QObject ( parent )
  , KIOProtocol ( pool, app, protocol() )
{
  MY_KDEBUG_BLOCK ( "<slave setup>" );
  try
  {
    // initialize the wrappers nodes
    //m_galleries->refreshNodes ( );
  }
  catch ( Exception &e ) { error ( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::KIOGallery3Protocol

/**
 * The destructor cleans up
 */
KIOGallery3Protocol::~KIOGallery3Protocol ( )
{
  MY_KDEBUG_BLOCK ( "<slave shutdown>" );
  try
  {
    kDebug() << "deleting existing backends";
    QHash<QString,G3Backend*>::const_iterator backend;
    for ( backend=m_backends.constBegin(); backend!=m_backends.constEnd(); backend++ )
    {
      kDebug() << "removing backend" << backend.value()->toPrintout();
      delete backend.value();
    }
  }
  catch ( Exception &e )
  {
    error ( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::~KIOGallery3Protocol

//======================

void KIOGallery3Protocol::slotRequestAuthInfo ( G3Backend* backend, AuthInfo& credentials, int attempt )
{
  MY_KDEBUG_BLOCK ( "<KIOGallery3Protocol::slotRequestAuthInfo>" );
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
  throw Exception ( Error(ERR_ABORTED), QString("Authentication cancelled to '%1'").arg(credentials.url.prettyUrl()) );
} // KIOGallery3Protocol::slotRequireAuthInfo

void KIOGallery3Protocol::slotMessageBox ( int& result, SlaveBase::MessageBoxType type, const QString &text, const QString &caption, const QString &buttonYes, const QString &buttonNo )
{
  result = messageBox ( type, text, caption, buttonYes, buttonNo );
  if ( 0==result )
    throw Exception ( Error(ERR_INTERNAL), QString("Communication error during user feedback") );
  kDebug() << text << caption << ">>" << result;
} // KIOGallery3Protocol::slotMessageBox

void KIOGallery3Protocol::slotMessageBox ( int& result, const QString &text, SlaveBase::MessageBoxType type, const QString &caption, const QString &buttonYes, const QString &buttonNo, const QString &dontAskAgainName )
{
  result = messageBox ( text, type, caption, buttonYes, buttonNo, dontAskAgainName );
  if ( 0==result )
    throw Exception ( Error(ERR_INTERNAL), QString("Communication error during user feedback") );
  kDebug() << text << caption << ">>" << result << dontAskAgainName;
} // KIOGallery3Protocol::slotMessageBox

void KIOGallery3Protocol::slotListUDSEntries ( const UDSEntryList entries )
{
  kDebug() << "(<UDSEntries[count]>)" << entries.count();
  listEntries ( entries );
  listEntry ( UDSEntry(), FALSE );
} // KIOGallery3Protocol::slotListUDSEntries

void KIOGallery3Protocol::slotListUDSEntry ( const UDSEntry entry )
{
  kDebug() << "(<UDSEntry>)" << entry.stringValue ( UDSEntry::UDS_NAME );
  listEntry ( entry, FALSE );
} // KIOGallery3Protocol::slotListUDSEntry

void KIOGallery3Protocol::slotStatUDSEntry ( const UDSEntry entry )
{
  kDebug() << "(<UDSEntry>)" << entry.stringValue ( UDSEntry::UDS_NAME );
  statEntry ( entry );
} // KIOGallery3Protocol::slotStatUDSEntry

//======================

void KIOGallery3Protocol::setHost ( const QString& host, g3index port, const QString& user, const QString& pass )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::setHost" );
  kDebug() << "(<host> <port> <user> <pass>)" << host << port << user << ( pass.isEmpty() ? "" : "<hidden password>" );
  try
  {
    selectConnection ( host, port, user, pass );
  }
  catch ( Exception &e )
  {
    messageBox ( Information, QString("Error %1:\n %2").arg(e.getCode()).arg(e.getText()), QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::setHost

void KIOGallery3Protocol::copy ( const KUrl& src, const KUrl& dest, int permissions, JobFlags flags )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::copy" );
  kDebug() << "(<src url> <dest url> <permissions> <flags>)" << src << dest << permissions << flags;
  try
  {
    throw Exception ( Error(ERR_UNSUPPORTED_ACTION),
                      QString("copy action not supported") );
  }
  catch ( Exception &e )
  {
    messageBox ( Information, QString("Error %1:\n %2").arg(e.getCode()).arg(e.getText()), QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::copy

/**
 */
void KIOGallery3Protocol::del ( const KUrl& targetUrl, bool isfile )
{
  // note: isfile signals if a directory or a file is meant to be deleted
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::del" );
  kDebug() << "(<url> <isfile>)" << targetUrl << isfile;
  try
  {
    // remove item from remote gallery3
    G3Backend* backend = selectBackend ( targetUrl );
    G3Item* item = backend->itemByUrl ( targetUrl );
    backend->removeItem ( item );
    finished();
  }
  catch ( Exception &e )
  {
    messageBox ( SlaveBase::Information, "removal of item failed", QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::del

/**
 */
void KIOGallery3Protocol::get ( const KUrl& targetUrl )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::get" );
  kDebug() << "(<url>)" << targetUrl;
  try
  {
    G3Item* item = itemByUrl ( targetUrl );
    switch ( item->type().toInt() )
    {
      default:
      case Entity::G3Type::NONE:
        throw Exception ( Error(ERR_SLAVE_DEFINED),
                          QString("unknown item type in action 'get'") );
      case Entity::G3Type::ALBUM:
        throw Exception ( Error(ERR_SLAVE_DEFINED),
                          QString("no way to 'get' a folder entry...") );
      case Entity::G3Type::PHOTO:
      case Entity::G3Type::MOVIE:
        kDebug() << "redirecting to:" << item->fileUrl();
        redirection ( item->fileUrl() );
        finished ( );
      case Entity::G3Type::TAG:
      case Entity::G3Type::COMMENT:
        kDebug() << "redirecting to:" << item->webUrl();
        redirection ( item->webUrl() );
        finished ( );
    } // switch type
  }
  catch ( Exception &e )
  {
    messageBox ( Information, QString("Error %1:\n %2").arg(e.getCode()).arg(e.getText()), QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::get

/**
 * Lists all entities as present under a given url
 */
void KIOGallery3Protocol::listDir ( const KUrl& targetUrl )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::listDir" );
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
    else if ( "/"==targetUrl.path() )
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
  catch ( Exception &e )
  {
    messageBox ( Information, QString("Error %1:\n %2").arg(e.getCode()).arg(e.getText()), QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::listDir

/**
 */
void KIOGallery3Protocol::mimetype ( const KUrl& targetUrl )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::mimetype" );
  kDebug() << "(<url>)" << targetUrl;
  try
  {
    G3Item* item = itemByUrl ( targetUrl );
    mimeType ( item->mimetype()->name() );
    finished();
  }
  catch ( Exception &e )
  {
    messageBox ( Information, QString("Error %1:\n %2").arg(e.getCode()).arg(e.getText()), QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::mimetype

/**
 */
void KIOGallery3Protocol::mkdir ( const KUrl& targetUrl, int permissions )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::mkdir" );
  kDebug() << "(<url> <permissions>)" << targetUrl << permissions;
  try
  {
    G3Backend* backend = selectBackend ( targetUrl );
//    KUrl    parent = target.directory();
    QString filename = targetUrl.fileName();
    G3Item* parent   = backend->itemByPath ( targetUrl.directory() );
    kDebug() << QString("creating new album '%1' in album '%2'").arg(filename).arg(targetUrl.directory());
    backend->createItem ( parent, filename );
    finished();
  }
  catch ( Exception &e )
  {
    messageBox ( Information, QString("Error %1:\n %2").arg(e.getCode()).arg(e.getText()), QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::mkdir

/**
 */
void KIOGallery3Protocol::put ( const KUrl& targetUrl, int permissions, KIO::JobFlags flags )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::put" );
  kDebug() << "(<url, <permissions> <flags>)" << targetUrl << permissions << flags;
  try
  {
    // strategy: save data stream to temp file and http-post that
    QByteArray     buffer;
    KTemporaryFile file;
//    if ( ! file.open(QIODevice::WriteOnly) )
    if ( ! file.open() )
      throw Exception ( Error(ERR_COULD_NOT_WRITE),
                        QString("failed to generate temporary file '%1'").arg(file.fileName()) );
    kDebug() << "using temporary file" << file.fileName() << "to upload content";
    int read_count=0, write_count=0;
    do
    {
      dataReq();
      buffer.clear ( );
      read_count  = readData ( buffer );
      write_count = file.write ( buffer );
      if  ( read_count<0 || write_count<0)
        throw Exception ( Error(ERR_SLAVE_DEFINED),
                          QString("failed to buffer data in temporary file").arg(targetUrl.prettyUrl()) );
    } while ( read_count>0 ); // a return value of 0 (zero) means: no more data
    file.close ( );
    // upload stream as new file to remote gallery
    G3Backend* backend = selectBackend ( targetUrl );
    // upload temporary file to server
    QString source = KStandardDirs::locate ( "tmp", file.fileName() );
    if ( source.isEmpty() )
      throw Exception ( Error(ERR_INTERNAL),
                        QString("failure while handling temporary file '%1'").arg(file.fileName()) );
    KMimeType::Ptr mimetype = KMimeType::findByPath ( source );
    QString        filename = targetUrl.fileName ( );
    G3Item* parent = backend->itemByPath ( targetUrl.directory() );
    // the backend part handles the upload request
    Entity::G3File g3file ( filename, mimetype, source );
    backend->createItem ( parent, filename, &g3file );
    // cleanup
    file.deleteLater ( );
    finished ( );
  }
  catch ( Exception &e )
  {
    messageBox ( Information, QString("Error %1:\n %2").arg(e.getCode()).arg(e.getText()), QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::put

void KIOGallery3Protocol::rename ( const KUrl& srcUrl, const KUrl& destUrl, KIO::JobFlags flags )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::rename" );
  kDebug() << "(<src> <dest> <flags>)" << srcUrl << destUrl;
  try
  {
    // we support only few types of "renaming" / "moving", deny the rest
    if ( srcUrl.scheme()!=destUrl.scheme() )
      throw Exception ( Error(ERR_UNSUPPORTED_ACTION), QString("moving of entities between different protocol schemes not supported") );
    if ( srcUrl.host()!=destUrl.host() )
      throw Exception ( Error(ERR_UNSUPPORTED_ACTION), QString("moving of entities between different gallery hosts not supported") );
    if (   (srcUrl.directory()!=destUrl.directory())
        && (srcUrl.isParentOf(destUrl)||destUrl.isParentOf(srcUrl)) )
      throw Exception ( Error(ERR_UNSUPPORTED_ACTION), QString("moving of entities between different galleries not supported") );
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
  catch ( Exception &e )
  {
    messageBox ( Information, QString("Error %1:\n %2").arg(e.getCode()).arg(e.getText()), QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::rename

/**
 */
void KIOGallery3Protocol::stat ( const KUrl& targetUrl )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::stat" );
  kDebug() << "(<url>)" << targetUrl;
  try
  {
    if ( targetUrl.path().isEmpty() )
    {
      KUrl _new_url = targetUrl;
      _new_url.setPath("/");
      kDebug() << "redirecting to:" << _new_url;
      redirection ( _new_url );
      finished ( );
      return;
    }
    else if ( "/"==targetUrl.path() )
    {
      const G3Item* item = itemBase ( targetUrl );
      statEntry ( item->toUDSEntry() );
      finished ( );
      return;
    }
    else
    {
      // non-root element
      const G3Item* item = itemByUrl ( targetUrl );
      statEntry ( item->toUDSEntry() );
      finished ( );
    }
  }
  catch ( Exception &e )
  {
    messageBox ( Information, QString("Error %1:\n %2").arg(e.getCode()).arg(e.getText()), QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::stat

/**
 * (sorry, not really a clue how this works currently)
 * TODO FIXME do-something!
 */
void KIOGallery3Protocol::symlink ( const QString& target, const KUrl& dest, KIO::JobFlags flags )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::symlink" );
  kDebug() << "(<target> <dest> <flags>)" << target << dest << flags;
  try
  {
    throw Exception ( Error(ERR_UNSUPPORTED_ACTION),
                        QString("sorry, currently not implemented...") );
  }
  catch ( Exception &e )
  {
    messageBox ( Information, QString("Error %1:\n %2").arg(e.getCode()).arg(e.getText()), QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::symlink

void KIOGallery3Protocol::special ( const QByteArray& data )
{
// TODO: use this in combination with a service menu to control all aspects of an item: description and so on, maybe even the order of items (priority) ?
/*
Used for any command that is specific to this slave (protocol) Examples are : HTTP POST, mount and unmount (kio_file)
Parameters:
data  packed data; the meaning is completely dependent on the slave, but usually starts with an int for the command number. Document your slave's commands, at least in its header file.
Reimplemented in FileProtocol, and HTTPProtocol.
*/
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::special" );
  kDebug() << "(<data>)";
  try
  {
    throw Exception ( Error(ERR_UNSUPPORTED_ACTION),
                        QString("sorry, currently not implemented...") );
  }
  catch ( Exception &e )
  {
    messageBox ( Information, QString("Error %1:\n %2").arg(e.getCode()).arg(e.getText()), QLatin1String("Failure") );
    error( e.getCode(), e.getText() );
  }
} // KIOGallery3Protocol::special

#include "kio_gallery3_protocol.moc"
