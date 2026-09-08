// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QPointF>
#include <QVector>
#include <QWidget>

namespace QindaQt::Apps::SystemMonitor {

// Samples use UTC milliseconds on x. NaN y values intentionally make a gap:
// paused or unavailable readings must not be rendered as a fabricated zero.
class MetricChart final : public QWidget {
  Q_OBJECT
public:
  explicit MetricChart(QString title, QString unit, QWidget *parent = nullptr);
  void setSamples(QVector<QPointF> samples, QColor color, double maximum = 0.0);
  void setLegend(QString legend);

protected:
  void paintEvent(QPaintEvent *event) override;
  QSize minimumSizeHint() const override;

private:
  QString m_title, m_unit, m_legend;
  QVector<QPointF> m_samples;
  QColor m_color;
  double m_maximum = 0.0;
};
} // namespace QindaQt::Apps::SystemMonitor
