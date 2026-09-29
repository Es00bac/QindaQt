// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/application_window_management/types.h>
#include <QObject>
#include <QString>
#include <memory>
class QWindow;
namespace QindaQt::ApplicationWindowManagement {
// Optional GUI-thread client. Borrowed windows must belong to this Qt Wayland
// connection. Applications create/show their own windows; the compositor never
// creates content or handles Ctrl+T. Successful requests are asynchronous;
// failed requests leave the application window independent. Zero means not
// submitted. Destroying a borrowed window cancels its outstanding requests.
// Destroy this client before QGuiApplication. Unsupported platforms fail
// quietly; use available() to keep ordinary application behavior.
class WindowPlacementClient final : public QObject {
    Q_OBJECT
public:
    explicit WindowPlacementClient(QObject *parent=nullptr);
    ~WindowPlacementClient() override;
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] quint32 place(QWindow *source, QWindow *created, Placement placement, QString *error=nullptr);
    void cancel(quint32 requestId);
Q_SIGNALS:
    void availableChanged(bool available);
    void finished(quint32 requestId, QindaQt::ApplicationWindowManagement::Status status,
                  const QString &containerId, const QString &message);
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
}
Q_DECLARE_METATYPE(QindaQt::ApplicationWindowManagement::Status)
