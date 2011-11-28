/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 119 $
 * $Date: 2011-09-12 09:35:04 +0200 (Mon, 12 Sep 2011) $
 */

/*!
 * @file
 * This is a header-only-library, so no separate .cpp file exists, had to be
 * compiled or linked. Instead it is enough to include this header for usage.
 * We try to give exceptions a somewhat automatical behaviour:
 * - readable debug output
 * - evaluable error code for reactions
 * - intuitive generation (throw)
 * @author Christian Reiner
 */

#ifndef UTILITY_EXCEPTION_H
#define UTILITY_EXCEPTION_H

#include <QtCore>
#include <kio/global.h>
#include <kio/job.h>
#include <kdebug.h>

namespace KIO
{
  namespace Gallery3
  {

    /*!
     * @class Exception
     * @brief Convenience wrapper around standard exception
     * This class serves as a header-only library (no object file to be linked)
     * It offers a convenient exception class with automatic transation and logging. 
     * @author Christian Reiner
     */
    class Exception
      : public QtConcurrent::Exception
    {
      public:
        inline Exception ( Error _code, const QString &_text=0 ) : code(_code),        text(_text) { kDebug()<<toPrintout(); }
        inline Exception ( int   _code, const QString &_text=0 ) : code(Error(_code)), text(_text) { kDebug()<<toPrintout(); }
        inline Exception ( KJob* _job ) : code(Error(_job->error())), text(_job->errorString())    { kDebug()<<toPrintout(); delete _job; }
        inline ~Exception () throw() {}
        inline Exception* clone () const { return new Exception(*this); }
        inline void       raise () const { throw *this; }
        inline void       debug () const
        {
          kDebug() << QString("ERROR %1: %2").arg(code).arg(text);
          kDebug() << "Backtrace:\n" << kBacktrace();
        }
      private:
        const Error   code;
        const QString text;
      public:
        inline const Error    getCode    () { return this->code; }
        inline const QString& getText    () { return this->text; }
        inline const QString  toPrintout () { return QString("### Exception Code %1: %2").arg(code).arg(text); }
    }; // class Exception

  } // namespace Gallery3
} // namespace KIO

#endif // UTILITY_EXCEPTION_H
