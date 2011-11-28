/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

/*!
 * @file
 * Defines class G3File, describing a file to be used during a request to the
 * remote Gallery3 system, typically a (temporary) file to be uploaded.
 * The class is a 'header only library', no methods are defined in an
 * additional .cpp file, so no linkage is required.
 * @see G3File
 * @author Christian Reiner
 */

#ifndef ENTITY_G3_FILE_H
#define ENTITY_G3_FILE_H

#include <QString>
#include <KTemporaryFile>
#include <KMimeType>

namespace KIO
{
  namespace Gallery3
  {
    /*!
     * @class G3File
     * @brief File representation for convenience
     * An internal wrapper class that describe a single file ressource to be
     * used during a request against a remote Galelry3 system. Typically it is
     * a file meant to be uploaded during the creation of a new item of type
     * 'photo' or 'movie'. 
     * Note that the wrapper does _not_ contain the file content itself, but
     * only some meta information
     * @author Christian Reiner
     */
    class G3File
    {
      private:
        const QString        m_filename;
        const KMimeType::Ptr m_mimetype;
        const QString        m_filepath;
      public:
        inline G3File ( const QString& filename, const KMimeType::Ptr& mimetype, const QString& filepath )
                      : m_filename(filename), m_mimetype(mimetype), m_filepath(filepath) { }
        inline const QString&       filename() const { return m_filename; }
        inline const KMimeType::Ptr mimetype() const { return m_mimetype; }
        inline const QString&       filepath() const { return m_filepath; }
    }; // class G3File
  } // namespace Gallery3
} // namespace KIO

#endif // ENTITY_G3_FILE_H
