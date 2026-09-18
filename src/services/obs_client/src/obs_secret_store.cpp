// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/obs_client/obs_secret_store.h>

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QVariantMap>

namespace QindaQt::Obs::SecretWire {

// org.freedesktop.Secret.Service's `Secret` struct:
//   (oayays) session, parameters, value, content type.
struct SecretValue {
    QDBusObjectPath session;
    QByteArray parameters;
    QByteArray value;
    QString contentType;
};

inline QDBusArgument &operator<<(QDBusArgument &argument,
                                 const SecretValue &secret) {
    argument.beginStructure();
    argument << secret.session << secret.parameters << secret.value
             << secret.contentType;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument,
                                       SecretValue &secret) {
    argument.beginStructure();
    argument >> secret.session >> secret.parameters >> secret.value >>
        secret.contentType;
    argument.endStructure();
    return argument;
}

} // namespace QindaQt::Obs::SecretWire

Q_DECLARE_METATYPE(QindaQt::Obs::SecretWire::SecretValue)

namespace QindaQt::Obs {

using SecretWire::SecretValue;

namespace {

constexpr auto SecretsService = "org.freedesktop.secrets";
constexpr auto ServicePath = "/org/freedesktop/secrets";
constexpr auto ServiceInterface = "org.freedesktop.Secret.Service";
constexpr auto ItemInterface = "org.freedesktop.Secret.Item";
constexpr auto DefaultCollection = "/org/freedesktop/secrets/aliases/default";
constexpr auto CollectionInterface = "org.freedesktop.Secret.Collection";
constexpr auto PlainAlgorithm = "plain";
constexpr auto Label = "QindaQt obs-websocket";
constexpr int CallTimeoutMs = 4000;

void registerTypes() {
    static const bool once = [] {
        qDBusRegisterMetaType<SecretValue>();
        // a{ss} is not one of QtDBus's built-in types; without this the
        // attribute map has no signature and every call fails to marshal.
        qDBusRegisterMetaType<QMap<QString, QString>>();
        return true;
    }();
    Q_UNUSED(once)
}

bool serviceAvailable(const QDBusConnection &bus) {
    return bus.isConnected() && bus.interface() != nullptr &&
           bus.interface()->isServiceRegistered(QLatin1String(SecretsService));
}

QDBusMessage call(const QDBusConnection &bus, const QString &path,
                  const QString &interface, const QString &method,
                  const QVariantList &arguments) {
    QDBusMessage message = QDBusMessage::createMethodCall(
        QLatin1String(SecretsService), path, interface, method);
    message.setArguments(arguments);
    return bus.call(message, QDBus::Block, CallTimeoutMs);
}

// Opens a plain session. The bus is the user's own and loopback-only, and
// the alternative (DH) buys nothing against an attacker who can already read
// that bus.
std::optional<QDBusObjectPath> openSession(const QDBusConnection &bus,
                                           QString *error) {
    const QDBusMessage reply =
        call(bus, QLatin1String(ServicePath), QLatin1String(ServiceInterface),
             QStringLiteral("OpenSession"),
             {QLatin1String(PlainAlgorithm),
              QVariant::fromValue(QDBusVariant(QString()))});
    if (reply.type() != QDBusMessage::ReplyMessage ||
        reply.arguments().size() != 2) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not open a keyring session: %1")
                         .arg(reply.errorMessage());
        }
        return std::nullopt;
    }
    return reply.arguments().at(1).value<QDBusObjectPath>();
}

} // namespace

SecretServiceObsStore::SecretServiceObsStore(QDBusConnection bus,
                                             QObject *parent)
    : ObsSecretStore(parent), m_bus(std::move(bus)) {
    registerTypes();
}

QMap<QString, QString> SecretServiceObsStore::attributes() {
    return QMap<QString, QString>{
        {QStringLiteral("application"), QStringLiteral("qindaqt")},
        {QStringLiteral("service"), QStringLiteral("obs-websocket")},
    };
}

std::optional<QString>
SecretServiceObsStore::password(QString *error) const {
    if (error != nullptr) {
        error->clear();
    }
    if (!serviceAvailable(m_bus)) {
        if (error != nullptr) {
            *error = QStringLiteral("The keyring (org.freedesktop.secrets) is "
                                    "not available");
        }
        return std::nullopt;
    }
    const QDBusMessage found =
        call(m_bus, QLatin1String(ServicePath), QLatin1String(ServiceInterface),
             QStringLiteral("SearchItems"),
             {QVariant::fromValue(attributes())});
    if (found.type() != QDBusMessage::ReplyMessage ||
        found.arguments().isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The keyring refused a search: %1")
                         .arg(found.errorMessage());
        }
        return std::nullopt;
    }
    const QList<QDBusObjectPath> unlocked =
        qdbus_cast<QList<QDBusObjectPath>>(found.arguments().at(0));
    if (unlocked.isEmpty()) {
        // No secret stored yet: the ordinary first-run state, not an error.
        return std::nullopt;
    }

    const auto session = openSession(m_bus, error);
    if (!session.has_value()) {
        return std::nullopt;
    }
    const QDBusMessage secret =
        call(m_bus, unlocked.first().path(), QLatin1String(ItemInterface),
             QStringLiteral("GetSecret"), {QVariant::fromValue(*session)});
    if (secret.type() != QDBusMessage::ReplyMessage ||
        secret.arguments().isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The keyring refused the secret: %1")
                         .arg(secret.errorMessage());
        }
        return std::nullopt;
    }
    SecretValue value;
    const QDBusArgument argument =
        secret.arguments().at(0).value<QDBusArgument>();
    argument >> value;
    return QString::fromUtf8(value.value);
}

bool SecretServiceObsStore::setPassword(const QString &password,
                                        QString *error) {
    if (error != nullptr) {
        error->clear();
    }
    if (password.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("Refusing to store an empty password");
        }
        return false;
    }
    if (!serviceAvailable(m_bus)) {
        if (error != nullptr) {
            *error = QStringLiteral("The keyring (org.freedesktop.secrets) is "
                                    "not available");
        }
        return false;
    }
    const auto session = openSession(m_bus, error);
    if (!session.has_value()) {
        return false;
    }
    QVariantMap properties;
    properties.insert(QStringLiteral("org.freedesktop.Secret.Item.Label"),
                      QString::fromLatin1(Label));
    properties.insert(QStringLiteral("org.freedesktop.Secret.Item.Attributes"),
                      QVariant::fromValue(attributes()));
    SecretValue value;
    value.session = *session;
    value.value = password.toUtf8();
    value.contentType = QStringLiteral("text/plain");

    const QDBusMessage reply =
        call(m_bus, QLatin1String(DefaultCollection),
             QLatin1String(CollectionInterface), QStringLiteral("CreateItem"),
             {QVariant::fromValue(properties), QVariant::fromValue(value),
              // Replace: one password, not a new item on every repair.
              true});
    if (reply.type() != QDBusMessage::ReplyMessage) {
        if (error != nullptr) {
            *error = QStringLiteral("The keyring refused to store the "
                                    "password: %1")
                         .arg(reply.errorMessage());
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Obs
