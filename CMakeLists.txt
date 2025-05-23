cmake_minimum_required(VERSION 3.27)

set(PROJECT_NAME "nadi_websocket_node")

# Set C++ standard to C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Disable compiler-specific extensions

include(FetchContent)
FetchContent_Populate(
  vcpkg
  GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
  GIT_TAG        2024.12.16
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/vcpkg"
)


# get latest git tag
execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Fallback if no git tag exists
if (NOT GIT_VERSION)
    set(GIT_VERSION "0.0.0") # standard version
endif()

# Set the version in the project
message(STATUS "Project version: ${GIT_VERSION}")
add_definitions(-DPROJECT_VERSION="${GIT_VERSION}")

list(APPEND CMAKE_PROJECT_TOP_LEVEL_INCLUDES "${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_PROJECT_TOP_LEVEL_INCLUDES)

set(CMAKE_TOOLCHAIN_FILE "${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti")

project(${PROJECT_NAME} VERSION 0.0.1)

if(MSVC)
    add_compile_options(
        $<$<CONFIG:>:/MD> #---------|
        $<$<CONFIG:Debug>:/MTd> #---|-- Statically link the runtime libraries
        $<$<CONFIG:Release>:/MT> #--|
    )
    #add_compile_options(/permissive-)
    #set(Boost_USE_STATIC_LIBS "On")
    #find_package( Boost 1.70 COMPONENTS system thread regex REQUIRED )
endif()


include(cmake_helpers/BuildOptions.cmake)
include(cmake_helpers/FindOrFetch.cmake)

# Find scope-guard header
find_path(SCOPE_GUARD_INCLUDE_DIRS "scope_guard.hpp")
if(NOT SCOPE_GUARD_INCLUDE_DIRS)
    message(FATAL_ERROR "scope-guard header not found. Ensure scope-guard is installed via vcpkg.")
endif()

add_library(${PROJECT_NAME} SHARED
               src/main.cpp)

target_include_directories(${PROJECT_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

# Ensure position-independent code for shared libraries (Linux/macOS)
set_target_properties(${PROJECT_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Set properties for DLL to ensure C-ABI and export symbols
target_compile_definitions(${PROJECT_NAME}
    PRIVATE NADIWS_EXPORTS # Used to control export/import in header
)

find_or_fetch_package(
  cmake_git_version 0.1 GIT_REPOSITORY
  https://github.com/dominicpoeschko/cmake_git_version.git GIT_TAG master)

find_package(Boost REQUIRED COMPONENTS system thread filesystem asio regex)
target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::asio Boost::system Boost::thread Boost::filesystem Boost::regex)

find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)

find_package(Crow CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Crow::Crow)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)


if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS OFF # We control exports manually
    )
endif()
