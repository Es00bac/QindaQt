// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include <functional>
#include <memory>

class QProcess;

namespace QindaQt::Shell {

class SettingsRouteLauncher final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
public:
    using Launch = std::function<bool(QString *)>;
    explicit SettingsRouteLauncher(Launch launch = {}, QObject *parent = nullptr);
    ~SettingsRouteLauncher() override;
    [[nodiscard]] const QString &errorText() const noexcept { return m_error; }
    Q_INVOKABLE bool openNotifications();
Q_SIGNALS:
    void errorTextChanged();
private:
    Launch m_launch;
    std::unique_ptr<QProcess> m_containedProcess;
    QString m_error;
};

} // namespace QindaQt::Shell
