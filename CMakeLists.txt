cmake_minimum_required(VERSION 3.20)
project(metadata_tool LANGUAGES CXX VERSION 1.0.2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ENABLE_BACKEND_EXIV2 "Enable image backend (exiv2)" ON)
option(ENABLE_BACKEND_POPPLER "Enable PDF backend (poppler)" ON)
option(ENABLE_BACKEND_TAGLIB "Enable audio backend (taglib)" ON)
option(ENABLE_BACKEND_MINIZIP "Enable ZIP backend (minizip)" ON)

# Portable build option: link static C++ runtime where possible for easier distribution

# Ensure CLI11 is available (prefer local cmake helper, fallback to FetchContent)
set(_CLI11_HELPER "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchCLI11.cmake")
if(EXISTS "${_CLI11_HELPER}")
  include("${_CLI11_HELPER}")
else()
  include(FetchContent)
  FetchContent_Declare(CLI11 GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git GIT_TAG v2.4.2)
  FetchContent_MakeAvailable(CLI11)
endif()

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
      target_link_libraries(core PUBLIC Exiv2::exiv2)
    elseif(TARGET exiv2lib)
      message(STATUS "Linking with target: exiv2lib")
      target_link_libraries(core PUBLIC exiv2lib)
    elseif(TARGET exiv2)
      message(STATUS "Linking with target: exiv2")
      target_link_libraries(core PUBLIC exiv2)
    elseif(TARGET PkgConfig::EXIV2)
      message(STATUS "Linking with target: PkgConfig::EXIV2")
      target_link_libraries(core PUBLIC PkgConfig::EXIV2)
    else()
      message(FATAL_ERROR "Exiv2 found but no known imported targets.")
    endif()
  else()
    message(STATUS "Exiv2 NOT found; image backend disabled. "
                   "Try: -DExiv2_DIR=/usr/lib/x86_64-linux-gnu/cmake/exiv2 "
                   "or -Dexiv2_DIR=/usr/lib/x86_64-linux-gnu/cmake/exiv2")
  endif()
endif()

# --- TagLib detection ---
if(ENABLE_BACKEND_TAGLIB)
  # Try CMake package first
  set(_TAG_TARGET "")
  find_package(Taglib QUIET)

  if(Taglib_FOUND)
    if(TARGET Taglib::Taglib)
      set(_TAG_TARGET Taglib::Taglib)
    else()
      message(STATUS "TagLib found (variables only); creating Taglib::Taglib interface target")
      add_library(Taglib::Taglib INTERFACE IMPORTED)
      if(DEFINED Taglib_INCLUDE_DIRS)
        target_include_directories(Taglib::Taglib INTERFACE ${Taglib_INCLUDE_DIRS})
      elseif(DEFINED TAGLIB_INCLUDE_DIRS)
        target_include_directories(Taglib::Taglib INTERFACE ${TAGLIB_INCLUDE_DIRS})
      endif()
      if(DEFINED Taglib_LIBRARIES)
        target_link_libraries(Taglib::Taglib INTERFACE ${Taglib_LIBRARIES})
      elseif(DEFINED TAGLIB_LIBRARIES)
        target_link_libraries(Taglib::Taglib INTERFACE ${TAGLIB_LIBRARIES})
      endif()
      if(DEFINED Taglib_LIBRARY_DIRS)
        target_link_directories(Taglib::Taglib INTERFACE ${Taglib_LIBRARY_DIRS})
      elseif(DEFINED TAGLIB_LIBRARY_DIRS)
        target_link_directories(Taglib::Taglib INTERFACE ${TAGLIB_LIBRARY_DIRS})
      endif()
      set(_TAG_TARGET Taglib::Taglib)
    endif()
  endif()

  # Also consult pkg-config for robust link flags (adds -L paths and extra libs)
  if(NOT _TAG_TARGET)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(TAGLIB QUIET IMPORTED_TARGET taglib)
      if(TARGET PkgConfig::TAGLIB)
        set(Taglib_FOUND TRUE)
        set(_TAG_TARGET PkgConfig::TAGLIB)
        message(STATUS "TagLib found via pkg-config imported target")
      elseif(TAGLIB_FOUND)
        if(NOT TARGET Taglib::Taglib)
          add_library(Taglib::Taglib INTERFACE IMPORTED)
        endif()
        if(TAGLIB_INCLUDE_DIRS)
          target_include_directories(Taglib::Taglib INTERFACE ${TAGLIB_INCLUDE_DIRS})
        endif()
        if(TAGLIB_LIBRARIES)
          target_link_libraries(Taglib::Taglib INTERFACE ${TAGLIB_LIBRARIES})
        endif()
        if(TAGLIB_LIBRARY_DIRS)
          target_link_directories(Taglib::Taglib INTERFACE ${TAGLIB_LIBRARY_DIRS})
        endif()
        set(Taglib_FOUND TRUE)
        set(_TAG_TARGET Taglib::Taglib)
        message(STATUS "TagLib found via pkg-config (manual interface)")
      endif()
    endif()
  endif()

  # On Apple, ensure absolute lib path or link directories are applied if we only have '-ltag'
  if(APPLE)
    # Prefer pkg-config imported target if available
    if(TARGET PkgConfig::TAGLIB)
      set(_TAG_TARGET PkgConfig::TAGLIB)
    elseif(TARGET Taglib::Taglib)
      # If Taglib::Taglib only links '-ltag', add common Homebrew/MacPorts dirs
      # and try to resolve absolute library path.
      find_library(TAGLIB_ABS NAMES tag taglib
        HINTS ${Taglib_LIBRARY_DIRS} ${TAGLIB_LIBRARY_DIRS}
              /opt/homebrew/lib /usr/local/lib /opt/local/lib)
      if(TAGLIB_ABS)
        # Replace any bare 'tag' entry by absolute library path
        target_link_libraries(Taglib::Taglib INTERFACE ${TAGLIB_ABS})
      endif()
      target_link_directories(Taglib::Taglib INTERFACE
        /opt/homebrew/lib /usr/local/lib /opt/local/lib)
    endif()
  endif()
endif()

if(ENABLE_BACKEND_TAGLIB AND Taglib_FOUND)
  message(STATUS "Enabling audio backend (TagLib)")
  target_sources(core PRIVATE src/backends/audio_taglib.cpp)
  target_compile_definitions(core PRIVATE HAVE_TAGLIB=1)
  if(TARGET PkgConfig::TAGLIB)
    target_link_libraries(core PUBLIC PkgConfig::TAGLIB)
  elseif(TARGET Taglib::Taglib)
    target_link_libraries(core PUBLIC Taglib::Taglib)
  else()
    message(FATAL_ERROR "TagLib requested but no link target available")
  endif()
endif()


if(ENABLE_BACKEND_MINIZIP)
  target_sources(core PRIVATE src/backends/zip_minizip.cpp src/backends/zip_minizip.hpp)
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

# Bundle third-party shared libraries for end users on macOS/Windows
if(APPLE OR WIN32)
  # Use CMake BundleUtilities to copy dependent shared libs next to the executable
  install(CODE [[
    include(BundleUtilities)
    # Ensure absolute install prefix when --prefix is relative
    get_filename_component(_prefix_abs "${CMAKE_INSTALL_PREFIX}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
    set(_exe "${_prefix_abs}/bin/metasweep")
    set(_search "")
    if(APPLE)
      # Common Homebrew/MacPorts locations
      set(_search "/opt/homebrew/lib;/usr/local/lib;/opt/local/lib")
    elseif(WIN32)
      if(DEFINED ENV{VCPKG_ROOT})
        set(_search "$ENV{VCPKG_ROOT}/installed/x64-windows/bin;$ENV{VCPKG_ROOT}/installed/x64-windows/lib")
      endif()
    endif()
    message(STATUS "Fixing up bundle: ${_exe}")
    fixup_bundle("${_exe}" "" "${_search}")
  ]])
endif()

enable_testing()
add_subdirectory(tests)

# --- Packaging (CPack) ---
include(GNUInstallDirs)
set(CPACK_PACKAGE_NAME "metasweep")
set(CPACK_PACKAGE_VENDOR "Metasweep")
set(CPACK_PACKAGE_CONTACT "metasweep")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME "metasweep-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "TGZ")
if(WIN32)
  list(APPEND CPACK_GENERATOR "ZIP")
endif()
include(CPack)
