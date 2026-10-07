cmake_minimum_required(VERSION 3.24)
project(MediaInstalledConsumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
add_executable(media-installed-consumer main.cpp)
target_include_directories(media-installed-consumer PRIVATE "${MEDIA_STAGE}/include")
target_link_libraries(media-installed-consumer PRIVATE
    "${MEDIA_STAGE}/lib64/libqindaqt_removable_media_client.a"
    "${MEDIA_STAGE}/lib64/libqindaqt_removable_media_protocol.a" Qt6::Core Qt6::DBus)
target_compile_options(media-installed-consumer PRIVATE -Wall -Wextra -Wpedantic -Werror)
enable_testing()
add_test(NAME staged.media-client-no-passive-launch COMMAND media-installed-consumer)
set_tests_properties(staged.media-client-no-passive-launch PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent" TIMEOUT 8)
