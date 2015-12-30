project(LAMMPS C CXX)
cmake_minimum_required(VERSION 3.2)
option(TESTING "TESTING" OFF)

file(GLOB SOURCES *.cpp)
list(REMOVE_ITEM SOURCES main.cpp)

add_custom_target(GenerateHeaders
           COMMAND sh Make.sh style
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_library(lammps SHARED ${SOURCES})
add_dependencies(lammps GenerateHeaders)

add_executable(lammps_bin main.cpp)
target_link_libraries(lammps_bin lammps)
set_target_properties(lammps_bin PROPERTIES OUTPUT_NAME lammps)

#=======================================

find_package(VTK NO_MODULE)

if(VTK_FOUND)
  INCLUDE(${VTK_USE_FILE})
  ADD_DEFINITIONS(-DLAMMPS_VTK)
  TARGET_LINK_LIBRARIES(lammps ${VTK_LIBRARIES})
  MESSAGE(STATUS "Found VTK")
else(VTK_FOUND)
  MESSAGE(STATUS "VTK NOT found!")
endif(VTK_FOUND)

#=======================================

find_package(JPEG)

if(JPEG_FOUND)
  INCLUDE_DIRECTORIES(${JPEG_INCLUDE_DIR})
  ADD_DEFINITIONS(-DLAMMPS_JPEG)
  TARGET_LINK_LIBRARIES(lammps jpeg)
else(JPEG_FOUND)
  MESSAGE(STATUS "JPEG NOT found!")
endif(JPEG_FOUND)

#=======================================

find_package(PNG)

if(PNG_FOUND)
  INCLUDE_DIRECTORIES(${PNG_INCLUDE_DIR})
  ADD_DEFINITIONS(-DLAMMPS_PNG)
  TARGET_LINK_LIBRARIES(lammps png)
else(PNG_FOUND)
  MESSAGE(STATUS "PNG NOT found!")
endif(PNG_FOUND)

#=======================================

find_package(MPI)

if(MPI_FOUND)
  INCLUDE_DIRECTORIES(${MPI_INCLUDE_PATH})
  TARGET_LINK_LIBRARIES(lammps ${MPI_LIBRARIES})

  if(MPI_COMPILE_FLAGS)
    SET_TARGET_PROPERTIES(lammps PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
  endif()

  if(MPI_LINK_FLAGS)
    SET_TARGET_PROPERTIES(lammps PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
  endif()
else(MPI_FOUND)
  MESSAGE(STATUS "MPI NOT found! Using MPI Stubs instead.")
  ADD_SUBDIRECTORY(STUBS)
  TARGET_LINK_LIBRARIES(lammps mpi_stubs)
endif(MPI_FOUND)

#=======================================

install(TARGETS lammps lammps_bin
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)

if(TESTING)
  MESSAGE(STATUS "Testing enabled.")
  enable_testing()
  include_directories(${CMAKE_CURRENT_SOURCE_DIR})
  add_subdirectory(tests)
endif(TESTING)
