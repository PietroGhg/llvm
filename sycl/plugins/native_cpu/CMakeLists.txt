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
  target_compile_definitions(LLVMSYCLLowerIR PRIVATE  __NATIVECPU_USE_DDK__)
  target_include_directories(LLVMSYCLLowerIR PRIVATE ${oneapi-ck_SOURCE_DIR}/modules/compiler/utils/include)
  target_link_libraries(LLVMSYCLLowerIR PRIVATE compiler-utils)
  add_dependencies(pi_native_cpu abacus_bitcode64)
endif()
