add_library(qindaqt_app_appearance STATIC
    include/qindaqt/app_appearance/application_appearance_controller.h
    include/qindaqt/app_appearance/appearance_resolver.h
    src/application_appearance_controller.cpp
    src/application_appearance_tokens.cpp
    src/appearance_resolver.cpp
)
add_library(QindaQt::AppAppearance ALIAS qindaqt_app_appearance)
target_include_directories(qindaqt_app_appearance PUBLIC include)
target_link_libraries(qindaqt_app_appearance
    PUBLIC Qt6::Core Qt6::Gui qindaqt_themes QindaQt::SettingsClient QindaQt::DesignTokens
    PRIVATE QindaQt::DesignTokens
)
target_compile_features(qindaqt_app_appearance PUBLIC cxx_std_20)
set_target_properties(qindaqt_app_appearance PROPERTIES POSITION_INDEPENDENT_CODE ON)
