cmake_minimum_required(VERSION 4.2)

set(BLITZ_VCPKG_ROOT "" CACHE PATH "Path to the vcpkg checkout")
if(NOT BLITZ_VCPKG_ROOT)
    if(DEFINED VCPKG_ROOT AND EXISTS "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        file(TO_CMAKE_PATH "${VCPKG_ROOT}" BLITZ_VCPKG_ROOT)
    elseif(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" BLITZ_VCPKG_ROOT)
    elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake")
        get_filename_component(BLITZ_VCPKG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../vcpkg" ABSOLUTE)
    else()
        find_program(BLITZ_VCPKG_EXECUTABLE NAMES vcpkg)
        if(BLITZ_VCPKG_EXECUTABLE)
            get_filename_component(_blitz_vcpkg_bin_dir "${BLITZ_VCPKG_EXECUTABLE}" DIRECTORY)
            if(EXISTS "${_blitz_vcpkg_bin_dir}/scripts/buildsystems/vcpkg.cmake")
                set(BLITZ_VCPKG_ROOT "${_blitz_vcpkg_bin_dir}")
            endif()
        endif()
        unset(_blitz_vcpkg_bin_dir)
        if(NOT BLITZ_VCPKG_ROOT AND EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
            set(BLITZ_VCPKG_ROOT "C:/vcpkg")
        endif()
    endif()
endif()

if((NOT DEFINED CMAKE_TOOLCHAIN_FILE OR CMAKE_TOOLCHAIN_FILE STREQUAL "") AND BLITZ_VCPKG_ROOT)
    set(CMAKE_TOOLCHAIN_FILE
        "${BLITZ_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "vcpkg toolchain file")
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR CMAKE_TOOLCHAIN_FILE STREQUAL "")
    message(FATAL_ERROR
        "vcpkg was not found. Set VCPKG_ROOT, set BLITZ_VCPKG_ROOT, or pass "
        "-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake.")
endif()

set(VCPKG_TARGET_TRIPLET "x86-windows" CACHE STRING "vcpkg target triplet")
set(VCPKG_HOST_TRIPLET "x64-windows" CACHE STRING "vcpkg host triplet")
set(VCPKG_MANIFEST_MODE ON CACHE BOOL "Use vcpkg manifest mode")
set(VCPKG_MANIFEST_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "vcpkg manifest directory")
set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed" CACHE PATH "vcpkg installed tree")

project(Blitzkrieg LANGUAGES C CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "Windows is required.")
endif()
if(NOT MSVC)
    message(FATAL_ERROR "Visual Studio/MSVC is required.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Build the Win32 target (for Visual Studio use -A Win32).")
endif()
if(NOT CMAKE_GENERATOR MATCHES "^Visual Studio")
    message(FATAL_ERROR "A Visual Studio generator is currently required.")
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)

find_path(SQUISH_INCLUDE_DIR NAMES squish.h REQUIRED)
find_library(SQUISH_LIBRARY NAMES squish libsquish REQUIRED)

set(BLITZ_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Sources/sdk" CACHE PATH "Bundled third-party SDK root")
set(D3D8_API_ROOT "${BLITZ_SDK_ROOT}/d3d8to9" CACHE PATH "d3d8to9 checkout")
set(FMOD_ROOT "${BLITZ_SDK_ROOT}/FMOD" CACHE PATH "FMOD Audio Engine checkout")

if(NOT EXISTS "${D3D8_API_ROOT}/source/d3d8.hpp")
    message(FATAL_ERROR "Missing ${D3D8_API_ROOT}/source/d3d8.hpp")
endif()
if(NOT EXISTS "${D3D8_API_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "Missing ${D3D8_API_ROOT}/CMakeLists.txt")
endif()

find_path(FMOD_INCLUDE_DIR NAMES fmod.hpp
    PATHS "${FMOD_ROOT}/Include"
    NO_DEFAULT_PATH REQUIRED)
find_library(FMOD_IMPLIB NAMES fmod_vc fmod
    PATHS "${FMOD_ROOT}/lib/x86"
    NO_DEFAULT_PATH REQUIRED)
file(GLOB FMOD_DLL_CANDIDATES LIST_DIRECTORIES FALSE "${FMOD_ROOT}/lib/x86/fmod.dll")
if(NOT FMOD_DLL_CANDIDATES)
    file(GLOB_RECURSE FMOD_DLL_CANDIDATES LIST_DIRECTORIES FALSE
        "${FMOD_ROOT}/lib/x86/*fmod.dll")
endif()
if(NOT FMOD_DLL_CANDIDATES)
    message(FATAL_ERROR "FMOD runtime DLL was not found under ${FMOD_ROOT}/lib/x86")
endif()
list(GET FMOD_DLL_CANDIDATES 0 FMOD_DLL)

add_library(FMOD::Core SHARED IMPORTED GLOBAL)
set_target_properties(FMOD::Core PROPERTIES
    IMPORTED_IMPLIB "${FMOD_IMPLIB}"
    IMPORTED_LOCATION "${FMOD_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${FMOD_INCLUDE_DIR}")

add_subdirectory("${D3D8_API_ROOT}" "${CMAKE_BINARY_DIR}/d3d8to9" EXCLUDE_FROM_ALL)

function(blitz_sources out dir)
    set(root "${CMAKE_CURRENT_SOURCE_DIR}/Sources/src/${dir}")
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
        "${root}/*.c"
        "${root}/*.cpp"
        "${root}/*.h"
        "${root}/*.rc"
        "${root}/*.def"
        "${root}/*.ico"
        "${root}/*.bmp"
        "${root}/*.cur"
        "${root}/*.txt")
    foreach(file IN LISTS ARGN)
        list(REMOVE_ITEM sources "${root}/${file}")
    endforeach()

    # GlobalsLoader.cpp precedes other static initializers in the original projects.
    set(globals_loader "${root}/GlobalsLoader.cpp")
    if(globals_loader IN_LIST sources)
        list(REMOVE_ITEM sources "${globals_loader}")
        list(PREPEND sources "${globals_loader}")
    endif()

    set(${out} ${sources} PARENT_SCOPE)
endfunction()

function(blitz_target_defaults target)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Sources/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/Sources/src/${target}"
        ${ZLIB_INCLUDE_DIRS})
    target_compile_definitions(${target} PRIVATE
        WIN32 _WINDOWS NOMINMAX WIN32_LEAN_AND_MEAN
        $<$<CONFIG:Release>:_FINALRELEASE;NDEBUG>
        $<$<CONFIG:RelWithDebInfo>:_BETARELEASE;NDEBUG>
        $<$<CONFIG:Debug>:_DEBUG;_DO_ASSERT_SLOW;_DO_CHECKED_CAST>)
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:C,CXX>:/W3>
        $<$<COMPILE_LANGUAGE:C,CXX>:/MP>
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:forScope>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:wchar_t->
        $<$<COMPILE_LANGUAGE:C,CXX>:/source-charset:.1251>
        $<$<COMPILE_LANGUAGE:C,CXX>:/execution-charset:.1251>
        $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/Zi>)
    target_link_options(${target} PRIVATE
        $<$<CONFIG:Release>:/DEBUG:FULL>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>)
    target_link_libraries(${target} PRIVATE comsupp oleaut32)
    set_property(TARGET ${target} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endfunction()

function(blitz_static target)
    blitz_sources(sources ${target} ${ARGN})
    add_library(${target} STATIC ${sources})
    blitz_target_defaults(${target})
endfunction()

function(blitz_shared target)
    blitz_sources(sources ${target} ${ARGN})
    add_library(${target} SHARED ${sources})
    blitz_target_defaults(${target})
endfunction()

function(blitz_stage_file target source)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${source}" "$<TARGET_FILE_DIR:${target}>"
        VERBATIM)
endfunction()

blitz_static(Misc)
blitz_static(Formats)
blitz_static(Common)
blitz_static(LuaLib)
blitz_static(RandomMapGen)

set(GAMESPY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Sources/src/GameSpy")
add_library(GameSpy STATIC
    "${GAMESPY_ROOT}/chat/chatCallbacks.c"
    "${GAMESPY_ROOT}/chat/chatChannel.c"
    "${GAMESPY_ROOT}/chat/chatCrypt.c"
    "${GAMESPY_ROOT}/chat/chatHandlers.c"
    "${GAMESPY_ROOT}/chat/chatMain.c"
    "${GAMESPY_ROOT}/chat/chatSocket.c"
    "${GAMESPY_ROOT}/queryreporting/gqueryreporting.c"
    "${GAMESPY_ROOT}/cengine/gserver.c"
    "${GAMESPY_ROOT}/cengine/gserverlist.c"
    "${GAMESPY_ROOT}/cengine/gutil.c"
    "${GAMESPY_ROOT}/darray.c"
    "${GAMESPY_ROOT}/hashtable.c"
    "${GAMESPY_ROOT}/md5c.c"
    "${GAMESPY_ROOT}/nonport.c"
    "${GAMESPY_ROOT}/peer/peerCallbacks.c"
    "${GAMESPY_ROOT}/peer/peerCEngine.c"
    "${GAMESPY_ROOT}/peer/peerGlobalCallbacks.c"
    "${GAMESPY_ROOT}/peer/peerKeys.c"
    "${GAMESPY_ROOT}/peer/peerMain.c"
    "${GAMESPY_ROOT}/peer/peerMangle.c"
    "${GAMESPY_ROOT}/peer/peerOperations.c"
    "${GAMESPY_ROOT}/peer/peerPing.c"
    "${GAMESPY_ROOT}/peer/peerPlayers.c"
    "${GAMESPY_ROOT}/peer/peerRooms.c"
    "${GAMESPY_ROOT}/pinger/pingerMain.c")
blitz_target_defaults(GameSpy)

blitz_static(Main)

blitz_shared(BugSlay STLDebugTerminate.cpp)
blitz_shared(StreamIO
    FileIO.def OpenStorage.cpp StructureSaver.cpp StructureSaverInternal.h)
blitz_shared(Image)
blitz_shared(Input)
blitz_shared(Net Net.rc)
blitz_shared(SFX)
blitz_shared(GFX Sphere.cpp Sphere.h)
blitz_shared(Anim Anim.rc)
blitz_shared(Scene MechTraceVisObj.cpp MechTraceVisObj.h)
blitz_shared(UI)
blitz_shared(GameTT)
blitz_shared(AILogic
    AIEditorSerialize.h AILogicTypes.h AIPath.def AnimUnit.cpp
    CCommonStates.h GunDescriptorInternal.cpp GunDescriptorInternal.h)

# Preserve the per-project defines from the original VC6 .dsp files.  These
# control static GlobalsLoader bootstrap behaviour and are REQUIRED before WinMain runs!
target_compile_definitions(BugSlay PRIVATE
    BUGSLAYER_EXPORTS _DONT_LOAD_SINGLETONS _DONT_LOAD_STREAMIO)
target_compile_definitions(StreamIO PRIVATE
    _STREAMIO_DLL _DONT_LOAD_SINGLETONS _DONT_LOAD_STREAMIO)
target_compile_definitions(Image PRIVATE _DONT_LOAD_SINGLETONS)

target_link_libraries(BugSlay PRIVATE comctl32 dbghelp version)

target_include_directories(Scene PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(Scene PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(Scene PRIVATE ${FFMPEG_LIBRARIES})

target_link_libraries(SFX PRIVATE FMOD::Core)

target_include_directories(Image PRIVATE "${SQUISH_INCLUDE_DIR}")
target_link_libraries(Image PRIVATE PNG::PNG ZLIB::ZLIB "${SQUISH_LIBRARY}")
target_link_libraries(StreamIO PRIVATE ZLIB::ZLIB pugixml::pugixml)

target_link_libraries(Formats PRIVATE Misc)
target_link_libraries(Common PRIVATE Misc)
target_link_libraries(RandomMapGen PRIVATE BugSlay Misc StreamIO)
target_link_libraries(LuaLib PRIVATE Misc)

target_link_libraries(StreamIO PRIVATE BugSlay Misc winmm ws2_32)
target_link_libraries(Image PRIVATE BugSlay Misc)
target_link_libraries(Input PRIVATE BugSlay Misc StreamIO winmm dinput8 dxguid)
target_compile_definitions(Input PRIVATE DIRECTINPUT_VERSION=0x0800)
target_link_libraries(Net PRIVATE BugSlay GameSpy Misc StreamIO ws2_32)
target_link_libraries(SFX PRIVATE BugSlay Misc StreamIO)
target_link_libraries(Anim PRIVATE BugSlay Misc StreamIO Formats)
target_link_libraries(UI PRIVATE BugSlay Misc StreamIO LuaLib)
target_link_libraries(Scene PRIVATE BugSlay Misc StreamIO SFX Formats winmm)
target_link_libraries(GameTT PRIVATE BugSlay Misc StreamIO Common Formats RandomMapGen version winmm)
target_link_libraries(AILogic PRIVATE BugSlay StreamIO Misc Image LuaLib Formats RandomMapGen ZLIB::ZLIB)

target_include_directories(GFX PRIVATE "${D3D8_API_ROOT}/source")
target_link_libraries(GFX PRIVATE BugSlay Image Misc StreamIO Formats d3d11 dxgi)

target_link_libraries(Main PRIVATE
    BugSlay UI AILogic Anim Image Input Scene SFX GFX
    GameSpy LuaLib ZLIB::ZLIB StreamIO Net Misc Formats RandomMapGen)

blitz_sources(GAME_SOURCES Game CManuverHorisontal.cpp BetaRelease/error.txt FastDebug/error.txt)
add_executable(Game WIN32 ${GAME_SOURCES})
blitz_target_defaults(Game)
target_link_libraries(Game PRIVATE
    BugSlay Main Misc StreamIO GameTT RandomMapGen Formats LuaLib Net
    version winmm Shlwapi)

add_dependencies(Game d3d8to9)

blitz_stage_file(Game "${FMOD_DLL}")
foreach(runtime_config IN ITEMS config.cfg defconf.cfg)
    blitz_stage_file(Game "${CMAKE_CURRENT_SOURCE_DIR}/Versions/Current/${runtime_config}")
endforeach()
blitz_stage_file(Game "$<TARGET_FILE:d3d8to9>")

add_custom_command(TARGET Game POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/bin,bin>"
        "$<TARGET_FILE_DIR:Game>"
    VERBATIM)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Data")
    add_custom_command(TARGET Game POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Data"
            "$<TARGET_FILE_DIR:Game>/Data"
        VERBATIM)
endif()
