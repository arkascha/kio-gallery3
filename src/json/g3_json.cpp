/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 81 $
 * $Date: 2011-08-13 13:08:50 +0200 (Sat, 13 Aug 2011) $
 */

/*!
 * @file
 * Implements the methods that wrap the functions provided by the QJson library.
 * @see G3JsonParser
 * @see G3JsonSerializer
 * @author Christian Reiner
 */

#include "json/g3_json.h"
#include "utility/exception.h"

using namespace KIO;
using namespace KIO::Gallery3;

/*!
 * QVariant G3JsonParser::g3parse ( QIODevice *io )
 * @brief Parse provoded JSon encoded data
 * @param io device to read json data from
 * @return   a generic QVariant object holdig the json data in a Qt representation
 * Reads and parses provided json encoded data into a generic QVariant object. 
 * @see G3JsonParser
 * @author Christian Reiner
 */
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

/*!
 * QVariant G3JsonParser::g3parse ( const QByteArray &jsonData )
 * @brief Parse provided JSon encoded data
 * @param iojsonData json data to be processed
 * @return   a generic QVariant object holdig the json data in a Qt representation
 * Reads and parses provided json encoded data into a generic QVariant object. 
 * @see G3JsonParser
 * @author Christian Reiner
 */
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

/*!
 * void G3JsonSerializer::g3serialize ( const QVariant &variant, QIODevice *out )
 * @brief Serialize provided structures to JSON encoded data
 * @param variant a QVariant that holds the structured data to be serialized
 * @param out     a device to write the serialized data to
 * Serialized the structured data given in a QVariant object into JSon encoded data.
 * @see G3JsonSerializer
 * @author Christian Reiner
 */
void G3JsonSerializer::g3serialize ( const QVariant &variant, QIODevice *out )
{
  bool ok = TRUE;
  QJson::Serializer::serialize ( variant, out, &ok );
  if ( ! ok )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("serializing data for the request to the remote gallery produced an error") );
} // G3JsonSerializer::g3serialize

/*!
 * void G3JsonSerializer::g3serialize ( const QVariant &variant, QIODevice *out )
 * @brief Serialize provided structures to JSON encoded data
 * @param variant a QVariant that holds the structured data to be serialized
 * @return        a QByteArray hlding the serialized data
 * Serialized the structured data given in a QVariant object into JSon encoded data.
 * @see G3JsonSerializer
 * @author Christian Reiner
 */
QByteArray G3JsonSerializer::g3serialize ( const QVariant &variant )
{
  bool ok = TRUE;
  QByteArray _result = QJson::Serializer::serialize ( variant );
  if ( ! ok )
    throw Exception ( Error(ERR_SLAVE_DEFINED),
                      QString("serializing data for the request to the remote gallery produced an error") );
  return _result;
} // G3JsonSerializer::g3serialize
