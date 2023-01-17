cmake_minimum_required(VERSION 3.12)
project(VTKData2Json)

find_package(VTK REQUIRED
  COMPONENTS
  CommonCore
  IOLegacy
  IOXML

  nlohmannjson
  cli11
  vtksys
)

if (NOT VTK_FOUND)
  message(FATAL_ERROR "Unable to find VTK.")
endif()

add_executable(serialize2json  main.cxx vtkSerializeToJson.cxx)
target_link_libraries(serialize2json PRIVATE ${VTK_LIBRARIES})
