#include "LinkBridge.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJniObject>
#include <QCoreApplication>

namespace {
constexpr const char *kBridge = "org/qindaqt/link/android/LinkBridge";
}

LinkBridge::LinkBridge(QObject *parent) : QObject(parent)
{
    QJniObject::callStaticMethod<void>(kBridge, "startService", "(Landroid/content/Context;)V",
                                       QNativeInterface::QAndroidApplication::context());
    connect(&m_timer, &QTimer::timeout, this, &LinkBridge::poll);
    m_timer.start(500);
    poll();
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
    if (differs)
        emit changed();
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
