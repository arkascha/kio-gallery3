/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

#ifndef KIO_GALLERY3_PROTOCOL_H
#define KIO_GALLERY3_PROTOCOL_H

#include <QtCore/QMutex>
#include <QString>
#include <QStringList>
#include <QMap>
#include <kio/global.h>
#include <kio/udsentry.h>
#include <ktemporaryfile.h>
#include "utility/defines.h"
#include "gallery3/g3_backend.h"
#include "kio_protocol.h"

namespace KIO
{
  namespace Gallery3
  {
    /**
    */
    class KIOGallery3Protocol
      : public QObject
      , public KIOProtocol
    {
      Q_OBJECT
      private:
        struct
        {
          QString host;
          g3index port;
          QString user;
          QString pass;
        } m_connection;
      protected:
        QHash<QString,G3Backend*> m_backends;
        void               selectConnection ( const QString& host, g3index port, const QString& user, const QString& pass );
        G3Backend*         selectBackend    ( const KUrl& base );
        int                countBackends    ( );
        G3Item*            itemBase         ( const KUrl& itemUrl );
        G3Item*            itemByUrl        ( const KUrl& itemUrl );
        QList<G3Item*>     itemsByUrl       ( const KUrl& itemUrl );
      public:
        inline const QString protocol ( ) { return QString("gallery3"); };
        KIOGallery3Protocol ( const QByteArray &pool, const QByteArray &app, QObject* parent=0 );
        virtual ~KIOGallery3Protocol();
      public slots:
        void slotListUDSEntries ( const UDSEntryList entries );
        void slotListUDSEntry   ( const UDSEntry entry );
        void slotStatUDSEntry   ( const UDSEntry entry );
      public:
        void setHost  ( const QString& host, g3index port, const QString& user, const QString& pass );
        void copy     ( const KUrl& src, const KUrl& dest, int permissions, JobFlags flags );
        void del      ( const KUrl& url, bool isfile );
        void get      ( const KUrl& url );
        void listDir  ( const KUrl& url );
        void mimetype ( const KUrl& url );
        void mkdir    ( const KUrl& url, int permissions );
        void put      ( const KUrl& url, int permissions, KIO::JobFlags flags );
        void rename   ( const KUrl& src, const KUrl& dest, KIO::JobFlags flags );
//        void setModificationTime ( const KUrl& url, const QDateTime& mtime );
        void stat     ( const KUrl& url );
        void symlink  ( const QString& target, const KUrl& dest, KIO::JobFlags flags );
        void special  ( const QByteArray& data );
    }; // class KIOGallery3Protocol

  } // namespace Gallery3
} // namespace KIO

#endif // KIO_GALLERY3_PROTOCOL_H
