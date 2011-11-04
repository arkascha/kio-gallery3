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
#include <kmimetype.h>
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

int KIOGallery3Protocol::countBackends ( )
{
  kDebug() << "(<>)";
  return m_backends.count ( );
} // KIOGallery3Protocol::countBackends

void KIOGallery3Protocol::selectConnection ( const QString& host, g3index port, const QString& user, const QString& pass )
{
  kDebug() << "(<host> <port> <user> <pass>)" << host << port << user << "<hidden password>";
  m_connection.host = host;
  m_connection.port = port;
  m_connection.user = user;
  m_connection.pass = pass;
} // KIOGallery3Protocol::selectConnection

G3Backend* KIOGallery3Protocol::selectBackend ( const KUrl& targetUrl )
{
  MY_KDEBUG_BLOCK ( "<KIOGallery3Protocol::selectBackend>" );
  kDebug() << "(<url>)" << targetUrl.prettyUrl();
  // enhance the url with the current (updated) connection details
  KUrl itemUrl = KUrl ( QString("%1://%2%3").arg(targetUrl.scheme()).arg(m_connection.host).arg(targetUrl.path()) );
  itemUrl.adjustPath ( KUrl::RemoveTrailingSlash );
  kDebug() << "corrected url:" << itemUrl.prettyUrl();
  G3Backend* backend = G3Backend::instantiate ( m_backends, itemUrl );
  return backend;
} // KIOGallery3Protocol::selectBackend

G3Item* KIOGallery3Protocol::itemBase ( const KUrl& itemUrl )
{
  MY_KDEBUG_BLOCK ( "<KIOGallery3Protocol::itemBase>" );
  kDebug() << "(<url>)" << itemUrl.prettyUrl();
  G3Backend* backend = selectBackend ( itemUrl );
  return backend->itemBase ( );
} // KIOGallery3Protocol::itemBase

G3Item* KIOGallery3Protocol::itemByUrl ( const KUrl& itemUrl )
{
  MY_KDEBUG_BLOCK ( "<KIOGallery3Protocol::itemByUrl>" );
  kDebug() << "(<url>)" << itemUrl.prettyUrl();
  G3Backend* backend = selectBackend ( itemUrl );
  QString path = KUrl::relativeUrl ( backend->baseUrl(), itemUrl );
  return backend->itemByPath ( path );
} // KIOGallery3Protocol::itemByUrl

QList<G3Item*> KIOGallery3Protocol::itemsByUrl ( const KUrl& itemUrl )
{
  MY_KDEBUG_BLOCK ( "<KIOGallery3Protocol::itemsByUrl>" );
  kDebug() << "(<url>)" << itemUrl.prettyUrl();
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
  kDebug() << "(<host> <port> <user> <pass>)" << host << port << user << "<hidden password>";
  selectConnection ( host, port, user, pass );
} // KIOGallery3Protocol::setHost

void KIOGallery3Protocol::copy ( const KUrl& src, const KUrl& dest, int permissions, JobFlags flags )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::copy" );
  kDebug() << "(<src url> <dest url> <permissions> <flags>)" << src.prettyUrl() << dest.prettyUrl() << permissions << flags;
  try
  {
    throw Exception ( Error(ERR_UNSUPPORTED_ACTION),
                      QString("copy action not supported") );
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::copy

/**
 */
void KIOGallery3Protocol::del ( const KUrl& targetUrl, bool isfile )
{
  // note: isfile signals if a directory or a file is meant to be deleted
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::del" );
  kDebug() << "(<url> <isfile>)" << targetUrl.prettyUrl ( ) << isfile;
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

/**
 */
void KIOGallery3Protocol::get ( const KUrl& targetUrl )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::get" );
  kDebug() << "(<url>)" << targetUrl.prettyUrl ( ) ;
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
        kDebug() << "redirecting to:" << item->fileUrl().prettyUrl ( );
        redirection ( item->fileUrl() );
        finished ( );
      case Entity::G3Type::TAG:
      case Entity::G3Type::COMMENT:
        kDebug() << "redirecting to:" << item->webUrl().prettyUrl ( );
        redirection ( item->webUrl() );
        finished ( );
    } // switch type
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::get

/**
 * Lists all entities as present under a given url
 */
void KIOGallery3Protocol::listDir ( const KUrl& targetUrl )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::listDir" );
  kDebug() << "(<url>)" << targetUrl.prettyUrl ( ) << targetUrl.scheme() << targetUrl.host() << targetUrl.path();
  try
  {
    if ( targetUrl.path().isEmpty() )
    {
      KUrl redirectUrl = targetUrl;
      redirectUrl.adjustPath ( KUrl::AddTrailingSlash );
      kDebug() << "redirecting to" << redirectUrl.prettyUrl();
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
      finished ( );
    } // else
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::listDir

/**
 */
void KIOGallery3Protocol::mimetype ( const KUrl& targetUrl )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::mimetype" );
  kDebug() << "(<url>)" << targetUrl.prettyUrl ( );
  try
  {
    G3Item* item = itemByUrl ( targetUrl );
    mimeType ( item->mimetype()->name() );
    finished();
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::mimetype

/**
 */
void KIOGallery3Protocol::mkdir ( const KUrl& targetUrl, int permissions )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::mkdir" );
  kDebug() << "(<url> <permissions>)" << targetUrl.prettyUrl ( ) << permissions;
  try
  {
    G3Backend* backend = selectBackend ( targetUrl );
//    KUrl    parent = target.directory();
    QString filename = targetUrl.fileName();
    G3Item* parent   = backend->itemByPath ( targetUrl.directory() );
    kDebug() << QString("creating new album '%1' in album '%2'").arg(filename).arg(targetUrl.directory());
    backend->createAlbum ( parent, filename );
    finished();
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::mkdir

/**
 */
void KIOGallery3Protocol::put ( const KUrl& targetUrl, int permissions, KIO::JobFlags flags )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::put" );
  kDebug() << "(<url, <permissions> <flags>)" << targetUrl.prettyUrl ( ) << permissions << flags;
  try
  {
    // strategy: save data stream to temp file and http-post that
    int            ret_val;
    QByteArray     buffer;
    KTemporaryFile file;
//    if ( ! file.open(QIODevice::WriteOnly) )
    if ( ! file.open() )
      throw Exception ( Error(ERR_COULD_NOT_WRITE),
                        QString("failed to generate temporary file '%1'").arg(file.fileName()) );
    kDebug() << "using temporary file" << file.fileName() << "to upload content";
    do
    {
      dataReq();
      int ret_val = readData ( buffer );
      if  ( ret_val<0 )
        throw Exception ( Error(ERR_COULD_NOT_READ), targetUrl.prettyUrl() ); // FIXME: show source file instead of target file
      file.write ( buffer );
    } while ( ret_val!=0 ); // a return value of 0 (zero) means: no more data
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
    G3Item* parent = backend->itemByPath ( targetUrl.path() );
    Entity::G3Type filetype;
    if ( mimetype->is("image") )
      filetype = Entity::G3Type::PHOTO;
    else if ( mimetype->is("video") )
      filetype = Entity::G3Type::MOVIE;
    else
      throw Exception ( Error(ERR_SLAVE_DEFINED),
                        QString("unsupported file type '%1' ( %2 )").arg(mimetype->name()).arg(mimetype->comment()) );
    // the backend part handles the upload request
    backend->createItem ( file, parent, filetype, filename );

    file.deleteLater ( );
    finished ( );
  }
  catch ( Exception &e ) { error ( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::put

void KIOGallery3Protocol::rename ( const KUrl& srcUrl, const KUrl& destUrl, KIO::JobFlags flags )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::rename" );
  kDebug() << "(<src> <dest> <flags>)" << srcUrl.prettyUrl() << destUrl.prettyUrl();
  try
  {
    if ( srcUrl.scheme()!=destUrl.scheme() )
    {
      kDebug() << QString("moving of entities between different protocol schemes not supported (%1/%2)")
                        .arg(srcUrl.scheme()).arg(destUrl.scheme());
      throw Exception ( Error(ERR_UNSUPPORTED_ACTION),
                        QString("moving of entities between different protocol schemes not supported (%1=>%2)").arg(srcUrl.scheme()).arg(destUrl.scheme()) );
    }
    G3Backend* backend = selectBackend ( srcUrl );
    G3Item* item = backend->itemByUrl ( srcUrl );
    QHash<QString,QString> attributes;
    attributes.insert ( "name", destUrl.fileName() );
    backend->updateItem ( item, attributes );
    finished ( );
  }
  catch ( Exception &e ) { error ( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::rename

/**
 */
void KIOGallery3Protocol::stat ( const KUrl& targetUrl )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::stat" );
  kDebug() << "(<url>)" << targetUrl.prettyUrl ( );
  try
  {
    if ( targetUrl.path().isEmpty() )
    {
      KUrl _new_url = targetUrl;
      _new_url.setPath("/");
      kDebug() << "redirecting to:" << _new_url.prettyUrl ( );
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
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::stat

/**
 * (sorry, not really a clue how this works currently)
 * TODO FIXME do-something!
 */
void KIOGallery3Protocol::symlink ( const QString& target, const KUrl& dest, KIO::JobFlags flags )
{
  MY_KDEBUG_BLOCK ( "KIOGallery3Protocol::symlink" );
  kDebug() << "(<target> <dest> <flags>)" << target << dest.prettyUrl() << flags;
  try
  {
    throw Exception ( Error(ERR_UNSUPPORTED_ACTION),
                        QString("sorry, currently not implemented...") );
  }
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
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
  catch ( Exception &e ) { error( e.getCode(), e.getText() ); }
} // KIOGallery3Protocol::special

#include "kio_gallery3_protocol.moc"
