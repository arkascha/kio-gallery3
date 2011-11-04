/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 107 $
 * $Date: 2011-08-19 18:21:32 +0200 (Fri, 19 Aug 2011) $
 */

#ifndef ENTITY_G3_COLLECTION_H
#define ENTITY_G3_COLLECTION_H

#include <QMap>
#include <kio/global.h>
#include <kio/udsentry.h>
#include "entity/g3_type.h"
#include "entity/g3_entity.h"

namespace KIO
{
  namespace Gallery3
  {

    class G3Item;
    //class G3Tag;
    //class G3Comment;

    class G3Collection
      : public G3Entity
    {
      private:
        QHash<int,G3Entity* const> m_members;
      public:
        G3Collection ( G3Backend* const backend );
        G3Collection ( G3Backend* const backend, const QHash<int,G3Entity* const>& members );
       ~G3Collection ( );
//        const QByteArray   toJSON   ( ) const;
//        G3Collection&  fromJSON ( const QByteArray& json );
        inline QHash<int,G3Entity* const>& members ( ) { return m_members; };
    }; // class G3Collection

  } // namespace Gallery3
} // namespace KIO

#endif // ENTITY_G3_COLLECTION_H