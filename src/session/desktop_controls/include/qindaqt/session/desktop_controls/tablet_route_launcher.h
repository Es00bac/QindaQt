// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace QindaQt::Session::DesktopControls {

// Opens Settings at Input → Pen & tablet with one device group selected.
//
// AGENT-CONTRACT: The argument vector is the deep-link contract with
// qindaqt-settings: `--page input --destination tablet --select <group>`.
// The Settings process rejects an unknown page, destination or selection
// without opening a wrong route, so a stale group id is a no-op there, never
// a surprise page here.
class TabletRouteLauncher final : public QObject {
    Q_OBJECT

public:
    // The launch function exists so a row can assert the exact argument
    // vector without starting a process.
    using Launch = std::function<bool(const QString &program,
                                      const QStringList &arguments)>;

    explicit TabletRouteLauncher(Launch launch = {}, QObject *parent = nullptr);
    ~TabletRouteLauncher() override;

    TabletRouteLauncher(const TabletRouteLauncher &) = delete;
    TabletRouteLauncher &operator=(const TabletRouteLauncher &) = delete;

    void openTabletSettings(const QString &deviceGroupId);

    [[nodiscard]] static QString settingsProgram();
    [[nodiscard]] static QStringList
    argumentsFor(const QString &deviceGroupId);

Q_SIGNALS:
    void launchFailed(const QString &reasonCode);

private:
    Launch m_launch;
};

} // namespace QindaQt::Session::DesktopControls
