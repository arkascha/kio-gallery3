/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

/*!
 * @file
 * Some global definitions, since there currently is no configuration
 * @author Christian Reiner
 */

#ifndef UTILITY_TYPEDEF_H
#define UTILITY_TYPEDEF_H

#include <QtGlobal>

/*!
 * @config ITEM_LIST_CHUNK_SIZE
 * The number of items retrieved in a single chunk from the remote Gallery3
 * system. The idea is to have a balance between side of request and reply
 * on the one hand and number of requests required on the other hand. 
 * Best value actually depends on the implementation of the Gallery3 code.
 */
#define ITEM_LIST_CHUNK_SIZE 8

/*!
 * @typedef quint16 g3index
 * We use a local identifier to describe the type of an item id.
 * The idea is to have a clear distinction between ordinary integers like
 * iterators or array indices and item ids. 
 * @todo check the size used inside the server side gallery3 code: what field type does the id have inside mysql there ?
 */
typedef quint16 g3index;

/*!
 * @define MIN
 * @define MAX
 * some macros that come in handy
 */
#define MIN(X,Y) ((X) < (Y) ? : (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? : (X) : (Y))

#endif // UTILITY_TYPEDEF_H
