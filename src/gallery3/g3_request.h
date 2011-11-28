/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 81 $
 * $Date: 2011-08-13 13:08:50 +0200 (Sat, 13 Aug 2011) $
 */

/*!
 * @file
 * Defines class G3Request
 * An object of type G3Request represents all aspects of a single request to
 * the remote Gallery3 system. As such it acts as a translation between logical
 * requests as defined in the API and the way these are realized by using
 * http(s) requests against the remote Gallery3 system.
 * @see G3Request
 * @author Christian Reiner
 */
#ifndef G3_REQUEST_H
#define G3_REQUEST_H

#include <QString>
#include <QByteArray>
#include <QHash>
#include <QVariant>
#include <QBuffer>
#include <KUrl>
#include <KTemporaryFile>
#include <kio/http.h>
#include <kio/authinfo.h>
#include <kio/job.h>
#include "utility/defines.h"
#include "json/g3_json.h"
#include "entity/g3_type.h"
#include <kio/slavebase.h>

namespace KIO
{
  namespace Gallery3
  {
    class G3Backend;
    class G3Item;
    class G3File;

    /*!
     * @class G3Request
     * @brief Worker for requests to remote Gallery3 system
     * Implements a request and its evaluation against a remote Gallery3 system
     * The basic strategy of this class is to map each gallery3-request onto a
     * single kio-request, this way re-using the existing http-slave
     * implementations.
     * Note that the constructor is protected: dont ever attempt to directly
     * create an object of this class, use the static public methods defined
     * towards the end of the class definition. These offer better usability
     * in form of a function-call-interface.
     * Use one of the statical methods to generate am object of this class.
     * @author Christian Reiner
     */
    class G3Request
      : public QObject
      , public G3JsonParser
      , public G3JsonSerializer
    {
      class Members
      {
        public:
          Members  ( G3Backend* const backend, KIO::HTTP_METHOD method, const QString& service, const G3File* const file );
          G3Backend* const       backend;
          const KIO::HTTP_METHOD method;
          const QString          service;
          const G3File* const    file;     // request upload file
          KUrl                   requestUrl;
          KUrl                   finalUrl;
          //! @todo fixme: maybe we have to use QSharedPointer here... calling delete on m_job in the descructor often causes a segfault...
          KIO::TransferJob*      job;
          // to be sent
          QHash<QString,QString> header;   // request header items
          QHash<QString,QString> query;    // request query items
          QString                boundary; // multi-part boundary
          // to be received
          int                    status;   // http status code
          QMap<QString,QString>  meta;     // result meta data
          QByteArray             payload;  // result payload
          QVariant               result;
      }; // struct Members
      Q_OBJECT
      private:
        Members* const m;
      private:
        KUrl       webUrlWithQueryItems   ( KUrl url, const QHash<QString,QString>& query );
        QByteArray webFormPostPayload     ( const QHash<QString,QString>& query );
        QByteArray webFileFormPostPayload ( const QHash<QString,QString>& query, const G3File* const file );
      protected:
        G3Request ( G3Backend* const backend, KIO::HTTP_METHOD method, const QString& service=QLatin1String(""), const G3File* const file=NULL );
        ~G3Request ( );
        int            httpStatusCode ( );
        bool           retryWithChangedCredentials ( int attempt = 0 );
        void           addHeaderItem  ( const QString& key, const QString& value );
        void           addQueryItem   ( const QString& key, const QString& value, bool skipIfEmpty=FALSE );
        void           addQueryItem   ( const QString& key, G3Type value, bool skipIfEmpty=FALSE );
        void           addQueryItem   ( const QString& key, const QStringList& values, bool skipIfEmpty=FALSE );
        void           setup          ( );
        void           process        ( );
        void           evaluate       ( );
        QString        toString       ( );
        G3Item*        toItem         ( QVariant& entry );
        QList<G3Item*> toItems        ( );
        g3index        toItemId       ( QVariant& entry );
        QList<g3index> toItemIds      ( );
        inline G3Item* toItem         ( ) { return toItem(m->result); }
        inline g3index toItemId       ( ) { return toItemId(m->result); }
      signals:
        void signalRequestAuthInfo ( G3Backend* backend, AuthInfo& credentials, int attempt );
        void signalMessageBox      ( int& result, SlaveBase::MessageBoxType type, const QString &text, const QString &caption=QString(), const QString &buttonYes=i18n("&Yes"), const QString &buttonNo=i18n("&No") );
        void signalMessageBox      ( int& result, const QString &text, SlaveBase::MessageBoxType type, const QString &caption=QString(), const QString &buttonYes=i18n("&Yes"), const QString &buttonNo=i18n("&No"), const QString &dontAskAgainName=QString() );
      public:
        static bool           g3Check        ( G3Backend* const backend );
        static bool           g3Login        ( G3Backend* const backend, AuthInfo& credentials );
        static QList<G3Item*> g3GetItems     ( G3Backend* const backend, const QStringList& urls, G3Type type=G3Type::NONE );
        static QList<G3Item*> g3GetItems     ( G3Backend* const backend, g3index id, G3Type type=G3Type::NONE );
        static QList<g3index> g3GetAncestors ( G3Backend* const backend, G3Item* item );
        static g3index        g3GetAncestor  ( G3Backend* const backend, G3Item* item );
        static G3Item*        g3GetItem      ( G3Backend* const backend, g3index id, const QString& scope=QLatin1String("direct"), const QString& name=QLatin1String(""), bool random=FALSE, G3Type type=G3Type::NONE );
        static void           g3PostItem     ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes, const G3File* const file=NULL );
        static void           g3PutItem      ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes );
        static void           g3DelItem      ( G3Backend* const backend, g3index id );
        static g3index        g3SetItem      ( G3Backend* const backend, g3index id, const QString& name=QLatin1String(""), G3Type type=G3Type::NONE, const QByteArray& file=0 );
        static void           g3FetchObject  ( G3Backend* const backend, const KUrl& url );
    }; // class G3Request

  } // namespace Gallery3
} // namespace KIO

#endif // G3_REQUEST_H
