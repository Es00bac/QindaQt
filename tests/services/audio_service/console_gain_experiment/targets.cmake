# SPDX-License-Identifier: GPL-3.0-or-later
# Opt-in diagnostic cohort, separate from production Worker and SDK gates.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GainWP REQUIRED IMPORTED_TARGET wireplumber-0.5>=0.5.15)
pkg_check_modules(GainPW REQUIRED IMPORTED_TARGET libpipewire-0.3>=1.6.8)
set(QINDAQT_GAIN_COHORT_RECEIPT "" CACHE FILEPATH "Exact retained Portage image receipt")
set(QINDAQT_GAIN_COHORT_RECEIPT_SHA256 "" CACHE STRING "Reviewed cohort receipt digest")
option(QINDAQT_GAIN_ALLOW_UNPATCHED_ASAN_CONTROL "Only the immutable private ASan red control" OFF)
option(QINDAQT_GAIN_ALLOW_FAILED_UPSTREAM_OLD_ASAN "Explicit failed-suite old-only diagnostic image" OFF)
if(NOT EXISTS "${QINDAQT_GAIN_COHORT_RECEIPT}"
   OR NOT QINDAQT_GAIN_COHORT_RECEIPT_SHA256 MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "The private gain experiment requires a pinned image receipt")
endif()
file(READ "${QINDAQT_GAIN_COHORT_RECEIPT}" gain_receipt LIMIT 65537)
string(LENGTH "${gain_receipt}" gain_receipt_length)
string(SHA256 gain_receipt_digest "${gain_receipt}")
if(gain_receipt_length GREATER 65536
   OR NOT gain_receipt_digest STREQUAL QINDAQT_GAIN_COHORT_RECEIPT_SHA256)
    message(FATAL_ERROR "Private gain image receipt changed")
endif()
string(JSON gain_mode GET "${gain_receipt}" mode)
if(gain_mode STREQUAL "unpatched-asan-control" AND NOT QINDAQT_GAIN_ALLOW_UNPATCHED_ASAN_CONTROL)
    message(FATAL_ERROR "Unpatched borrowed-Core module is red-control-only")
endif()
if(QINDAQT_GAIN_ALLOW_FAILED_UPSTREAM_OLD_ASAN
   AND (NOT gain_mode STREQUAL "unpatched-asan-control"
        OR NOT QINDAQT_GAIN_ALLOW_UNPATCHED_ASAN_CONTROL))
    message(FATAL_ERROR "Failed-upstream image admission is explicit old-ASan-only")
endif()
foreach(pair IN ITEMS "PIPEWIRE;daemon" "WIREPLUMBER;policy"
                      "PIPEWIRE_CONFIG;serverConfig" "PIPEWIRE_CLIENT_CONFIG;clientConfig"
                      "WIREPLUMBER_CONFIG;policyConfig" "CORE_IMAGE;core")
    list(GET pair 0 variable)
    list(GET pair 1 artifact)
    string(JSON QINDAQT_GAIN_${variable} GET "${gain_receipt}" artifacts ${artifact} path)
    if(NOT EXISTS "${QINDAQT_GAIN_${variable}}")
        message(FATAL_ERROR "Missing private cohort artifact: ${artifact}")
    endif()
endforeach()
set(gain_owner "${QINDAQT_AUDIO_GAIN_SOURCE_ROOT}/src/services/audio_service/src")
set(gain_tests "${QINDAQT_AUDIO_GAIN_SOURCE_ROOT}/tests/services/audio_service")

qt_add_executable(qindaqt_audio_gain_seed_stream_state ${gain_tests}/gain_seed_stream_state.cpp)
target_link_libraries(qindaqt_audio_gain_seed_stream_state PRIVATE Qt6::Core PkgConfig::GainWP)
qindaqt_enable_warnings(qindaqt_audio_gain_seed_stream_state)

qt_add_library(qindaqt_audio_gain_experiment_support STATIC
    ${gain_tests}/support/gain_private_runtime.cpp
    ${gain_tests}/support/gain_native_graph.cpp
    ${gain_tests}/support/gain_loaded_image.cpp
    ${gain_tests}/support/gain_qualified_image.cpp
    ${gain_tests}/support/gain_qualification_evidence.cpp
    ${gain_tests}/support/gain_qualification_receipt.cpp
    ${gain_tests}/support/gain_upstream_suite.cpp
    ${gain_tests}/support/gain_pcm_stream.cpp
    ${gain_owner}/console_routing_gain.cpp
    ${gain_owner}/console_send_admission.cpp
    ${gain_owner}/wireplumber_context_core.cpp
    ${gain_owner}/wireplumber_send_nodes.cpp
    ${gain_owner}/wireplumber_graph.cpp
    ${gain_owner}/wireplumber_graph_lookup.cpp
    ${gain_owner}/wireplumber_volume.cpp
    ${gain_owner}/wireplumber_latency.cpp
    ${gain_owner}/latency_policy.cpp
    ${gain_owner}/wireplumber_routing.cpp)
target_include_directories(qindaqt_audio_gain_experiment_support PUBLIC ${gain_owner} ${gain_tests})
target_link_libraries(qindaqt_audio_gain_experiment_support PUBLIC
    QindaQt::AudioProtocol Qt6::Test "${QINDAQT_GAIN_CORE_IMAGE}"
    PkgConfig::GainWP PkgConfig::GainPW "${CMAKE_DL_LIBS}")
target_compile_definitions(qindaqt_audio_gain_experiment_support PRIVATE QT_NO_KEYWORDS
    QINDAQT_GAIN_COHORT_RECEIPT="${QINDAQT_GAIN_COHORT_RECEIPT}"
    QINDAQT_GAIN_COHORT_RECEIPT_SHA256="${QINDAQT_GAIN_COHORT_RECEIPT_SHA256}"
    QINDAQT_GAIN_ALLOW_UNPATCHED_ASAN_CONTROL=$<BOOL:${QINDAQT_GAIN_ALLOW_UNPATCHED_ASAN_CONTROL}>
    QINDAQT_GAIN_ALLOW_FAILED_UPSTREAM_OLD_ASAN=$<BOOL:${QINDAQT_GAIN_ALLOW_FAILED_UPSTREAM_OLD_ASAN}>
    QINDAQT_PIPEWIRE_EXECUTABLE="${QINDAQT_GAIN_PIPEWIRE}"
    QINDAQT_WIREPLUMBER_EXECUTABLE="${QINDAQT_GAIN_WIREPLUMBER}"
    QINDAQT_PIPEWIRE_TEST_CONFIG="${QINDAQT_GAIN_PIPEWIRE_CONFIG}"
    QINDAQT_PIPEWIRE_CLIENT_TEST_CONFIG="${QINDAQT_GAIN_PIPEWIRE_CLIENT_CONFIG}"
    QINDAQT_WIREPLUMBER_TEST_CONFIG="${QINDAQT_GAIN_WIREPLUMBER_CONFIG}"
    QINDAQT_GAIN_STATE_SEED_EXECUTABLE="$<TARGET_FILE:qindaqt_audio_gain_seed_stream_state>")
add_dependencies(qindaqt_audio_gain_experiment_support qindaqt_audio_gain_seed_stream_state)
qindaqt_enable_warnings(qindaqt_audio_gain_experiment_support)

foreach(row IN ITEMS routing_gain send_admission private_environment image_preflight qualification_receipt
                     core_scope_runtime loopback_gain_runtime borrowed_core_lifetime)
    qt_add_executable(qindaqt_audio_gain_experiment_${row}
        ${gain_tests}/tst_console_${row}.cpp)
    target_link_libraries(qindaqt_audio_gain_experiment_${row} PRIVATE
        qindaqt_audio_gain_experiment_support)
    target_compile_definitions(qindaqt_audio_gain_experiment_${row} PRIVATE QT_NO_KEYWORDS)
    qindaqt_enable_warnings(qindaqt_audio_gain_experiment_${row})
    # The private receipt poison method is explicitly selected by the recipe.
    if(row STREQUAL "image_preflight")
        set(gain_function actualPinnedImageIsAdmitted)
    else()
        set(gain_function "")
    endif()
    add_test(NAME qindaqt.audio-gain-experiment-${row}
        COMMAND qindaqt_audio_gain_experiment_${row} ${gain_function})
    set_tests_properties(qindaqt.audio-gain-experiment-${row} PROPERTIES
        LABELS "audio;private-gain-experiment" TIMEOUT 240 RUN_SERIAL TRUE)
endforeach()
# Fault-only allocator interposition is linked/exported into this diagnostic
# executable alone. The loaded old/patched upstream module bytes are unchanged.
target_sources(qindaqt_audio_gain_experiment_borrowed_core_lifetime PRIVATE
    ${gain_tests}/support/gain_stream_failure.cpp)
target_link_options(qindaqt_audio_gain_experiment_borrowed_core_lifetime PRIVATE
    "-Wl,--export-dynamic")
