// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_token_channel.h"

#include <QDeadlineTimer>

#include <array>
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <unistd.h>
#include <utility>

namespace QindaQt::Services::NotificationPresentation {
namespace {

class DescriptorGuard final {
public:
    explicit DescriptorGuard(int descriptor) noexcept : m_descriptor(descriptor) {}
    ~DescriptorGuard()
    {
        if (m_descriptor >= 0) {
            ::close(m_descriptor);
        }
    }

    DescriptorGuard(const DescriptorGuard &) = delete;
    DescriptorGuard &operator=(const DescriptorGuard &) = delete;

private:
    int m_descriptor = -1;
};

TokenChannelReadResult readFailure(TokenChannelStatus status, QString message)
{
    return {status, std::nullopt, std::move(message)};
}

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

TokenChannelReadResult PresentationTokenChannel::readAndClose(int descriptor)
{
    if (descriptor < 0) {
        return readFailure(TokenChannelStatus::InvalidDescriptor,
                           QStringLiteral("presentation token descriptor is invalid"));
    }
    DescriptorGuard closeDescriptor(descriptor);
    std::array<char, 66> bytes{};
    qsizetype used = 0;
    QDeadlineTimer deadline(2'000);
    while (used < qsizetype(bytes.size())) {
        pollfd readiness{descriptor, short(POLLIN | POLLHUP), 0};
        int ready = -1;
        do {
            ready = ::poll(&readiness, 1,
                           static_cast<int>(deadline.remainingTime()));
        } while (ready < 0 && errno == EINTR && !deadline.hasExpired());
        if (ready == 0 || deadline.hasExpired()) {
            return readFailure(
                TokenChannelStatus::ReadTimedOut,
                QStringLiteral("presentation token descriptor read timed out"));
        }
        if (ready < 0 || (readiness.revents & (POLLERR | POLLNVAL)) != 0) {
            return readFailure(
                TokenChannelStatus::ReadFailed,
                QStringLiteral("could not poll presentation token descriptor"));
        }
        const ssize_t count = ::read(descriptor, bytes.data() + used,
                                     bytes.size() - std::size_t(used));
        if (count == 0) {
            break;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return readFailure(
                TokenChannelStatus::ReadFailed,
                QStringLiteral("could not read presentation token descriptor: %1")
                    .arg(QString::fromLocal8Bit(std::strerror(errno))));
        }
        used += qsizetype(count);
    }
    if (used != 65 || bytes[64] != '\n') {
        return readFailure(
            TokenChannelStatus::InvalidRecord,
            QStringLiteral("presentation token descriptor contained an invalid record"));
    }
    QString tokenError;
    auto token = PresentationAccessToken::fromHex(
        QString::fromLatin1(bytes.data(), 64), &tokenError);
    if (!token) {
        return readFailure(TokenChannelStatus::InvalidRecord, std::move(tokenError));
    }
    return {TokenChannelStatus::Received, std::move(token), {}};
}

bool PresentationTokenChannel::writeAndClose(
    int descriptor, const PresentationAccessToken &token, QString *error)
{
    if (descriptor < 0) {
        setError(error, QStringLiteral("presentation token descriptor is invalid"));
        return false;
    }
    DescriptorGuard closeDescriptor(descriptor);
    const QByteArray record = token.toHex().toLatin1() + '\n';
    qsizetype written = 0;
    while (written < record.size()) {
        const ssize_t count = ::write(
            descriptor, record.constData() + written,
            std::size_t(record.size() - written));
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            setError(error,
                     QStringLiteral("could not write presentation token descriptor: %1")
                         .arg(QString::fromLocal8Bit(std::strerror(errno))));
            return false;
        }
        if (count == 0) {
            setError(error, QStringLiteral("presentation token descriptor stopped accepting data"));
            return false;
        }
        written += qsizetype(count);
    }
    setError(error, {});
    return true;
}

QString tokenChannelStatusName(TokenChannelStatus status)
{
    switch (status) {
    case TokenChannelStatus::Received:
        return QStringLiteral("received");
    case TokenChannelStatus::InvalidDescriptor:
        return QStringLiteral("invalid-descriptor");
    case TokenChannelStatus::ReadFailed:
        return QStringLiteral("read-failed");
    case TokenChannelStatus::ReadTimedOut:
        return QStringLiteral("read-timed-out");
    case TokenChannelStatus::InvalidRecord:
        return QStringLiteral("invalid-record");
    }
    return QStringLiteral("unknown");
}

} // namespace QindaQt::Services::NotificationPresentation
