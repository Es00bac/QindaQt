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
    m_fast.setInterval(150);
    connect(&m_fast, &QTimer::timeout, this, &LinkBridge::pollInput);
    pollDevice();
    pollLan();
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
    pollInput();
    if (m_tick % 2 == 0)
        pollLan();
}

void LinkBridge::pollLan()
{
    const QJniObject json = QJniObject::callStaticObjectMethod(kBridge, "lanJson", "()Ljava/lang/String;");
    const QVariantMap next = QJsonDocument::fromJson(json.toString().toUtf8()).object().toVariantMap();
    if (next != m_lan) {
        m_lan = next;
        emit lanChanged();
    }
}

QString LinkBridge::setManualAddress(const QString &address)
{
    QJniObject jaddr = QJniObject::fromString(address);
    const QJniObject err = QJniObject::callStaticObjectMethod(kBridge, "setManualAddress", "(Ljava/lang/String;)Ljava/lang/String;",
                                                              jaddr.object<jstring>());
    pollLan();
    return err.toString();
}

void LinkBridge::selectDesktop(const QString &id)
{
    QJniObject jid = QJniObject::fromString(id);
    QJniObject::callStaticMethod<void>(kBridge, "selectDesktop", "(Ljava/lang/String;)V", jid.object<jstring>());
    pollLan();
}

void LinkBridge::pollInput()
{
    const QJniObject input = QJniObject::callStaticObjectMethod(kBridge, "inputJson", "()Ljava/lang/String;");
    const QJniObject macro = QJniObject::callStaticObjectMethod(kBridge, "macroJson", "()Ljava/lang/String;");
    const QVariantMap i = QJsonDocument::fromJson(input.toString().toUtf8()).object().toVariantMap();
    const QVariantMap m = QJsonDocument::fromJson(macro.toString().toUtf8()).object().toVariantMap();
    if (i != m_input || m != m_macro) {
        m_input = i;
        m_macro = m;
        emit inputChanged();
    }
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

void LinkBridge::inputBegin()
{
    call("inputBegin");
    m_fast.start();
    pollInput();
}

void LinkBridge::inputEnd()
{
    m_fast.stop();
    call("inputEnd");
    pollInput();
}

bool LinkBridge::touch(int fingers, double x0, double y0, double x1, double y1)
{
    return QJniObject::callStaticMethod<jboolean>(kBridge, "touch", "(IDDDD)Z", fingers, x0, y0, x1, y1);
}

void LinkBridge::setSensitivity(double value)
{
    if (qFuzzyCompare(m_sensitivity, value))
        return;
    m_sensitivity = value;
    QJniObject::callStaticMethod<void>(kBridge, "setSensitivity", "(D)V", value);
    emit sensitivityChanged();
}

void LinkBridge::click(const QString &button)
{
    QJniObject jb = QJniObject::fromString(button);
    QJniObject::callStaticMethod<void>(kBridge, "click", "(Ljava/lang/String;)V", jb.object<jstring>());
}

void LinkBridge::sendKey(const QString &name, int mods)
{
    QJniObject jn = QJniObject::fromString(name);
    QJniObject::callStaticMethod<void>(kBridge, "sendKey", "(Ljava/lang/String;I)V", jn.object<jstring>(), mods);
}

void LinkBridge::sendChar(int codePoint, int mods)
{
    QJniObject::callStaticMethod<void>(kBridge, "sendChar", "(II)V", codePoint, mods);
}

// AGENT-GUARD: typed text passes through here only to be handed to the engine; it is never logged or kept.
void LinkBridge::sendText(const QString &text)
{
    QJniObject jt = QJniObject::fromString(text);
    QJniObject::callStaticMethod<void>(kBridge, "sendText", "(Ljava/lang/String;)V", jt.object<jstring>());
}

void LinkBridge::macroRun(const QString &id)
{
    QJniObject jid = QJniObject::fromString(id);
    QJniObject::callStaticMethod<void>(kBridge, "macroRun", "(Ljava/lang/String;)V", jid.object<jstring>());
    pollInput();
}

void LinkBridge::macroCancel()
{
    QJniObject::callStaticMethod<void>(kBridge, "macroCancel", "()V");
    pollInput();
}

void LinkBridge::haptic()
{
    QJniObject::callStaticMethod<void>(kBridge, "haptic", "(Landroid/content/Context;)V",
                                       QNativeInterface::QAndroidApplication::context());
}
