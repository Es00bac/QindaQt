// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Private session bus, StatusNotifier wire types, and the generic Properties
// adaptor shared by the transport tests. The scripted item itself lives in
// status_notifier_fake_item_test_support.h so each header stays within the
// source-shape budget.

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QMetaProperty>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QVariantList>

namespace QindaQt::StatusNotifier::TestSupport
{

// Wire form of one IconPixmap entry: D-Bus struct (iiay). Declared with
// marshallers and registered with qDBusRegisterMetaType so the fake serves
// the exact a(iiay) struct array the StatusNotifier spec mandates; a plain
// QVariantList degrades to an array of variants ('av') that real items never
// produce.
//
// AGENT-NOTE: the structs are Q_GADGETs with their fields as properties, and
// the registration has an in-process consequence a real item cannot produce:
// when the fake and the client share a process (every row here), a demarshal
// of the registered signature yields the registered C++ type (e.g. a
// ToolTip struct field demarshals as QList<FakePixmapWire>, not
// QDBusArgument). The production client decodes that shape generically via
// gadget introspection; a production shell process never registers the item
// wire types, so there the plain QDBusArgument shape is what arrives.
struct FakePixmapWire
{
    Q_GADGET
    Q_PROPERTY(int width MEMBER width CONSTANT)
    Q_PROPERTY(int height MEMBER height CONSTANT)
    Q_PROPERTY(QByteArray argb MEMBER argb CONSTANT)

public:
    int width = 0;
    int height = 0;
    QByteArray argb;
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const FakePixmapWire &pixmap)
{
    argument.beginStructure();
    argument << pixmap.width << pixmap.height << pixmap.argb;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument,
                                       FakePixmapWire &pixmap)
{
    argument.beginStructure();
    argument >> pixmap.width >> pixmap.height >> pixmap.argb;
    argument.endStructure();
    return argument;
}

// Qt 6.11 moc's MEMBER-property setter instantiates operator== on the
// property type even for read-mostly fakes; without these the generated moc
// file fails to compile inside QtMocHelpers::setProperty.
inline bool operator==(const FakePixmapWire &lhs, const FakePixmapWire &rhs)
{
    return lhs.width == rhs.width && lhs.height == rhs.height
        && lhs.argb == rhs.argb;
}

// Wire form of ToolTip: D-Bus struct (sa(iiay)ss).
struct FakeToolTipWire
{
    Q_GADGET
    Q_PROPERTY(QString iconName MEMBER iconName CONSTANT)
    Q_PROPERTY(QList<FakePixmapWire> pixmaps MEMBER pixmaps CONSTANT)
    Q_PROPERTY(QString title MEMBER title CONSTANT)
    Q_PROPERTY(QString description MEMBER description CONSTANT)

public:
    QString iconName;
    QList<FakePixmapWire> pixmaps;
    QString title;
    QString description;
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const FakeToolTipWire &toolTip)
{
    argument.beginStructure();
    argument << toolTip.iconName << toolTip.pixmaps << toolTip.title
             << toolTip.description;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument,
                                       FakeToolTipWire &toolTip)
{
    argument.beginStructure();
    argument >> toolTip.iconName >> toolTip.pixmaps >> toolTip.title
        >> toolTip.description;
    argument.endStructure();
    return argument;
}

inline bool operator==(const FakeToolTipWire &lhs, const FakeToolTipWire &rhs)
{
    return lhs.iconName == rhs.iconName && lhs.pixmaps == rhs.pixmaps
        && lhs.title == rhs.title && lhs.description == rhs.description;
}

// Registers the fake wire types with the Qt meta and D-Bus marshaller
// systems; idempotent, safe to call from every test row. The QList container
// registration is what lets a ToolTip struct field demarshal back into a
// QVariant holding QList<FakePixmapWire> instead of failing.
inline void registerFakeWireTypes()
{
    static const bool registered = [] {
        qDBusRegisterMetaType<FakePixmapWire>();
        qDBusRegisterMetaType<FakeToolTipWire>();
        qDBusRegisterMetaType<QList<FakePixmapWire>>();
        return true;
    }();
    Q_UNUSED(registered)
}

// A private session bus owned by the test process. Spawning a dedicated
// dbus-daemon keeps every row off the host session bus; nothing here touches
// the developer's desktop.
class PrivateSessionBus final
{
public:
    ~PrivateSessionBus() { stop(); }

    [[nodiscard]] bool start(QString *error)
    {
        m_process.start(QStringLiteral("dbus-daemon"),
                        {QStringLiteral("--session"), QStringLiteral("--nofork"),
                         QStringLiteral("--nopidfile"),
                         QStringLiteral("--print-address=1")});
        if (!m_process.waitForStarted(5'000) || !m_process.waitForReadyRead(5'000)) {
            *error = m_process.errorString();
            return false;
        }
        m_address = QString::fromUtf8(m_process.readLine()).trimmed();
        if (m_address.isEmpty()) {
            *error = QStringLiteral("private dbus-daemon did not publish an address");
            return false;
        }
        return true;
    }

    void stop() noexcept
    {
        if (m_process.state() == QProcess::NotRunning) {
            return;
        }
        m_process.terminate();
        if (!m_process.waitForFinished(1'000)) {
            m_process.kill();
            m_process.waitForFinished(1'000);
        }
    }

    [[nodiscard]] const QString &address() const noexcept { return m_address; }

private:
    QProcess m_process;
    QString m_address;
};

inline QDBusConnection connectToPrivateBus(const QString &address, const QString &name)
{
    return QDBusConnection::connectToBus(address, name);
}

// QtDBus advertises org.freedesktop.D-Bus.Properties for ExportAllProperties
// registrations but never dispatches Get/GetAll/Set to them; a
// QDBusAbstractAdaptor child plus the ExportAdaptors flag is required (same
// trap as the production watcher; see its AGENT-NOTE). Real tray items serve
// the interface via generated adaptors, so the fake must too or readers time
// out on every property fetch.
class FakePropertiesAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.D-Bus.Properties")

public:
    explicit FakePropertiesAdaptor(QObject *parent)
        : QDBusAbstractAdaptor(parent)
    {
    }

public slots:
    [[nodiscard]] QDBusVariant Get(const QString &interfaceName,
                                   const QString &propertyName)
    {
        Q_UNUSED(interfaceName);
        const QVariantMap overrides =
            parent()->property("wireOverrides").toMap();
        const auto override = overrides.constFind(propertyName);
        if (override != overrides.constEnd()) {
            return QDBusVariant(override.value());
        }
        return QDBusVariant(parent()->property(propertyName.toUtf8().constData()));
    }

    [[nodiscard]] QVariantMap GetAll(const QString &interfaceName)
    {
        Q_UNUSED(interfaceName);
        QVariantMap values;
        const QMetaObject *meta = parent()->metaObject();
        for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
            const QMetaProperty property = meta->property(i);
            if (property.isReadable()) {
                values[QString::fromUtf8(property.name())] = property.read(parent());
            }
        }
        // QObject lookup gives dynamic properties precedence over typed MEMBER
        // properties, but QMetaProperty iteration above cannot see them; the
        // wireOverrides map is the explicit staging point for values a row
        // cannot express through the typed MEMBERs (hostile shapes) — a plain
        // setProperty("IconPixmap", ...) fails type conversion against the
        // typed MEMBER and would silently stage nothing.
        const QVariantMap overrides = parent()->property("wireOverrides").toMap();
        for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
            values[it.key()] = it.value();
        }
        return values;
    }

    void Set(const QString &interfaceName,
             const QString &propertyName,
             const QDBusVariant &value)
    {
        Q_UNUSED(interfaceName);
        Q_UNUSED(propertyName);
        Q_UNUSED(value);
    }
};
} // namespace QindaQt::StatusNotifier::TestSupport
