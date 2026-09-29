// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/clipboard_wayland_adapter/clipboard_wayland_adapter.h>

#include <QImage>
#include <QObject>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>

namespace QindaQt::Screenshot {

// Puts a capture (or a recording's path) on the clipboard.
//
// AGENT-NOTE (ADR-0289): KWin only accepts a wl_data_device selection from
// the client with keyboard focus, and a windowless launch or a notification
// button has none. The publisher therefore offers through ext-data-control
// using the desktop's existing adapter (the one the clipboard service uses)
// and falls back to QClipboard only when data control is unavailable.
//
// AGENT-CONTRACT: on Wayland the owner of a selection must keep running to
// answer pastes. `holdsSelection()` stays true until another client replaces
// the selection; the application uses it to delay exit.
class ClipboardPublisher final : public QObject,
                                 private Services::ClipboardWayland::CaptureObserver {
    Q_OBJECT

public:
    using AdapterFactory =
        std::function<std::unique_ptr<Services::ClipboardWayland::ClipboardWaylandAdapter>()>;

    // `factory` may return null (tests, non-Wayland sessions): QClipboard is
    // then the only route.
    explicit ClipboardPublisher(AdapterFactory factory, QObject *parent = nullptr);
    ~ClipboardPublisher() override;

    void copyImage(const QImage &image);
    void copyText(const QString &text);

    [[nodiscard]] bool holdsSelection() const { return m_holding; }

Q_SIGNALS:
    void copied(bool viaDataControl);
    void failed(const QString &message);
    void holdsSelectionChanged();

private:
    void captureAvailabilityChanged(bool available) override;
    void captured(Services::ClipboardWayland::SelectionKind,
                  const Services::ClipboardModel::ClipboardValue &) override {}
    void captureRefused(Services::ClipboardWayland::SelectionKind,
                        Services::ClipboardModel::ClipboardError) override {}

    void publish(Services::ClipboardModel::ClipboardValue value);
    void publishPending();
    void fallBack();
    void setHolding(bool holding);

    AdapterFactory m_factory;
    std::unique_ptr<Services::ClipboardWayland::ClipboardWaylandAdapter> m_adapter;
    std::optional<Services::ClipboardModel::ClipboardValue> m_pending;
    QTimer m_availabilityDeadline;
    QTimer m_leaseWatch;
    bool m_holding = false;
    bool m_adapterFailed = false;
};

} // namespace QindaQt::Screenshot
