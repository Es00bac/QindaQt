// SPDX-License-Identifier: GPL-3.0-or-later
#include "console_dock.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>

namespace QindaQt::ObsBridge {

// A peak meter in dBFS from -60 to 0; grey until the console has heard audio.
class LevelBar final : public QWidget {
public:
    explicit LevelBar(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(10);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setLevel(const Audio::Level &level)
    {
        m_level = level;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        const QRectF bounds = rect();
        painter.fillRect(bounds, QColor(40, 40, 40));
        if (!m_level.known) {
            return;
        }
        const double fraction = std::clamp((m_level.peakDb + 60.0) / 60.0, 0.0, 1.0);
        QRectF fill = bounds;
        fill.setWidth(bounds.width() * fraction);
        painter.fillRect(fill, m_level.peakDb > -6.0 ? QColor(220, 80, 60)
                             : m_level.peakDb > -18.0 ? QColor(230, 190, 60)
                                                      : QColor(80, 190, 110));
    }

private:
    Audio::Level m_level;
};

ConsoleDock::ConsoleDock(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    m_status = new QLabel(tr("Audio1: starting"), this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_rowsLayout = new QVBoxLayout;
    layout->addLayout(m_rowsLayout);
    layout->addStretch(1);
}

void ConsoleDock::setSources(const QList<DesiredSource> &sources)
{
    QStringList order;
    for (const DesiredSource &source : sources) {
        order.append(source.consoleId);
        Row &row = m_rows[source.consoleId];
        if (row.widget == nullptr) {
            row.widget = new QWidget(this);
            auto *rowLayout = new QHBoxLayout(row.widget);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            row.name = new QLabel(row.widget);
            row.bar = new LevelBar(row.widget);
            rowLayout->addWidget(row.name, 1);
            rowLayout->addWidget(row.bar, 1);
            m_rowsLayout->addWidget(row.widget);
        }
        row.name->setText(source.sourceName);
        row.name->setToolTip(source.captureDevice.isEmpty()
                                 ? tr("No device yet")
                                 : tr("Captures %1").arg(source.captureDevice));
    }
    // Rows the console dropped go away; the rest keep their meters.
    for (auto it = m_rows.begin(); it != m_rows.end();) {
        if (!order.contains(it.key())) {
            m_rowsLayout->removeWidget(it.value().widget);
            it.value().widget->deleteLater();
            it = m_rows.erase(it);
        } else {
            ++it;
        }
    }
    m_order = order;
}

void ConsoleDock::setLevels(const QList<Audio::LevelReading> &levels)
{
    for (const Audio::LevelReading &reading : levels) {
        const auto row = m_rows.constFind(reading.id);
        if (row != m_rows.cend() && row->bar != nullptr) {
            row->bar->setLevel(reading.level);
        }
    }
}

void ConsoleDock::setServiceState(const QString &state, const QString &reasonCode)
{
    m_status->setText(reasonCode.isEmpty() ? tr("Audio1: %1").arg(state)
                                           : tr("Audio1: %1 (%2)").arg(state, reasonCode));
}

} // namespace QindaQt::ObsBridge
