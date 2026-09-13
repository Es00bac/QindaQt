// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <functional>

namespace QindaQt::Apps::SettingsAppearance {

using DecorationReconfigureRequest = std::function<bool(QString *error)>;

// KWin owns the active decoration plugin outside Settings1. This controller
// is the narrow platform adapter: it inventories native/Aurorae decorations,
// preserves unrelated kwinrc keys, and requests one live KWin reconfigure.
// QML receives only validated value rows and explicit select/apply actions.
class WindowDecorationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList decorations READ decorations CONSTANT)
    Q_PROPERTY(QString configuredId READ configuredId NOTIFY stateChanged)
    Q_PROPERTY(QString configuredName READ configuredName NOTIFY stateChanged)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY stateChanged)
    Q_PROPERTY(bool selectedUsesQindaQt READ selectedUsesQindaQt NOTIFY stateChanged)
    Q_PROPERTY(bool applyAvailable READ applyAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)

public:
    // AGENT-CONTRACT: configPath is one explicit kwinrc file; auroraeRoots
    // contain theme directories (not arbitrary scan roots). The callback is
    // invoked only after a successful QSettings sync and must report
    // whether KWin accepted the live-reconfigure request. Construction and
    // calls stay on the GUI thread.
    WindowDecorationController(QString configPath, QStringList auroraeRoots,
                               DecorationReconfigureRequest reconfigure,
                               QObject *parent = nullptr);

    [[nodiscard]] QVariantList decorations() const { return m_decorations; }
    [[nodiscard]] QString configuredId() const { return m_configuredId; }
    [[nodiscard]] QString configuredName() const;
    [[nodiscard]] QString selectedId() const { return m_selectedId; }
    [[nodiscard]] bool selectedUsesQindaQt() const;
    [[nodiscard]] bool applyAvailable() const;
    [[nodiscard]] QString statusText() const { return m_statusText; }
    [[nodiscard]] QString errorText() const { return m_errorText; }

    Q_INVOKABLE bool selectDecoration(const QString &id);
    Q_INVOKABLE bool applySelection();
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void stateChanged();

private:
    [[nodiscard]] QVariantMap entry(const QString &id) const;
    void loadCatalog(const QStringList &auroraeRoots);
    void readConfigured();
    void setError(QString error);

    QString m_configPath;
    DecorationReconfigureRequest m_reconfigure;
    QVariantList m_decorations;
    QString m_configuredId;
    QString m_selectedId;
    QString m_statusText;
    QString m_errorText;
};

[[nodiscard]] QString windowDecorationConfigPath();
[[nodiscard]] QStringList windowDecorationThemeRoots();
[[nodiscard]] bool requestKWinDecorationReconfigure(QString *error);

} // namespace QindaQt::Apps::SettingsAppearance
