cmake_minimum_required(VERSION 3.16)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(ioquakelive_launcher C)

set(CODE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(ROOT ${CODE}/..)

set(LAUNCHER_VERSION "" CACHE STRING "Version to report instead of PRODUCT_VERSION")
set(LAUNCHER_ICNS "" CACHE FILEPATH "macOS bundle icon")

file(STRINGS ${CODE}/qcommon/q_shared.h VERSION_LINE REGEX "^#define PRODUCT_VERSION \"")
string(REGEX REPLACE "^#define PRODUCT_VERSION \"([^\"]+)\".*" "\\1" PRODUCT_VERSION "${VERSION_LINE}")
if(LAUNCHER_VERSION)
  set(PRODUCT_VERSION ${LAUNCHER_VERSION})
endif()

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
foreach(name SDL_TEST_LIBRARY SDL_TESTS SDL_EXAMPLES SDL_INSTALL SDL_AUDIO SDL_GPU SDL_CAMERA SDL_JOYSTICK SDL_HAPTIC SDL_HIDAPI
             SDL_POWER SDL_SENSOR SDL_VULKAN SDL_OPENGLES SDL_RENDER_GPU SDL_RENDER_VULKAN SDL_RENDER_D3D SDL_RENDER_D3D12)
  set(${name} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(name SDL_VIDEO SDL_RENDER SDL_DIALOG SDL_TRAY)
  set(${name} ON CACHE BOOL "" FORCE)
endforeach()
if(WIN32)
  set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
  set(SDL_RENDER_D3D11 ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(${CODE}/SDL3 ${CMAKE_BINARY_DIR}/SDL3 EXCLUDE_FROM_ALL)

set(SOURCES
  launcher.c
  launcher_jpeg.c
  launcher_logo.c
  launcher_fonts.c
  launcher_flags.c
  launcher_steam.c
  ${CODE}/renderercommon/tr_stbtt.c)
foreach(name inflate inffast inftrees adler32 crc32 zutil)
  list(APPEND SOURCES ${CODE}/zlib/${name}.c)
endforeach()
foreach(name jaricom jcapimin jcapistd jcarith jccoefct jccolor jcdctmgr jchuff jcinit jcmainct jcmarker jcmaster jcomapi jcparam
             jcprepct jcsample jctrans jdapimin jdapistd jdarith jdatadst jdatasrc jdcoefct jdcolor jddctmgr jdhuff jdinput
             jdmainct jdmarker jdmaster jdmerge jdpostct jdsample jdtrans jerror jfdctflt jfdctfst jfdctint jidctflt jidctfst
             jidctint jmemmgr jmemnobs jquant1 jquant2 jutils)
  list(APPEND SOURCES ${CODE}/jpeg-8c/${name}.c)
endforeach()

function(launcher_target TARGET SHOT)
  if(SHOT)
    set(SKIP EXCLUDE_FROM_ALL)
    set(OUT ${CMAKE_BINARY_DIR}/bin/captura$<0:>)
  else()
    set(SKIP "")
    set(OUT ${CMAKE_BINARY_DIR}/bin$<0:>)
  endif()
  if(APPLE)
    add_executable(${TARGET} MACOSX_BUNDLE ${SKIP} ${SOURCES})
    set_target_properties(${TARGET} PROPERTIES
      OUTPUT_NAME ioquakelive-launcher
      MACOSX_BUNDLE_BUNDLE_NAME "ioquakelive Launcher"
      MACOSX_BUNDLE_GUI_IDENTIFIER "org.ioquakelive.launcher"
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PRODUCT_VERSION}
      MACOSX_BUNDLE_BUNDLE_VERSION ${PRODUCT_VERSION})
    if(LAUNCHER_ICNS)
      target_sources(${TARGET} PRIVATE ${LAUNCHER_ICNS})
      set_source_files_properties(${LAUNCHER_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
      get_filename_component(ICNS_NAME ${LAUNCHER_ICNS} NAME)
      set_target_properties(${TARGET} PROPERTIES MACOSX_BUNDLE_ICON_FILE ${ICNS_NAME})
    endif()
    target_sources(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/OFL.txt)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/OFL.txt PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_link_options(${TARGET} PRIVATE -dead_strip)
  elseif(WIN32)
    if(SHOT)
      add_executable(${TARGET} ${SKIP} ${SOURCES})
    else()
      add_executable(${TARGET} WIN32 ${SOURCES} ${CODE}/sys/win_resource.rc)
      target_include_directories(${TARGET} PRIVATE ${ROOT}/misc/msvc)
      target_link_options(${TARGET} PRIVATE /MANIFEST:NO)
    endif()
    target_compile_definitions(${TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(${TARGET} PRIVATE /utf-8)
    target_link_libraries(${TARGET} PRIVATE winhttp shell32 dwmapi advapi32 ole32 oleaut32 uuid user32 gdi32 winmm imm32 version setupapi)
  else()
    add_executable(${TARGET} ${SKIP} ${SOURCES})
    target_compile_options(${TARGET} PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(${TARGET} PRIVATE -Wl,--gc-sections -s)
  endif()
  set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUT})
  if(SHOT)
    target_compile_definitions(${TARGET} PRIVATE LAUNCHER_SHOT)
  endif()
  target_compile_definitions(${TARGET} PRIVATE NO_GZIP PRODUCT_VERSION="${PRODUCT_VERSION}")
  target_include_directories(${TARGET} PRIVATE ${CODE}/zlib)
  target_link_libraries(${TARGET} PRIVATE SDL3::SDL3-static)
endfunction()

launcher_target(ioquakelive-launcher OFF)
launcher_target(ioquakelive-launcher-captura ON)
