// SPDX-License-Identifier: GPL-3.0-or-later
#include "raw_capture_decoder.h"

#include <QCoreApplication>

namespace QindaQt::Screenshot {
namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Screenshot", text);
}

bool supportedFormat(QImage::Format format)
{
    switch (format) {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    case QImage::Format_RGBX8888:
    case QImage::Format_RGBA8888:
    case QImage::Format_RGBA8888_Premultiplied:
    case QImage::Format_BGR30:
    case QImage::Format_A2BGR30_Premultiplied:
    case QImage::Format_RGB30:
    case QImage::Format_A2RGB30_Premultiplied:
    case QImage::Format_RGBX64:
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
    case QImage::Format_RGBX16FPx4:
    case QImage::Format_RGBA16FPx4:
    case QImage::Format_RGBA16FPx4_Premultiplied:
        return true;
    default:
        return false;
    }
}

DecodedCapture failure(const QString &error)
{
    DecodedCapture result;
    result.error = error;
    return result;
}

} // namespace

DecodedCapture decodeRawCapture(const QVariantMap &metadata, const QByteArray &bytes)
{
    bool widthOk = false;
    bool heightOk = false;
    bool strideOk = false;
    bool formatOk = false;
    const uint width = metadata.value(QStringLiteral("width")).toUInt(&widthOk);
    const uint height = metadata.value(QStringLiteral("height")).toUInt(&heightOk);
    const uint stride = metadata.value(QStringLiteral("stride")).toUInt(&strideOk);
    const uint formatValue = metadata.value(QStringLiteral("format")).toUInt(&formatOk);
    if (metadata.value(QStringLiteral("type")).toString() != QLatin1String("raw") || !widthOk
        || !heightOk || !strideOk || !formatOk)
        return failure(tr("KWin returned a screenshot QindaQt could not read."));
    if (width == 0 || height == 0 || width > uint(kMaxCaptureEdge) || height > uint(kMaxCaptureEdge))
        return failure(tr("KWin returned a screenshot with an impossible size."));
    if (formatValue >= uint(QImage::NImageFormats)
        || !supportedFormat(static_cast<QImage::Format>(formatValue)))
        return failure(tr("KWin returned a screenshot in a pixel format QindaQt does not read."));
    const auto format = static_cast<QImage::Format>(formatValue);
    const quint64 bytesPerPixel = QImage::toPixelFormat(format).bitsPerPixel() / 8U;
    const quint64 expected = quint64(stride) * height;
    if (quint64(stride) < quint64(width) * bytesPerPixel || expected > quint64(kMaxRawCaptureBytes))
        return failure(tr("KWin returned a screenshot that is too large or malformed."));
    if (expected != quint64(bytes.size()))
        return failure(tr("The screenshot arrived incomplete."));

    const QImage view(reinterpret_cast<const uchar *>(bytes.constData()), int(width), int(height),
                      qsizetype(stride), format);
    DecodedCapture result;
    // copy() detaches from `bytes`, which the caller releases right after.
    result.image = view.copy();
    if (result.image.isNull())
        return failure(tr("There was not enough memory for the screenshot."));
    bool scaleOk = false;
    const qreal scale = metadata.value(QStringLiteral("scale")).toReal(&scaleOk);
    result.scale = scaleOk && scale >= 0.25 && scale <= 8.0 ? scale : 1.0;
    return result;
}

bool isKWinCancellation(const QString &errorName)
{
    return errorName == QLatin1String("org.kde.KWin.ScreenShot2.Error.Cancelled");
}

QString describeKWinError(const QString &errorName, const QString &message)
{
    if (errorName == QLatin1String("org.kde.KWin.ScreenShot2.Error.NoAuthorized"))
        return tr("KWin did not allow this screenshot. QindaQt Screenshot must be installed so "
                  "KWin can find its desktop entry.");
    if (errorName == QLatin1String("org.kde.KWin.ScreenShot2.Error.NoActiveWindow"))
        return tr("There is no active window to capture.");
    if (errorName == QLatin1String("org.kde.KWin.ScreenShot2.Error.InvalidWindow"))
        return tr("That window closed before it could be captured.");
    if (errorName == QLatin1String("org.kde.KWin.ScreenShot2.Error.InvalidScreen"))
        return tr("That screen is no longer connected.");
    if (errorName == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown"))
        return tr("The screenshot service is not running. Screenshots need the QindaQt desktop "
                  "session.");
    if (errorName == QLatin1String("org.freedesktop.DBus.Error.NoReply")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.Timeout"))
        return tr("KWin did not answer the screenshot request in time.");
    return message.isEmpty() ? tr("The screenshot failed.")
                             : tr("The screenshot failed: %1").arg(message);
}

} // namespace QindaQt::Screenshot
