cmake_minimum_required(VERSION 3.24)
project(ModynToolbox VERSION 1.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Sql Concurrent Test Multimedia MultimediaWidgets)
enable_testing()
add_library(modyn_shell
  app/ui/components/video_surface.cpp app/ui/components/video_surface.h
  app/ui/components/video_editor.cpp app/ui/components/video_editor.h
  app/resources/assets.qrc
  app/ui/components/spin_box.cpp
  app/storage/state_repository.cpp app/storage/data_transfer.cpp
  app/core/files.cpp
  app/catalog/package_manager.cpp
  app/jobs/job_runner.cpp app/jobs/job_runner.h
  app/jobs/scheduler.cpp app/jobs/scheduler.h
  app/ui/workbench.cpp app/ui/workbench.h
  app/ui/theme/theme.cpp
  app/ui/components/icons.cpp
  app/ui/components/combo_box.cpp
  app/ui/components/tool_view.cpp
  app/ui/components/tool_panel.cpp app/ui/components/tool_panel.h
  app/ui/components/tool_navigation.cpp app/ui/components/tool_navigation.h
  app/ui/view_spec.cpp
  app/ui/window/chrome_window.cpp app/ui/window/chrome_window.h
  app/catalog/package_catalog.cpp
  app/platform/windows/window_chrome.cpp)
target_link_libraries(modyn_shell PUBLIC Qt6::Widgets Qt6::Sql Qt6::Concurrent Qt6::Multimedia Qt6::MultimediaWidgets)
target_include_directories(modyn_shell PUBLIC app)
target_compile_definitions(modyn_shell PUBLIC MODYN_VERSION="${PROJECT_VERSION}")
add_executable(ModynToolboxApp WIN32 app/main.cpp app/automation.cpp)
target_link_libraries(ModynToolboxApp PRIVATE modyn_shell)
target_sources(ModynToolboxApp PRIVATE app/resources/windows.rc)
target_include_directories(ModynToolboxApp PRIVATE app/resources)
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
add_executable(modyn_catalog_tests app/tests/unit/test_package_catalog.cpp)
target_include_directories(modyn_catalog_tests PRIVATE app)
target_link_libraries(modyn_catalog_tests PRIVATE modyn_shell Qt6::Test)
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

add_executable(ModynWorkerHost app/worker_host/main.cpp)
target_link_libraries(ModynWorkerHost PRIVATE Qt6::Core)
set_target_properties(ModynWorkerHost PROPERTIES OUTPUT_NAME ModynToolbox.WorkerHost RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(modyn_job_tests app/tests/integration/test_jobs.cpp)
target_link_libraries(modyn_job_tests PRIVATE modyn_shell Qt6::Test)
set_target_properties(modyn_job_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME jobs COMMAND modyn_job_tests -o -,txt -o jobs-results.txt,txt)
set_tests_properties(jobs PROPERTIES TIMEOUT 120 ENVIRONMENT "MODYN_SOURCE_ROOT=${CMAKE_SOURCE_DIR};TEMP=${CMAKE_BINARY_DIR}/tests/tmp;TMP=${CMAKE_BINARY_DIR}/tests/tmp")

add_executable(ModynLauncher WIN32 app/launcher.cpp app/resources/windows.rc)
target_include_directories(ModynLauncher PRIVATE app/resources)
set_target_properties(ModynLauncher PROPERTIES OUTPUT_NAME ModynToolbox RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(modyn_system_tests app/tests/integration/test_system.cpp)
target_link_libraries(modyn_system_tests PRIVATE modyn_shell Qt6::Test)
set_target_properties(modyn_system_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME full_system COMMAND modyn_system_tests -o -,txt -o system-results.txt,txt)
set_tests_properties(full_system PROPERTIES TIMEOUT 120 ENVIRONMENT "MODYN_SOURCE_ROOT=${CMAKE_SOURCE_DIR};QT_QPA_PLATFORM=offscreen;TEMP=${CMAKE_BINARY_DIR}/tests/tmp;TMP=${CMAKE_BINARY_DIR}/tests/tmp")
add_test(NAME business_cores COMMAND "${CMAKE_SOURCE_DIR}/.local/runtimes/pillow/python.exe" -B "${CMAKE_SOURCE_DIR}/tools/tests/verify_cores.py")
set_tests_properties(business_cores PROPERTIES TIMEOUT 120)

add_executable(modyn_fault_tests app/tests/integration/test_faults.cpp)
target_link_libraries(modyn_fault_tests PRIVATE modyn_shell Qt6::Test)
set_target_properties(modyn_fault_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME worker_faults COMMAND modyn_fault_tests -o -,txt -o fault-results.txt,txt)
set_tests_properties(worker_faults PROPERTIES TIMEOUT 120 ENVIRONMENT "MODYN_SOURCE_ROOT=${CMAKE_SOURCE_DIR};TEMP=${CMAKE_BINARY_DIR}/tests/tmp;TMP=${CMAKE_BINARY_DIR}/tests/tmp")

set_property(TARGET ModynLauncher PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(modyn_sdk_compile app/tests/fixtures/sdk_compile.cpp)
target_link_libraries(modyn_sdk_compile PRIVATE Qt6::Core)

add_test(NAME host_crash_tree COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/app/tests/integration/test_host_crash.py" "$<TARGET_FILE:ModynToolboxApp>" "${CMAKE_SOURCE_DIR}")
set_tests_properties(host_crash_tree PROPERTIES TIMEOUT 60)

# Real media tests use only private generated fixtures and the independent tool build.
add_test(NAME video_core COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/video-clip/tests/verify_video.py")
set_tests_properties(video_core PROPERTIES TIMEOUT 120)
add_executable(modyn_video_tests app/tests/unit/test_video_editor.cpp)
target_link_libraries(modyn_video_tests PRIVATE modyn_shell Qt6::Test)
set_target_properties(modyn_video_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME video_editor COMMAND modyn_video_tests -o -,txt -o video-editor-results.txt,txt)
set_tests_properties(video_editor PROPERTIES TIMEOUT 90 DEPENDS video_core
 ENVIRONMENT "MODYN_SOURCE_ROOT=${CMAKE_SOURCE_DIR};QT_QPA_PLATFORM=offscreen;TEMP=${CMAKE_BINARY_DIR}/tests/tmp;TMP=${CMAKE_BINARY_DIR}/tests/tmp")

add_test(NAME video_host COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/video-clip/tests/verify_host.py" --binary "$<TARGET_FILE:ModynToolboxApp>")
set_tests_properties(video_host PROPERTIES TIMEOUT 180 DEPENDS video_core)
