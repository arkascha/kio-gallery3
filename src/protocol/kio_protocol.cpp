/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

/*!
 * @file
 * Implements the (few) methods of the generic class KIOProtocol.
 * @see KIOProtocol
 * @author Christian Reiner
 */

#include <QFile>
#include <kmimetype.h>
#include <kio/netaccess.h>
#include "utility/exception.h"
#include "protocol/kio_protocol.h"

using namespace KIO;
using namespace KIO::Gallery3;

//==========

/*!
 * KIOProtocol::KIOProtocol ( const QByteArray& pool, const QByteArray& app, const QString& protocol )
 * @brief Constructor
 * @param pool
 * @param app
 * @param protocol the name of the protocol as published inside the kio runtime system
 * The standard constructor as called by the main loop setup at slace setup.
 * @see KIOProtocol
 * @author Christian Reiner
 */
KIOProtocol::KIOProtocol ( const QByteArray& pool, const QByteArray& app, const QString& protocol )
  : SlaveBase ( protocol.toUtf8(), pool, app )
{
  KDebug::Block block ( "KIOProtocol:KIOProtocol" );
} // KIOProtocol::KIOProtocol

/*!
 * KIOProtocol::~KIOProtocol ( )
 * @brief Destructor
 * @see KIOProtocol
 * @author Christian Reiner
 */
KIOProtocol::~KIOProtocol ( )
{
  KDebug::Block block ( "KIOProtocol::~KIOProtocol" );
} // KIOProtocol::~KIOProtocol

