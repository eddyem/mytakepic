cmake_minimum_required(VERSION 2.6)
set(PROJ takepic)
project(${PROJ})
set(VERSION "0.2.0")
set(CMAKE_COLOR_MAKEFILE ON)
if(DEFINED DEBUG)
	add_definitions(-DEBUG)
endif()
if(DEFINED CMAKE_INSTALL_PREFIX AND CMAKE_INSTALL_PREFIX MATCHES "/usr/local")
	set(CMAKE_INSTALL_PREFIX "/usr")
endif()
message("Install dir prefix: ${CMAKE_INSTALL_PREFIX}")
#aux_source_directory(. SOURCES)
set(SOURCES takepic.c usage.c)
if(NOT DEFINED NOBTA)
	set(SOURCES ${SOURCES} bta_print.c)
	add_definitions(-DUSE_BTA)
endif()
set(LCPATH locale/ru)
set(CFLAGS -O3 -Wall -Werror -W -std=c99)
set(PO_FILE ${LCPATH}/messages.po)
set(MO_FILE ${LCPATH}/LC_MESSAGES/${PROJ}.mo)
set(RU_FILE ${LCPATH}/ru.po)
find_package(PkgConfig REQUIRED)
set(MODULES cfitsio>=3.0 fli>=1.71)
if(DEFINED USEPNG)
	set(MODULES ${MODULES} libpng>=1.2)
	add_definitions(-DUSEPNG)
endif()
pkg_check_modules(${PROJ} REQUIRED ${MODULES})
if(DEFINED USERAW)
	add_definitions(-DUSERAW)
endif()
add_executable(${PROJ} ${SOURCES} ${PO_FILE} ${MO_FILE})
target_link_libraries(${PROJ} ${${PROJ}_LIBRARIES})
include_directories(${${PROJ}_INCLUDE_DIRS})
link_directories(${${PROJ}_LIBRARY_DIRS})
add_definitions(${CFLAGS} -DLOCALEDIR=\"${CMAKE_INSTALL_PREFIX}/share/locale\"
		-DPACKAGE_VERSION=\"${VERSION}\" -DGETTEXT_PACKAGE=\"${PROJ}\")

# Installation of the program
INSTALL(FILES ${MO_FILE} DESTINATION "share/locale/ru/LC_MESSAGES")
        #PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
INSTALL(TARGETS ${PROJ} DESTINATION "bin")
        #PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
# Script to be executed at installation time (kind of post-intallation script) to
# change the right accesses on the installed files
#INSTALL(SCRIPT inst.cmake)

find_package(Gettext REQUIRED)
find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext)
if(NOT GETTEXT_XGETTEXT_EXECUTABLE OR NOT GETTEXT_MSGFMT_EXECUTABLE)
	message(FATAL_ERROR "xgettext not found")
endif()
add_custom_command(
	OUTPUT ${PO_FILE}
	COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE} --from-code=koi8-r ${SOURCES} -c -k_ -kN_ -o ${PO_FILE}
	COMMAND sed 's/charset=UTF-8/charset=koi8-r/' ${PO_FILE} | enconv > tmp && mv -f tmp ${PO_FILE}
	COMMAND ${GETTEXT_MSGMERGE_EXECUTABLE} -Uis ${RU_FILE} ${PO_FILE}
	DEPENDS ${SOURCES})
add_custom_command(
	OUTPUT ${MO_FILE}
	COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} ${RU_FILE} -o ${MO_FILE}
	DEPENDS ${RU_FILE})
