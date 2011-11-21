/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

#ifndef ENTITY_G3_ENTITY_H
#define ENTITY_G3_ENTITY_H

#include <QVariant>
#include <kio/global.h>
#include <kio/udsentry.h>
#include <kmimetype.h>
#include <kdatetime.h>
#include "utility/defines.h"
#include "entity/g3_type.h"
#include "json/g3_json.h"

namespace KIO
{
  namespace Gallery3
  {

    class G3Backend;
    
    /**
    * This class describes all aspects of an 'entity', as defined by the gallery3 API.
    *
    * The class has somewhat passive character: all data is treated more or less constant
    * - the private members hold basic information as detected, requested or decided upon
    * - all private members are published via direct access methods (read only)
    * - in addition a number of convenience constructions are offered as methods as well
    *   these are generated based only on the constant settings stored in the members mentioned above
    */
    class G3Entity
      : public G3JsonParser
      , public G3JsonSerializer
    {
      private:
      protected:
        const Entity::G3Type   m_type;
        G3Backend* const       m_backend;
      public:
        G3Entity ( const Entity::G3Type type, G3Backend* const backend );
        inline const Entity::G3Type type ( ) const { return m_type; };
        virtual const QString toPrintout ( ) const = 0;
  //      QByteArray toJSON     ( ) const;
  //      Entity&    fromJSON   ( const QByteArray& json );
    }; // class G3Entity

  } // namespace Gallery3
} // namespace KIO

#endif // ENTITY_G3_ENTITY_H
