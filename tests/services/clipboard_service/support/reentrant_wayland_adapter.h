// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/clipboard_wayland_adapter/clipboard_wayland_adapter.h>

#include <functional>
#include <memory>
#include <utility>

class ReentrantWaylandAdapter final
    : public QindaQt::Services::ClipboardWayland::ClipboardWaylandAdapter {
public:
    using Value = QindaQt::Services::ClipboardModel::ClipboardValue;
    using Kind = QindaQt::Services::ClipboardWayland::SelectionKind;
    void setObserver(QindaQt::Services::ClipboardWayland::CaptureObserver *next) override
    { observer = next; }
    QindaQt::Services::ClipboardWayland::StartStatus start() override
    { return QindaQt::Services::ClipboardWayland::StartStatus::Started; }
    void stop() override { setCaptureEnabled(false); available = false; }
    void setCaptureEnabled(bool enabled) override {
        captureEnabled = enabled;
        if (!enabled) transfer.reset();
        if (onCapture) { auto once = std::exchange(onCapture, {}); once(enabled); }
    }
    bool isAvailable() const noexcept override {
        if (onAvailable) { auto once = std::exchange(onAvailable, {}); once(); }
        return available;
    }
    qint64 peerProcessId() const noexcept override { return 42; }
    bool publishSelection(Kind, const Value &value) override {
        ++publications;
        published = value;
        if (onPublish) { auto once = std::exchange(onPublish, {}); once(); }
        return true;
    }
    void offer(const Value &value) {
        if (captureEnabled && observer) observer->captured(Kind::Clipboard, value);
    }
    void offerAliased(const Value &value) {
        transfer = std::make_unique<Value>(value);
        if (captureEnabled && observer) observer->captured(Kind::Clipboard, *transfer);
        // A denied capture legitimately destroys transfer inside the callback.
        // Do not retain or inspect its borrowed value after that call.
    }

    QindaQt::Services::ClipboardWayland::CaptureObserver *observer = nullptr;
    bool captureEnabled = false, available = true;
    int publications = 0;
    Value published;
    std::unique_ptr<Value> transfer;
    mutable std::function<void()> onAvailable;
    std::function<void(bool)> onCapture;
    std::function<void()> onPublish;
};
