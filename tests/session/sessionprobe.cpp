// SPDX-License-Identifier: GPL-3.0-or-later
#include "compositorworkflow.h"

#include <QBackingStore>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QRegion>
#include <QResizeEvent>
#include <QScreen>
#include <QTextStream>
#include <QTimer>
#include <QWindow>

namespace {

class PaintedProbeWindow final : public QWindow
{
public:
    PaintedProbeWindow(QString title, QSize initialSize, QColor color)
        : m_store(this), m_color(std::move(color))
    {
        setTitle(std::move(title));
        resize(initialSize);
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

bool verifyXWayland(const QString &display)
{
    if (display.isEmpty()) {
        return false;
    }
    QProcess probe;
    probe.setProgram(QStringLiteral("xdpyinfo"));
    probe.setArguments({QStringLiteral("-display"), display});
    probe.start();
    return probe.waitForFinished(5000) && probe.exitStatus() == QProcess::NormalExit &&
           probe.exitCode() == 0;
}

QJsonObject collectResult(const QWindow &primary, const QWindow &secondary, const QWindow &page,
                          QindaQt::Test::CompositorWorkflowMode compositorMode)
{
    QJsonArray outputs;
    for (const auto *screen : QGuiApplication::screens()) {
        outputs.append(QJsonObject{{QStringLiteral("name"), screen->name()},
                                   {QStringLiteral("width"), screen->geometry().width()},
                                   {QStringLiteral("height"), screen->geometry().height()},
                                   // Wayland advertises an integer client buffer
                                   // scale; the compositor inventory below carries
                                   // the exact fractional output scale.
                                   {QStringLiteral("bufferScale"), screen->devicePixelRatio()}});
    }
    const auto display = QString::fromUtf8(qgetenv("DISPLAY"));
    const auto workflow = QindaQt::Test::exerciseCompositorWorkflow(
        primary.title(), secondary.title(), page.title(), compositorMode);
    return QJsonObject{
        {QStringLiteral("platform"), QGuiApplication::platformName()},
        {QStringLiteral("waylandDisplay"), QString::fromUtf8(qgetenv("WAYLAND_DISPLAY"))},
        {QStringLiteral("xwaylandDisplay"), display},
        {QStringLiteral("xwaylandReachable"), verifyXWayland(display)},
        {QStringLiteral("compositorService"), workflow.serviceAvailable},
        {QStringLiteral("compositorKWinAbi"), workflow.kwinAbi},
        {QStringLiteral("compositorControlMode"), workflow.controlMode},
        {QStringLiteral("compositorMutationsEnabled"), workflow.mutationsEnabled},
        {QStringLiteral("compositorInputObserverActive"), workflow.inputObserverActive},
        {QStringLiteral("compositorInputConsumesEvents"), workflow.inputConsumesEvents},
        {QStringLiteral("compositorInputDevices"), workflow.inputDevices},
        {QStringLiteral("compositorWorkflow"), workflow.workflowPassed},
        {QStringLiteral("compositorFailure"), workflow.failure},
        {QStringLiteral("compositorEvidence"), workflow.evidence},
        {QStringLiteral("compositorOutputs"), workflow.outputs},
        {QStringLiteral("windowExposed"),
         primary.isExposed() && secondary.isExposed() && page.isExposed()},
        {QStringLiteral("outputs"), outputs}};
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    PaintedProbeWindow primary(QStringLiteral("QindaQt nested probe primary"), QSize(640, 480),
                               QColor(QStringLiteral("#31506b")));
    primary.show();
    PaintedProbeWindow secondary(QStringLiteral("QindaQt nested probe secondary"), QSize(520, 360),
                                 QColor(QStringLiteral("#6b3150")));
    secondary.show();
    PaintedProbeWindow page(QStringLiteral("QindaQt nested probe page"), QSize(460, 420),
                            QColor(QStringLiteral("#506b31")));
    page.show();

    const bool pluginExpected =
        qEnvironmentVariableIntValue("QINDAQT_EXPECT_COMPOSITOR_PLUGIN") == 1;
    const bool readOnlyExpected =
        qEnvironmentVariableIntValue("QINDAQT_EXPECT_READ_ONLY_CONTROL") == 1;
    const bool compositorOutputsExpected =
        qEnvironmentVariableIntValue("QINDAQT_EXPECT_COMPOSITOR_OUTPUTS") == 1;
    QTimer::singleShot(
        700, &application,
        [&application, &primary, &secondary, &page, pluginExpected, readOnlyExpected,
         compositorOutputsExpected] {
            const auto mode =
                pluginExpected     ? QindaQt::Test::CompositorWorkflowMode::DevelopmentMutations
                : readOnlyExpected ? QindaQt::Test::CompositorWorkflowMode::ProductionReadOnly
                                   : QindaQt::Test::CompositorWorkflowMode::InventoryOnly;
            const auto result = collectResult(primary, secondary, page, mode);
            QTextStream(stdout) << "QINDAQT_PROBE="
                                << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
            const bool valid =
                result.value(QStringLiteral("platform")) == QStringLiteral("wayland") &&
                !result.value(QStringLiteral("waylandDisplay")).toString().isEmpty() &&
                result.value(QStringLiteral("xwaylandReachable")).toBool() &&
                !result.value(QStringLiteral("outputs")).toArray().isEmpty() &&
                (!compositorOutputsExpected ||
                 (result.value(QStringLiteral("compositorService")).toBool() &&
                  !result.value(QStringLiteral("compositorOutputs")).toArray().isEmpty())) &&
                (!pluginExpected ||
                 (result.value(QStringLiteral("compositorService")).toBool() &&
                  result.value(QStringLiteral("compositorWorkflow")).toBool())) &&
                (!readOnlyExpected ||
                 (result.value(QStringLiteral("compositorService")).toBool() &&
                  result.value(QStringLiteral("compositorWorkflow")).toBool() &&
                  result.value(QStringLiteral("compositorControlMode")) ==
                      QStringLiteral("read-only")));
            application.exit(valid ? 0 : 1);
        });
    return application.exec();
}
