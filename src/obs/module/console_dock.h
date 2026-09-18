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
    void setLevels(const QList<Audio::LevelReading> &levels);
    void setServiceState(const QString &state, const QString &reasonCode);

    [[nodiscard]] qsizetype rowCount() const { return m_rows.size(); }

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
