/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

#include <QObject>
#include <QCryptographicHash>
#include <QVariant>
#include <qjson/parser.h>
#include <qjson/serializer.h>
#include <qjson/qobjecthelper.h>
#include <kurl.h>
#include <kio/netaccess.h>
#include <klocalizedstring.h>
#include <klocale.h>
#include <kdatetime.h>
#include "utility/debug.h"
#include "utility/exception.h"
#include "kio_gallery3_protocol.h"
#include "gallery3/g3_backend.h"
#include "entity/g3_entity.h"
//#include "entity/g3_item.h"
//#include "entity/g3_collection.h"

using namespace KIO;
using namespace KIO::Gallery3;

/**
 */
G3Entity::G3Entity ( const Entity::G3Type type, G3Backend* const backend )
  : G3JsonParser     ( )
  , G3JsonSerializer ( )
  , m_type    ( type )
  , m_backend ( backend )
{
  kDebug() << "(<type> <backend>)" << type.toString() << backend->toPrintout();
} // G3Entity::G3Entity
