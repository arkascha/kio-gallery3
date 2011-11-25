/* This file is part of 'kio-gallery3'
 * Copyright (C) 2011 Christian Reiner ("arkascha") <kio-gallery3@christian-reiner.info>
 *
 * $Author: arkascha $
 * $Revision: 82 $
 * $Date: 2011-08-13 14:42:57 +0200 (Sat, 13 Aug 2011) $
 */

#include <stdlib.h>
#include <unistd.h>

#include <QCoreApplication>
#include <KApplication>
#include <kcmdlineargs.h>
#include <kcomponentdata.h>
#include <kaboutdata.h>
#include "utility/defines.h"
#include "utility/exception.h"
#include "protocol/kio_gallery3_protocol.h"
#include "about_gallery3.data"

extern "C" { int KDE_EXPORT kdemain(int argc, char **argv); }
int kdemain( int argc, char **argv )
{
  KAboutData aboutData ( ABOUT_APP_NAME,
                         ABOUT_CATALOG_NAME,
                         ki18n(ABOUT_PROGRAM_NAME),
                         ABOUT_VERSION,
                         ki18n(ABOUT_DESCRIPTION),
                         ABOUT_LICENCE_TYPE,
                         ki18n(ABOUT_COPYRIGHT),
                         ki18n(ABOUT_INFORMATION),
                         ABOUT_WEBPAGE,
                         ABOUT_EMAIL );
  KComponentData componentData ( aboutData );

  // we use a KApplication here cause the background http jobs we start must be able to popup message boxes
//  QCoreApplication app( argc, argv );
  KApplication app( NULL, argc, argv, ABOUT_APP_NAME, TRUE );

  if (argc != 4)
  {
    fprintf(stderr, i18n("Usage: ").arg("kio_gallery3 protocol domain-socket1 domain-socket2\n").toUtf8() );
    exit(-1);
  }

  kDebug() << QString("started kio slave '%1' with PID %2").arg(argv[0]).arg(getpid());
  try
  {
    KIO::Gallery3::KIOGallery3Protocol slave(argv[2], argv[3]);
    slave.dispatchLoop();
  }
  catch ( KIO::Gallery3::Exception e )
  {
    exit ( e.getCode() );
  } // catch

  kDebug() << QString("stopped kio slave '%1' with PID %2").arg(argv[0]).arg(getpid());
  return ( 0 );
} // kdemain

