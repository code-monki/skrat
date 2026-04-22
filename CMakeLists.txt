cmake_minimum_required(VERSION 3.16)

project(skrat VERSION 0.2.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt6 ships as CMake config packages (Qt6Config.cmake). If neither
# CMAKE_PREFIX_PATH nor Qt6_DIR points at a kit, probe common installs so
# `cmake -S . -B build` works after e.g. `brew install qt` on macOS.
if((NOT CMAKE_PREFIX_PATH) AND ((NOT DEFINED Qt6_DIR) OR (Qt6_DIR STREQUAL "")))
  set(_skrat_qt6_prefixes "")
  if(DEFINED ENV{QTDIR} AND NOT "$ENV{QTDIR}" STREQUAL "")
    list(APPEND _skrat_qt6_prefixes "$ENV{QTDIR}")
  endif()
  if(DEFINED ENV{HOMEBREW_PREFIX} AND NOT "$ENV{HOMEBREW_PREFIX}" STREQUAL "")
    list(APPEND _skrat_qt6_prefixes
      "$ENV{HOMEBREW_PREFIX}/opt/qt@6"
      "$ENV{HOMEBREW_PREFIX}/opt/qt"
    )
  endif()
  if(APPLE)
    list(APPEND _skrat_qt6_prefixes
      "/opt/homebrew/opt/qt@6"
      "/opt/homebrew/opt/qt"
      "/usr/local/opt/qt@6"
      "/usr/local/opt/qt"
    )
  endif()
  foreach(_skrat_qt IN LISTS _skrat_qt6_prefixes)
    if(EXISTS "${_skrat_qt}/lib/cmake/Qt6/Qt6Config.cmake")
      list(PREPEND CMAKE_PREFIX_PATH "${_skrat_qt}")
      break()
    endif()
  endforeach()
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Pdf PdfWidgets)

add_executable(skrat
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.h
)

target_include_directories(skrat PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(skrat PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::PrintSupport
  Qt6::Pdf
  Qt6::PdfWidgets
)

target_compile_definitions(skrat PRIVATE "SKRAT_APP_VERSION=\"${PROJECT_VERSION}\"")

if(APPLE)
  set(_skrat_icns "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/skrat.icns")
  if(NOT EXISTS "${_skrat_icns}")
    message(FATAL_ERROR "Missing ${_skrat_icns} — run ./packaging/macos/build-icns.sh on macOS.")
  endif()
  set_target_properties(skrat PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER com.codemonki.skrat
    MACOSX_BUNDLE_BUNDLE_NAME "skrat"
    MACOSX_BUNDLE_COPYRIGHT "GPL-3.0-or-later"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE skrat
  )
  # CMake often leaves CFBundleIconFile empty / omits Resources; macdeployqt can strip them too.
  add_custom_command(TARGET skrat POST_BUILD
    COMMAND /bin/bash "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/install-bundle-icon.sh"
            "$<TARGET_FILE_DIR:skrat>/../.."
    VERBATIM
    COMMENT "Install skrat.icns and CFBundleIconFile into bundle"
  )
endif()

if(WIN32)
  set_target_properties(skrat PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(MSVC)
  target_compile_definitions(skrat PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
