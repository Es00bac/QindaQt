// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "capture_request.h"
#include "raw_capture_decoder.h"

#include <QObject>

namespace QindaQt::Screenshot {

// The seam between the capture flow and whatever produces pixels.
//
// AGENT-CONTRACT: one request at a time; `finished` fires exactly once per
// accepted capture() call, asynchronously, with an owned image or a
// sentence. cancel() drops the pending request without emitting. GUI-thread
// confined; the owner outlives the port's pending work.
class CapturePort : public QObject {
    Q_OBJECT

public:
    explicit CapturePort(QObject *parent = nullptr) : QObject(parent) {}
    ~CapturePort() override = default;

    // Returns false (and emits nothing) while a capture is already running.
    virtual bool capture(const KWinCaptureCall &call) = 0;
    virtual void cancel() = 0;
    [[nodiscard]] virtual bool busy() const = 0;

Q_SIGNALS:
    void finished(const QindaQt::Screenshot::DecodedCapture &result);
};

} // namespace QindaQt::Screenshot
