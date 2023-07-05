add_sycl_plugin(native_cpu
  SOURCES
    pi_native_cpu.cpp
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR} 
  LIBRARIES
    sycl
)

option(Native_CPU_USE_DDK "Use the oneAPI construction kit for Native CPU" 1)

if(Native_CPU_USE_DDK)
  include(FetchContent)
  FetchContent_Declare(oneapi-ck
   GIT_REPOSITORY https://github.com/PietroGhg/oneapi-construction-kit.git
   GIT_TAG        pietro/native_cpu
  )
  FetchContent_GetProperties(oneapi-ck)
  if(NOT oneapi-ck_POPULATED)
    message("Cloning oneAPI Construction Kit")
    FetchContent_Populate(oneapi-ck)
    message("Consruction kit cloned in ${oneapi-ck_SOURCE_DIR}")
    set(CA_NATIVE_CPU 1)
    add_subdirectory(${oneapi-ck_SOURCE_DIR} ${oneapi-ck_BINARY_DIR})
  endif()
  #run the equivalent of ninja abacus_bitcode64
  #include(ExternalProject)
  #  ExternalProject_Add(oneapi-ck
  #    GIT_REPOSITORY https://github.com/PietroGhg/oneapi-construction-kit.git
  #    GIT_TAG pietro/in-tree
  #    CMAKE_GENERATOR Ninja
  #    CMAKE_ARGS -DCA_LLVM_INSTALL_DIR=${CMAKE_INSTALL_PREFIX} -DCA_ENABLE_API=cl  -DSpirvTools_spirv-as_EXECUTABLE=/home/pietro/dpcpp_host_device/SPIRV-Tools/build/tools/spirv-as
  #    BUILD_COMMAND ninja abacus_bitcode64
  #    INSTALL_COMMAND ""
  #  )
endif()
