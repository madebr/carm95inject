cmake_minimum_required(VERSION 3.15)
project(carmabypass)

set(CARM95_PATH "" CACHE STRING "Directory containing CARM95.EXE")

if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    conan_basic_setup()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Detours REQUIRED)

add_library(extradebug SHARED
    extradebug.cpp
)
target_link_libraries(extradebug PRIVATE Detours::Detours)
target_link_options(extradebug PRIVATE /export:DetourFinishHelperProcess,@1,NONAME)

add_executable(carm95inj
    carma95inj.cpp
)
target_link_libraries(carm95inj PRIVATE Detours::Detours)

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
