// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QHash>
#include <functional>
#include <qindaqt/window_management/command.h>
namespace QindaQt::WindowManagement {
struct ContextSnapshot final {
  QString windowId;
  QString containerId;
  quint64 topologyRevision = 0;
  QByteArray sceneFingerprint = {};
};
struct ResolvedTarget final {
  QString windowId;
  QString containerId;
  // The hotkey subject for a secondary Current destination, independent of
  // a named primary target. Filled by Controller, not the transport caller.
  QString contextWindowId = {};
  QString contextContainerId = {};
};
struct Resolution final {
  Status status = Status::Unavailable;
  ResolvedTarget target;
  QString message;
  QStringList candidates;
};
class Authority {
public:
  virtual ~Authority() = default;
  // Live unique bus owner + confirmed opt-in. Never trust a supplied PID.
  [[nodiscard]] virtual bool authorized(const QString &uniqueOwner) const = 0;
  [[nodiscard]] virtual bool locked() const = 0;
};
class Scene {
public:
  virtual ~Scene() = default;
  [[nodiscard]] virtual std::optional<ContextSnapshot> capture() const = 0;
  [[nodiscard]] virtual bool current(const ContextSnapshot &snapshot) const = 0;
  [[nodiscard]] virtual Resolution
  resolve(const Target &target, const ContextSnapshot &snapshot) const = 0;
  [[nodiscard]] virtual QStringList capabilities() const = 0;
};
class Executor {
public:
  virtual ~Executor() = default;
  // Synchronous admission/dispatch on the scene thread. Accepted means the
  // mutation committed; Dispatched means a launch is pending, not placement.
  [[nodiscard]] virtual Result execute(const Command &command,
                                       const ResolvedTarget &target) = 0;
};
using Clock = std::function<qint64()>;
using Nonce = std::function<QString()>;
// Owns bounded, expiring, single-use command contexts. Dependencies are
// borrowed and must outlive it; every method runs on their common owning
// thread. The authority is checked before parsing or target lookup. No
// operation is queued or replayed by this controller. Consumers invalidate on
// owner/opt-in/lock transitions so disabling and re-enabling cannot revive an
// earlier capture.
class Controller final {
public:
  Controller(Authority &authority, Scene &scene, Executor &executor,
             Clock clock = {}, Nonce nonce = {});
  [[nodiscard]] Result begin(const QString &uniqueOwner);
  [[nodiscard]] Result submit(const QString &uniqueOwner,
                              const QString &contextId, const QByteArray &wire);
  [[nodiscard]] Result cancel(const QString &uniqueOwner,
                              const QString &contextId);
  void invalidate() noexcept;
  [[nodiscard]] QStringList capabilities() const;

private:
  struct Context final {
    QString owner;
    ContextSnapshot snapshot;
    qint64 expires = 0;
  };
  struct Rate final {
    qint64 start = 0;
    qsizetype count = 0;
  };
  void expire(qint64 now);
  [[nodiscard]] bool admit(const QString &owner, qint64 now);
  Authority &m_authority;
  Scene &m_scene;
  Executor &m_executor;
  Clock m_clock;
  Nonce m_nonce;
  QHash<QString, Context> m_contexts;
  QHash<QString, Rate> m_rates;
};
} // namespace QindaQt::WindowManagement
