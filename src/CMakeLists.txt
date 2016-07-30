add_library(ribosome SHARED
	fpool.cpp
	locale.cpp
)
target_link_libraries(ribosome
	${Boost_LIBRARIES}
	${GLOG_LIBRARIES}
	${TIDY_LIBRARIES}
)
set_target_properties(ribosome PROPERTIES
	VERSION ${DEBFULLVERSION}
	SOVERSION ${RIBOSOME_VERSION_MAJOR}
	LINKER_LANGUAGE CXX
)

install(TARGETS	ribosome LIBRARY DESTINATION ${INSTALL_LIB_DIR})
