cmake_minimum_required(VERSION 3.15)
project(carmabypass)

set(CARM95_PATH "" CACHE STRING "Directory containing CARM95.EXE")

if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    conan_basic_setup(NO_OUTPUT_DIRS)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Detours REQUIRED)

option(INJECT_DR_DPRINTF "Inject dr_dprintf" ON)
option(INJECT_DRFOPEN "Inject DRfopen" ON)

if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++ -s)
endif()

add_library(extradebug SHARED
    extradebug.cpp
)
if(MSVC)
    target_sources(extradebug PRIVATE hook_msvc.def)
elseif(MINGW)
    target_sources(extradebug PRIVATE hook_mingw.def)
endif()
target_link_libraries(extradebug PRIVATE Detours::Detours)
if(MINGW)
    target_link_libraries(extradebug PRIVATE libwinpthread.a)
endif()
target_compile_definitions(extradebug
    PRIVATE
        "$<$<BOOL:${INJECT_DR_DPRINTF}>:INJECT_DR_DPRINTF>"
        "$<$<BOOL:${INJECT_DRFOPEN}>:INJECT_DRFOPEN>"
)
set_target_properties(extradebug
    PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "bin/plugins"
        LIBRARY_OUTPUT_DIRECTORY "bin/plugins"
)

add_executable(carm95inj
    carm95inj.cpp
)
target_link_libraries(carm95inj PRIVATE Detours::Detours)
if(MINGW)
    target_link_libraries(carm95inj PRIVATE libwinpthread.a)
endif()
set_target_properties(carm95inj
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "bin"
        LIBRARY_OUTPUT_DIRECTORY "bin"
)

if(CARM95_PATH)
    add_custom_target(copy_to_carm95_folder
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:carm95inj>" "${CARM95_PATH}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CARM95_PATH}/plugins"
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:extradebug>" "${CARM95_PATH}/plugins"
    )
endif()

install(TARGETS carm95inj
    RUNTIME DESTINATION "."
)
install(TARGETS extradebug
    RUNTIME DESTINATION "plugins"
)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_FILE_NAME "carm95injector")
include(CPack)
