include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

file(GLOB CORE_TEST_SOURCES *.cpp)
add_executable(run_core_tests ${CORE_TEST_SOURCES})
target_link_libraries(run_core_tests gtest gtest_mpi_main lammps)

add_test(core_tests run_core_tests)