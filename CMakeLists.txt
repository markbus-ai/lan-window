cmake_minimum_required(VERSION 3.21)
project(lan-window VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "Compila fuera del árbol de fuentes, p. ej. cmake -S . -B ../build")
endif()

if(WIN32)
  set(_sender_default OFF)
else()
  set(_sender_default ON)
endif()
option(LANWINDOW_BUILD_SENDER "Compilar el emisor (solo Linux: PipeWire + portal)" ${_sender_default})
option(LANWINDOW_BUILD_TESTS "Compilar los tests" ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

# Núcleo común: configuración, pipelines GStreamer y widgets del receptor.
add_library(lanwindow_core STATIC
  src/common/config.cpp
  src/common/config.h
  src/common/gst_util.cpp
  src/common/gst_util.h
  src/common/receiver_pipeline.cpp
  src/common/receiver_pipeline.h
  src/common/sender_pipeline.cpp
  src/common/sender_pipeline.h
  src/receiver/receiver_window.cpp
  src/receiver/receiver_window.h
  src/receiver/video_widget.cpp
  src/receiver/video_widget.h
)
target_include_directories(lanwindow_core PUBLIC src)
target_link_libraries(lanwindow_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets PkgConfig::GST)
if(MSVC)
  target_compile_options(lanwindow_core PRIVATE /W4)
else()
  target_compile_options(lanwindow_core PRIVATE -Wall -Wextra)
endif()

add_executable(lan-window-receiver WIN32 src/receiver/receiver_main.cpp)
target_link_libraries(lan-window-receiver PRIVATE lanwindow_core)
install(TARGETS lan-window-receiver RUNTIME DESTINATION bin)

if(LANWINDOW_BUILD_SENDER)
  pkg_check_modules(PORTAL REQUIRED IMPORTED_TARGET libportal gio-unix-2.0)
  add_library(lanwindow_sender_ui STATIC
    src/sender/portal_capture.cpp
    src/sender/portal_capture.h
    src/sender/sender_window.cpp
    src/sender/sender_window.h
  )
  target_link_libraries(lanwindow_sender_ui PUBLIC lanwindow_core PkgConfig::PORTAL)
  target_compile_options(lanwindow_sender_ui PRIVATE -Wall -Wextra)
  add_executable(lan-window-sender src/sender/sender_main.cpp)
  target_link_libraries(lan-window-sender PRIVATE lanwindow_sender_ui)
  install(TARGETS lan-window-sender RUNTIME DESTINATION bin)
endif()

if(LANWINDOW_BUILD_TESTS)
  enable_testing()
  find_package(Qt6 REQUIRED COMPONENTS Test)
  add_executable(loopback_test tests/loopback_test.cpp)
  target_link_libraries(loopback_test PRIVATE lanwindow_core Qt6::Test)
  add_test(NAME loopback_test COMMAND loopback_test)
  set_tests_properties(loopback_test PROPERTIES TIMEOUT 180)
  add_test(NAME receiver_self_check COMMAND lan-window-receiver --self-check)
  set_tests_properties(receiver_self_check PROPERTIES TIMEOUT 60
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  if(LANWINDOW_BUILD_SENDER)
    add_test(NAME sender_self_check COMMAND lan-window-sender --self-check)
    set_tests_properties(sender_self_check PROPERTIES TIMEOUT 60
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(ui_smoke_test tests/ui_smoke_test.cpp)
    target_link_libraries(ui_smoke_test PRIVATE lanwindow_sender_ui Qt6::Test)
    add_test(NAME ui_smoke_test COMMAND ui_smoke_test)
    set_tests_properties(ui_smoke_test PROPERTIES TIMEOUT 120
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()
endif()
