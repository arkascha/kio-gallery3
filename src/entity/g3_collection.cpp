/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

#include <QVariant>
#include <qjson/parser.h>
#include <qjson/serializer.h>
#include "utility/debug.h"
#include "utility/exception.h"
#include "gallery3/g3_backend.h"
#include "entity/g3_collection.h"

using namespace KIO;
using namespace KIO::Gallery3;

G3Collection::G3Collection ( G3Backend* const backend )
  : G3Entity ( Entity::G3Type::NONE, backend )
{
  kDebug() << "(<backend>)" << backend->toPrintout();
} // G3Collection::G3Collection

G3Collection::G3Collection ( G3Backend* const backend, const QHash<int,G3Entity* const>& members )
  : G3Entity ( Entity::G3Type::NONE, backend )
{
  kDebug() << "(<backend> <members>)" << backend->toPrintout() << members.count();
  m_members = members;
} // Collection::G3Collection

G3Collection::~G3Collection ( )
{
  kDebug() << "(<>)";
} // G3Collection::~G3Collection

//==========

/*
QByteArray G3Collection::toJSON ( ) const
{
  kDebug() << "creating JSON notation of node list holding" << m_nodes.size() << "entries";
  // FIXME: guess this wont work since the map holds only pointers, not objects as values
  QVariantMap _nodes;
  G3Collection::const_iterator _iterator;
  for ( _iterator=m_nodes.begin(); _iterator!=m_nodes.end(); _iterator++ )
    _nodes.insert ( _iterator.key(), _iterator.value()->toJSON() );
  QJson::Serializer _serializer;
  return _serializer.serialize ( _nodes );
} // G3Collection::toJSON

G3Collection& G3Collection::fromJSON ( const QByteArray& json )
{
  kDebug();
  QJson::Parser parser;
  bool ok;
  QVariantMap _nodes = parser.parse ( json, &ok ).toMap();
  if ( ! ok )
    throw Exception ( Error(ERR_INTERNAL), "Failed to deserialize json notation of node list" );
  // create nodes one by one and push them into the cleared list
  m_nodes.clear ( );
  QVariantMap::iterator _iterator;
  for ( _iterator=_nodes.begin(); _iterator!=_nodes.end(); _iterator++ )
    m_nodes.insert ( _iterator.key(), new NodeWrapper(_iterator.value().toByteArray()) );
  kDebug() << "created node list holding" << m_nodes.size() << "entries from JSON notation";
  return *this;
} // G3Collection::fromJSON
*/
