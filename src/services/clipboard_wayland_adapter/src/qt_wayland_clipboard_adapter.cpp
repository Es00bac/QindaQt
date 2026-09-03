// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_wayland_adapter/production_clipboard_wayland_adapter.h>

#include <qindaqt/services/clipboard_model/clipboard_media.h>

#include "qwayland-ext-data-control-v1.h"
#include "qt_wayland_clipboard_sources_p.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSocketNotifier>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace QindaQt::Services::ClipboardWayland {
namespace {

using ClipboardModel::ClipboardError;
using ClipboardModel::ClipboardFormat;
using ClipboardModel::ClipboardValue;
using ClipboardModel::MediaClass;
constexpr size_t kMaxActiveSources = 8;

class QtClipboardAdapter;

class Offer final : public QtWayland::ext_data_control_offer_v1 {
public:
    Offer(::ext_data_control_offer_v1 *object, QtClipboardAdapter *owner)
        : QtWayland::ext_data_control_offer_v1(object), m_owner(owner) {}
    ~Offer() override
    {
        if (object() != nullptr) {
            destroy();
        }
    }

    [[nodiscard]] const QStringList &mediaTypes() const noexcept { return m_mediaTypes; }

protected:
    void ext_data_control_offer_v1_offer(const QString &mediaType) override
    {
        m_mediaTypes.append(mediaType);
    }

private:
    QtClipboardAdapter *m_owner = nullptr;
    QStringList m_mediaTypes;
};

class Device final : public QtWayland::ext_data_control_device_v1 {
public:
    Device(::ext_data_control_device_v1 *object, QtClipboardAdapter *owner);
    ~Device() override;

protected:
    void ext_data_control_device_v1_data_offer(::ext_data_control_offer_v1 *offer) override;
    void ext_data_control_device_v1_selection(::ext_data_control_offer_v1 *offer) override;
    void ext_data_control_device_v1_primary_selection(::ext_data_control_offer_v1 *offer) override;
    void ext_data_control_device_v1_finished() override;

private:
    QtClipboardAdapter *m_owner = nullptr;
};

struct Transfer {
    SelectionKind kind = SelectionKind::Clipboard;
    Offer *offer = nullptr;
    QStringList mediaTypes;
    ClipboardValue value;
    qsizetype nextFormat = 0;
    int readFd = -1;
    std::unique_ptr<QSocketNotifier> notifier;
};

class QtClipboardAdapter final : public QObject, public ClipboardWaylandAdapter {
public:
    void setObserver(CaptureObserver *observer) override { m_observer = observer; }

    [[nodiscard]] StartStatus start() override
    {
        if (m_running) {
            return StartStatus::AlreadyStarted;
        }
        m_display = wl_display_connect(nullptr);
        if (m_display == nullptr) {
            return StartStatus::ConnectionUnavailable;
        }
        m_registry = wl_display_get_registry(m_display);
        if (m_registry == nullptr) {
            cleanupConnection();
            return StartStatus::ConnectionUnavailable;
        }
        static const wl_registry_listener listener{&globalAdded, &globalRemoved};
        if (wl_registry_add_listener(m_registry, &listener, this) < 0) {
            cleanupConnection();
            return StartStatus::ConnectionUnavailable;
        }
        const int fd = wl_display_get_fd(m_display);
#if defined(Q_OS_LINUX)
        struct ucred credentials {};
        socklen_t size = sizeof(credentials);
        if (fd < 0 || ::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &size) != 0
            || size != sizeof(credentials) || credentials.pid <= 1) {
            cleanupConnection();
            return StartStatus::ConnectionUnavailable;
        }
        m_peerProcessId = static_cast<qint64>(credentials.pid);
#else
        cleanupConnection();
        return StartStatus::UnsupportedPlatform;
#endif
        m_readNotifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read, this);
        m_writeNotifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Write, this);
        m_writeNotifier->setEnabled(false);
        connect(m_readNotifier.get(), &QSocketNotifier::activated, this,
                [this] { dispatchReadable(); });
        connect(m_writeNotifier.get(), &QSocketNotifier::activated, this, [this] { flush(); });
        m_running = true;
        flush();
        return StartStatus::Started;
    }

    void stop() override
    {
        if (!m_running && m_display == nullptr) {
            return;
        }
        m_running = false;
        setAvailable(false);
        cancelTransfer();
        m_clipboardOffer.reset();
        m_primaryOffer.reset();
        m_offers.clear();
        for (const auto &source : m_sources) {
            delete source.data();
        }
        m_sources.clear();
        m_device.reset();
        m_manager.reset();
        if (m_seat != nullptr) {
            wl_seat_destroy(m_seat);
            m_seat = nullptr;
        }
        m_readNotifier.reset();
        m_writeNotifier.reset();
        cleanupConnection();
        m_observer = nullptr;
    }

    void setCaptureEnabled(const bool enabled) override
    {
        m_captureEnabled = enabled;
        if (!enabled) {
            cancelTransfer();
        }
    }

    [[nodiscard]] bool isAvailable() const noexcept override { return m_available; }
    [[nodiscard]] qint64 peerProcessId() const noexcept override
    {
        return m_running ? m_peerProcessId : 0;
    }

    [[nodiscard]] bool publishSelection(const SelectionKind kind,
                                        const ClipboardValue &value) override
    {
        if (!m_available || m_manager == nullptr || m_device == nullptr
            || value.formats.isEmpty()) {
            return false;
        }
        std::erase_if(m_sources, [](const auto &source) { return source.isNull(); });
        if (m_sources.size() >= kMaxActiveSources) {
            return false;
        }
        auto *raw = m_manager->create_data_source();
        if (raw == nullptr) {
            return false;
        }
        auto *source = new DataControlSource(raw, value, this);
        for (const auto &format : value.formats) {
            source->offer(format.mediaType);
        }
        m_sources.push_back(source);
        if (kind == SelectionKind::Clipboard) {
            m_device->set_selection(source->object());
        } else {
            m_device->set_primary_selection(source->object());
        }
        flush();
        return true;
    }

    void introduceOffer(::ext_data_control_offer_v1 *raw)
    {
        m_offers.push_back(std::make_unique<Offer>(raw, this));
    }

    void selectOffer(const SelectionKind kind, ::ext_data_control_offer_v1 *raw)
    {
        std::unique_ptr<Offer> selected;
        if (raw != nullptr) {
            const auto it = std::find_if(m_offers.begin(), m_offers.end(),
                                         [raw](const auto &offer) {
                                             return offer->object() == raw;
                                         });
            if (it == m_offers.end()) {
                return;
            }
            selected = std::move(*it);
            m_offers.erase(it);
        }
        if (kind == SelectionKind::Clipboard) {
            m_clipboardOffer = std::move(selected);
            beginCapture(kind, m_clipboardOffer.get());
        } else {
            m_primaryOffer = std::move(selected);
            beginCapture(kind, m_primaryOffer.get());
        }
    }

    void deviceFinished()
    {
        m_device.reset();
        setAvailable(false);
        cancelTransfer();
    }

private:
    static void globalAdded(void *data, wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version)
    {
        auto *self = static_cast<QtClipboardAdapter *>(data);
        if (qstrcmp(interface, ext_data_control_manager_v1_interface.name) == 0
            && self->m_manager == nullptr) {
            self->m_manager = std::make_unique<DataControlManager>();
            self->m_managerGlobal = name;
            self->m_manager->init(registry, name, static_cast<int>(std::min(version, 1U)));
        } else if (qstrcmp(interface, wl_seat_interface.name) == 0
                   && self->m_seat == nullptr) {
            self->m_seatGlobal = name;
            self->m_seat = static_cast<wl_seat *>(wl_registry_bind(
                registry, name, &wl_seat_interface, std::min(version, 9U)));
        }
        self->createDeviceIfReady();
    }

    static void globalRemoved(void *data, wl_registry *, uint32_t name)
    {
        auto *self = static_cast<QtClipboardAdapter *>(data);
        if (name == self->m_managerGlobal || name == self->m_seatGlobal) {
            self->transportLost();
        }
    }

    void createDeviceIfReady()
    {
        if (m_manager != nullptr && m_seat != nullptr && m_device == nullptr) {
            auto *raw = m_manager->get_data_device(m_seat);
            if (raw != nullptr) {
                m_device = std::make_unique<Device>(raw, this);
                setAvailable(true);
                flush();
            }
        }
    }

    void beginCapture(SelectionKind kind, Offer *offer)
    {
        cancelTransfer();
        if (!m_captureEnabled || offer == nullptr) {
            return;
        }
        QStringList admitted;
        ClipboardError refusal = ClipboardError::None;
        for (const QString &raw : offer->mediaTypes()) {
            const auto canonical = ClipboardModel::canonicalizeMediaType(raw);
            if (!canonical.accepted()) {
                continue;
            }
            const MediaClass mediaClass = ClipboardModel::classifyMediaType(canonical.canonical);
            if (mediaClass == MediaClass::Sensitive) {
                refusal = ClipboardError::SensitiveRefused;
            } else if (mediaClass == MediaClass::OneTime
                       && refusal != ClipboardError::SensitiveRefused) {
                refusal = ClipboardError::OneTimeRefused;
            } else if (mediaClass == MediaClass::Storable
                       && !admitted.contains(canonical.canonical)) {
                admitted.append(canonical.canonical);
            }
        }
        if (refusal != ClipboardError::None || admitted.isEmpty()
            || admitted.size() > ClipboardModel::kMaxFormatsPerItem) {
            if (m_observer != nullptr) {
                m_observer->captureRefused(
                    kind, refusal != ClipboardError::None ? refusal
                                                          : ClipboardError::NonStorableRefused);
            }
            return;
        }
        m_transfer = std::make_unique<Transfer>();
        m_transfer->kind = kind;
        m_transfer->offer = offer;
        m_transfer->mediaTypes = admitted;
        requestNextFormat();
    }

    void requestNextFormat()
    {
        if (m_transfer == nullptr || m_transfer->nextFormat >= m_transfer->mediaTypes.size()) {
            if (m_transfer != nullptr && m_observer != nullptr) {
                m_observer->captured(m_transfer->kind, m_transfer->value);
            }
            m_transfer.reset();
            return;
        }
        int pipes[2] = {-1, -1};
        if (::pipe2(pipes, O_CLOEXEC | O_NONBLOCK) != 0) {
            refuseTransfer(ClipboardError::MalformedData);
            return;
        }
        m_transfer->readFd = pipes[0];
        const QString mediaType = m_transfer->mediaTypes.at(m_transfer->nextFormat);
        m_transfer->offer->receive(mediaType, pipes[1]);
        ::close(pipes[1]);
        m_transfer->notifier = std::make_unique<QSocketNotifier>(
            m_transfer->readFd, QSocketNotifier::Read, this);
        connect(m_transfer->notifier.get(), &QSocketNotifier::activated, this,
                [this] { readTransfer(); });
        flush();
    }

    void readTransfer()
    {
        if (m_transfer == nullptr || m_transfer->readFd < 0) {
            return;
        }
        QByteArray bytes;
        char buffer[16 * 1024];
        bool complete = false;
        while (true) {
            const ssize_t count = ::read(m_transfer->readFd, buffer, sizeof(buffer));
            if (count > 0) {
                bytes.append(buffer, static_cast<qsizetype>(count));
                continue;
            }
            if (count == 0) {
                complete = true;
            } else if (errno == EINTR) {
                continue;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                refuseTransfer(ClipboardError::MalformedData);
                return;
            }
            break;
        }
        if (!bytes.isEmpty()) {
            if (m_totalTransferBytes > ClipboardModel::kMaxItemPayloadBytes - bytes.size()) {
                refuseTransfer(ClipboardError::OversizedValue);
                return;
            }
            m_partial.append(bytes);
            m_totalTransferBytes += bytes.size();
        }
        if (!complete) {
            return;
        }
        const QString mediaType = m_transfer->mediaTypes.at(m_transfer->nextFormat);
        m_transfer->value.formats.append({mediaType, std::exchange(m_partial, {})});
        m_transfer->notifier.reset();
        ::close(std::exchange(m_transfer->readFd, -1));
        ++m_transfer->nextFormat;
        requestNextFormat();
    }

    void refuseTransfer(ClipboardError error)
    {
        const SelectionKind kind = m_transfer->kind;
        cancelTransfer();
        if (m_observer != nullptr) {
            m_observer->captureRefused(kind, error);
        }
    }

    void cancelTransfer()
    {
        if (m_transfer != nullptr && m_transfer->readFd >= 0) {
            ::close(m_transfer->readFd);
        }
        m_transfer.reset();
        m_partial.clear();
        m_totalTransferBytes = 0;
    }

    void dispatchReadable()
    {
        if (m_display != nullptr && wl_display_dispatch(m_display) < 0) {
            transportLost();
        }
    }

    void flush()
    {
        if (m_display == nullptr) {
            return;
        }
        if (wl_display_flush(m_display) >= 0) {
            if (m_writeNotifier != nullptr) {
                m_writeNotifier->setEnabled(false);
            }
        } else if (errno == EAGAIN && m_writeNotifier != nullptr) {
            m_writeNotifier->setEnabled(true);
        } else {
            transportLost();
        }
    }

    void transportLost()
    {
        setAvailable(false);
        cancelTransfer();
        if (m_readNotifier != nullptr) {
            m_readNotifier->setEnabled(false);
        }
        if (m_writeNotifier != nullptr) {
            m_writeNotifier->setEnabled(false);
        }
    }

    void setAvailable(bool available)
    {
        if (std::exchange(m_available, available) != available && m_observer != nullptr) {
            m_observer->captureAvailabilityChanged(available);
        }
    }

    void cleanupConnection()
    {
        if (m_registry != nullptr) {
            wl_registry_destroy(m_registry);
            m_registry = nullptr;
        }
        if (m_display != nullptr) {
            wl_display_disconnect(m_display);
            m_display = nullptr;
        }
        m_peerProcessId = 0;
    }

    CaptureObserver *m_observer = nullptr;
    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_seat *m_seat = nullptr;
    std::unique_ptr<DataControlManager> m_manager;
    std::unique_ptr<Device> m_device;
    std::vector<std::unique_ptr<Offer>> m_offers;
    std::unique_ptr<Offer> m_clipboardOffer;
    std::unique_ptr<Offer> m_primaryOffer;
    std::vector<QPointer<DataControlSource>> m_sources;
    std::unique_ptr<Transfer> m_transfer;
    std::unique_ptr<QSocketNotifier> m_readNotifier;
    std::unique_ptr<QSocketNotifier> m_writeNotifier;
    QByteArray m_partial;
    qsizetype m_totalTransferBytes = 0;
    quint32 m_managerGlobal = 0;
    quint32 m_seatGlobal = 0;
    qint64 m_peerProcessId = 0;
    bool m_running = false;
    bool m_available = false;
    bool m_captureEnabled = false;
};

Device::Device(::ext_data_control_device_v1 *object, QtClipboardAdapter *owner)
    : QtWayland::ext_data_control_device_v1(object), m_owner(owner) {}

Device::~Device()
{
    if (object() != nullptr) {
        destroy();
    }
}

void Device::ext_data_control_device_v1_data_offer(::ext_data_control_offer_v1 *offer)
{
    m_owner->introduceOffer(offer);
}

void Device::ext_data_control_device_v1_selection(::ext_data_control_offer_v1 *offer)
{
    m_owner->selectOffer(SelectionKind::Clipboard, offer);
}

void Device::ext_data_control_device_v1_primary_selection(::ext_data_control_offer_v1 *offer)
{
    m_owner->selectOffer(SelectionKind::Primary, offer);
}

void Device::ext_data_control_device_v1_finished()
{
    m_owner->deviceFinished();
}

} // namespace

std::unique_ptr<ClipboardWaylandAdapter> makeProductionClipboardWaylandAdapter()
{
    return std::make_unique<QtClipboardAdapter>();
}

} // namespace QindaQt::Services::ClipboardWayland
