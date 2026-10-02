// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <optional>
#include <qindaqt/services/shortcuts_client/transport.h>
namespace QindaQt::Services::Shortcuts {
namespace {
QStringList texts(const QList<QKeySequence> &keys) {
  QStringList result;
  for (const auto &key : keys)
    result.append(key.toString(QKeySequence::PortableText));
  return result;
}
std::optional<QList<QKeySequence>> sequences(const QVariant &wire) {
  QList<QKeySequence> result;
  const auto values = qdbus_cast<QStringList>(wire);
  if (values.size() > 4)
    return {};
  for (const auto &text : values) {
    QKeySequence key(text, QKeySequence::PortableText);
    if (key.isEmpty() || key.toString(QKeySequence::PortableText) != text)
      return {};
    result.append(key);
  }
  return result;
}
QDBusMessage call(const QDBusConnection &bus, const QString &owner,
                  const QString &member, QVariantList args, QString *error) {
  if (owner.isEmpty()) {
    if (error)
      *error = QStringLiteral("Native shortcut authority is unavailable");
    return {};
  }
  auto message = QDBusMessage::createMethodCall(owner, QLatin1String(Path),
                                                QLatin1String(Service), member);
  message.setArguments(args);
  const auto reply = bus.call(message, QDBus::Block, 4000);
  if (reply.type() != QDBusMessage::ReplyMessage && error)
    *error = reply.errorMessage();
  return reply;
}
bool accepted(const QDBusMessage &reply, QString *error, bool conflict) {
  if (reply.type() != QDBusMessage::ReplyMessage ||
      reply.arguments().size() != (conflict ? 2 : 1) ||
      reply.arguments().first().metaType() != QMetaType::fromType<bool>()) {
    if (error && error->isEmpty())
      *error = QStringLiteral("Native shortcut reply is malformed");
    return false;
  }
  const bool ok = reply.arguments().first().toBool();
  if (!ok && error)
    *error =
        conflict && !reply.arguments().at(1).toString().isEmpty()
            ? reply.arguments().at(1).toString()
            : QStringLiteral("Native shortcut authority refused this change");
  return ok;
}
} // namespace
QtShortcutTransport::QtShortcutTransport(QDBusConnection bus, QObject *parent)
    : QObject(parent), m_bus(bus),
      m_watch(QLatin1String(Service), bus,
              QDBusServiceWatcher::WatchForOwnerChange, this) {
  connect(&m_watch, &QDBusServiceWatcher::serviceOwnerChanged, this,
          &QtShortcutTransport::ownerChanged);
  if (bus.interface()) {
    const QDBusReply<QString> owner =
        bus.interface()->serviceOwner(QLatin1String(Service));
    if (owner.isValid())
      attach(owner.value());
  }
}
bool QtShortcutTransport::available() const { return !m_owner.isEmpty(); }
void QtShortcutTransport::attach(const QString &owner) {
  if (!m_owner.isEmpty()) {
    m_bus.disconnect(m_owner, QLatin1String(Path), QLatin1String(Service),
                     "Activated", this,
                     SIGNAL(activated(QString, QString, quint64)));
    m_bus.disconnect(m_owner, QLatin1String(Path), QLatin1String(Service),
                     "Deactivated", this,
                     SIGNAL(deactivated(QString, QString, quint64)));
    m_bus.disconnect(m_owner, QLatin1String(Path), QLatin1String(Service),
                     "BindingsChanged", this, SIGNAL(bindingsChanged()));
  }
  m_owner = owner;
  if (m_owner.isEmpty())
    return;
  m_bus.connect(m_owner, QLatin1String(Path), QLatin1String(Service),
                "Activated", this,
                SIGNAL(activated(QString, QString, quint64)));
  m_bus.connect(m_owner, QLatin1String(Path), QLatin1String(Service),
                "Deactivated", this,
                SIGNAL(deactivated(QString, QString, quint64)));
  m_bus.connect(m_owner, QLatin1String(Path), QLatin1String(Service),
                "BindingsChanged", this, SIGNAL(bindingsChanged()));
}
void QtShortcutTransport::ownerChanged(const QString &, const QString &old,
                                       const QString &owner) {
  attach(owner);
  if (!old.isEmpty())
    Q_EMIT authorityLost();
}
QList<Binding> QtShortcutTransport::bindings(QString *error) const {
  const auto reply =
      call(m_bus, m_owner, QStringLiteral("ListBindings"), {}, error);
  QList<Binding> result;
  if (reply.type() != QDBusMessage::ReplyMessage ||
      reply.arguments().size() != 1 ||
      reply.signature() != QLatin1String("a{sv}")) {
    if (error && error->isEmpty())
      *error =
          QStringLiteral("Native shortcut reply has an unexpected signature");
    return result;
  }
  const auto rows = qdbus_cast<QVariantMap>(reply.arguments().first());
  if (rows.size() > 4096) {
    if (error)
      *error = QStringLiteral("Too many native shortcut bindings");
    return {};
  }
  for (auto it = rows.cbegin(); it != rows.cend(); ++it) {
    const auto row = qdbus_cast<QVariantMap>(it.value());
    const auto keys = sequences(row.value("keys")),
               defaults = sequences(row.value("defaults"));
    const auto component = row.value("component").toString(),
               action = row.value("action").toString();
    if (!keys || !defaults || component.isEmpty() || action.isEmpty() ||
        it.key() != component + QChar(0x1f) + action) {
      if (error)
        *error = QStringLiteral("Invalid native shortcut binding");
      return {};
    }
    result.append({component, action, row.value("description").toString(),
                   row.value("componentLabel").toString(), *keys, *defaults,
                   row.value("active").toBool(), row.value("repeat").toBool()});
  }
  return result;
}
bool QtShortcutTransport::registerBinding(const Binding &binding,
                                          bool transient,
                                          QString *error) const {
  return accepted(call(m_bus, m_owner, QStringLiteral("Register"),
                       {binding.component, binding.action, binding.description,
                        texts(binding.keys), binding.repeat, transient},
                       error),
                  error, true);
}
bool QtShortcutTransport::setShortcuts(const QString &component,
                                       const QString &action,
                                       const QList<QKeySequence> &keys,
                                       QString *error) const {
  return accepted(call(m_bus, m_owner, QStringLiteral("SetShortcuts"),
                       {component, action, texts(keys)}, error),
                  error, true);
}
QStringList QtShortcutTransport::conflicts(const QList<QKeySequence> &keys,
                                           const QString &component,
                                           const QString &action,
                                           QString *error) const {
  const auto reply = call(m_bus, m_owner, QStringLiteral("Conflicts"),
                          {texts(keys), component, action}, error);
  if (reply.type() != QDBusMessage::ReplyMessage ||
      reply.signature() != QLatin1String("as"))
    return {QStringLiteral("authority-unavailable")};
  return qdbus_cast<QStringList>(reply.arguments().first());
}
bool QtShortcutTransport::unregisterBinding(const QString &component,
                                            const QString &action,
                                            QString *error) const {
  return accepted(call(m_bus, m_owner, QStringLiteral("Unregister"),
                       {component, action}, error),
                  error, false);
}
} // namespace QindaQt::Services::Shortcuts
