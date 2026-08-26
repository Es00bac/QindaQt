// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "../common/catalogpaths.h"
#include "runtimepanelwindowfactory.h"

#include "qindaqt/profiles/profile_types.h"
#include "qindaqt/shell_layout/panel_layout_solver.h"
#include "qindaqt/shell_surface/layer_shell_surface_backend.h"
#include "qindaqt/shell_surface/panel_surface_controller.h"
#include "qindaqt/shell_surface/qt_output_inventory.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QTextStream>

namespace QindaQt::Shell {

ShellRuntimeApplication::ShellRuntimeApplication(QGuiApplication &application)
    : m_application(application)
{
    m_outputDebounce.setSingleShot(true);
    m_outputDebounce.setInterval(0);
    connect(&m_outputDebounce, &QTimer::timeout, this, [this] {
        QString error;
        if (!reconcileSurfaces(&error)) {
            qWarning().noquote() << "QindaQt shell kept its prior surface set:" << error;
        }
    });
}

ShellRuntimeApplication::~ShellRuntimeApplication() = default;

int ShellRuntimeApplication::run()
{
    const RuntimeOptionsResult parsed = parseRuntimeOptions(m_application);
    if (!parsed.options.has_value()) {
        qCritical().noquote() << parsed.error;
        return 2;
    }

    QString error;
    if (!loadCatalogs(*parsed.options, &error)) {
        qCritical().noquote() << error;
        return 2;
    }
    if (parsed.options->listOnly) {
        printCatalog();
        return 0;
    }
    if (!QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        qCritical().noquote()
            << "qindaqt-shell requires Qt's Wayland platform; use qindaqt-shell-preview for"
               " offscreen or X11 development";
        return 3;
    }
    if (!initializeRuntime(&error)) {
        qCritical().noquote() << error;
        return 4;
    }
    return m_application.exec();
}

bool ShellRuntimeApplication::loadCatalogs(const RuntimeOptions &options, QString *error)
{
    const QString profileDirectory = resolveCatalogDataDirectory(options.profileDirectory,
                                                                 "QINDAQT_PROFILE_DIR",
                                                                 QINDAQT_SOURCE_PROFILE_DIR,
                                                                 QStringLiteral("qindaqt/profiles"));
    const QString themeDirectory = resolveCatalogDataDirectory(options.themeDirectory,
                                                               "QINDAQT_THEME_DIR",
                                                               QINDAQT_SOURCE_THEME_DIR,
                                                               QStringLiteral("qindaqt/themes"));
    if (!m_profiles.loadDirectory(profileDirectory, error) ||
        !m_themes.loadDirectory(themeDirectory, error)) {
        return false;
    }
    if (!m_profiles.selectById(options.profileId)) {
        *error = QStringLiteral("Unknown profile: %1").arg(options.profileId);
        return false;
    }

    const QString requestedTheme = !options.themeId.isEmpty()
        ? options.themeId
        : m_profiles.current().value(QStringLiteral("defaultTheme")).toString();
    if (!m_themes.selectById(requestedTheme)) {
        *error = QStringLiteral("Unknown theme: %1").arg(requestedTheme);
        return false;
    }
    return true;
}

void ShellRuntimeApplication::printCatalog() const
{
    QTextStream output(stdout);
    output << "Profiles:\n";
    for (const auto &profile : m_profiles.profiles()) {
        output << "  " << profile.id << " - " << profile.name << '\n';
    }
    output << "Themes:\n";
    for (const auto &theme : m_themes.themes()) {
        output << "  " << theme.id << " - " << theme.name << '\n';
    }
}

bool ShellRuntimeApplication::initializeRuntime(QString *error)
{
    const int profileIndex = m_profiles.currentIndex();
    if (profileIndex < 0 ||
        static_cast<qsizetype>(profileIndex) >= m_profiles.profiles().size()) {
        *error = QStringLiteral("selected profile catalog state is invalid");
        return false;
    }

    const auto &profile = m_profiles.profiles().at(profileIndex);
    m_windowFactory =
        std::make_unique<RuntimePanelWindowFactory>(m_engine, profile, m_themes.current());
    m_backend =
        std::make_unique<ShellSurface::LayerShellSurfaceBackend>(*m_windowFactory);
    m_controller = std::make_unique<ShellSurface::PanelSurfaceController>(*m_backend);

    if (!reconcileSurfaces(error)) {
        m_controller.reset();
        m_backend.reset();
        m_windowFactory.reset();
        return false;
    }

    for (QScreen *screen : m_application.screens()) {
        attachOutputSignals(screen);
    }
    connect(&m_application, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
        attachOutputSignals(screen);
        scheduleOutputReconcile();
    });
    connect(&m_application, &QGuiApplication::screenRemoved, this,
            [this](QScreen *) { scheduleOutputReconcile(); });
    reportDeferredHideModes();
    return true;
}

bool ShellRuntimeApplication::reconcileSurfaces(QString *error)
{
    const auto inventory = ShellSurface::QtOutputInventory::read();
    if (!inventory.ok()) {
        *error = inventory.error;
        return false;
    }
    const int profileIndex = m_profiles.currentIndex();
    if (profileIndex < 0 ||
        static_cast<qsizetype>(profileIndex) >= m_profiles.profiles().size()) {
        *error = QStringLiteral("selected profile disappeared during surface reconciliation");
        return false;
    }

    const auto layout = ShellLayout::PanelLayoutSolver::solve(
        m_profiles.profiles().at(profileIndex).panels, inventory.outputs);
    if (!layout.ok()) {
        *error = layout.error.message;
        return false;
    }
    if (!m_controller) {
        *error = QStringLiteral("panel surface controller is not initialized");
        return false;
    }
    const auto result = m_controller->reconcile(layout);
    if (!result.ok()) {
        *error = result.message;
        return false;
    }
    return true;
}

void ShellRuntimeApplication::attachOutputSignals(QScreen *screen)
{
    if (screen == nullptr) {
        return;
    }
    const auto schedule = [this] { scheduleOutputReconcile(); };
    connect(screen, &QScreen::geometryChanged, this, schedule);
    connect(screen, &QScreen::physicalDotsPerInchChanged, this, schedule);
    connect(screen, &QScreen::logicalDotsPerInchChanged, this, schedule);
    connect(screen, &QScreen::orientationChanged, this, schedule);
}

void ShellRuntimeApplication::scheduleOutputReconcile()
{
    m_outputDebounce.start();
}

void ShellRuntimeApplication::reportDeferredHideModes() const
{
    const int profileIndex = m_profiles.currentIndex();
    if (profileIndex < 0 ||
        static_cast<qsizetype>(profileIndex) >= m_profiles.profiles().size()) {
        return;
    }
    for (const auto &panel : m_profiles.profiles().at(profileIndex).panels) {
        if (panel.hideMode != Profiles::HideMode::Never) {
            qWarning().noquote()
                << QStringLiteral("Panel '%1' requests hide mode '%2'; this first production"
                                  " surface slice keeps it visible until the live visibility"
                                  " adapter lands")
                       .arg(panel.id, Profiles::toString(panel.hideMode));
        }
    }
}

} // namespace QindaQt::Shell
