// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinscreenshotpreviewport.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QSocketNotifier>
#include <QUuid>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace QindaQt::Shell {
namespace {

constexpr auto ScreenshotService = "org.kde.KWin.ScreenShot2";
constexpr auto ScreenshotPath = "/org/kde/KWin/ScreenShot2";
constexpr auto ScreenshotInterface = "org.kde.KWin.ScreenShot2";
constexpr auto CompositorService = "org.qindaqt.Compositor";
constexpr qsizetype MaxRawBytes = 64 * 1024 * 1024;
constexpr int MaxQueuedRequests = 64;

ShellTaskListApplet::TaskListPreviewResult decodeImage(
    const ShellTaskListApplet::TaskListPreviewRequest &request,
    const QVariantMap &metadata, const QByteArray &bytes)
{
    using ShellTaskListApplet::TaskListPreviewResult;
    TaskListPreviewResult result{request.windowId, request.revision, false, {}, {}};
    bool widthOk = false;
    bool heightOk = false;
    bool strideOk = false;
    bool formatOk = false;
    const uint width = metadata.value(QStringLiteral("width")).toUInt(&widthOk);
    const uint height = metadata.value(QStringLiteral("height")).toUInt(&heightOk);
    const uint stride = metadata.value(QStringLiteral("stride")).toUInt(&strideOk);
    const uint format = metadata.value(QStringLiteral("format")).toUInt(&formatOk);
    const quint64 expected = quint64(stride) * height;
    // AGENT-GUARD: Validate every shape field before QImage receives a raw
    // pointer. ScreenShot2 currently returns premultiplied ARGB32; an unknown
    // format or a partial pipe may otherwise read outside the byte buffer.
    if (metadata.value(QStringLiteral("type")).toString()
            != QLatin1StringView("raw")
        || QUuid(metadata.value(QStringLiteral("windowId")).toString())
            != QUuid(request.windowId)
        || !widthOk || !heightOk || !strideOk || !formatOk
        || width == 0 || height == 0 || width > 8192 || height > 8192
        || stride < quint64(width) * 4 || expected > MaxRawBytes
        || expected != quint64(bytes.size())
        || format != uint(QImage::Format_ARGB32_Premultiplied)) {
        result.reason = QStringLiteral("invalid KWin window preview payload");
        return result;
    }
    const QImage view(reinterpret_cast<const uchar *>(bytes.constData()),
                      int(width), int(height), int(stride),
                      QImage::Format_ARGB32_Premultiplied);
    if (view.isNull()) {
        result.reason = QStringLiteral("KWin window preview image is empty");
        return result;
    }
    const QSize maximum = request.maxSize.boundedTo(QSize(1024, 1024))
                              .expandedTo(QSize(16, 16));
    result.image = view.scaled(maximum, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
    result.ok = !result.image.isNull();
    if (!result.ok)
        result.reason = QStringLiteral("could not scale KWin window preview");
    return result;
}

} // namespace

KWinScreenshotPreviewPort::KWinScreenshotPreviewPort(QDBusConnection bus,
                                                     QObject *parent)
    : TaskListAppletPreviewPort(parent), m_bus(std::move(bus))
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(8000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        finish(false, {}, QStringLiteral("KWin window preview timed out"));
    });
}

KWinScreenshotPreviewPort::~KWinScreenshotPreviewPort()
{
    cancelAll();
}

void KWinScreenshotPreviewPort::requestPreview(
    const ShellTaskListApplet::TaskListPreviewRequest &request)
{
    if (QUuid(request.windowId).isNull() || request.maxSize.isEmpty()) {
        Q_EMIT previewFinished({request.windowId, request.revision, false, {},
                                QStringLiteral("invalid window preview request")});
        return;
    }
    if (m_queue.size() >= MaxQueuedRequests) {
        Q_EMIT previewFinished({request.windowId, request.revision, false, {},
                                QStringLiteral("window preview queue is full")});
        return;
    }
    m_queue.enqueue(request);
    startNext();
}

void KWinScreenshotPreviewPort::cancelAll()
{
    ++m_serial;
    m_timeout.stop();
    m_queue.clear();
    m_active.reset();
    m_metadata.clear();
    m_bytes.clear();
    m_kwinOwner.clear();
    m_replyReceived = false;
    m_pipeEnded = false;
    closePipe();
}

void KWinScreenshotPreviewPort::startNext()
{
    if (m_active || m_queue.isEmpty())
        return;
    m_active = m_queue.dequeue();
    m_metadata.clear();
    m_bytes.clear();
    m_replyReceived = false;
    m_pipeEnded = false;
    const quint64 serial = ++m_serial;

    auto *interface = m_bus.interface();
    if (!m_bus.isConnected() || !interface) {
        finish(false, {}, QStringLiteral("session bus is unavailable"));
        return;
    }
    const auto screenshotOwner = interface->serviceOwner(
        QString::fromLatin1(ScreenshotService));
    const auto compositorOwner = interface->serviceOwner(
        QString::fromLatin1(CompositorService));
    if (!screenshotOwner.isValid() || !compositorOwner.isValid()
        || screenshotOwner.value().isEmpty()
        || screenshotOwner.value() != compositorOwner.value()) {
        finish(false, {}, QStringLiteral("KWin screenshot owner does not match the compositor"));
        return;
    }
    m_kwinOwner = screenshotOwner.value();

    int descriptors[2] = {-1, -1};
    if (::pipe2(descriptors, O_CLOEXEC) != 0) {
        finish(false, {}, QStringLiteral("could not create window preview pipe"));
        return;
    }
    m_readFd = descriptors[0];
    const int flags = ::fcntl(m_readFd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(m_readFd, F_SETFL, flags | O_NONBLOCK) != 0) {
        ::close(descriptors[1]);
        finish(false, {}, QStringLiteral("could not configure window preview pipe"));
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(ScreenshotService),
        QString::fromLatin1(ScreenshotPath),
        QString::fromLatin1(ScreenshotInterface),
        QStringLiteral("CaptureWindow"));
    message.setArguments({m_active->windowId,
                          QVariantMap{{QStringLiteral("include-decoration"), true},
                                      {QStringLiteral("include-shadow"), false}},
                          QVariant::fromValue(QDBusUnixFileDescriptor(descriptors[1]))});
    const QDBusPendingCall pending = m_bus.asyncCall(message, 6000);
    // AGENT-GUARD: Drop every local copy of the write end after sending.
    // Retaining the QDBusMessage's QVariant-held copy prevents pipe EOF even
    // after KWin finishes, leaving a complete capture stuck until timeout.
    message = QDBusMessage();
    ::close(descriptors[1]);

    m_notifier = new QSocketNotifier(m_readFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this,
            [this](QSocketDescriptor, QSocketNotifier::Type) { drainPipe(); });
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, serial] {
                const QDBusPendingReply<QVariantMap> reply = *watcher;
                watcher->deleteLater();
                if (reply.isError())
                    receivedReply(serial, {}, reply.error().message());
                else
                    receivedReply(serial, reply.value(), {});
            });
    m_timeout.start();
}

void KWinScreenshotPreviewPort::drainPipe()
{
    if (m_readFd < 0 || !m_active)
        return;
    char chunk[65536];
    while (true) {
        const ssize_t count = ::read(m_readFd, chunk, sizeof(chunk));
        if (count > 0) {
            if (m_bytes.size() > MaxRawBytes - count) {
                finish(false, {}, QStringLiteral("KWin window preview is too large"));
                return;
            }
            m_bytes.append(chunk, count);
            continue;
        }
        if (count == 0) {
            m_pipeEnded = true;
            closePipe();
            finishIfReady();
            return;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        finish(false, {}, QStringLiteral("could not read KWin window preview"));
        return;
    }
}

void KWinScreenshotPreviewPort::receivedReply(quint64 serial,
                                              const QVariantMap &metadata,
                                              const QString &error)
{
    if (!m_active || serial != m_serial)
        return;
    if (!error.isEmpty()) {
        finish(false, {}, error);
        return;
    }
    m_metadata = metadata;
    m_replyReceived = true;
    const quint64 stride = metadata.value(QStringLiteral("stride")).toULongLong();
    const quint64 height = metadata.value(QStringLiteral("height")).toULongLong();
    if (stride == 0 || height == 0 || height > 8192
        || stride > quint64(MaxRawBytes) / height) {
        finish(false, {}, QStringLiteral("KWin window preview is too large"));
        return;
    }
    finishIfReady();
}

void KWinScreenshotPreviewPort::finishIfReady()
{
    if (!m_active || !m_replyReceived || !m_pipeEnded)
        return;
    auto *interface = m_bus.interface();
    const auto owner = interface
                           ? interface->serviceOwner(QString::fromLatin1(ScreenshotService))
                           : QDBusReply<QString>();
    const auto compositorOwner = interface
                                     ? interface->serviceOwner(QString::fromLatin1(CompositorService))
                                     : QDBusReply<QString>();
    if (!owner.isValid() || !compositorOwner.isValid()
        || owner.value() != m_kwinOwner
        || compositorOwner.value() != m_kwinOwner) {
        finish(false, {}, QStringLiteral("KWin screenshot owner changed during capture"));
        return;
    }
    auto result = decodeImage(*m_active, m_metadata, m_bytes);
    finish(result.ok, std::move(result.image), std::move(result.reason));
}

void KWinScreenshotPreviewPort::finish(bool ok, QImage image, QString reason)
{
    if (!m_active)
        return;
    const auto request = *m_active;
    ++m_serial;
    m_timeout.stop();
    closePipe();
    m_active.reset();
    m_metadata.clear();
    m_bytes.clear();
    m_kwinOwner.clear();
    m_replyReceived = false;
    m_pipeEnded = false;
    Q_EMIT previewFinished({request.windowId, request.revision, ok,
                            std::move(image), std::move(reason)});
    QTimer::singleShot(0, this, &KWinScreenshotPreviewPort::startNext);
}

void KWinScreenshotPreviewPort::closePipe()
{
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_readFd >= 0) {
        ::close(m_readFd);
        m_readFd = -1;
    }
}

} // namespace QindaQt::Shell
