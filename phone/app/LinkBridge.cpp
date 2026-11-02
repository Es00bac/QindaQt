#include "LinkBridge.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJniObject>
#include <QCoreApplication>
#include <QGuiApplication>

namespace {
constexpr const char *kBridge = "org/qindaqt/link/android/LinkBridge";
}

LinkBridge::LinkBridge(QObject *parent) : QObject(parent)
{
    QJniObject::callStaticMethod<void>(kBridge, "startService", "(Landroid/content/Context;)V",
                                       QNativeInterface::QAndroidApplication::context());
    connect(&m_timer, &QTimer::timeout, this, &LinkBridge::poll);
    m_timer.start(500);
    pollDevice();
    poll();
    // Returning from a settings page is when permission states change.
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState s) {
        if (s == Qt::ApplicationActive) {
            pollDevice();
            emit changed();
        }
    });
}

void LinkBridge::poll()
{
    const QJniObject json = QJniObject::callStaticObjectMethod(kBridge, "stateJson", "()Ljava/lang/String;");
    const QVariantMap next = QJsonDocument::fromJson(json.toString().toUtf8()).object().toVariantMap();
    // lastStatusAgeMs changes every poll; only notify on fields the UI shows.
    auto key = [](const QVariantMap &m) {
        return m.value("phase").toString() + '|' + m.value("desktopName").toString() + '|'
            + m.value("code").toString() + '|' + m.value("message").toString() + '|'
            + m.value("paired").toString() + '|' + m.value("pairedName").toString();
    };
    const bool differs = key(next) != key(m_s);
    m_s = next;
    // Device facts change slowly; refresh every ~5 s.
    bool devChanged = false;
    if (++m_tick % 10 == 0) {
        const QVariantMap before = m_dev;
        pollDevice();
        devChanged = before != m_dev;
    }
    if (differs || devChanged)
        emit changed();
}

void LinkBridge::pollDevice()
{
    const QJniObject json = QJniObject::callStaticMethod<jstring>(
        kBridge, "deviceJson", "(Landroid/content/Context;)Ljava/lang/String;",
        QNativeInterface::QAndroidApplication::context());
    m_dev = QJsonDocument::fromJson(json.toString().toUtf8()).object().toVariantMap();
}

QString LinkBridge::transport() const
{
    const QVariant t = m_s.value(QStringLiteral("transport"));
    if (t.isValid())
        return t.toString();
    const QString p = phase();
    return (p == QLatin1String("connected") || p == QLatin1String("confirm") || p == QLatin1String("ready")
            || p == QLatin1String("pairing") || p == QLatin1String("stale"))
        ? QStringLiteral("usb") : QString();
}

void LinkBridge::openSettings(const QString &key)
{
    QJniObject jkey = QJniObject::fromString(key);
    QJniObject::callStaticMethod<void>(kBridge, "openSettings", "(Landroid/content/Context;Ljava/lang/String;)V",
                                       QNativeInterface::QAndroidApplication::context(), jkey.object<jstring>());
}

void LinkBridge::call(const char *method)
{
    QJniObject::callStaticMethod<void>(kBridge, method, "()V");
    poll();
}

void LinkBridge::pair() { call("pair"); }
void LinkBridge::pairAgain() { call("pairAgain"); }
void LinkBridge::accept() { call("accept"); }
void LinkBridge::reject() { call("reject"); }
void LinkBridge::forget() { call("forget"); }
