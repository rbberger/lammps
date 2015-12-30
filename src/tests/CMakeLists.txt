project(lammps_tests)

# add googletest library
add_subdirectory(../../lib/googletest googletest)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

macro(subdirlist result curdir)
  file(GLOB children RELATIVE ${curdir} ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${curdir}/${child})
      list(APPEND dirlist ${child})
    endif()
  endforeach()
  set(${result} ${dirlist})
endmacro()

# add tests from subdirectories
# this allows packages to add their own tests after installation
subdirlist(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})

foreach(subdir ${SUBDIRS})
    add_subdirectory(${subdir})
endforeach()

# google test main with MPI_Init and MPI_Finalize calls
add_library(gtest_mpi_main STATIC gtest_mpi_main.cpp)