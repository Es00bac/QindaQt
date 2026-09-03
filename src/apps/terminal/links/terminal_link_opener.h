// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "links/terminal_link.h"

#include <QStringList>

class QWidget;

namespace QindaQt::Apps::Terminal {

class TerminalLinkSpawner {
public:
  virtual ~TerminalLinkSpawner() = default;
  [[nodiscard]] virtual bool spawn(const QString &absoluteProgram,
                                   const QStringList &arguments,
                                   QString *diagnostic) = 0;
};

class TerminalLinkConfirmation {
public:
  virtual ~TerminalLinkConfirmation() = default;
  [[nodiscard]] virtual bool confirm(const TerminalLink &link,
                                     QWidget *parent) = 0;
};

struct TerminalLinkOpenResult final {
  bool started = false;
  bool cancelled = false;
  QString diagnostic;
};

// Owns confirmation and argv construction only. Collaborators are injected,
// borrowed, and must outlive this GUI-thread object. No shell string exists.
class TerminalLinkOpener final {
public:
  TerminalLinkOpener(QString absoluteProgram, TerminalLinkSpawner *spawner,
                     TerminalLinkConfirmation *confirmation);

  [[nodiscard]] TerminalLinkOpenResult open(const TerminalLink &link,
                                            QWidget *parent) const;
  [[nodiscard]] static QString resolveXdgOpen();

private:
  QString m_absoluteProgram;
  TerminalLinkSpawner *m_spawner = nullptr;
  TerminalLinkConfirmation *m_confirmation = nullptr;
};

class DetachedTerminalLinkSpawner final : public TerminalLinkSpawner {
public:
  [[nodiscard]] bool spawn(const QString &absoluteProgram,
                           const QStringList &arguments,
                           QString *diagnostic) override;
};

class MessageBoxTerminalLinkConfirmation final
    : public TerminalLinkConfirmation {
public:
  [[nodiscard]] bool confirm(const TerminalLink &link,
                             QWidget *parent) override;
};

} // namespace QindaQt::Apps::Terminal
