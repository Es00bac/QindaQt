add_library(qindaqt_bluetooth_protocol STATIC)

target_sources(qindaqt_bluetooth_protocol PRIVATE
    src/bluetooth_dbus.cpp
    src/bluetooth_validation.cpp
)

target_include_directories(qindaqt_bluetooth_protocol
    PUBLIC include
)

target_link_libraries(qindaqt_bluetooth_protocol
    PUBLIC Qt6::Core Qt6::DBus
)

# Enable strict compiler warnings
if(MSVC)
    target_compile_options(qindaqt_bluetooth_protocol PRIVATE /W4 /WX)
else()
    target_compile_options(qindaqt_bluetooth_protocol PRIVATE -Wall -Wextra -Werror)
endif()

# C++20 support
target_compile_features(qindaqt_bluetooth_protocol PRIVATE cxx_std_20)
