cmake_minimum_required(VERSION 2.8)
set(PROJ fli_control)
project(${PROJ})
set(MINOR_VERSION "0")
set(MID_VERSION "0")
set(MAJOR_VERSION "1")
set(VERSION "${MAJOR_VERSION}.${MID_VERSION}.${MINOR_VERSION}")

message("VER: ${VERSION}")

# default flags
set(CFLAGS ${CFLAGS} -O2 -std=gnu99)
add_definitions(-D_XOPEN_SOURCE=666 -D_DEFAULT_SOURCE -D_GNU_SOURCE)

set(CMAKE_COLOR_MAKEFILE ON)

set(SOURCES main.c cmdlnopts.c flifunc.c)

# cmake -DDEBUG=yes -> debugging
if(DEFINED DEBUG AND DEBUG STREQUAL "yes")
    set(CFLAGS ${CFLAGS} -Wextra -Wall -Werror -W)
    add_definitions(-DEBUG)
    set(CMAKE_VERBOSE_MAKEFILE "ON")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
find_package(CFITSIO REQUIRED)
find_package(PkgConfig REQUIRED)
set(MODULES fli>=1.71 usefull_macros)
# additional modules on condition
if(DEFINED USEPNG AND USEPNG STREQUAL "yes")
    set(MODULES ${MODULES} libpng>=1.2)
    add_definitions(-DUSEPNG)
endif()
pkg_check_modules(${PROJ} REQUIRED ${MODULES})
if(DEFINED USERAW AND USERAW STREQUAL "yes")
    add_definitions(-DUSERAW)
endif()
if(DEFINED IMAGEVIEW AND IMAGEVIEW STREQUAL "yes")
    list(APPEND SOURCES events.c imageview.c)
    find_package(OpenGL REQUIRED)
    find_package(GLUT REQUIRED)
    find_package(X11 REQUIRED)
    find_package(Threads REQUIRED)
    list(APPEND ${PROJ}_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
    add_definitions(-DIMAGEVIEW)
endif()

# change wrong behaviour with install prefix
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND CMAKE_INSTALL_PREFIX MATCHES "/usr/local")
    message("Change default install path to /usr")
    set(CMAKE_INSTALL_PREFIX "/usr")
endif()
message("Install dir prefix: ${CMAKE_INSTALL_PREFIX}")

# directory should contain dir locale/ru for gettext translations
set(LCPATH ${CMAKE_SOURCE_DIR}/locale/ru)

if(NOT DEFINED LOCALEDIR)
    if(DEFINED DEBUG AND DEBUG STREQUAL "yes")
        set(LOCALEDIR ${CMAKE_CURRENT_SOURCE_DIR}/locale)
    else()
        set(LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
    endif()
endif()

# gettext files
set(PO_FILE ${LCPATH}/messages.po)
set(MO_FILE ${LCPATH}/LC_MESSAGES/${PROJ}.mo)
set(RU_FILE ${LCPATH}/ru.po)

# exe file
add_executable(${PROJ} ${SOURCES} ${PO_FILE} ${MO_FILE})
target_link_libraries(${PROJ} ${CFITSIO_LIBRARIES} ${X11_LIBRARIES} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} ${${PROJ}_LIBRARIES} -lm)
include_directories(${${PROJ}_INCLUDE_DIRS} image_view_module)
link_directories(${${PROJ}_LIBRARY_DIRS} )
add_definitions(${CFLAGS} -DLOCALEDIR=\"${LOCALEDIR}\"
        -DPACKAGE_VERSION=\"${VERSION}\" -DGETTEXT_PACKAGE=\"${PROJ}\"
        -DMINOR_VERSION=\"${MINOR_VERSION}\" -DMID_VERSION=\"${MID_VERSION}\"
        -DMAJOR_VERSION=\"${MAJOR_VESION}\")

# Installation of the program
INSTALL(FILES ${MO_FILE} DESTINATION "share/locale/ru/LC_MESSAGES")
INSTALL(TARGETS ${PROJ} DESTINATION "bin")

find_package(Gettext REQUIRED)
find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext)
if(NOT GETTEXT_XGETTEXT_EXECUTABLE OR NOT GETTEXT_MSGFMT_EXECUTABLE)
    message(FATAL_ERROR "xgettext not found")
endif()
file(MAKE_DIRECTORY ${LCPATH})
file(MAKE_DIRECTORY ${LCPATH}/LC_MESSAGES)

add_custom_command(
    OUTPUT ${PO_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE} --from-code=utf-8 ${SOURCES} -c -k_ -kN_ -o ${PO_FILE}
    COMMAND sed -i 's/charset=.*\\\\n/charset=koi8-r\\\\n/' ${PO_FILE}
    COMMAND enconv ${PO_FILE}
    DEPENDS ${SOURCES}
)
# we need this to prewent ru.po & .mo from deleting by make clean
add_custom_command(
    OUTPUT ${MO_FILE}
    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} ${RU_FILE} -o ${MO_FILE}
    DEPENDS ${RU_FILE} ru_file_updated
)

add_custom_command(
    OUTPUT ru_file_updated
    COMMAND [ -f ${RU_FILE} ] && ${GETTEXT_MSGMERGE_EXECUTABLE} -Uis ${RU_FILE} ${PO_FILE} || cp ${PO_FILE} ${RU_FILE}
    COMMAND ${CMAKE_COMMAND} -E touch ru_file_updated
    BYPRODUCTS ${RU_FILE}
    DEPENDS ${PO_FILE}
)

add_custom_target(MO_FILE ALL DEPENDS ${MO_FILE})
