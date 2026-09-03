// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qt_wayland_clipboard_sources_p.h"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace QindaQt::Services::ClipboardWayland {

struct DataControlSource::PendingWrite {
    int fd = -1;
    QByteArray payload;
    qsizetype offset = 0;
    std::unique_ptr<QSocketNotifier> notifier;
};

DataControlSource::DataControlSource(::ext_data_control_source_v1 *object,
                                     ClipboardModel::ClipboardValue value,
                                     QObject *parent)
    : QObject(parent), QtWayland::ext_data_control_source_v1(object),
      m_value(std::move(value))
{
}

DataControlSource::~DataControlSource()
{
    for (const auto &pending : m_writes) {
        if (pending->fd >= 0) {
            ::close(pending->fd);
        }
    }
    if (object() != nullptr) {
        destroy();
    }
}

void DataControlSource::ext_data_control_source_v1_send(const QString &mediaType,
                                                        int32_t fd)
{
    QByteArray payload;
    for (const auto &format : m_value.formats) {
        if (format.mediaType == mediaType) {
            payload = format.payload;
            break;
        }
    }
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(fd);
        return;
    }
    auto pending = std::make_unique<PendingWrite>();
    pending->fd = fd;
    pending->payload = std::move(payload);
    pending->notifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Write, this);
    PendingWrite *identity = pending.get();
    connect(pending->notifier.get(), &QSocketNotifier::activated, this,
            [this, identity] { pump(identity); });
    m_writes.push_back(std::move(pending));
    pump(identity);
}

void DataControlSource::pump(PendingWrite *pending)
{
    while (pending->offset < pending->payload.size()) {
        const ssize_t written = ::write(
            pending->fd, pending->payload.constData() + pending->offset,
            static_cast<size_t>(pending->payload.size() - pending->offset));
        if (written > 0) {
            pending->offset += static_cast<qsizetype>(written);
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        } else {
            finish(pending);
            return;
        }
    }
    finish(pending);
}

void DataControlSource::finish(PendingWrite *pending)
{
    const auto found = std::find_if(m_writes.begin(), m_writes.end(),
                                    [pending](const auto &candidate) {
                                        return candidate.get() == pending;
                                    });
    if (found == m_writes.end()) {
        return;
    }
    ::close((*found)->fd);
    (*found)->fd = -1;
    m_writes.erase(found);
}

} // namespace QindaQt::Services::ClipboardWayland
