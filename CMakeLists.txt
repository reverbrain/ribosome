cmake_minimum_required (VERSION 2.6)
project (ribosome)

FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+)" WARP_VERSION_MAJOR "${DEBFULLVERSION}")
STRING (REGEX MATCH "([0-9]+$)" WARP_VERSION_MINOR "${DEBFULLVERSION}")

set(CMAKE_CXX_FLAGS "-g -std=c++0x -W -Wall -Wextra -fstack-protector-all")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(Boost REQUIRED COMPONENTS system locale)

include_directories(
	${PROJECT_SOURCE_DIR}/include
	${Boost_INCLUDE_DIRS}
)

link_directories(
	${Boost_LIBRARY_DIRS}
)

#add_subdirectory(src)

FILE(GLOB headers
	"${PROJECT_SOURCE_DIR}/include/ribosome/*.hpp"
)
install(FILES ${headers} DESTINATION include/ribosome)
