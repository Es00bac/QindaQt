// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>

#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusObjectPath>
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
// QVariantList would be lossily encoded as an array of variants (av) that
// real items never produce.
struct FakePixmapWire
{
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

// Wire form of ToolTip: D-Bus struct (sa(iiay)ss).
struct FakeToolTipWire
{
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

// Registers the fake wire types with the Qt meta and D-Bus marshaller
// systems; idempotent, safe to call from every test row.
inline void registerFakeWireTypes()
{
    static const bool registered = [] {
        qDBusRegisterMetaType<FakePixmapWire>();
        qDBusRegisterMetaType<FakeToolTipWire>();
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

// Registers a fake item the way a real KStatusNotifierItem-based item is
// served: Properties adaptor attached, all slots/properties/signals exported.
inline bool registerFakeItem(QDBusConnection &connection,
                             const QString &path,
                             QObject *item)
{
    // Owned by the item through QObject parent-child; must outlive the
    // connection unregister, which teardown of the item guarantees.
    new FakePropertiesAdaptor(item);
    return connection.registerObject(
        path,
        item,
        QDBusConnection::ExportAdaptors | QDBusConnection::ExportAllSlots
            | QDBusConnection::ExportAllProperties
            | QDBusConnection::ExportAllSignals);
}

// A scripted org.kde.StatusNotifierItem used by the transport tests. The
// property setters let a row stage hostile payloads before or after export;
// the intent slots record every invocation they receive.
class FakeStatusNotifierItem final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Category MEMBER category CONSTANT)
    Q_PROPERTY(QString Id MEMBER id CONSTANT)
    Q_PROPERTY(QString Title MEMBER title NOTIFY titleChanged)
    Q_PROPERTY(QString Status MEMBER status NOTIFY statusChanged)
    Q_PROPERTY(quint32 WindowId MEMBER windowId CONSTANT)
    Q_PROPERTY(QString IconName MEMBER iconName NOTIFY titleChanged)
    Q_PROPERTY(QList<FakePixmapWire> IconPixmap MEMBER iconPixmap NOTIFY titleChanged)
    Q_PROPERTY(QString OverlayIconName MEMBER overlayIconName NOTIFY titleChanged)
    Q_PROPERTY(QString AttentionIconName MEMBER attentionIconName NOTIFY titleChanged)
    Q_PROPERTY(QList<FakePixmapWire> AttentionPixmap MEMBER attentionPixmap NOTIFY titleChanged)
    Q_PROPERTY(QString AttentionMovieName MEMBER attentionMovieName NOTIFY titleChanged)
    Q_PROPERTY(FakeToolTipWire ToolTip MEMBER toolTip NOTIFY titleChanged)
    Q_PROPERTY(bool ItemIsMenu MEMBER itemIsMenu CONSTANT)
    Q_PROPERTY(QDBusObjectPath Menu MEMBER menu CONSTANT)
    Q_PROPERTY(QString UnknownExtension MEMBER unknownExtension CONSTANT)

public:
    struct RecordedIntent {
        QString member;
        QList<QVariant> arguments;
    };

    QString category = QStringLiteral("ApplicationStatus");
    QString id = QStringLiteral("org.qindaqt.fake");
    QString title = QStringLiteral("Fake item");
    QString status = QStringLiteral("Active");
    quint32 windowId = 42;
    QString iconName = QStringLiteral("fake-icon");
    QList<FakePixmapWire> iconPixmap;
    QString overlayIconName = QStringLiteral("fake-overlay");
    QString attentionIconName;
    QList<FakePixmapWire> attentionPixmap;
    QString attentionMovieName;
    FakeToolTipWire toolTip;
    bool itemIsMenu = false;
    QDBusObjectPath menu = QDBusObjectPath(QStringLiteral("/Menu"));
    QString unknownExtension = QStringLiteral("ignored");
    QList<RecordedIntent> recordedIntents;

    explicit FakeStatusNotifierItem()
    {
        registerFakeWireTypes();
    }

    // Wire form of one pixmap: (width, height, argb-bytes).
    static FakePixmapWire pixmap(int width, int height, const QByteArray &argb)
    {
        FakePixmapWire pixmap;
        pixmap.width = width;
        pixmap.height = height;
        pixmap.argb = argb;
        return pixmap;
    }

    static FakePixmapWire pixmap(int width, int height, quint32 fill)
    {
        QByteArray bytes;
        bytes.resize(width * height * 4);
        for (int index = 0; index < width * height; ++index) {
            bytes[index * 4 + 0] = char(fill & 0xFF);
            bytes[index * 4 + 1] = char((fill >> 8) & 0xFF);
            bytes[index * 4 + 2] = char((fill >> 16) & 0xFF);
            bytes[index * 4 + 3] = char((fill >> 24) & 0xFF);
        }
        return pixmap(width, height, bytes);
    }

public slots:
    void Activate(int x, quint32 y) { record(QStringLiteral("Activate"), {x, y}); }
    void SecondaryActivate(int x, quint32 y)
    {
        record(QStringLiteral("SecondaryActivate"), {x, y});
    }
    void ContextMenu(int x, quint32 y) { record(QStringLiteral("ContextMenu"), {x, y}); }
    void Scroll(int delta, const QString &orientation)
    {
        record(QStringLiteral("Scroll"), {delta, orientation});
    }

signals:
    void NewTitle();
    void NewIcon();
    void NewAttentionIcon();
    void NewOverlayIcon();
    void NewToolTip();
    void NewStatus(const QString &status);
    void NewIconThemePath(const QString &path);
    void titleChanged();
    void statusChanged();

private:
    void record(QString member, QList<QVariant> arguments)
    {
        recordedIntents.append({std::move(member), std::move(arguments)});
    }
};

} // namespace QindaQt::StatusNotifier::TestSupport
