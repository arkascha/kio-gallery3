#!/bin/sh
BASEDIR="../" # root of translatable sources
PROJECT="kio-clipboard" # project name
BUGADDR="" # MSGID-Bugs
WDIR=`pwd` # working dir

echo "Preparing rc files"
cd ${BASEDIR}
find . -name '*.rc' -o -name '*.ui' -o -name '*.kcfg' | sort > ${WDIR}/rcfiles.list
xargs --arg-file=${WDIR}/rcfiles.list extractrc --ignore-no-input > ${WDIR}/rc.cpp
# additional string for KAboutData
#echo 'i18nc("NAME OF TRANSLATORS","Translation Team: Names");' >> ${WDIR}/rc.cpp
#echo 'i18nc("EMAIL OF TRANSLATORS","Translation Team: Emails");' >> ${WDIR}/rc.cpp
cd ${WDIR}
echo "Done preparing rc files"

echo "Extracting messages"
cd ${BASEDIR}
# see above on sorting
find . -name '*.cpp' -o -name '*.h' -o -name '*.c' -o -name '*.data' | sort > ${WDIR}/infiles.list
echo "rc.cpp" >> ${WDIR}/infiles.list
cd ${WDIR}
xgettext --from-code=UTF-8 -C -kde -ci18n -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -ktr2i18n:1 \
         -kI18N_NOOP:1 -kI18N_NOOP2:1c,2 -kaliasLocale -kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 \
         --files-from=infiles.list -D ${BASEDIR} -D ${WDIR} -o ${PROJECT}.pot || { echo "error while calling xgettext. aborting."; exit 1; }
echo "Done extracting messages"

echo "Merging translations"
catalogs=`find . -name '*.po'`
for cat in $catalogs; do
echo $cat
msgmerge -o $cat.new $cat ${PROJECT}.pot
mv $cat.new $cat
done
echo "Done merging translations"

echo "Cleaning up"
cd ${WDIR}
rm rcfiles.list
rm infiles.list
rm rc.cpp
echo "Done"
