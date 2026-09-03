// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_wayland_adapter/clipboard_wayland_adapter.h>

class FakeWaylandAdapter final
    : public QindaQt::Services::ClipboardWayland::ClipboardWaylandAdapter {
public:
    void setObserver(QindaQt::Services::ClipboardWayland::CaptureObserver *value) override
    { observer = value; }
    QindaQt::Services::ClipboardWayland::StartStatus start() override
    { available = true; return QindaQt::Services::ClipboardWayland::StartStatus::Started; }
    void stop() override { available = false; captureEnabled = false; }
    void setCaptureEnabled(bool value) override { captureEnabled = value; }
    bool isAvailable() const noexcept override { return available; }
    qint64 peerProcessId() const noexcept override { return 42; }
    bool publishSelection(QindaQt::Services::ClipboardWayland::SelectionKind kind,
                          const QindaQt::Services::ClipboardModel::ClipboardValue &value) override
    { lastPublishedKind = kind; lastPublished = value; return publishSucceeds; }
    void offer(const QindaQt::Services::ClipboardModel::ClipboardValue &value)
    { if (captureEnabled && observer != nullptr) observer->captured(
          QindaQt::Services::ClipboardWayland::SelectionKind::Clipboard, value); }

    QindaQt::Services::ClipboardWayland::CaptureObserver *observer = nullptr;
    QindaQt::Services::ClipboardModel::ClipboardValue lastPublished;
    QindaQt::Services::ClipboardWayland::SelectionKind lastPublishedKind =
        QindaQt::Services::ClipboardWayland::SelectionKind::Clipboard;
    bool available = true;
    bool captureEnabled = false;
    bool publishSucceeds = true;
};
