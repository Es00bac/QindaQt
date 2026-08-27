// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace QindaQt::Shell {

class SettingsRouteLauncher final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
public:
    using Launch = std::function<bool(QString *)>;
    explicit SettingsRouteLauncher(Launch launch = {}, QObject *parent = nullptr);
    [[nodiscard]] const QString &errorText() const noexcept { return m_error; }
    Q_INVOKABLE bool openNotifications();
Q_SIGNALS:
    void errorTextChanged();
private:
    Launch m_launch;
    QString m_error;
};

} // namespace QindaQt::Shell
