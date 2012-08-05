cmake_minimum_required(VERSION 2.8)

find_package(Boost COMPONENTS random unit_test_framework thread REQUIRED)


add_definitions(-pedantic -Wall -O2 -Wfatal-errors)
#add_definitions(-pedantic -Wall -O0 -ggdb)

add_library(boost_arena
	obstack.cpp
)
target_link_libraries(boost_arena "${Boost_RANDOM_LIBRARY}")


add_executable(arena_test
	arena_test.cpp
)

target_link_libraries(arena_test
	boost_arena
	"${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}"
)

add_executable(arena_benchmark
	arena_benchmark.cpp
)

target_link_libraries(arena_benchmark
	boost_arena
	"${Boost_RANDOM_LIBRARY}"
	"${Boost_THREAD_LIBRARY}"
)
