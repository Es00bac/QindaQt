// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QHash>
#include <QObject>
#include <QRect>
#include <QSet>
#include <QString>
namespace QindaQt::Compositor::KWinIntegration {
class ManagedWindowRegistry;
// Owns ordinary-window fractional maximize/restore independently of grouped
// placement. Registry is borrowed; calls and signals run on the GUI thread.
class KWinSemanticWindowPlacement final : public QObject {
public:
  explicit KWinSemanticWindowPlacement(ManagedWindowRegistry &registry,
                                       QObject *parent = nullptr);
  [[nodiscard]] bool maximize(const QString &windowId, double fraction,
                              QString *error);
  [[nodiscard]] bool place(const QString &windowId, const QRect &frame,
                           QString *error);
  [[nodiscard]] bool restore(const QString &windowId, QString *error);
  [[nodiscard]] bool isMaximized(const QString &windowId) const;

private:
  struct State {
    QRectF restore;
    QRect target;
    double fraction = 0.9;
  };
  void refresh();
  void observe(const QString &windowId);
  ManagedWindowRegistry &m_registry;
  QHash<QString, State> m_states;
  QSet<QString> m_observedIds;
  bool m_applying = false;
};
} // namespace QindaQt::Compositor::KWinIntegration
