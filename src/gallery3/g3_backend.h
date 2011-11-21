/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

#ifndef G3_BACKEND_H
#define G3_BACKEND_H

#include <QHash>
#include <ktemporaryfile.h>
#include "utility/defines.h"
#include "gallery3/g3_request.h"
#include "entity/g3_file.h"
#include "entity/g3_item.h"

namespace KIO
{
  namespace Gallery3
  {
    class EntityObject;

    /**
     * class G3Backend
     *
     * This class serves as a local mirror of a remote gallery3 system and offers two levels: 
     * - it maps local requests to the gallery3 data onto the remote gallery3 API installation
     * - it keeps a local cache of items to improve performance by reducing the total number of requests required
     *
     * Note that one single backend can only communicate and stand for one distinct remote gallery3 system.
     * When the slace addresses more than a single such system several backends are created automatically. 
     */
    class G3Backend
      : public QObject
    {
      class Members
      {
        public:
        inline Members ( const KUrl& g3Url ) : baseUrl(g3Url) { };
        AuthInfo               credentials;
        const KUrl             baseUrl;
        KUrl                   restUrl;
        QHash<g3index,G3Item*> items;
      }; // struct Members
      Q_OBJECT
      private:
        Members* const m;
      protected:
      public:
        static G3Backend* const instantiate ( QObject* parent, QHash<QString,G3Backend*>& backends, const KUrl g3Url );
        G3Backend ( QObject* parent, const KUrl& g3Url );
        ~G3Backend ( );
        const UDSEntry                       toUDSEntry     ( );
        const UDSEntryList                   toUDSEntryList ( );
        const QString                        toPrintout     ( ) const;
        inline AuthInfo&                     credentials ( )       { return m->credentials; }
        inline const KUrl&                   baseUrl     ( ) const { return m->baseUrl; };
        inline const KUrl&                   restUrl     ( ) const { return m->restUrl; };
        inline const QHash<g3index,G3Item*>& items       ( ) const { return m->items;    };
        G3Item*                              item       ( g3index id );
        G3Item*                              itemBase   ( );
        G3Item*                              itemById   ( g3index id );
        G3Item*                              itemByUrl  ( const KUrl& itemUrl );
        G3Item*                              itemByPath ( const QString& path );
        G3Item*                              itemByPath ( const QStringList& breadcrumbs );
        QHash<g3index,G3Item*>               members           ( g3index id );
        QHash<g3index,G3Item*>               members           ( G3Item* item );
        QList<G3Item*>                       membersByItemId   ( g3index id );
        QList<G3Item*>                       membersByItemPath ( const QString& path );
        QList<G3Item*>                       membersByItemPath ( const QStringList& breadcrumbs );
        int                                  countItems  ( );
        bool                                 login       ( AuthInfo& credentials );
        void                                 pushItem    ( G3Item* item );
        G3Item*                              popItem     ( g3index id );
        void                                 removeItem  ( G3Item* item );
        G3Item* const                        updateItem  ( G3Item* item, const QHash<QString,QString>& attributes );
        G3Item* const                        createItem  ( G3Item* parent, const QString& name, const Entity::G3File* const file=NULL );
        void                                 fetchFile   ( G3Item* item );
        void                                 fetchResize ( G3Item* item );
        void                                 fetchThumb  ( G3Item* item );
        void                                 fetchCover  ( G3Item* item );
    }; // class G3Backend

  } // namespace Gallery3
} // namespace KIO
#endif // G3_BACKEND_H