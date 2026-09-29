// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "capture_port.h"
#include "capture_request.h"
#include "frame_store.h"

#include <QObject>
#include <QRect>
#include <QTimer>
#include <QVariantList>

#include <functional>

namespace QindaQt::Screenshot {

// One output as the overlay needs it: a name and its logical rectangle.
struct ScreenGeometry {
    QString name;
    QRect geometry;
};
using ScreenLayout = std::function<QList<ScreenGeometry>()>;

// Sequences one capture: hide, wait out the delay, ask the port, and for a
// region let the user frame the frozen workspace before cropping.
//
// AGENT-CONTRACT: phases are idle → waiting → capturing → (selecting) → idle.
// Exactly one of captured / cancelled / failed ends every start(). The flow
// owns no window; QML shows the overlay while phase is "selecting" and
// hides the main window on hideRequested. Selection math is delegated to
// region_geometry so rows pin it without QML.
class CaptureFlow final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY optionsChanged)
    Q_PROPERTY(int delaySeconds READ delaySeconds WRITE setDelaySeconds NOTIFY optionsChanged)
    Q_PROPERTY(bool includePointer READ includePointer WRITE setIncludePointer NOTIFY optionsChanged)
    Q_PROPERTY(bool includeDecorations READ includeDecorations WRITE setIncludeDecorations NOTIFY optionsChanged)
    Q_PROPERTY(bool windowMode READ windowMode NOTIFY optionsChanged)
    Q_PROPERTY(QVariantList modeChoices READ modeChoices CONSTANT)
    Q_PROPERTY(QVariantList delayChoices READ delayChoices CONSTANT)
    Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(int countdown READ countdown NOTIFY countdownChanged)
    Q_PROPERTY(QVariantList overlayScreens READ overlayScreens NOTIFY overlayChanged)
    Q_PROPERTY(QRect workspaceBounds READ workspaceBounds NOTIFY overlayChanged)
    Q_PROPERTY(qreal captureScale READ captureScale NOTIFY overlayChanged)

public:
    CaptureFlow(CapturePort &port, FrameStore &frames, ScreenLayout layout, QObject *parent = nullptr);
    ~CaptureFlow() override;

    [[nodiscard]] CaptureOptions options() const { return m_options; }
    void setOptions(const CaptureOptions &options);
    // Time given to our own windows to unmap before a capture starts.
    void setSettleMilliseconds(int milliseconds) { m_settleMs = milliseconds; }

    [[nodiscard]] QString mode() const { return captureModeId(m_options.mode); }
    void setMode(const QString &id);
    [[nodiscard]] int delaySeconds() const { return m_options.delaySeconds; }
    void setDelaySeconds(int seconds);
    [[nodiscard]] bool includePointer() const { return m_options.includePointer; }
    void setIncludePointer(bool include);
    [[nodiscard]] bool includeDecorations() const { return m_options.includeDecorations; }
    void setIncludeDecorations(bool include);
    [[nodiscard]] bool windowMode() const { return isWindowMode(m_options.mode); }
    [[nodiscard]] QVariantList modeChoices() const;
    [[nodiscard]] QVariantList delayChoices() const;

    [[nodiscard]] QString phase() const { return m_phase; }
    [[nodiscard]] int countdown() const { return m_countdown; }
    [[nodiscard]] QVariantList overlayScreens() const { return m_overlayScreens; }
    [[nodiscard]] QRect workspaceBounds() const { return m_workspaceBounds; }
    [[nodiscard]] qreal captureScale() const;

    Q_INVOKABLE bool start();
    Q_INVOKABLE void cancel();
    // QML hands geometry over as real-valued point/rect; the flow rounds to
    // whole logical pixels once, here.
    Q_INVOKABLE void confirmRegion(const QRectF &logical);
    Q_INVOKABLE QRect selectionFromPoints(const QPointF &anchor, const QPointF &current) const;
    Q_INVOKABLE QRect nudgeSelection(const QRectF &selection, int dx, int dy, bool resize) const;
    Q_INVOKABLE QRect keyboardSelection(int screenIndex) const;
    // Physical pixel size a logical selection will have, for the overlay label.
    Q_INVOKABLE QSize pixelSize(const QRectF &logical) const;

Q_SIGNALS:
    void optionsChanged();
    void phaseChanged();
    void countdownChanged();
    void overlayChanged();
    void hideRequested();
    void captured(const QImage &image, const QString &modeId);
    void cancelled();
    void failed(const QString &message);
    // The overlay's own feedback; the flow stays in "selecting".
    void selectionRejected(const QString &message);

private:
    void setPhase(const QString &phase);
    void tick();
    void requestCapture();
    void handleCapture(const DecodedCapture &result);
    void endSelection();

    CapturePort &m_port;
    FrameStore &m_frames;
    ScreenLayout m_layout;
    CaptureOptions m_options;
    QString m_phase = QStringLiteral("idle");
    int m_countdown = 0;
    int m_settleMs = 250;
    QTimer m_countdownTimer;
    QTimer m_settleTimer;
    QVariantList m_overlayScreens;
    QRect m_workspaceBounds;
};

} // namespace QindaQt::Screenshot
