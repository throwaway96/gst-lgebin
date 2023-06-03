cmake_minimum_required(VERSION 2.6)
project (lgebin)

find_package(PkgConfig)

add_definitions (
	-D_GNU_SOURCE
	-DHAVE_CONFIG_H
)

if("${PLATFORM_CHIP}" STREQUAL "mtk5398")
	add_definitions( -DMTK5398 )
endif()

set(SUB_DIRS src test)

if(TARGET)
	set(CMAKE_C_COMPILER "${TARGET}-gcc")

	if(TARGET_ROOT)
		include_directories(${TARGET_ROOT}/include)
		set(CMAKE_LIBRARY_PATH ${TARGET_ROOT}/lib)
		set(LINK_FLAGS -L ${TARGET_ROOT}/lib)
	endif(TARGET_ROOT)
endif(TARGET)

pkg_check_modules (GLIB2   	glib-2.0)
pkg_check_modules (GOBJECT gobject-2.0)
pkg_check_modules (GST gstreamer-0.10)
pkg_check_modules (GSTBASE gstreamer-base-0.10)
pkg_check_modules (URIPARSER liburiparser)

include_directories (${GST_INCLUDE_DIRS})
include_directories (${GOBJECT_INCLUDE_DIRS})
include_directories (${URIPARSER_INCLUDE_DIRS})

#find_package(GST COMPONENTS gstreamer REQUIRED)

message(STATUS "TARGET_ROOT: ${TARGET_ROOT}")
message(STATUS "CMAKE_LIBRARY_PATH: ${CMAKE_LIBRARY_PATH}")
message(STATUS "GST_INCLUDE_DIRS: ${GST_INCLUDE_DIRS}")
message(STATUS "GST_LIBRARIES: ${GST_LIBRARIES}")
message(STATUS "lgebin_BINARY_DIR: ${lgebin_BINARY_DIR}")

subdirs(${SUB_DIRS})
