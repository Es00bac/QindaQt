// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "capture_port.h"

#include <QList>
#include <QTimer>

// A capture port that records requests and answers when told to, so flow
// and UI rows never need KWin.
class FakeCapturePort final : public QindaQt::Screenshot::CapturePort {
    Q_OBJECT

public:
    QList<QindaQt::Screenshot::KWinCaptureCall> calls;
    bool active = false;
    int cancels = 0;

    bool capture(const QindaQt::Screenshot::KWinCaptureCall &call) override
    {
        if (active)
            return false;
        active = true;
        calls.append(call);
        return true;
    }
    void cancel() override
    {
        ++cancels;
        active = false;
    }
    [[nodiscard]] bool busy() const override { return active; }

    void answer(const QindaQt::Screenshot::DecodedCapture &result)
    {
        active = false;
        Q_EMIT finished(result);
    }
    void answerImage(const QImage &image, qreal scale = 1.0)
    {
        QindaQt::Screenshot::DecodedCapture result;
        result.image = image;
        result.scale = scale;
        answer(result);
    }
};
