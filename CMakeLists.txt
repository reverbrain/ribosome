cmake_minimum_required (VERSION 2.6)
project (ribosome)

FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+)" WARP_VERSION_MAJOR "${DEBFULLVERSION}")
STRING (REGEX MATCH "([0-9]+$)" WARP_VERSION_MINOR "${DEBFULLVERSION}")

set(CMAKE_CXX_FLAGS "-g -std=c++0x -W -Wall -Wextra -fstack-protector-all")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(ICU REQUIRED COMPONENTS i18n io)
set(CMAKE_EXE_LINKER_FLAGS -pthread)
find_package(Threads REQUIRED)

include(cmake/locate_library.cmake)
LOCATE_LIBRARY(GLOG "glog/logging.h" "glog")

find_path(EIGEN3_INCLUDE_DIRS NAMES "signature_of_eigen3_matrix_library" PATH_SUFFIXES "eigen3")
if (NOT EIGEN3_INCLUDE_DIRS)
	message(FATAL_ERROR "Eigen3 development files have not been found")
endif()

configure_file(cmake/RibosomeConfig.cmake.in "${PROJECT_BINARY_DIR}/cmake/RibosomeConfig.cmake" @ONLY)

get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
if ("${LIB64}" STREQUAL "TRUE")
    set(LIB_SUFFIX 64)
else()
    set(LIB_SUFFIX "")
endif()

set(INSTALL_LIB_DIR lib${LIB_SUFFIX} CACHE PATH "Installation directory for libraries")
mark_as_advanced(INSTALL_LIB_DIR)

option(WANT_GTEST "Disable if there is no way to download Google test suite" ON)

if (WANT_GTEST)
	include(ExternalProject)
	ExternalProject_Add(googletest
		URL https://github.com/google/googletest/archive/release-1.7.0.tar.gz
		PREFIX "test/googletest-release-1.7.0"
		INSTALL_COMMAND ""
		LOG_DOWNLOAD ON
		LOG_CONFIGURE ON
		LOG_BUILD ON
	)

	ExternalProject_Get_Property(googletest source_dir)
	set(GTEST_INCLUDE_DIRS ${source_dir}/include)

	ExternalProject_Get_Property(googletest binary_dir)
	set(GTEST_LIBRARY_PATH ${binary_dir}/${CMAKE_FIND_LIBRARY_PREFIXES}gtest.a)
	set(GTEST_LIBRARIES gtest)
	add_library(${GTEST_LIBRARIES} UNKNOWN IMPORTED)
	set_property(TARGET ${GTEST_LIBRARIES} PROPERTY IMPORTED_LOCATION ${GTEST_LIBRARY_PATH})
	add_dependencies(${GTEST_LIBRARIES} googletest)
endif()



include_directories(
	${PROJECT_SOURCE_DIR}/include
	${GTEST_INCLUDE_DIRS}
	${GLOG_INCLUDE_DIRS}
	${ICU_INCLUDE_DIRS}
	${EIGEN3_INCLUDE_DIRS}
)

link_directories(
	${ICU_LIBRARY_DIRS}
	${GLOG_LIBRARY_DIRS}
)

add_subdirectory(test)

FILE(GLOB headers
	"${PROJECT_SOURCE_DIR}/include/ribosome/*.hpp"
)
install(FILES "${PROJECT_BINARY_DIR}/cmake/RibosomeConfig.cmake" DESTINATION share/ribosome/cmake)
install(FILES ${headers} DESTINATION include/ribosome)
