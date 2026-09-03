// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Scripted org.kde.StatusNotifierItem fake served on the private bus; split
// from status_notifier_private_bus_test_support.h (bus + wire types + property
// adaptor) so each header stays within the source-shape budget.

#include "status_notifier_private_bus_test_support.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusServiceWatcher>

namespace QindaQt::StatusNotifier::TestSupport
{

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

    // Production-faithful watcher monitoring: real KStatusNotifierItem-based
    // items watch org.kde.StatusNotifierWatcher and re-register when a
    // replacement watcher acquires the name (their previous registration died
    // with the old watcher). Without this a restart scenario can never
    // re-admit the item into the replacement watcher's population.
    void watchAndReregister(const QDBusConnection &connection, const QString &path)
    {
        m_busName = connection.name();
        m_path = path;
        auto *watcher = new QDBusServiceWatcher(
            QStringLiteral("org.kde.StatusNotifierWatcher"),
            connection,
            QDBusServiceWatcher::WatchForRegistration,
            this);
        connect(watcher, &QDBusServiceWatcher::serviceRegistered,
                this, [this]() { reregister(); });
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

    void reregister()
    {
        auto message = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.StatusNotifierWatcher"),
            QStringLiteral("/StatusNotifierWatcher"),
            QStringLiteral("org.kde.StatusNotifierWatcher"),
            QStringLiteral("RegisterStatusNotifierItem"));
        message << QVariant(m_path);
        QDBusConnection(m_busName).asyncCall(message);
    }

    QString m_busName;
    QString m_path;
};

} // namespace QindaQt::StatusNotifier::TestSupport
