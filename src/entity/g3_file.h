/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

#ifndef ENTITY_G3_FILE_H
#define ENTITY_G3_FILE_H

#include <QString>
#include <KTemporaryFile>
#include <KMimeType>
//#include <kio/job.h>

namespace KIO
{
  namespace Gallery3
  {
    namespace Entity
    {
      class G3File
      {
        private:
          const QString        m_filename;
          const KMimeType::Ptr m_mimetype;
          const QString        m_filepath;
        public:
          inline G3File ( const QString& filename, const KMimeType::Ptr& mimetype, const QString& filepath )
                        : m_filename(filename), m_mimetype(mimetype), m_filepath(filepath) { };
          inline const QString&        filename() const { return m_filename; };
          inline const KMimeType::Ptr  mimetype() const { return m_mimetype; };
          inline const QString&        filepath() const { return m_filepath; };
      }; // class G3File
    } // namespace Entity
  } // namespace Gallery3
} // namespace KIO


#endif // ENTITY_G3_FILE_H
