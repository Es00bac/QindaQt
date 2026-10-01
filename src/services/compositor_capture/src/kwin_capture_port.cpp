// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/compositor_capture/kwin_capture_port.h>

#include <QCoreApplication>
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QSocketNotifier>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace QindaQt::CompositorCapture {
namespace {

const auto ScreenshotService = QindaQt::CompositorNames::screenshotService;
const auto ScreenshotPath = QindaQt::CompositorNames::screenshotPath;
const auto ScreenshotInterface = QindaQt::CompositorNames::screenshotInterface;
const auto KWinService = QindaQt::CompositorNames::service;
// The pipe may still be draining after the reply; allow for a large
// multi-output capture on a busy machine.
constexpr int PipeGraceMilliseconds = 10000;

DecodedCapture failure(const QString &error)
{
    DecodedCapture result;
    result.error = error;
    return result;
}

QString ownerOf(const QDBusConnection &bus, QLatin1StringView service)
{
    auto *interface = bus.interface();
    if (!interface)
        return {};
    const QDBusReply<QString> owner = interface->serviceOwner(QString(service));
    return owner.isValid() ? owner.value() : QString();
}

} // namespace

KWinCapturePort::KWinCapturePort(QDBusConnection bus, QObject *parent)
    : CapturePort(parent)
    , m_bus(std::move(bus))
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this,
            [this] { finish(failure(tr("KWin did not finish the screenshot in time."))); });
}

KWinCapturePort::~KWinCapturePort()
{
    cancel();
}

bool KWinCapturePort::capture(const KWinCaptureCall &call)
{
    if (m_active)
        return false;
    reset();
    m_active = true;
    const quint64 serial = ++m_serial;

    if (!m_bus.isConnected()) {
        QTimer::singleShot(0, this, [this] {
            finish(failure(tr("There is no session bus, so KWin cannot be asked for a screenshot.")));
        });
        return true;
    }
    const QString screenshotOwner = ownerOf(m_bus, ScreenshotService);
    const QString kwinOwner = ownerOf(m_bus, KWinService);
    // AGENT-GUARD: only the compositor itself may answer. Another process
    // owning the ScreenShot2 name could otherwise feed arbitrary pixels into
    // a file the user believes shows their screen.
    if (screenshotOwner.isEmpty() || screenshotOwner != kwinOwner) {
        QTimer::singleShot(0, this, [this] {
            finish(failure(tr("The screenshot service is not running. Screenshots need the QindaQt "
                              "desktop session.")));
        });
        return true;
    }
    m_kwinOwner = screenshotOwner;

    int descriptors[2] = {-1, -1};
    if (::pipe2(descriptors, O_CLOEXEC | O_NONBLOCK) != 0) {
        QTimer::singleShot(0, this, [this] { finish(failure(tr("Could not open a screenshot pipe."))); });
        return true;
    }
    // Only the read end is non-blocking; KWin's writer thread blocks.
    const int writeFlags = ::fcntl(descriptors[1], F_GETFL, 0);
    if (writeFlags >= 0)
        static_cast<void>(::fcntl(descriptors[1], F_SETFL, writeFlags & ~O_NONBLOCK));
    m_readFd = descriptors[0];

    QDBusMessage message = QDBusMessage::createMethodCall(
        QString(ScreenshotService), QString(ScreenshotPath),
        QString(ScreenshotInterface), call.method);
    QVariantList arguments = call.leadingArguments;
    arguments.append(call.options);
    arguments.append(QVariant::fromValue(QDBusUnixFileDescriptor(descriptors[1])));
    message.setArguments(arguments);
    const QDBusPendingCall pending = m_bus.asyncCall(message, call.timeoutMilliseconds);
    // AGENT-GUARD: drop every local copy of the write end once sent. A copy
    // kept alive in the message or argument list prevents pipe EOF, and a
    // complete capture would then sit until the timeout (ADR-0241 finding).
    message = QDBusMessage();
    arguments.clear();
    ::close(descriptors[1]);

    m_notifier = new QSocketNotifier(m_readFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this,
            [this](QSocketDescriptor, QSocketNotifier::Type) { drainPipe(); });
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, serial] {
        const QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError())
            receivedReply(serial, {}, reply.error().name(), reply.error().message());
        else
            receivedReply(serial, reply.value(), {}, {});
    });
    m_timeout.start(call.timeoutMilliseconds + PipeGraceMilliseconds);
    return true;
}

void KWinCapturePort::cancel()
{
    ++m_serial;
    reset();
}

void KWinCapturePort::reset()
{
    m_timeout.stop();
    closePipe();
    m_metadata.clear();
    m_bytes.clear();
    m_kwinOwner.clear();
    m_replyReceived = false;
    m_pipeEnded = false;
    m_active = false;
}

void KWinCapturePort::drainPipe()
{
    if (m_readFd < 0 || !m_active)
        return;
    char chunk[65536];
    while (true) {
        const ssize_t count = ::read(m_readFd, chunk, sizeof(chunk));
        if (count > 0) {
            if (m_bytes.size() > kMaxRawCaptureBytes - count) {
                finish(failure(tr("The screenshot is larger than QindaQt accepts.")));
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
        finish(failure(tr("The screenshot could not be read from KWin.")));
        return;
    }
}

void KWinCapturePort::receivedReply(quint64 serial, const QVariantMap &metadata,
                                    const QString &errorName, const QString &errorMessage)
{
    if (!m_active || serial != m_serial)
        return;
    if (!errorName.isEmpty()) {
        DecodedCapture result;
        result.cancelled = isKWinCancellation(errorName);
        if (!result.cancelled)
            result.error = describeKWinError(errorName, errorMessage);
        finish(result);
        return;
    }
    m_metadata = metadata;
    m_replyReceived = true;
    finishIfReady();
}

void KWinCapturePort::finishIfReady()
{
    if (!m_active || !m_replyReceived || !m_pipeEnded)
        return;
    if (ownerOf(m_bus, ScreenshotService) != m_kwinOwner) {
        finish(failure(tr("KWin restarted during the screenshot.")));
        return;
    }
    finish(decodeRawCapture(m_metadata, m_bytes));
}

void KWinCapturePort::finish(DecodedCapture result)
{
    if (!m_active)
        return;
    ++m_serial;
    reset();
    Q_EMIT finished(result);
}

void KWinCapturePort::closePipe()
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

} // namespace QindaQt::CompositorCapture
