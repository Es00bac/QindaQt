// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_printing/printing_tool_ports.h>
#include <QObject>
#include <QVariantList>

namespace QindaQt::Apps::SettingsPrinting {

// Same-thread route model; borrowed catalog/starter outlive it. Construction and
// refresh inspect metadata only. QML receives three closed presentation rows,
// never paths/argv/URLs. launchTool rejects unknown ids and re-resolves the
// installed plan. Busy rejects reentrant dispatch; no retry replays a launch.
// This process-local API has no persistence or stable binary ABI.
class PrintingSettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList tools READ tools NOTIFY toolsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
public:
    PrintingSettingsModel(PrintingToolCatalog &catalog, PrintingArgvStarter &starter,
                          QObject *parent = nullptr);
    [[nodiscard]] QVariantList tools() const;
    [[nodiscard]] bool busy() const noexcept { return m_busy; }
    [[nodiscard]] QString statusText() const { return m_statusText; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void launchTool(const QString &id);

Q_SIGNALS:
    void toolsChanged();
    void busyChanged();
    void statusTextChanged();

private:
    void setBusy(bool busy);
    void setStatus(const QString &status);
    PrintingToolCatalog &m_catalog;
    PrintingArgvStarter &m_starter;
    PrintingToolInventory m_tools;
    bool m_busy = false;
    QString m_statusText;
};

} // namespace QindaQt::Apps::SettingsPrinting
