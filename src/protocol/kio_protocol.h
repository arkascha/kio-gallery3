/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 89 $
 * $Date: 2011-08-13 23:12:52 +0200 (Sat, 13 Aug 2011) $
 */

/*!
 * @file
 * Defines a generic kio protocol.
 * To be extended by a specific derivation. 
 * @author Christian Reiner
 */

#ifndef KIO_PROTOCOL_H
#define KIO_PROTOCOL_H

#include <QString>
#include <kio/global.h>
#include <kio/slavebase.h>
#include <kio/udsentry.h>
#include "utility/defines.h"

namespace KIO
{
  namespace Gallery3
  {
    class G3Frontend;

    /*!
     * @class KIOProtocol
     * @brief Generic kio protocol class
     * This is an abstract base class common to all specialized kio-gallery3 protocols.
     * @author Christian Reiner
     */
    class KIOProtocol
      : public SlaveBase
    {
      private:
      protected:
      public:
        virtual const QString protocol ( ) = 0;
        KIOProtocol ( const QByteArray& pool, const QByteArray& app, const QString& protocol );
        ~KIOProtocol ( );
      protected:
      public:
        virtual void setHost  ( const QString& host, g3index port, const QString& user, const QString& pass ) = 0; 
        virtual void copy     ( const KUrl& src, const KUrl& dest, int permissions, JobFlags flags ) = 0;
        virtual void del      ( const KUrl& url, bool isfile ) = 0;
        virtual void get      ( const KUrl& url ) = 0;
        virtual void listDir  ( const KUrl& url ) = 0;
        virtual void mimetype ( const KUrl& url ) = 0;
        virtual void mkdir    ( const KUrl& url, int permissions ) = 0;
        virtual void put      ( const KUrl& url, int permissions, KIO::JobFlags flags ) = 0;
        virtual void rename   ( const KUrl& src, const KUrl& dest, KIO::JobFlags flags ) = 0;
        virtual void stat     ( const KUrl& url ) = 0;
        virtual void symlink  ( const QString& target, const KUrl& dest, KIO::JobFlags flags ) = 0;
  //      virtual void setModificationTime ( const KUrl& url, const QDateTime& mtime ) = 0;
    }; // class KIOProtocol

  } // namespace Gallery3
} // namespace KIO

#endif // KIO_PROTOCOL_H