// SPDX-License-Identifier: LGPL-3.0-or-later
#include "pkexec_sddm_config_writer.h"

namespace QindaQt::Apps::SettingsLoginScreen {

PkexecSddmConfigWriter::PkexecSddmConfigWriter(QString pkexecPath,
                                               QString helperPath,
                                               QObject *parent)
    : SddmConfigWriteClient(parent),
      m_pkexecPath(std::move(pkexecPath)),
      m_helperPath(std::move(helperPath)) {}

PkexecSddmConfigWriter::~PkexecSddmConfigWriter() {
  if (m_process != nullptr) {
    // Destruction mid-write (window closed during the prompt): let the
    // helper finish or die with its session; the file is written
    // atomically, so a half-delivered payload cannot corrupt it.
    m_process->kill();
    m_process->waitForFinished(1000);
  }
}

void PkexecSddmConfigWriter::write(const SddmOwnedChangeSet &changes) {
  if (m_process != nullptr) {
    // AGENT-GUARD: the model serializes writes; a second concurrent
    // pkexec would race the same drop-in file.
    Q_EMIT writeFinished(false, QStringLiteral("Another change is still "
                                               "being saved."));
    return;
  }
  const QString payload = serializeOwnedChangeSet(changes);
  if (payload.isEmpty()) {
    Q_EMIT writeFinished(false, QStringLiteral("Nothing to save."));
    return;
  }
  m_process = new QProcess(this);
  connect(m_process, &QProcess::finished, this,
          &PkexecSddmConfigWriter::onFinished);
  m_process->start(m_pkexecPath, {m_helperPath});
  m_process->write(payload.toUtf8());
  m_process->closeWriteChannel();
}

void PkexecSddmConfigWriter::onFinished(const int exitCode,
                                        const QProcess::ExitStatus status) {
  QString error;
  const QString stderrText =
      QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
  if (status != QProcess::NormalExit) {
    error = QStringLiteral("The configuration helper did not finish.");
  } else if (exitCode == 126 || exitCode == 127) {
    // pkexec itself: 126 = authorization refused/dismissed, 127 = the
    // action or helper could not be launched at all.
    error = exitCode == 126
        ? QStringLiteral("Authentication was refused or cancelled — the "
                         "change was not saved.")
        : QStringLiteral("The login screen helper could not be started "
                         "(is the QindaQt polkit policy installed?)");
  } else if (exitCode != 0) {
    error = stderrText.isEmpty()
        ? QStringLiteral("The configuration helper rejected the change "
                         "(code %1).").arg(exitCode)
        : stderrText;
  }
  m_process->deleteLater();
  m_process = nullptr;
  Q_EMIT writeFinished(error.isEmpty(), error);
}

} // namespace QindaQt::Apps::SettingsLoginScreen
