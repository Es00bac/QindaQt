// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QImage>
#include <QMetaType>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Screenshot {

// Largest raw payload accepted from KWin: three 8K outputs at 4 bytes per
// pixel fit; anything beyond is refused rather than allocated.
inline constexpr qsizetype kMaxRawCaptureBytes = 512LL * 1024 * 1024;
inline constexpr int kMaxCaptureEdge = 32767;

// One finished capture. `scale` is KWin's device pixel ratio for the image:
// image pixels per logical pixel. A failure carries a sentence, never a
// partial image.
struct DecodedCapture {
    QImage image;
    qreal scale = 1.0;
    QString error;
    // True when the user or KWin cancelled (Escape in the window picker);
    // a cancellation is not an error to report.
    bool cancelled = false;

    [[nodiscard]] bool ok() const { return error.isEmpty() && !cancelled && !image.isNull(); }
};

// Validates ScreenShot2's reply metadata against the bytes read from the
// pipe and returns an owned deep copy.
//
// AGENT-GUARD: every shape field is checked before QImage receives a raw
// pointer: an unknown format, a short stride or a truncated pipe would
// otherwise read outside `bytes`.
[[nodiscard]] DecodedCapture decodeRawCapture(const QVariantMap &metadata, const QByteArray &bytes);

// The user-facing sentence for a ScreenShot2 D-Bus error name.
[[nodiscard]] QString describeKWinError(const QString &errorName, const QString &message);
[[nodiscard]] bool isKWinCancellation(const QString &errorName);

} // namespace QindaQt::Screenshot

Q_DECLARE_METATYPE(QindaQt::Screenshot::DecodedCapture)
