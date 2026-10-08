// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/voice_configuration/voice_configuration_client.h>
#include <QtCore/QObject>
namespace QindaQt::Apps::SettingsVoice {
// Borrows a same-thread public client. No credential draft/property persists
// in this projection; QML clears its masked editor on every submission/refusal.
class VoiceCredentialsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString effectiveText READ effectiveText NOTIFY changed)
public:
    explicit VoiceCredentialsModel(Services::VoiceConfiguration::Client &client,
                                   QObject *parent = nullptr);
    bool available() const;
    bool ready() const { return m_client.ready(); }
    bool busy() const;
    QString statusText() const;
    QString effectiveText() const;
    Q_INVOKABLE bool save(const QString &key);
    Q_INVOKABLE bool reload();
Q_SIGNALS:
    void changed();
    void clearEntry();
private:
    Services::VoiceConfiguration::Client &m_client;
};
}
