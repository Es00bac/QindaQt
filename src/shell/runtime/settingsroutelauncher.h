// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

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
    // Opens the Settings app's Customize (panel editing) route.
    Q_INVOKABLE bool openCustomize();
    // AGENT-CONTRACT: `destination` names a tab within the page and
    // `selection` an item to select there; both are optional and both are
    // passed through verbatim as `--destination` / `--select`. The Settings
    // process treats an unknown value as "open the page anyway", so a stale
    // id never costs the user the route.
    Q_INVOKABLE bool openRoute(const QString &page,
                               const QString &destination = {},
                               const QString &selection = {});
Q_SIGNALS:
    void errorTextChanged();
private:
    Launch m_launch;
    std::unique_ptr<QProcess> m_containedProcess;
    QString m_containedPage;
    QStringList m_containedArguments;
    QString m_error;
};

} // namespace QindaQt::Shell
