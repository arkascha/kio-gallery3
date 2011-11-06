/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 81 $
 * $Date: 2011-08-13 13:08:50 +0200 (Sat, 13 Aug 2011) $
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
#include "gallery3/g3_backend.h"
#include "entity/g3_type.h"
#include "entity/g3_file.h"
#include "entity/g3_item.h"
#include "entity/g3_collection.h"

namespace KIO
{
  namespace Gallery3
  {

    /**
     * class G3Request
     * This class implements a request and its evaluation towards a remote gallery3 system.
     * 
     * The basic strategy of this class is to map each gallery3-request onto a single kio-request, this way 
     * re-using the existing http-slave implementations. 
     * 
     * Note that the constructor is protected: dont ever attempt to directly create an object of this class,
     * use the static public methods defined towards the end of the class definition. These offer better
     * usability in form of a function-call-interface.
     */
    class G3Request
      : public G3JsonParser
      , public G3JsonSerializer
    {
      private:
        G3Backend* const            m_backend;
        const KIO::HTTP_METHOD      m_method;
        const QString               m_service;
        const Entity::G3File* const m_file;     // request upload file
        KUrl                        m_requestUrl;
        KUrl                        m_finalUrl;
        KIO::TransferJob*           m_job;
        // to be sent
        QHash<QString,QString>      m_header;   // request header items
        QHash<QString,QString>      m_query;    // request query items
        QString                     m_boundary; // multi-part boundary
        // to be receiced
        QMap<QString,QString>       m_meta;     // result meta data
        QByteArray                  m_payload;  // result payload
        QVariant                    m_result;
        KUrl       webUrlWithQueryItems   ( KUrl url, const QHash<QString,QString>& query );
        QByteArray webFormPostPayload     ( const QHash<QString,QString>& query );
        QByteArray webFileFormPostPayload ( const QHash<QString,QString>& query, const Entity::G3File* const file );
        const QString g3RequestKey           ( );
      protected:
        G3Request ( G3Backend* const backend, KIO::HTTP_METHOD method, const QString& service, const Entity::G3File* const file=NULL );
        void           addHeaderItem ( const QString& key, const QString& value );
        void           addQueryItem  ( const QString& key, const QString& value, bool skipIfEmpty=FALSE );
        void           addQueryItem  ( const QString& key, Entity::G3Type value, bool skipIfEmpty=FALSE );
        void           addQueryItem  ( const QString& key, const QStringList& values, bool skipIfEmpty=FALSE );
        void           setup         ( );
        void           process       ( );
        void           evaluate      ( );
        G3Item*        toItem        ( QVariant& entry );
        QList<G3Item*> toItems       ( );
        g3index        toItemId      ( QVariant& entry );
        QList<g3index> toItemIds     ( );
        inline G3Item* toItem        ( ) { return toItem(m_result); };
        inline g3index toItemId      ( ) { return toItemId(m_result); };
      public:
        static bool           g3Check        ( G3Backend* const backend );
        static void           g3Login        ( G3Backend* const backend, const AuthInfo& credentials );
        static QList<G3Item*> g3GetItems     ( G3Backend* const backend, const QStringList& urls, Entity::G3Type type=Entity::G3Type::NONE );
        static QList<G3Item*> g3GetItems     ( G3Backend* const backend, g3index id, Entity::G3Type type=Entity::G3Type::NONE );
        static QList<g3index> g3GetAncestors ( G3Backend* const backend, G3Item* item );
        static g3index        g3GetAncestor  ( G3Backend* const backend, G3Item* item );
        static G3Item*        g3GetItem      ( G3Backend* const backend, g3index id, const QString& scope="direct", const QString& name="", bool random=FALSE, Entity::G3Type type=Entity::G3Type::NONE );
        static void           g3PostItem     ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes, const Entity::G3File* const file=NULL );
        static g3index        g3PutItem      ( G3Backend* const backend, g3index id, const QHash<QString,QString>& attributes, Entity::G3Type type );
        static void           g3DelItem      ( G3Backend* const backend, g3index id );
        static g3index        g3SetItem      ( G3Backend* const backend, g3index id, const QString& name="", Entity::G3Type type=Entity::G3Type::NONE, const QByteArray& file=0 );
    }; // class G3Request

  } // namespace Gallery3
} // namespace KIO

#endif // G3_REQUEST_H
