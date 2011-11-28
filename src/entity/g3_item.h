/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

/*!
 * @file
 * Defines class G3Item and the all classes derived from that
 * Such an item mirrors an item contained inside the remote Gallery3 system
 * in the local hierarchical object structure
 * @see G3Item
 * @author Christian Reiner
 */

#ifndef ENTITY_G3_ITEM_H
#define ENTITY_G3_ITEM_H

#include <QVariant>
#include <kio/global.h>
#include <kio/udsentry.h>
#include <kmimetype.h>
#include <kdatetime.h>
#include "utility/defines.h"
#include "entity/g3_type.h"

namespace KIO
{
  namespace Gallery3
  {
    class G3Backend;
    /*!
    * @class G3Item
    * 
    * This class describes all aspects of an 'item', as defined by the gallery3 API.
    * It has somewhat passive character: all data is treated more or less constant
    * - the private members hold basic information as detected, requested or decided upon
    * - all private members are published via direct access methods (read only)
    * - in addition a number of convenience constructions are offered as methods as well
    *   these are generated based only on the constant settings stored in the members mentioned above
    * 
    * @see G3AlbumItem
    * @see G3PhotoItemG3MovieItem
    * @see G3TagItem
    * @see G3CommentItem
    */
    class G3Item
      : public QObject
    {
      class Members
      {
        public:
        inline Members ( const G3Type type, G3Backend* const backend, const QVariantMap& attributes) : type(type), backend(backend), attributes(attributes) { }
        const G3Type           type;
        G3Backend* const       backend;
        g3index                id;
        QString                name;
        KMimeType::Ptr         mimetype;
        G3Item*                parent;
        QHash<g3index,G3Item*> members;
        QVariantMap            attributes;
      }; // class Members
      Q_OBJECT
      private:
        Members* const m;
      protected:
        G3Item ( const G3Type type, G3Backend* const backend, const QVariantMap& data );
      public:
        static G3Item* const instantiate ( G3Backend* const backend, const QVariantMap& data );
        ~G3Item ( );
      signals:
        void signalUDSEntry ( const UDSEntry& entry ) const;
      public:
        inline const G3Type         type     ( ) const { return m->type; }
        inline const g3index        id       ( ) const { return m->id; }
        inline const QString        name     ( ) const { return m->name; }
        inline const KMimeType::Ptr mimetype ( ) const { return m->mimetype; }
        inline int                  size            ( bool strict=FALSE ) const { return (G3Type::ALBUM==m->type.toInt()) ? 0L : attributeMapToken(QLatin1String("entity"),QLatin1String("file_size"),QVariant::Int).toInt(); }
        inline bool                 canEdit         ( bool strict=FALSE ) const { return attributeMapToken ( QLatin1String("entity"), QLatin1String("can_edit"), QVariant::Bool, strict ).toBool(); }
        inline const KUrl           restUrl         ( bool strict=FALSE ) const { return KUrl ( attributeToken    (                          QLatin1String("url"),               QVariant::String, strict ).toString() ); }
        inline const KUrl           coverUrl        ( bool strict=FALSE ) const { return KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("album_cover"),       QVariant::String, strict ).toString() ); }
        inline const KUrl           webUrl          ( bool strict=FALSE ) const { return KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("web_url"),           QVariant::String, strict ).toString() ); }
        inline const KUrl           webUrlPublic    ( bool strict=FALSE ) const { return KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("web_url_public"),    QVariant::String, strict ).toString() ); }
        inline const KUrl           fileUrl         ( bool strict=FALSE ) const { return KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("file_url"),          QVariant::String, strict ).toString() ); }
        inline const KUrl           fileUrlPublic   ( bool strict=FALSE ) const { return KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("file_url_public"),   QVariant::String, strict ).toString() ); }
        inline const KUrl           resizeUrl       ( bool strict=FALSE ) const { return KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("resize_url"),        QVariant::String, strict ).toString() ); }
        inline const KUrl           resizeUrlPublic ( bool strict=FALSE ) const { return KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("resize_url_public"), QVariant::String, strict ).toString() ); }
        inline const KUrl           thumbUrl        ( bool strict=FALSE ) const { return KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("thumb_url"),         QVariant::String, strict ).toString() ); }
        inline const KUrl           thumbUrlPublic  ( bool strict=FALSE ) const { return KUrl ( attributeMapToken ( QLatin1String("entity"), QLatin1String("thumb_url_public"),  QVariant::String, strict ).toString() ); }
        QStringList            path              ( ) const;
        G3Item*                parent            ( ) const;
        G3Item*                member            ( const QString& name );
        G3Item*                member            ( g3index id );
        QHash<g3index,G3Item*> members           ( );
        bool                   containsMember    ( const QString& name );
        bool                   containsMember    ( g3index id );
        int                    countMembers      ( );
        void                   buildMemberItems  ( );
        void                   pushMember        ( G3Item* member );
        G3Item*                popMember         ( G3Item* member );
        G3Item*                popMember         ( g3index id );
        void                   setParent         ( G3Item* parent );
        const QVariant         attributeToken    ( const QString& attribute, QVariant::Type type, bool strict=FALSE ) const;
        const QVariant         attributeMap      ( const QString& attribute, bool strict=FALSE ) const;
        const QVariant         attributeList     ( const QString& attribute, bool strict=FALSE ) const;
        const QVariant         attributeString   ( const QString& attribute, bool strict=FALSE ) const;
        const QVariant         attributeMapToken ( const QString& attribute, const QString& token, QVariant::Type type, bool strict=FALSE ) const;
        const G3Item& operator<< ( G3Item* member );
        const G3Item& operator<< ( G3Item& member );
        const QString                toPrintout     ( ) const;
        const UDSEntry               toUDSEntry     ( ) const;
        const UDSEntryList           toUDSEntryList ( bool signalEntries=FALSE ) const;
        const QHash<QString,QString> toAttributes   ( ) const;
  //      QByteArray toJSON ( ) const;
  //      G3Item&    fromJSON ( const QByteArray& json );
    }; // class G3Item

//==========

    /*!
     * @class G3AlbumItem
     * Specialized item representing an album in the remote Gallery3 system
     */
    class G3AlbumItem
      : public G3Item
    {
      public:
        G3AlbumItem ( G3Backend* const backend, const QVariantMap& attributes );
    }; // class G3AlbumItem

    /*!
     * @class G3MovieItem
     * Specialized item representing a movie in the remote Gallery3 system
     */
    class G3MovieItem
      : public G3Item
    {
      public:
        G3MovieItem ( G3Backend* const backend, const QVariantMap& attributes );
    }; // class G3MovieItem

    /*!
     * @class G3PhotoItem
     * Specialized item representing a photo in the remote Gallery3 system
     */
    class G3PhotoItem: public G3Item
    {
      public:
        G3PhotoItem ( G3Backend* const backend, const QVariantMap& attributes );
    }; // class G3PhotoItem

    /*!
     * @class G3TagItem
     * Specialized item representing a tag in the remote Gallery3 system
     */
    class G3TagItem: public G3Item
    {
      public:
        G3TagItem ( G3Backend* const backend, const QVariantMap& attributes );
    }; // class G3TagItem

    /*!
     * @class G3CommentItem
     * Specialized item representing a comment in the remote Gallery3 system
     */
    class G3CommentItem: public G3Item
    {
      public:
        G3CommentItem ( G3Backend* const backend, const QVariantMap& attributes );
    }; // class G3CommentItem

  } // namespace Gallery3
} // namespace KIO

#endif // ENTITY_G3_ITEM_H