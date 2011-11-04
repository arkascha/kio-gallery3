/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

#ifndef UTILITY_TYPEDEF_H
#define UTILITY_TYPEDEF_H

#include <QtGlobal>

// best value actually depends on the implementation of the gallery3 code (server side)
#define ITEM_LIST_CHUNK_SIZE 8

// just a shortcut for convenience, so the real storage size can be changed if required
// TODO: check the size used inside the server side gallery3 code: what field type does the id have inside mysql there ?
typedef quint16 g3index;

// some macros that come in handy
#define MIN(X,Y) ((X) < (Y) ? : (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? : (X) : (Y))

#endif // UTILITY_TYPEDEF_H
