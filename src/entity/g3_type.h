/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

#ifndef ENTITY_G3_TYPE_H
#define ENTITY_G3_TYPE_H

#include <sys/stat.h>
#include <QHash>
#include <QDataStream>
#include <QFile>
#include <kio/authinfo.h>
#include <kio/udsentry.h>

namespace KIO
{
  namespace Gallery3
  {
    namespace Entity
    {

      /**
       * class G3Type
       * Implements an "intelligent type definition" that connects both worlds understanding of a 'type': 
       * - c++ style type in form of an enumeration and projection on an integer to be used in switch statements
       * - gallery3 style type in form of strings describing the meaning, content and usage of an item
       * This is a header-only implementation, no source or object file has to be be considered. 
       */
      class G3Type
      {
        private:
          QHash<int,QString> m_name_map;
          QHash<int,int>     m_node_map;
        protected:
          int                m_value;
        public:
          enum { NONE, ALBUM, MOVIE, PHOTO, TAG, COMMENT };
          inline G3Type ( int value )           { setup(); m_value = value; };
          inline G3Type ( const char* name )    { setup(); m_value = m_name_map.key(name); };
          inline G3Type ( const QString& name ) { setup(); m_value = m_name_map.key(name); };
          inline G3Type ( )                     { setup(); };
          inline void setup ( )
          {
            m_name_map.insert ( G3Type::NONE,    QString("") );
            m_name_map.insert ( G3Type::ALBUM,   QString("album") );
            m_name_map.insert ( G3Type::MOVIE,   QString("movie") );
            m_name_map.insert ( G3Type::PHOTO,   QString("photo") );
            m_name_map.insert ( G3Type::TAG,     QString("tag") );
            m_name_map.insert ( G3Type::COMMENT, QString("tag") );
            m_node_map.insert ( G3Type::NONE,    0 );
            m_node_map.insert ( G3Type::ALBUM,   S_IFDIR );
            m_node_map.insert ( G3Type::MOVIE,   S_IFREG );
            m_node_map.insert ( G3Type::PHOTO,   S_IFREG );
            m_node_map.insert ( G3Type::TAG,     S_IFREG );
            m_node_map.insert ( G3Type::COMMENT, S_IFREG );
          }; // constructor
          inline int           operator[]    ( const char* value )   const { return m_name_map.key(value); };
          inline const QString operator[]    ( int         key   )   const { return m_name_map.value(key); };
          inline int           toInt         ( )                     const { return m_value; };
          inline const QString toString      ( )                     const { return m_name_map[m_value]; };
          inline int           toUDSFileType ( )                     const { return m_node_map[m_value]; };
          inline bool          operator==    ( const G3Type& other ) const { return m_value==other.m_value; };
          inline bool          operator==    (       G3Type& other ) const { return m_value==other.m_value; };
          inline bool          operator!=    ( const G3Type& other ) const { return m_value!=other.m_value; };
          inline bool          operator!=    (       G3Type& other ) const { return m_value!=other.m_value; };
          inline int           operator=     ( int value )                 { m_value=value;  return value; };
          inline int           operator=     ( const QString& name)        { m_value = m_name_map.key(name); return m_value; };
      }; // class G3Type

      QDataStream & operator<< ( QDataStream & stream, const G3Type & type );

    } // namespace Entity
  } // namespeace Gallery3
} // namespace KIO

#endif // ENTITY_G3_TYPE_H
