cmake_minimum_required (VERSION 2.6)
project (ribosome)

FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+)" WARP_VERSION_MAJOR "${DEBFULLVERSION}")
STRING (REGEX MATCH "([0-9]+$)" WARP_VERSION_MINOR "${DEBFULLVERSION}")

set(CMAKE_CXX_FLAGS "-g -std=c++0x -W -Wall -Wextra -fstack-protector-all")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(ICU REQUIRED COMPONENTS i18n io)

find_path(EIGEN3_INCLUDE_DIRS NAMES "signature_of_eigen3_matrix_library" PATH_SUFFIXES "eigen3")
if (NOT EIGEN3_INCLUDE_DIRS)
	message(FATAL_ERROR "Eigen3 development files have not been found")
endif()

configure_file(cmake/RibosomeConfig.cmake.in "${PROJECT_BINARY_DIR}/cmake/RibosomeConfig.cmake" @ONLY)

include_directories(
	${PROJECT_SOURCE_DIR}/include
	${ICU_INCLUDE_DIRS}
	${EIGEN3_INCLUDE_DIRS}
)

link_directories(
	${ICU_LIBRARY_DIRS}
)

add_subdirectory(src)

FILE(GLOB headers
	"${PROJECT_SOURCE_DIR}/include/ribosome/*.hpp"
)
install(FILES "${PROJECT_BINARY_DIR}/cmake/RibosomeConfig.cmake" DESTINATION share/ribosome/cmake)
install(FILES ${headers} DESTINATION include/ribosome)
