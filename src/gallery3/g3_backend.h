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
    {
      public:
        static G3Backend* const instantiate ( QHash<QString,G3Backend*>& backends, const KUrl g3Url );
      private:
        const KUrl             m_base_url;
        KUrl                   m_rest_url;
        QHash<g3index,G3Item*> m_items;
      protected:
      public:
        G3Backend ( const KUrl& g3Url );
        ~G3Backend ( );
        const UDSEntry                       toUDSEntry     ( );
        const UDSEntryList                   toUDSEntryList ( );
        const QString                        toPrintout     ( ) const;
        inline const KUrl&                   baseUrl ( ) const { return m_base_url; };
        inline const KUrl&                   restUrl ( ) const { return m_rest_url; };
        inline const QHash<g3index,G3Item*>& items   ( ) const { return m_items; };
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
        void                                 pushItem    ( G3Item* item );
        G3Item*                              popMember   ( G3Item* item );
        G3Item*                              popItem     ( g3index id );
        void                                 removeItem  ( G3Item* item );
        void                                 updateItem  ( G3Item* item, QHash<QString,QString>& attributes );
        G3Item* const                        createItem  ( G3Item* parent, const QString& name, const Entity::G3File* const file=NULL );
        // TODO: updateItem ( ... );
    }; // class G3Backend

  } // namespace Gallery3
} // namespace KIO
#endif // G3_BACKEND_H