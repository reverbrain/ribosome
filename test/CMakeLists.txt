add_executable(ribosome_test_main main.cpp)
target_link_libraries(ribosome_test_main
	${GLOG_LIBRARIES}
	${GTEST_LIBRARIES}
)

add_executable(ribosome_test_icu icu.cpp)
target_link_libraries(ribosome_test_icu
	${GLOG_LIBRARIES}
	${GTEST_LIBRARIES}
	${ICU_LIBRARIES}
)
