cmake_minimum_required(VERSION 3.8)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12" CACHE STRING "Minimum OS X deployment version")

if(MSVC)
    SET(CMAKE_BUILD_TYPE RelWithDebInfo)
else()
    #SET(CMAKE_BUILD_TYPE Release)
endif()

set(PORTABLE 1)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if(APPLE)
    set(CMAKE_INSTALL_NAME_DIR "@executable_path")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
elseif(LINUX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

project(GRIN_PLUS_PLUS)

message("Grin++ Version: ${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]") #Case insensitive match
    message(FATAL_ERROR "Clang not supported")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++17 -stdlib=libc++ -fno-aligned-allocation")
endif()

if(MSVC)
    message("Setting flags for Visual Studio C++")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")

    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4 /WX" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
    endif()

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL
        CMAKE_C_FLAGS_RELWITHDEBINFO
        )
foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    set(${CompilerFlag} "${${CompilerFlag}}" CACHE STRING "msvc compiler flags" FORCE)
    message("MSVC flags: ${CompilerFlag}:${${CompilerFlag}}")
endforeach()
    #set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

    SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /INCREMENTAL:NO")
    SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /OPT:REF /INCREMENTAL:NO")
    SET(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} /OPT:REF /INCREMENTAL:NO")
else ()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wno-deprecated-copy -Wno-pessimizing-move")
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(WITH_GFLAGS OFF)
add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_definitions(-DNOMINMAX)
add_definitions(-DASIO_STANDALONE)
add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

include(CheckFunctionExists)
set(CMAKE_REQUIRED_INCLUDES string.h)
CHECK_FUNCTION_EXISTS(memset_s HAS_MEMSET_S)
if (HAS_MEMSET_S)
	add_definitions(-DHAS_MEMSET_S)
endif ()

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/deps
    ${PROJECT_SOURCE_DIR}/deps/mio/single_include
    ${PROJECT_SOURCE_DIR}/deps/stduuid/include
    ${PROJECT_SOURCE_DIR}/deps/asio/asio/include
    ${PROJECT_SOURCE_DIR}/deps/ghc
    ${PROJECT_SOURCE_DIR}/deps/rocksdb-6.7.3/include
    ${PROJECT_SOURCE_DIR}/deps/fmt/include
    ${PROJECT_SOURCE_DIR}/deps/jsoncpp
    ${PROJECT_SOURCE_DIR}/deps/zlib
    ${PROJECT_SOURCE_DIR}/deps/Catch2
    ${PROJECT_SOURCE_DIR}/deps/cppcodec
    ${PROJECT_SOURCE_DIR}/deps/scheduler/include
    ${CMAKE_CURRENT_BINARY_DIR}/deps/zlib
)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})

# Dependencies
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

#find_package(unofficial-sodium CONFIG REQUIRED)
include(sqlite3)
include(minizip)
include(jsoncpp)
include(zlib)
include(rocksdb)
include(croaring)
include(civetweb)
include(uint128)
include(ctaes)
include(scrypt)
include(secp256k1-zkp)
include(sha3)
include(reproc)
include(bitcoin)
add_subdirectory(deps/cuckoo)
add_subdirectory(deps/cppcodec)
add_subdirectory(deps/chachapoly)
add_subdirectory(deps/rfc6234)
add_subdirectory(deps/libsodium-1.0.18)
#add_subdirectory(deps/stduuid)

# Grin++ Projects
add_definitions(-DUNICODE)
include(fmtlib)
add_definitions(-DSPDLOG_WCHAR_FILENAMES)
option(GRINPP_TESTS "Build tests" true)
option(GRINPP_STATIC "Static build" true)
if(GRINPP_STATIC)
    add_definitions(-DGRINPP_STATIC)
else()
    if(GRINPP_TESTS)
        MESSAGE(FATAL_ERROR "Tests require a static build")
    endif()
endif()

if (MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4 /WX" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
     endif()
endif (MSVC)

add_subdirectory(src/Infrastructure)
add_subdirectory(src/Crypto)
add_subdirectory(src/Core)
add_subdirectory(src/PMMR)
add_subdirectory(src/PoW)
add_subdirectory(src/Database)
add_subdirectory(src/BlockChain)
add_subdirectory(src/TxPool)
add_subdirectory(src/Net)
add_subdirectory(src/P2P)
add_subdirectory(src/Wallet)
add_subdirectory(src/Server)

if(GRINPP_TESTS AND NOT LINUX)
    add_subdirectory(tests)
endif(GRINPP_TESTS AND NOT LINUX)
