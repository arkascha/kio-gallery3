/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 81 $
 * $Date: 2011-08-13 13:08:50 +0200 (Sat, 13 Aug 2011) $
 */

#ifndef G3_JSON_H
#define G3_JSON_H

#include <QVariant>
#include <qjson/parser.h>
#include <qjson/serializer.h>

namespace KIO
{
  namespace Gallery3
  {

    class G3JsonParser
    : protected QJson::Parser
    {
      public:
        QVariant g3parse ( QIODevice *io );
        QVariant g3parse ( const QByteArray& jsonData );
    }; // class KIOGallery3Json

    class G3JsonSerializer
    : protected QJson::Serializer
    {
      public:
        void       g3serialize ( const QVariant& variant, QIODevice* out );
        QByteArray g3serialize ( const QVariant& variant );
    }; // class KIOGallery3JsonSerializer

  } // namespace Gallery3
} // namespace KIO

#endif // G3_JSON_H
