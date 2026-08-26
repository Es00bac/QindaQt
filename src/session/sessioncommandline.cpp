// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessioncommandline.h"

#include "installpaths.h"

#include <QCommandLineOption>
#include <QCommandLineParser>

#include <type_traits>
#include <utility>

namespace QindaQt::Session {
namespace {

void configureParser(QCommandLineParser &parser)
{
    parser.setApplicationDescription(
        QStringLiteral("Launch the QindaQt KWin-derived Wayland compositor session."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("drm"), QStringLiteral("Run directly on DRM/KMS (default).")});
    parser.addOption({QStringLiteral("windowed"), QStringLiteral("Run nested on WAYLAND_DISPLAY.")});
    parser.addOption({QStringLiteral("virtual"), QStringLiteral("Use KWin's virtual framebuffer.")});
    parser.addOption({QStringLiteral("kwin"),
                      QStringLiteral("KWin executable to launch."),
                      QStringLiteral("path"),
                      QStringLiteral("kwin_wayland")});
    parser.addOption({QStringLiteral("socket"),
                      QStringLiteral("Child Wayland socket name."),
                      QStringLiteral("name"),
                      QStringLiteral("qindaqt-0")});
    parser.addOption({QStringLiteral("width"),
                      QStringLiteral("Virtual or windowed output width."),
                      QStringLiteral("pixels"),
                      QStringLiteral("1920")});
    parser.addOption({QStringLiteral("height"),
                      QStringLiteral("Virtual or windowed output height."),
                      QStringLiteral("pixels"),
                      QStringLiteral("1080")});
    parser.addOption({QStringLiteral("scale"),
                      QStringLiteral("Initial output scale."),
                      QStringLiteral("factor"),
                      QStringLiteral("1")});
    parser.addOption({QStringLiteral("output-count"),
                      QStringLiteral("Number of virtual or windowed outputs."),
                      QStringLiteral("count"),
                      QStringLiteral("1")});
    parser.addOption({QStringLiteral("no-xwayland"), QStringLiteral("Do not start rootless XWayland.")});
    parser.addOption({QStringLiteral("no-lockscreen"), QStringLiteral("Disable lock-screen support.")});
    parser.addOption({QStringLiteral("no-global-shortcuts"),
                      QStringLiteral("Disable compositor global shortcuts.")});
    parser.addOption({QStringLiteral("replace"), QStringLiteral("Replace an existing KWin instance.")});
    parser.addOption({QStringLiteral("test-scenario"),
                      QStringLiteral("Validated display scenario passed to QindaQt adapters."),
                      QStringLiteral("path")});
    parser.addOption({QStringLiteral("session"),
                      QStringLiteral("Session process whose exit stops the compositor."),
                      QStringLiteral("path")});
    parser.addOption({QStringLiteral("plugin-root"),
                      QStringLiteral("Qt plugin root containing kwin/plugins."),
                      QStringLiteral("path"),
                      InstallPaths::pluginRoot()});
}

template<typename Number>
bool parseNumber(const QString &text, Number *destination)
{
    bool valid = false;
    if constexpr (std::is_same_v<Number, int>) {
        *destination = text.toInt(&valid);
    } else {
        *destination = text.toDouble(&valid);
    }
    return valid;
}

std::optional<SessionOptions> fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return std::nullopt;
}

} // namespace

std::optional<SessionOptions> SessionCommandLine::parse(const QStringList &arguments,
                                                        QString *error)
{
    QCommandLineParser parser;
    configureParser(parser);
    if (!parser.parse(arguments)) {
        return fail(error, parser.errorText());
    }

    const int selectedBackends = static_cast<int>(parser.isSet(QStringLiteral("drm")))
        + static_cast<int>(parser.isSet(QStringLiteral("windowed")))
        + static_cast<int>(parser.isSet(QStringLiteral("virtual")));
    if (selectedBackends > 1) {
        return fail(error, QStringLiteral("choose only one of --drm, --windowed, or --virtual"));
    }

    SessionOptions options;
    if (parser.isSet(QStringLiteral("windowed"))) {
        options.backend = Backend::NestedWayland;
    } else if (parser.isSet(QStringLiteral("virtual"))) {
        options.backend = Backend::Virtual;
    }
    options.parentWaylandDisplay = QString::fromUtf8(qgetenv("WAYLAND_DISPLAY"));
    options.kwinExecutable = parser.value(QStringLiteral("kwin"));
    options.socketName = parser.value(QStringLiteral("socket"));
    options.xwayland = !parser.isSet(QStringLiteral("no-xwayland"));
    options.lockscreen = !parser.isSet(QStringLiteral("no-lockscreen"));
    options.globalShortcuts = !parser.isSet(QStringLiteral("no-global-shortcuts"));
    options.replace = parser.isSet(QStringLiteral("replace"));
    options.testScenario = parser.value(QStringLiteral("test-scenario"));
    options.sessionExecutable = parser.value(QStringLiteral("session"));
    options.pluginRoot = parser.value(QStringLiteral("plugin-root"));

    int width = 0;
    int height = 0;
    if (!parseNumber(parser.value(QStringLiteral("width")), &width)
        || !parseNumber(parser.value(QStringLiteral("height")), &height)
        || !parseNumber(parser.value(QStringLiteral("scale")), &options.scale)
        || !parseNumber(parser.value(QStringLiteral("output-count")), &options.outputCount)) {
        return fail(error, QStringLiteral("width, height, scale, and output-count must be numeric"));
    }
    options.outputSize = QSize(width, height);
    return options;
}

QString SessionCommandLine::helpText()
{
    QCommandLineParser parser;
    configureParser(parser);
    return parser.helpText();
}

} // namespace QindaQt::Session
