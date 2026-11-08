cmake_minimum_required(VERSION 3.24)
project(ModynToolbox VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Test)
enable_testing()
add_library(modyn_shell
  app/ui/workbench.cpp app/ui/workbench.h
  app/ui/theme/theme.cpp
  app/ui/components/icons.cpp
  app/ui/components/combo_box.cpp
  app/ui/components/tool_view.cpp
  app/ui/view_spec.cpp
  app/ui/window/chrome_window.cpp app/ui/window/chrome_window.h
  app/catalog/package_catalog.cpp
  app/platform/windows/window_chrome.cpp)
target_link_libraries(modyn_shell PUBLIC Qt6::Widgets)
target_include_directories(modyn_shell PUBLIC app)
add_executable(ModynToolboxApp WIN32 app/main.cpp)
target_link_libraries(ModynToolboxApp PRIVATE modyn_shell)
set_target_properties(ModynToolboxApp PROPERTIES OUTPUT_NAME ModynToolbox.App)
add_executable(modyn_shell_tests app/tests/unit/test_workbench.cpp)
target_link_libraries(modyn_shell_tests PRIVATE modyn_shell Qt6::Test)
add_test(NAME workbench COMMAND modyn_shell_tests -o -,txt -o workbench-results.txt,txt)
add_executable(modyn_worker app/tests/fixtures/worker.cpp)
target_link_libraries(modyn_worker PRIVATE Qt6::Core)
foreach(target ModynToolboxApp modyn_shell_tests modyn_worker)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endforeach()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME worker_transport COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/app/tests/integration/test_transport.py" "$<TARGET_FILE:modyn_worker>" "${CMAKE_SOURCE_DIR}")
if(WIN32)
  target_link_libraries(modyn_shell PRIVATE dwmapi)
endif()
add_executable(modyn_catalog_tests app/tests/unit/test_package_catalog.cpp app/catalog/package_catalog.cpp)
target_include_directories(modyn_catalog_tests PRIVATE app)
target_link_libraries(modyn_catalog_tests PRIVATE Qt6::Test)
set_target_properties(modyn_catalog_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME package_catalog COMMAND modyn_catalog_tests -o -,txt -o catalog-results.txt,txt)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests/tmp")
set_tests_properties(workbench package_catalog worker_transport PROPERTIES
  ENVIRONMENT "MODYN_SOURCE_ROOT=${CMAKE_SOURCE_DIR};QT_QPA_PLATFORM=offscreen;TEMP=${CMAKE_BINARY_DIR}/tests/tmp;TMP=${CMAKE_BINARY_DIR}/tests/tmp")
set_tests_properties(workbench package_catalog worker_transport PROPERTIES TIMEOUT 45)

add_executable(modyn_view_tests app/tests/unit/test_view_spec.cpp)
target_link_libraries(modyn_view_tests PRIVATE modyn_shell Qt6::Test)
set_target_properties(modyn_view_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME view_spec COMMAND modyn_view_tests -o -,txt -o view-results.txt,txt)
set_tests_properties(view_spec PROPERTIES TIMEOUT 45
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;TEMP=${CMAKE_BINARY_DIR}/tests/tmp;TMP=${CMAKE_BINARY_DIR}/tests/tmp")
if(WIN32)
  add_executable(modyn_native_tests app/tests/unit/test_window_native.cpp)
  target_link_libraries(modyn_native_tests PRIVATE modyn_shell Qt6::Test)
  set_target_properties(modyn_native_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  add_test(NAME window_native COMMAND modyn_native_tests -o -,txt -o native-results.txt,txt)
  set_tests_properties(window_native PROPERTIES TIMEOUT 45
    ENVIRONMENT "QT_QPA_PLATFORM=windows;TEMP=${CMAKE_BINARY_DIR}/tests/tmp;TMP=${CMAKE_BINARY_DIR}/tests/tmp")
endif()

add_test(NAME host_start COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/app/tests/integration/test_host_start.py" "$<TARGET_FILE:ModynToolboxApp>" "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/tests/tmp")
set_tests_properties(host_start PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
