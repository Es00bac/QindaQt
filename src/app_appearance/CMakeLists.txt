add_library(qindaqt_app_appearance STATIC
    src/application_appearance_controller.cpp
    src/appearance_resolver.cpp
)
add_library(QindaQt::AppAppearance ALIAS qindaqt_app_appearance)
target_include_directories(qindaqt_app_appearance PUBLIC include)
target_link_libraries(qindaqt_app_appearance
    PUBLIC Qt6::Core Qt6::Gui QindaQt::Themes QindaQt::SettingsClient
    PRIVATE QindaQt::DesignTokens
)
target_compile_features(qindaqt_app_appearance PUBLIC cxx_std_20)
