cmake_minimum_required(VERSION 3.25)
project(AudioObserver LANGUAGES CXX)
get_filename_component(AUDIO_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(QINDAQT_ENABLE_STRICT_WARNINGS ON)
find_package(Qt6 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WP REQUIRED IMPORTED_TARGET wireplumber-0.5)
pkg_check_modules(PW REQUIRED IMPORTED_TARGET libpipewire-0.3)
include(${AUDIO_SOURCE_ROOT}/cmake/QindaQtCompiler.cmake)
add_subdirectory(${AUDIO_SOURCE_ROOT}/src/services/audio_protocol audio_protocol)
add_executable(audio_graph_observer
 ${AUDIO_SOURCE_ROOT}/tests/services/audio_service/audio_graph_observer.cpp
 ${AUDIO_SOURCE_ROOT}/src/services/audio_service/src/wireplumber_graph.cpp
 ${AUDIO_SOURCE_ROOT}/src/services/audio_service/src/wireplumber_latency.cpp
 ${AUDIO_SOURCE_ROOT}/src/services/audio_service/src/latency_policy.cpp)
target_include_directories(audio_graph_observer PRIVATE ${AUDIO_SOURCE_ROOT}/src/services/audio_service/src)
target_compile_definitions(audio_graph_observer PRIVATE QT_NO_KEYWORDS)
target_link_libraries(audio_graph_observer PRIVATE QindaQt::AudioProtocol PkgConfig::WP PkgConfig::PW)
qindaqt_enable_warnings(audio_graph_observer)
