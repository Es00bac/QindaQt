// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/notification_policy.h>
#include <QBuffer>
#include <QImageReader>
#include <QDBusMetaType>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
namespace QindaQt::Services::Portal {
namespace {
struct ImageData { int width, height, stride; bool alpha; int bits, channels; QByteArray bytes; };
QDBusArgument &operator<<(QDBusArgument &a, const ImageData &v) {
    a.beginStructure(); a << v.width << v.height << v.stride << v.alpha << v.bits << v.channels << v.bytes; a.endStructure(); return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, ImageData &v) {
    a.beginStructure(); a >> v.width >> v.height >> v.stride >> v.alpha >> v.bits >> v.channels >> v.bytes; a.endStructure(); return a;
}
}
} // namespace QindaQt::Services::Portal
Q_DECLARE_METATYPE(QindaQt::Services::Portal::ImageData)
namespace QindaQt::Services::Portal {
bool decodeNotificationIcon(const QDBusUnixFileDescriptor &descriptor, QVariantMap *hints) {
    if (!descriptor.isValid() || !hints) return false;
    const int fd = descriptor.fileDescriptor(); struct stat status{};
    const int seals = fcntl(fd, F_GET_SEALS);
    constexpr int immutable = F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW;
    if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size <= 0 || status.st_size > 4 * 1024 * 1024
        || seals < 0 || (seals & immutable) != immutable) return false;
    QByteArray bytes(static_cast<qsizetype>(status.st_size), Qt::Uninitialized); qsizetype readBytes = 0;
    while (readBytes < bytes.size()) {
        const auto count = pread(fd, bytes.data() + readBytes, static_cast<size_t>(bytes.size() - readBytes), readBytes);
        if (count <= 0) return false;
        readBytes += count;
    }
    QBuffer buffer(&bytes); if (!buffer.open(QIODevice::ReadOnly)) return false;
    QImageReader reader(&buffer); const auto format = reader.format(); const auto size = reader.size();
    // The resident does not draw or initialize a GUI/font stack. Raster icon
    // codecs are bounded here; unsupported SVG/other formats fail explicitly.
    if ((format != "png" && format != "jpeg") || !size.isValid() || size.width() > 2048 || size.height() > 2048
        || static_cast<qint64>(size.width()) * size.height() > 2 * 1024 * 1024) return false;
    const auto image = reader.read().convertToFormat(QImage::Format_RGBA8888); if (image.isNull()) return false;
    qDBusRegisterMetaType<ImageData>();
    ImageData value{image.width(), image.height(), static_cast<int>(image.bytesPerLine()), true, 8, 4,
        QByteArray(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes())};
    hints->insert(QStringLiteral("image-data"), QVariant::fromValue(value)); return true;
}
} // namespace QindaQt::Services::Portal
