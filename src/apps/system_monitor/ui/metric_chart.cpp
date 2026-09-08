// SPDX-License-Identifier: GPL-3.0-or-later
#include "metric_chart.h"

#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace QindaQt::Apps::SystemMonitor {
namespace {

QString axisLabel(double value, const QString &unit) {
  if (unit != QLatin1String("B/s")) {
    return QString::number(value, 'f', value <= 100.0 ? 0 : 1) +
           QLatin1Char(' ') + unit;
  }
  static const QStringList units{QStringLiteral("B/s"),
                                 QStringLiteral("KiB/s"),
                                 QStringLiteral("MiB/s"),
                                 QStringLiteral("GiB/s")};
  int index = 0;
  while (value >= 1024.0 && index < units.size() - 1) {
    value /= 1024.0;
    ++index;
  }
  return QString::number(value, 'f', index == 0 ? 0 : 1) + QLatin1Char(' ') +
         units.at(index);
}

} // namespace

MetricChart::MetricChart(QString title, QString unit, QWidget *parent)
    : QWidget(parent), m_title(std::move(title)), m_unit(std::move(unit)) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void MetricChart::setSamples(QVector<QPointF> samples, QColor color,
                             double maximum) {
  m_samples = std::move(samples);
  m_color = color;
  m_maximum = maximum;
  update();
}

void MetricChart::setLegend(QString legend) {
  m_legend = std::move(legend);
  update();
}

QSize MetricChart::minimumSizeHint() const { return {220, 128}; }

void MetricChart::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QColor text = palette().color(QPalette::Text);
  painter.fillRect(rect(), palette().color(QPalette::Base));
  QFont heading = font();
  heading.setBold(true);
  painter.setFont(heading);
  painter.setPen(text);
  painter.drawText(QRect(12, 8, width() - 24, 22), Qt::AlignVCenter, m_title);
  painter.setFont(font());
  painter.setPen(text);
  painter.drawText(QRect(12, 30, width() - 24, 18), Qt::AlignVCenter, m_legend);

  const QRectF graph(12, 56, qMax(1, width() - 24), qMax(1, height() - 78));
  painter.setPen(QPen(palette().color(QPalette::Mid).lighter(130)));
  for (int row = 0; row < 4; ++row) {
    const qreal y = graph.top() + graph.height() * row / 3.0;
    painter.drawLine(QPointF(graph.left(), y), QPointF(graph.right(), y));
  }
  if (m_samples.size() < 2)
    return;

  double maximum = m_maximum;
  for (const auto &sample : m_samples) {
    if (std::isfinite(sample.y()))
      maximum = qMax(maximum, sample.y());
  }
  maximum = qMax(1.0, maximum);
  const double first = m_samples.first().x();
  const double last = qMax(m_samples.last().x(), first + 1.0);
  painter.setPen(text);
  painter.drawText(QRectF(graph.right() - 96, graph.top() - 18, 96, 16),
                   Qt::AlignRight, axisLabel(maximum, m_unit));
  painter.drawText(QRectF(graph.left(), graph.bottom() + 4, 130, 16),
                   Qt::AlignLeft,
                   tr("%1 s history").arg(qRound((last - first) / 1000.0)));

  QPainterPath path;
  bool connected = false;
  for (const auto &sample : m_samples) {
    if (!std::isfinite(sample.y())) {
      connected = false;
      continue;
    }
    const qreal x =
        graph.left() + graph.width() * (sample.x() - first) / (last - first);
    const qreal y = graph.bottom() -
                    graph.height() * qBound(0.0, sample.y() / maximum, 1.0);
    if (connected)
      path.lineTo(x, y);
    else
      path.moveTo(x, y);
    connected = true;
  }
  painter.setPen(QPen(
      m_color.isValid() ? m_color : palette().color(QPalette::Highlight), 2));
  painter.drawPath(path);
}

} // namespace QindaQt::Apps::SystemMonitor
