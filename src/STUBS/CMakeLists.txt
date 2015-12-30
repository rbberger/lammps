add_library(mpi_stubs SHARED mpi.c)
target_include_directories(mpi_stubs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
