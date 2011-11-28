/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

/*!
 * @file
 * Definfition of the KDE protocols 'gallery3(s)' in form of a kio slave.
 * @author Christian Reiner
 */

#ifndef KIO_PROTOCOL_GALLERY3_H
#define KIO_PROTOCOL_GALLERY3_H

#include <QtCore/QMutex>
#include <QString>
#include <QStringList>
#include <QMap>
#include <kio/global.h>
#include <kio/udsentry.h>
#include <ktemporaryfile.h>
#include "utility/defines.h"
#include "protocol/kio_protocol.h"

namespace KIO
{
  namespace Gallery3
  {
    class G3Backend;
    class G3Item;
    
    /*!
     * @class KIOGallery3Protocol
     * @brief KIO Protocol 'gallery3'
     * This class defines the gallery3 specific aspects of the kde slave protocol.
     * Derived from the more general class KIOProtocol it only adds specific behaviour. 
     * @author Christian Reiner
     * @see KIOProtocol
     * @see KIOGallery3Protocol::Member
    */
    class KIOGallery3Protocol
      : public QObject
      , public KIOProtocol
    {
      Q_OBJECT
      private:
        class Members
        {
          public:
            inline Members ( ) : backends(QHash<QString,G3Backend*>()) { }
            struct
            {
              QString host;
              g3index port;
              QString user;
              QString pass;
            } connection;
            QHash<QString,G3Backend*> backends;
        }; // class Members
      private:
        Members* const m;
      protected:
        void           selectConnection ( const QString& host, qint16 port, const QString& user, const QString& pass );
        G3Backend*     selectBackend    ( const KUrl& base );
        G3Item*        itemBase         ( const KUrl& itemUrl );
        G3Item*        itemByUrl        ( const KUrl& itemUrl );
        QList<G3Item*> itemsByUrl       ( const KUrl& itemUrl );
      public:
        inline const QString protocol ( ) { return QString("gallery3"); }
        KIOGallery3Protocol ( const QByteArray &pool, const QByteArray &app, QObject* parent=0 );
        virtual ~KIOGallery3Protocol();
      public slots:
        void slotRequestAuthInfo ( G3Backend* backend, AuthInfo& credentials, int attempt );
        void slotMessageBox      ( int& result, MessageBoxType type, const QString &text, const QString &caption=QString(), const QString &buttonYes=i18n("&Yes"), const QString &buttonNo=i18n("&No") );
        void slotMessageBox      ( int& result, const QString &text, MessageBoxType type, const QString &caption=QString(), const QString &buttonYes=i18n("&Yes"), const QString &buttonNo=i18n("&No"), const QString &dontAskAgainName=QString() );
        void slotListUDSEntries  ( const UDSEntryList entries );
        void slotListUDSEntry    ( const UDSEntry entry );
        void slotStatUDSEntry    ( const UDSEntry entry );
        void slotData            ( KIO::Job* job, const QByteArray& data );
        void slotMimetype        ( KIO::Job* job, const QString& type );
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

#endif // KIO_PROTOCOL_GALLERY3_H
