cmake_minimum_required(VERSION 2.8)

find_package(Boost COMPONENTS random unit_test_framework REQUIRED)


add_definitions(-pedantic -Wall)

add_library(obstack
	obstack.cpp
)
target_link_libraries(obstack "${Boost_RANDOM_LIBRARY}")


add_executable(obstack_test
	obstack_test.cpp
)

target_link_libraries(obstack_test
	obstack
	"${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}"
)
