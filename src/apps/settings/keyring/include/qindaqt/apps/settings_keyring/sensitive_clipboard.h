// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QMimeData>
#include <qindaqt/services/keyring/secure_buffer.h>
#include <memory>
namespace QindaQt::Apps::SettingsKeyring {
// GUI-thread clipboard owner. Construct only with a QGuiApplication.
// copy retains explicit shared byte ownership; caller must not reuse/clear it.
// Provider expires/wipes after30s or clear/destruction. It clears the platform
// selection ONLY when still its own MIME object, preserving replacements.
// Unavoidable platform/requester plaintext copies are not universally wiped.
class SensitiveClipboard final : public QObject {
    Q_OBJECT
public:
    explicit SensitiveClipboard(QObject *parent=nullptr,int lifetimeMilliseconds=30000);
    ~SensitiveClipboard() override;
    bool copy(std::shared_ptr<qindaqt::keyring::SecureBuffer> bytes);
    void clear();
private:
    QPointer<QMimeData> m_owned;
    QTimer m_timeout;
};
}
