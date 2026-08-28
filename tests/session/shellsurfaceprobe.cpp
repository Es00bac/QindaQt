// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellsurfaceprotocoltrace.h"

#include <QBackingStore>
#include <QElapsedTimer>
#include <QExposeEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegion>
#include <QResizeEvent>
#include <QScreen>
#include <QTextStream>
#include <QTimer>
#include <QWindow>

#include <algorithm>

namespace {

constexpr int expectedSurfaceCount = 2;
constexpr int expectedTopZone = 30;
constexpr int expectedBottomZone = 54;
constexpr int expectedReservation = expectedTopZone + expectedBottomZone;
constexpr int stableSampleCount = 3;
constexpr int globalTimeoutMilliseconds = 15'000;
constexpr int shellStopGraceMilliseconds = 1'000;
constexpr int compositorUnmapSettleMilliseconds = 250;
constexpr qsizetype maximumDiagnosticBytes = 8 * 1024;

QJsonObject rectangleJson(const QRect &rectangle)
{
    return {
        {QStringLiteral("x"), rectangle.x()},
        {QStringLiteral("y"), rectangle.y()},
        {QStringLiteral("width"), rectangle.width()},
        {QStringLiteral("height"), rectangle.height()},
    };
}

class PaintedMaximizedWindow final : public QWindow {
public:
    PaintedMaximizedWindow()
        : m_store(this)
        , m_color(QStringLiteral("#31506b"))
    {
        setTitle(QStringLiteral("QindaQt shell-surface work-area probe"));
        setFlags(Qt::Window | Qt::FramelessWindowHint);
        resize(QSize(640, 480));
    }

protected:
    void exposeEvent(QExposeEvent *event) override
    {
        QWindow::exposeEvent(event);
        paint();
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWindow::resizeEvent(event);
        paint();
    }

private:
    void paint()
    {
        if (!isExposed() || size().isEmpty()) {
            return;
        }
        m_store.resize(size());
        const QRegion region(QRect(QPoint{}, size()));
        m_store.beginPaint(region);
        QPainter painter(m_store.paintDevice());
        painter.fillRect(region.boundingRect(), m_color);
        painter.end();
        m_store.endPaint();
        m_store.flush(region);
    }

    QBackingStore m_store;
    QColor m_color;
};

class ShellSurfaceProbe final : public QObject {
public:
    explicit ShellSurfaceProbe(QGuiApplication &application)
        : m_application(application)
    {
        m_tick.setInterval(50);
        connect(&m_tick, &QTimer::timeout, this, [this] { advance(); });
        connect(&m_shell, &QProcess::readyReadStandardError, this,
                [this] { drainShellError(); });
        connect(&m_shell, &QProcess::readyReadStandardOutput, this,
                [this] { appendDiagnostic(m_shell.readAllStandardOutput()); });
    }

    void start()
    {
        if (QGuiApplication::platformName() != QStringLiteral("wayland")) {
            fail(QStringLiteral("probe requires Qt's Wayland platform"));
            return;
        }
        const auto screens = m_application.screens();
        if (screens.size() != 1 || screens.constFirst() == nullptr) {
            fail(QStringLiteral("probe requires exactly one live output"));
            return;
        }
        m_screen = screens.constFirst();
        m_outputGeometry = m_screen->geometry();
        if (m_outputGeometry.width() <= 0 ||
            m_outputGeometry.height() <= expectedReservation) {
            fail(QStringLiteral("output geometry cannot contain the proof profile"));
            return;
        }
        m_expectedReservedSize = QSize(m_outputGeometry.width(),
                                       m_outputGeometry.height() - expectedReservation);
        m_window.setScreen(m_screen);
        m_window.showMaximized();
        m_globalElapsed.start();
        m_stageElapsed.start();
        m_tick.start();
    }

private:
    enum class Stage {
        Baseline,
        LayerMapping,
        NormalWithShell,
        ReservedMaximize,
        ShellExit,
        UnmapSettlement,
        NormalWithoutShell,
        RestoredMaximize,
        Finished,
    };

    void advance()
    {
        if (m_stage == Stage::Finished) {
            return;
        }
        drainShellError();
        if (m_globalElapsed.elapsed() > globalTimeoutMilliseconds) {
            fail(QStringLiteral("timed out during stage %1").arg(static_cast<int>(m_stage)));
            return;
        }

        switch (m_stage) {
        case Stage::Baseline:
            if (stableAt(m_outputGeometry.size())) {
                m_baselineGeometry = m_window.geometry();
                startShell();
            }
            break;
        case Stage::LayerMapping:
            if (m_shell.state() == QProcess::NotRunning) {
                fail(QStringLiteral("qindaqt-shell exited before mapping both panels"));
                return;
            }
            if (protocolReady()) {
                m_window.showNormal();
                enter(Stage::NormalWithShell);
            }
            break;
        case Stage::NormalWithShell:
            if (!m_window.windowStates().testFlag(Qt::WindowMaximized)) {
                m_window.showMaximized();
                enter(Stage::ReservedMaximize);
            }
            break;
        case Stage::ReservedMaximize:
            if (stableAt(m_expectedReservedSize)) {
                m_reservedGeometry = m_window.geometry();
                // AGENT-CONTRACT: This immutable snapshot is taken while the
                // reduced work area is live and before shell teardown can
                // destroy roles. Python validates this active mapped epoch;
                // the final trace remains separate lifecycle diagnostics.
                m_activeMappedProtocol = m_protocol.evidence().toJson();
                m_activeMappedSnapshotTaken = true;
                m_shell.terminate();
                enter(Stage::ShellExit);
            }
            break;
        case Stage::ShellExit:
            if (m_shell.state() == QProcess::NotRunning) {
                m_protocol.finish();
                enter(Stage::UnmapSettlement);
            } else if (m_stageElapsed.elapsed() > shellStopGraceMilliseconds) {
                m_shell.kill();
            }
            break;
        case Stage::UnmapSettlement:
            if (m_stageElapsed.elapsed() >= compositorUnmapSettleMilliseconds) {
                m_window.showNormal();
                enter(Stage::NormalWithoutShell);
            }
            break;
        case Stage::NormalWithoutShell:
            if (!m_window.windowStates().testFlag(Qt::WindowMaximized)) {
                m_window.showMaximized();
                enter(Stage::RestoredMaximize);
            }
            break;
        case Stage::RestoredMaximize:
            if (stableAt(m_outputGeometry.size())) {
                m_restoredGeometry = m_window.geometry();
                succeed();
            }
            break;
        case Stage::Finished:
            break;
        }
    }

    bool stableAt(const QSize &expected)
    {
        const bool matching = m_window.isExposed() &&
            m_window.windowStates().testFlag(Qt::WindowMaximized) &&
            m_window.size() == expected;
        m_stableSamples = matching ? m_stableSamples + 1 : 0;
        return m_stableSamples >= stableSampleCount;
    }

    bool protocolReady() const
    {
        return m_protocol.evidence().provesMappedSurfaces(expectedSurfaceCount);
    }

    void startShell()
    {
        const QString executable = qEnvironmentVariable("QINDAQT_SHELL_EXECUTABLE");
        const QString profileDirectory = qEnvironmentVariable("QINDAQT_SHELL_PROFILE_DIR");
        const QString profileId = qEnvironmentVariable("QINDAQT_SHELL_PROFILE_ID").trimmed();
        const QString themeDirectory = qEnvironmentVariable("QINDAQT_SHELL_THEME_DIR");
        if (!QFileInfo(executable).isExecutable() || !QFileInfo(profileDirectory).isDir() ||
            profileId.isEmpty() || !QFileInfo(themeDirectory).isDir()) {
            fail(QStringLiteral("shell executable, profile, or catalog directory is unavailable"));
            return;
        }

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        // AGENT-CONTRACT: This direct protocol trace is test-only evidence of
        // the zwlr role and configure handshake. Production never enables or
        // parses WAYLAND_DEBUG, and the raw transcript is not emitted.
        environment.insert(QStringLiteral("WAYLAND_DEBUG"), QStringLiteral("client"));
        m_shell.setProcessEnvironment(environment);
        m_shell.setProgram(executable);
        m_shell.setArguments({
            QStringLiteral("--profile"), profileId,
            QStringLiteral("--theme"), QStringLiteral("qinda-dark"),
            QStringLiteral("--profile-dir"), profileDirectory,
            QStringLiteral("--theme-dir"), themeDirectory,
        });
        m_shell.setProcessChannelMode(QProcess::SeparateChannels);
        m_shell.start();
        if (!m_shell.waitForStarted(3'000)) {
            fail(QStringLiteral("could not start qindaqt-shell: %1").arg(m_shell.errorString()));
            return;
        }
        m_shellStarted = true;
        enter(Stage::LayerMapping);
    }

    void drainShellError()
    {
        if (!m_shell.isOpen()) {
            return;
        }
        const QByteArray chunk = m_shell.readAllStandardError();
        if (chunk.isEmpty()) {
            return;
        }
        m_protocol.ingest(chunk);
        appendDiagnostic(chunk);
    }

    void appendDiagnostic(const QByteArray &chunk)
    {
        if (m_diagnostic.size() >= maximumDiagnosticBytes) {
            return;
        }
        const qsizetype remaining = maximumDiagnosticBytes - m_diagnostic.size();
        m_diagnostic.append(chunk.first(std::min(remaining, chunk.size())));
    }

    void enter(Stage stage)
    {
        m_stage = stage;
        m_stageElapsed.restart();
        m_stableSamples = 0;
    }

    QJsonObject result(bool passed, const QString &failure = {}) const
    {
        const bool affected = m_outputGeometry.isValid() && m_baselineGeometry.isValid() &&
            m_baselineGeometry.size() == m_outputGeometry.size() &&
            m_reservedGeometry.size() == m_expectedReservedSize;
        const bool restored = m_outputGeometry.isValid() && m_restoredGeometry.isValid() &&
            m_restoredGeometry.size() == m_outputGeometry.size();
        return {
            {QStringLiteral("passed"), passed},
            {QStringLiteral("failure"), failure},
            {QStringLiteral("platform"), QGuiApplication::platformName()},
            {QStringLiteral("outputName"), m_screen ? m_screen->name() : QString{}},
            {QStringLiteral("outputGeometry"), rectangleJson(m_outputGeometry)},
            {QStringLiteral("outputScale"), m_screen ? m_screen->devicePixelRatio() : 0.0},
            {QStringLiteral("baselineWindowGeometry"), rectangleJson(m_baselineGeometry)},
            {QStringLiteral("reservedWindowGeometry"), rectangleJson(m_reservedGeometry)},
            {QStringLiteral("restoredWindowGeometry"), rectangleJson(m_restoredGeometry)},
            {QStringLiteral("expectedReservation"), expectedReservation},
            {QStringLiteral("maximizedWorkAreaAffected"), affected},
            {QStringLiteral("workAreaRestoredAfterShellExit"), restored},
            {QStringLiteral("shellStarted"), m_shellStarted},
            {QStringLiteral("shellStopped"), m_shell.state() == QProcess::NotRunning},
            {QStringLiteral("activeMappedSnapshotTaken"), m_activeMappedSnapshotTaken},
            {QStringLiteral("activeMappedLayerProtocol"), m_activeMappedProtocol},
            {QStringLiteral("layerProtocol"), m_protocol.evidence().toJson()},
        };
    }

    void printResult(const QJsonObject &document)
    {
        QTextStream(stdout) << "QINDAQT_SHELL_SURFACE_PROBE="
                            << QJsonDocument(document).toJson(QJsonDocument::Compact) << '\n';
    }

    void succeed()
    {
        enter(Stage::Finished);
        m_tick.stop();
        printResult(result(true));
        m_application.exit(0);
    }

    void fail(const QString &message)
    {
        if (m_stage == Stage::Finished) {
            return;
        }
        if (m_shell.state() != QProcess::NotRunning) {
            m_shell.kill();
            m_shell.waitForFinished(1'000);
        }
        drainShellError();
        m_protocol.finish();
        enter(Stage::Finished);
        m_tick.stop();
        printResult(result(false, message));
        if (!m_diagnostic.isEmpty()) {
            QTextStream(stderr) << "qindaqt-shell bounded diagnostic:\n"
                                << QString::fromUtf8(m_diagnostic) << '\n';
        }
        m_application.exit(1);
    }

    QGuiApplication &m_application;
    PaintedMaximizedWindow m_window;
    QScreen *m_screen = nullptr;
    QProcess m_shell;
    QTimer m_tick;
    QElapsedTimer m_globalElapsed;
    QElapsedTimer m_stageElapsed;
    QindaQt::Test::ShellSurfaceProtocolTrace m_protocol;
    Stage m_stage = Stage::Baseline;
    QRect m_outputGeometry;
    QRect m_baselineGeometry;
    QRect m_reservedGeometry;
    QRect m_restoredGeometry;
    QSize m_expectedReservedSize;
    QByteArray m_diagnostic;
    QJsonObject m_activeMappedProtocol;
    int m_stableSamples = 0;
    bool m_shellStarted = false;
    bool m_activeMappedSnapshotTaken = false;
};

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    ShellSurfaceProbe probe(application);
    QTimer::singleShot(0, &application, [&probe] { probe.start(); });
    return application.exec();
}
