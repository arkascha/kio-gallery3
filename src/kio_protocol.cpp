/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

#include <QFile>
#include <kmimetype.h>
#include <kio/netaccess.h>
#include "utility/exception.h"
#include "kio_protocol.h"

using namespace KIO;
using namespace KIO::Gallery3;

//==========

/**
 * The standard constructor as called by the main loop setup at slace setup.
 */
KIOProtocol::KIOProtocol ( const QByteArray& pool, const QByteArray& app, const QString& protocol )
  : SlaveBase ( protocol.toUtf8(), pool, app )
{
  KDebug::Block block ( "KIOProtocol:KIOProtocol" );
} // KIOProtocol::KIOProtocol

KIOProtocol::~KIOProtocol ( )
{
  KDebug::Block block ( "KIOProtocol::~KIOProtocol" );
} // KIOProtocol::~KIOProtocol

