// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_client/display_transport.h>

#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::DisplayClient {

class QtDisplayTransport final : public DisplayTransport {
  Q_OBJECT

public:
  // Retains a value handle to connection; the named Qt connection must stay
  // registered for this object's lifetime. Use this object on its creating
  // Qt thread only. D-Bus errors are normalized to bounded reason codes.
  // stop() suppresses late watcher completions; destroy Clients before this
  // transport so its final owner retirement cannot reach a dead borrower.
  explicit QtDisplayTransport(const QDBusConnection &connection,
                              QString serviceName = {},
                              QObject *parent = nullptr);
  ~QtDisplayTransport() override;

  void start() override;
  void stop() override;
  void requestActivation() override;
  void fetchSnapshot(const QString &owner, quint64 requestId) override;
  void submitStage(const QString &owner, quint64 requestId,
                   const QString &transactionId,
                   const Display::Candidate &candidate) override;
  void submitPreview(const QString &owner, quint64 requestId,
                     const QString &transactionId) override;
  void submitConfirm(const QString &owner, quint64 requestId,
                     const QString &transactionId) override;
  void submitCancel(const QString &owner, quint64 requestId,
                    const QString &transactionId) override;

private Q_SLOTS:
  void onServiceOwnerChanged(const QString &service, const QString &oldOwner,
                             const QString &newOwner);
  void onChanged(const QString &epoch, quint64 revision, bool available);
  void onBusDisconnected();

private:
  void setOwner(const QString &owner);
  void queryInitialOwner();
  void submitMethod(const QString &owner, quint64 requestId,
                    const QString &method, const QVariantList &arguments);

  class Private;
  std::unique_ptr<Private> d;
};

} // namespace QindaQt::DisplayClient
