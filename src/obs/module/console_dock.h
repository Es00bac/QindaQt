// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/obs_bridge/console_sources.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QVBoxLayout;

namespace QindaQt::ObsBridge {

class LevelBar;

// The "QindaQt Console" dock (ADR-0208): one row per bus and strip with the
// console's own meter reading (dBFS peak from Audio1, not OBS's mixer), so a
// streamer sees at a glance which console endpoint carries signal and which
// OBS source it became. Presentation only: it never issues an operation.
class ConsoleDock final : public QWidget {
    Q_OBJECT

public:
    explicit ConsoleDock(QWidget *parent = nullptr);

    void setSources(const QList<DesiredSource> &sources);
    // An empty batch is the service's "metering stopped" (and what the client
    // sends when the last meter lease is released): every bar returns to the
    // grey of "nothing heard", it does not freeze on its last reading.
    void setLevels(const QList<Audio::LevelReading> &levels);
    void setServiceState(const QString &state, const QString &reasonCode);

    [[nodiscard]] qsizetype rowCount() const { return m_rows.size(); }

Q_SIGNALS:
    // The dock's meters are visible (true) or not (false). The bridge holds an
    // Audio1 meter lease exactly while this is true (ADR-0394): a dock that is
    // closed, tabbed behind another or on a hidden window shows no meter, so
    // nothing should be decoded for it.
    void meterVisibilityChanged(bool visible);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    struct Row {
        QWidget *widget = nullptr;
        QLabel *name = nullptr;
        LevelBar *bar = nullptr;
    };

    QLabel *m_status = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    QHash<QString, Row> m_rows;
    QStringList m_order;
};

} // namespace QindaQt::ObsBridge
