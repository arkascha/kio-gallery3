find_package        ( KDE4 "4.5" REQUIRED )
include             ( KDE4Defaults )
include_directories ( ${KDE4_INCLUDES} ${QT_INCLUDES} )
add_definitions     ( ${QT_DEFINITIONS} ${KDE4_DEFINITIONS} )

set ( SRCS json/g3_json.cpp
           entity/g3_entity.cpp
           entity/g3_item.cpp
           gallery3/g3_backend.cpp
           gallery3/g3_request.cpp
           kio_gallery3.cpp
           kio_gallery3_protocol.cpp
           kio_protocol.cpp )

set ( CMAKE_CXX_FLAGS "-fexceptions" )

kde4_add_plugin ( kio_gallery3  ${SRCS} )

target_link_libraries ( kio_gallery3  ${KDE4_KIO_LIBS} qjson )

install ( TARGETS kio_gallery3       DESTINATION ${PLUGIN_INSTALL_DIR} )
install ( FILES   gallery3.protocol  DESTINATION ${SERVICES_INSTALL_DIR} )
install ( FILES   gallery3s.protocol DESTINATION ${SERVICES_INSTALL_DIR} )

add_subdirectory ( utility )
add_subdirectory ( entity )
add_subdirectory ( json )
add_subdirectory ( gallery3 )
