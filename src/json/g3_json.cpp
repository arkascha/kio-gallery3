/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 81 $
 * $Date: 2011-08-13 13:08:50 +0200 (Sat, 13 Aug 2011) $
 */

#include "json/g3_json.h"
#include "utility/exception.h"

using namespace KIO;
using namespace KIO::Gallery3;

QVariant G3JsonParser::g3parse ( QIODevice *io )
{
  bool ok = TRUE;
  QVariant result = QJson::Parser::parse ( io, &ok );
  if ( ! ok )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("parsing the response from the remote gallery produced an error at line %1: \n%2")
                             .arg(errorLine()).arg(errorString()));
  return result;
} // KIOGallery3Json::g3parse

QVariant G3JsonParser::g3parse ( const QByteArray &jsonData )
{
  bool ok = TRUE;
  QVariant result = QJson::Parser::parse ( jsonData, &ok );
  if ( ! ok )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("parsing the response from the remote gallery produced an error at line %1: \n%2")
                             .arg(errorLine()).arg(errorString()));
  return result;
} // KIOGallery3Json::g3parse

void G3JsonSerializer::g3serialize ( const QVariant &variant, QIODevice *out )
{
  bool ok = TRUE;
  QJson::Serializer::serialize ( variant, out, &ok );
  if ( ! ok )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("serializing data for the request to the remote gallery produced an error") );
} // G3JsonSerializer::g3serialize

QByteArray G3JsonSerializer::g3serialize ( const QVariant &variant )
{
  bool ok = TRUE;
  QByteArray _result = QJson::Serializer::serialize ( variant );
  if ( ! ok )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("serializing data for the request to the remote gallery produced an error") );
  return _result;
} // G3JsonSerializer::g3serialize
