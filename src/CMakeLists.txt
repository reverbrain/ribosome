add_library(ribosome SHARED
	exception.cpp
	fpool.cpp
	locale.cpp
)
target_link_libraries(ribosome
	${GLOG_LIBRARIES}
	${TIDY_LIBRARIES}
)
set_target_properties(ribosome PROPERTIES
	VERSION ${DEBFULLVERSION}
	SOVERSION ${RIBOSOME_VERSION_MAJOR}
	LINKER_LANGUAGE CXX
)

add_executable(ribosome_rans_coder rans.cpp)
target_link_libraries(ribosome_rans_coder
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
	ribosome
)

install(TARGETS	ribosome_rans_coder RUNTIME DESTINATION bin COMPONENT runtime)
install(TARGETS	ribosome LIBRARY DESTINATION ${INSTALL_LIB_DIR})
