// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <memory>
class QWindow;
namespace QindaQt::Platform::ForeignParent {
// GUI-thread xdg-foreign-v2 import on the caller's actual Qt Wayland display.
// Borrowed window and QGuiApplication outlive this adapter. Exactly one parent
// is supported; replacement clears the old proxy. Empty handle is unparented;
// an unsupported/missing/destroyed nonempty parent emits lost, never silently
// degrades to an unparented consent. This proves parent relation, not identity
// or permission. Vendor/QPA headers remain private (ADR0318, Qt6.11 baseline).
class ForeignParent final : public QObject {
    Q_OBJECT
public:
    explicit ForeignParent(QObject *parent = nullptr);
    ~ForeignParent() override;
    void attach(QWindow &window, const QString &portalHandle);
    void clear();
Q_SIGNALS:
    void ready();
    void lost();
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Platform::ForeignParent
