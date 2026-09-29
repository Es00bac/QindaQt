// SPDX-License-Identifier: GPL-3.0-or-later
#include "capture_flow.h"

#include "region_geometry.h"

#include <QCoreApplication>
#include <QVariantMap>

#include <algorithm>

namespace QindaQt::Screenshot {
namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Screenshot", text);
}

QString modeText(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Region:
        return tr("Rectangular region");
    case CaptureMode::AllScreens:
        return tr("All screens");
    case CaptureMode::CurrentScreen:
        return tr("Current screen");
    case CaptureMode::ActiveWindow:
        return tr("Active window");
    case CaptureMode::WindowUnderPointer:
        return tr("Window under the pointer");
    }
    return {};
}

QString modeIcon(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Region:
        return QStringLiteral("crop");
    case CaptureMode::AllScreens:
        return QStringLiteral("monitor");
    case CaptureMode::CurrentScreen:
        return QStringLiteral("monitor");
    case CaptureMode::ActiveWindow:
        return QStringLiteral("panel-top");
    case CaptureMode::WindowUnderPointer:
        return QStringLiteral("mouse-pointer");
    }
    return {};
}

} // namespace

CaptureFlow::CaptureFlow(CapturePort &port, FrameStore &frames, ScreenLayout layout, QObject *parent)
    : QObject(parent)
    , m_port(port)
    , m_frames(frames)
    , m_layout(std::move(layout))
{
    m_countdownTimer.setInterval(1000);
    connect(&m_countdownTimer, &QTimer::timeout, this, &CaptureFlow::tick);
    m_settleTimer.setSingleShot(true);
    connect(&m_settleTimer, &QTimer::timeout, this, &CaptureFlow::requestCapture);
    connect(&m_port, &CapturePort::finished, this, &CaptureFlow::handleCapture);
}

CaptureFlow::~CaptureFlow() = default;

void CaptureFlow::setOptions(const CaptureOptions &options)
{
    if (m_options == options)
        return;
    m_options = options;
    Q_EMIT optionsChanged();
}

void CaptureFlow::setMode(const QString &id)
{
    const auto mode = captureModeFromId(id);
    if (!mode || *mode == m_options.mode)
        return;
    m_options.mode = *mode;
    Q_EMIT optionsChanged();
}

void CaptureFlow::setDelaySeconds(int seconds)
{
    if (seconds < 0 || seconds > kMaxDelaySeconds || seconds == m_options.delaySeconds)
        return;
    m_options.delaySeconds = seconds;
    Q_EMIT optionsChanged();
}

void CaptureFlow::setIncludePointer(bool include)
{
    if (include == m_options.includePointer)
        return;
    m_options.includePointer = include;
    Q_EMIT optionsChanged();
}

void CaptureFlow::setIncludeDecorations(bool include)
{
    if (include == m_options.includeDecorations)
        return;
    m_options.includeDecorations = include;
    Q_EMIT optionsChanged();
}

QVariantList CaptureFlow::modeChoices() const
{
    QVariantList choices;
    for (const auto mode : {CaptureMode::Region, CaptureMode::AllScreens, CaptureMode::CurrentScreen,
                            CaptureMode::ActiveWindow, CaptureMode::WindowUnderPointer}) {
        choices.append(QVariantMap{{QStringLiteral("id"), captureModeId(mode)},
                                   {QStringLiteral("text"), modeText(mode)},
                                   {QStringLiteral("iconName"), modeIcon(mode)}});
    }
    return choices;
}

QVariantList CaptureFlow::delayChoices() const
{
    QVariantList choices;
    for (const int seconds : allowedDelays()) {
        choices.append(QVariantMap{
            {QStringLiteral("seconds"), seconds},
            {QStringLiteral("text"), seconds == 0 ? tr("No delay") : tr("%1 seconds").arg(seconds)}});
    }
    return choices;
}

qreal CaptureFlow::captureScale() const
{
    return m_frames.workspace().scale;
}

bool CaptureFlow::start()
{
    if (m_phase != QLatin1String("idle"))
        return false;
    Q_EMIT hideRequested();
    m_countdown = m_options.delaySeconds;
    Q_EMIT countdownChanged();
    setPhase(QStringLiteral("waiting"));
    if (m_countdown > 0)
        m_countdownTimer.start();
    else
        m_settleTimer.start(m_settleMs);
    return true;
}

void CaptureFlow::tick()
{
    m_countdown = std::max(0, m_countdown - 1);
    Q_EMIT countdownChanged();
    if (m_countdown == 0) {
        m_countdownTimer.stop();
        m_settleTimer.start(0);
    }
}

void CaptureFlow::requestCapture()
{
    setPhase(QStringLiteral("capturing"));
    if (!m_port.capture(kwinCallFor(m_options))) {
        setPhase(QStringLiteral("idle"));
        Q_EMIT failed(tr("Another screenshot is still being taken."));
    }
}

void CaptureFlow::handleCapture(const DecodedCapture &result)
{
    if (m_phase != QLatin1String("capturing"))
        return;
    if (result.cancelled) {
        setPhase(QStringLiteral("idle"));
        Q_EMIT cancelled();
        return;
    }
    if (!result.ok()) {
        setPhase(QStringLiteral("idle"));
        Q_EMIT failed(result.error.isEmpty() ? tr("The screenshot failed.") : result.error);
        return;
    }
    if (m_options.mode != CaptureMode::Region) {
        setPhase(QStringLiteral("idle"));
        Q_EMIT captured(result.image, captureModeId(m_options.mode));
        return;
    }
    const QList<ScreenGeometry> screens = m_layout ? m_layout() : QList<ScreenGeometry>();
    QRect bounds;
    QList<QRect> rects;
    for (const ScreenGeometry &screen : screens) {
        bounds = bounds.united(screen.geometry);
        rects.append(screen.geometry);
    }
    WorkspaceFrame frame{result.image, bounds.topLeft(), result.scale};
    // AGENT-GUARD: without a screen layout there is nothing to draw the
    // overlay on; treat the workspace image as one output at the origin.
    if (rects.isEmpty()) {
        frame.origin = {};
        bounds = frame.logicalBounds();
        rects.append(bounds);
    }
    m_frames.setWorkspace(frame, rects);
    m_workspaceBounds = bounds;
    m_overlayScreens.clear();
    const int revision = m_frames.workspaceRevision();
    for (qsizetype index = 0; index < rects.size(); ++index) {
        const QRect &rect = rects.at(index);
        m_overlayScreens.append(QVariantMap{
            {QStringLiteral("index"), int(index)},
            {QStringLiteral("name"), index < screens.size() ? screens.at(index).name : QString()},
            {QStringLiteral("x"), rect.x()},
            {QStringLiteral("y"), rect.y()},
            {QStringLiteral("width"), rect.width()},
            {QStringLiteral("height"), rect.height()},
            {QStringLiteral("source"),
             QStringLiteral("image://capture/screen/%1/%2").arg(revision).arg(index)}});
    }
    Q_EMIT overlayChanged();
    setPhase(QStringLiteral("selecting"));
}

void CaptureFlow::confirmRegion(const QRectF &logical)
{
    if (m_phase != QLatin1String("selecting"))
        return;
    const QImage image =
        cropRegion(m_frames.workspace(), logical.toRect().intersected(m_workspaceBounds));
    if (image.isNull()) {
        Q_EMIT selectionRejected(tr("Select an area first, or press Esc to cancel."));
        return;
    }
    endSelection();
    Q_EMIT captured(image, captureModeId(CaptureMode::Region));
}

void CaptureFlow::cancel()
{
    m_countdownTimer.stop();
    m_settleTimer.stop();
    if (m_phase == QLatin1String("idle"))
        return;
    m_port.cancel();
    endSelection();
    Q_EMIT cancelled();
}

void CaptureFlow::endSelection()
{
    m_frames.clearWorkspace();
    m_overlayScreens.clear();
    m_workspaceBounds = {};
    Q_EMIT overlayChanged();
    setPhase(QStringLiteral("idle"));
}

QRect CaptureFlow::selectionFromPoints(const QPointF &anchor, const QPointF &current) const
{
    return Screenshot::selectionFromPoints(anchor.toPoint(), current.toPoint(), m_workspaceBounds);
}

QRect CaptureFlow::nudgeSelection(const QRectF &selection, int dx, int dy, bool resize) const
{
    return nudgedSelection(selection.toRect(), dx, dy, resize, m_workspaceBounds);
}

QRect CaptureFlow::keyboardSelection(int screenIndex) const
{
    if (screenIndex < 0 || screenIndex >= m_overlayScreens.size())
        return keyboardStartSelection(m_workspaceBounds);
    const QVariantMap screen = m_overlayScreens.at(screenIndex).toMap();
    return keyboardStartSelection(QRect(screen.value(QStringLiteral("x")).toInt(),
                                        screen.value(QStringLiteral("y")).toInt(),
                                        screen.value(QStringLiteral("width")).toInt(),
                                        screen.value(QStringLiteral("height")).toInt()));
}

QSize CaptureFlow::pixelSize(const QRectF &logical) const
{
    return toImagePixels(m_frames.workspace(), logical.toRect()).size();
}

void CaptureFlow::setPhase(const QString &phase)
{
    if (m_phase == phase)
        return;
    m_phase = phase;
    Q_EMIT phaseChanged();
}

} // namespace QindaQt::Screenshot
