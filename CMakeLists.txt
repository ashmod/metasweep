cmake_minimum_required(VERSION 3.20)
project(metadata_tool LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ENABLE_BACKEND_EXIV2 "Enable image backend (exiv2)" ON)
option(ENABLE_BACKEND_POPPLER "Enable PDF backend (poppler)" ON)
option(ENABLE_BACKEND_TAGLIB "Enable audio backend (taglib)" ON)
option(ENABLE_BACKEND_MINIZIP "Enable ZIP backend (minizip)" ON)

include(cmake/FetchCLI11.cmake)

find_package(fmt CONFIG QUIET)
if(NOT fmt_FOUND)
  message(STATUS "Using bundled fmt via FetchContent")
  include(FetchContent)
  FetchContent_Declare(fmtlib GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG 11.0.2)
  FetchContent_MakeAvailable(fmtlib)
endif()

add_library(core
  src/core/detect.cpp
  src/core/policy.cpp
  src/core/report.cpp
  src/core/sanitize.cpp
  src/util/fs.cpp
  src/util/log.cpp
)

target_include_directories(core PUBLIC include src)
target_link_libraries(core PRIVATE fmt::fmt)

if(ENABLE_BACKEND_EXIV2)
  set(_exiv2_found FALSE)
  set(_exiv2_pkg "")

  find_package(Exiv2 CONFIG QUIET)
  if(Exiv2_FOUND)
    set(_exiv2_found TRUE)
    set(_exiv2_pkg "Exiv2")
  else()
    find_package(exiv2 CONFIG QUIET)
    if(exiv2_FOUND)
      set(_exiv2_found TRUE)
      set(_exiv2_pkg "exiv2")
    endif()
  endif()

  if(NOT _exiv2_found)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(EXIV2 QUIET IMPORTED_TARGET exiv2)
      if(EXIV2_FOUND)
        set(_exiv2_found TRUE)
        set(_exiv2_pkg "pkg-config(exiv2)")
      endif()
    endif()
  endif()

  if(_exiv2_found)
    message(STATUS "Exiv2 found via ${_exiv2_pkg} (Exiv2_DIR='${Exiv2_DIR}', exiv2_DIR='${exiv2_DIR}')")
    target_sources(core PRIVATE
      src/backends/image_exiv2.cpp
      src/backends/image_exiv2.hpp
    )
    target_compile_definitions(core PRIVATE HAVE_EXIV2=1)

    if(TARGET Exiv2::exiv2)
      message(STATUS "Linking with target: Exiv2::exiv2")
      target_link_libraries(core PRIVATE Exiv2::exiv2)
    elseif(TARGET exiv2lib)
      message(STATUS "Linking with target: exiv2lib")
      target_link_libraries(core PRIVATE exiv2lib)
    elseif(TARGET exiv2)
      message(STATUS "Linking with target: exiv2")
      target_link_libraries(core PRIVATE exiv2)
    elseif(TARGET PkgConfig::EXIV2)
      message(STATUS "Linking with target: PkgConfig::EXIV2")
      target_link_libraries(core PRIVATE PkgConfig::EXIV2)
    else()
      message(FATAL_ERROR "Exiv2 found but no known imported targets.")
    endif()
  else()
    message(STATUS "Exiv2 NOT found; image backend disabled. "
                   "Try: -DExiv2_DIR=/usr/lib/x86_64-linux-gnu/cmake/exiv2 "
                   "or -Dexiv2_DIR=/usr/lib/x86_64-linux-gnu/cmake/exiv2")
  endif()
endif()

if(ENABLE_BACKEND_TAGLIB)
  find_package(Taglib QUIET)
  if(Taglib_FOUND)
    target_sources(core PRIVATE src/backends/audio_taglib.cpp)
    target_compile_definitions(core PRIVATE HAVE_TAGLIB=1)
    target_link_libraries(core PRIVATE Taglib::Taglib)
  endif()
endif()

if(ENABLE_BACKEND_MINIZIP)
  target_sources(core PRIVATE src/backends/zip_minizip.cpp)
  target_compile_definitions(core PRIVATE HAVE_MINIZIP=1)
endif()

if(ENABLE_BACKEND_POPPLER)
  target_sources(core PRIVATE src/backends/pdf_info.cpp)
  target_compile_definitions(core PRIVATE HAVE_POPPLER=1)
endif()

# --- Executable ---
add_executable(metasweep src/main.cpp src/cli/commands.cpp)
target_link_libraries(metasweep PRIVATE core CLI11::CLI11 fmt::fmt)

install(TARGETS metasweep RUNTIME DESTINATION bin)
install(DIRECTORY policies/ DESTINATION share/metasweep/policies)

enable_testing()
add_subdirectory(tests)
