// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VaultBridge.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJniObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
constexpr const char *kBridge = "org/qindaqt/link/android/VaultBridge";
constexpr const char *kCtx = "Landroid/content/Context;";

QJniObject ctx() { return QNativeInterface::QAndroidApplication::context(); }

QString str(const char *method, const char *sig, auto... args)
{
    return QJniObject::callStaticObjectMethod(kBridge, method, sig, args...).toString();
}
QJniObject js(const QString &s) { return QJniObject::fromString(s); }
}

VaultBridge::VaultBridge(QObject *parent) : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &VaultBridge::poll);
    // The Java side locks 30 s after the UI goes away; tell it when the UI is (not) in front.
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [](Qt::ApplicationState s) {
        QJniObject::callStaticMethod<void>(kBridge, "setForeground", "(Landroid/content/Context;Z)V", ctx().object(),
                                           jboolean(s == Qt::ApplicationActive));
        // The activity may have been recreated while away: re-apply FLAG_SECURE before any frame is shown.
        if (s == Qt::ApplicationActive)
            QJniObject::callStaticMethod<void>(kBridge, "reapplySecure", "(Landroid/content/Context;)V", ctx().object());
    });
}

void VaultBridge::setActive(bool on)
{
    if (m_active == on)
        return;
    m_active = on;
    if (on) {
        m_rev = -1;
        poll();
        m_timer.start(500);
    } else {
        m_timer.stop();
    }
    emit activeChanged();
}

void VaultBridge::poll()
{
    const QString json = str("stateJson", "(Landroid/content/Context;)Ljava/lang/String;", ctx().object());
    const QVariantMap next = QJsonDocument::fromJson(json.toUtf8()).object().toVariantMap();
    const int rev = next.value(QStringLiteral("rev")).toInt();
    if (rev == m_rev)
        return;
    m_rev = rev;
    m_s = next;
    if (state() == QLatin1String("unlocked")) {
        const QString list = str("listJson", "(Landroid/content/Context;)Ljava/lang/String;", ctx().object());
        m_entries = QJsonDocument::fromJson(list.toUtf8()).array().toVariantList();
    } else {
        m_entries.clear();
    }
    emit entriesChanged();
    emit changed();
}

void VaultBridge::unlock()
{
    QJniObject::callStaticMethod<void>(kBridge, "requestUnlock", "(Landroid/content/Context;)V", ctx().object());
    poll();
}
void VaultBridge::lock()
{
    QJniObject::callStaticMethod<void>(kBridge, "lock", "(Landroid/content/Context;)V", ctx().object());
    poll();
}
void VaultBridge::touch()
{
    QJniObject::callStaticMethod<void>(kBridge, "touch", "(Landroid/content/Context;)V", ctx().object());
}
void VaultBridge::setSecure(bool on)
{
    QJniObject::callStaticMethod<void>(kBridge, "setSecure", "(Landroid/content/Context;Z)V", ctx().object(), jboolean(on));
}
void VaultBridge::eraseVault()
{
    QJniObject::callStaticMethod<void>(kBridge, "eraseVault", "(Landroid/content/Context;)V", ctx().object());
    poll();
}
void VaultBridge::setCopyEnabled(bool on)
{
    QJniObject::callStaticMethod<void>(kBridge, "setCopyEnabled", "(Landroid/content/Context;Z)V", ctx().object(), jboolean(on));
    poll();
}
void VaultBridge::clearBackupStatus()
{
    QJniObject::callStaticMethod<void>(kBridge, "clearBackupStatus", "(Landroid/content/Context;)V", ctx().object());
    poll();
}

QVariantMap VaultBridge::entry(const QString &id)
{
    const QString j = str("entryJson", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;", ctx().object(),
                          js(id).object<jstring>());
    return QJsonDocument::fromJson(j.toUtf8()).object().toVariantMap();
}

QVariantMap VaultBridge::reveal(const QString &id)
{
    touch();
    const QString j = str("revealJson", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;", ctx().object(),
                          js(id).object<jstring>());
    return QJsonDocument::fromJson(j.toUtf8()).object().toVariantMap();
}

QString VaultBridge::save(const QVariantMap &entry)
{
    const QString json = QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(entry)).toJson(QJsonDocument::Compact));
    const QString r = str("saveEntry", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;", ctx().object(),
                          js(json).object<jstring>());
    poll();
    return r;
}

bool VaultBridge::remove(const QString &id)
{
    const bool ok = QJniObject::callStaticMethod<jboolean>(kBridge, "deleteEntry", "(Landroid/content/Context;Ljava/lang/String;)Z",
                                                           ctx().object(), js(id).object<jstring>());
    poll();
    return ok;
}

bool VaultBridge::copyPassword(const QString &id)
{
    return QJniObject::callStaticMethod<jboolean>(kBridge, "copyPassword", "(Landroid/content/Context;Ljava/lang/String;)Z",
                                                  ctx().object(), js(id).object<jstring>());
}

QString VaultBridge::generatePassword(int length, bool lower, bool upper, bool digits, bool symbols, bool avoidAmbiguous)
{
    return str("generatePassword", "(IZZZZZ)Ljava/lang/String;", jint(length), jboolean(lower), jboolean(upper), jboolean(digits),
               jboolean(symbols), jboolean(avoidAmbiguous));
}

QString VaultBridge::generatePassphrase(int words, const QString &separator, bool capitalize, bool number)
{
    return str("generatePassphrase", "(ILjava/lang/String;ZZ)Ljava/lang/String;", jint(words), js(separator).object<jstring>(),
               jboolean(capitalize), jboolean(number));
}

QVariantMap VaultBridge::passphraseHint(const QString &passphrase)
{
    const QString j = str("passphraseHint", "(Ljava/lang/String;)Ljava/lang/String;", js(passphrase).object<jstring>());
    return QJsonDocument::fromJson(j.toUtf8()).object().toVariantMap();
}

QString VaultBridge::exportBackup(const QString &pass, const QString &confirm)
{
    const QString r = str("exportBackup", "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                          ctx().object(), js(pass).object<jstring>(), js(confirm).object<jstring>());
    poll();
    return r;
}

QString VaultBridge::importBackup(const QString &pass, const QString &mode)
{
    const QString r = str("importBackup", "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                          ctx().object(), js(pass).object<jstring>(), js(mode).object<jstring>());
    poll();
    return r;
}

void VaultBridge::typeBegin(const QString &id, const QString &field)
{
    QJniObject::callStaticMethod<void>(kBridge, "typeBegin", "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)V",
                                       ctx().object(), js(id).object<jstring>(), js(field).object<jstring>());
}
void VaultBridge::typeConfirm()
{
    QJniObject::callStaticMethod<void>(kBridge, "typeConfirm", "(Landroid/content/Context;)V", ctx().object());
}
void VaultBridge::typeCancel()
{
    QJniObject::callStaticMethod<void>(kBridge, "typeCancel", "(Landroid/content/Context;)V", ctx().object());
}
QVariantMap VaultBridge::typeState()
{
    const QString j = str("typeStateJson", "(Landroid/content/Context;)Ljava/lang/String;", ctx().object());
    return QJsonDocument::fromJson(j.toUtf8()).object().toVariantMap();
}
