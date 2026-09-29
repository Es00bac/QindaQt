# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Core Test)
foreach(behavior secure_buffer collection_store storage_failures storage_decoding)
    add_executable(tst_keyring_${behavior} tst_${behavior}.cpp)
    set_target_properties(tst_keyring_${behavior} PROPERTIES AUTOMOC ON)
    target_link_libraries(tst_keyring_${behavior} PRIVATE QindaQt::KeyringStorage Qt6::Core Qt6::Test)
    add_test(NAME keyring_${behavior} COMMAND tst_keyring_${behavior})
endforeach()

target_include_directories(tst_keyring_storage_decoding PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/services/keyring/src")

if(TARGET qindaqt_keyring_service)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    if(TARGET QindaQt::SessionSupervisorSupport)
    add_executable(qindaqt_keyring_session_owner_helper session_owner_helper.cpp)
    target_include_directories(qindaqt_keyring_session_owner_helper PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(qindaqt_keyring_session_owner_helper PRIVATE QindaQt::SessionSupervisorSupport Qt6::Core)
    qindaqt_enable_warnings(qindaqt_keyring_session_owner_helper)
    endif()
    add_test(NAME keyring_secret_service COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_secret_service.py" "$<TARGET_FILE:qindaqt-keyring>")
    if(NOT TARGET QindaQt::SessionSupervisorSupport)
        set_tests_properties(keyring_secret_service PROPERTIES ENVIRONMENT "QINDAQT_KEYRING_OWNER_HELPER=disabled")
    endif()
    set_tests_properties(keyring_secret_service PROPERTIES TIMEOUT 120 LABELS "keyring;dbus;private-fixture")
endif()

if(TARGET qindaqt-keyring-prompt)
    find_package(QindaTK CONFIG REQUIRED)
    add_executable(tst_keyring_prompt_qml tst_prompt_qml.cpp
        "${PROJECT_SOURCE_DIR}/src/services/keyring/prompt/prompt_controller.cpp"
        "${PROJECT_SOURCE_DIR}/src/services/keyring/prompt/prompt_controller.h")
    target_include_directories(tst_keyring_prompt_qml PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(tst_keyring_prompt_qml PRIVATE qindaqt_keyring_service
        QindaQt::AuthenticationOverlay QindaTK::qindatk Qt6::Test Qt6::Qml Qt6::Quick)
    target_compile_definitions(tst_keyring_prompt_qml PRIVATE
        QINDAQT_KEYRING_PROMPT_QML="${PROJECT_SOURCE_DIR}/src/services/keyring/prompt/Main.qml")
    qindaqt_enable_warnings(tst_keyring_prompt_qml)
    add_test(NAME keyring_prompt_qml COMMAND tst_keyring_prompt_qml)
    set_tests_properties(keyring_prompt_qml PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1")
endif()

add_subdirectory(pam)
