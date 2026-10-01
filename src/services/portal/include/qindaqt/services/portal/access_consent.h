// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/request_registry.h>
#include <QDBusArgument>
#include <QVariantMap>
#include <optional>
namespace QindaQt::Services::Portal {
struct ChoiceOption { QString id, label; };
using ChoiceOptions = QList<ChoiceOption>;
struct AccessChoice { QString id, label; ChoiceOptions options; QString initial; };
using AccessChoices = QList<AccessChoice>;
struct ChoiceValue { QString id, value; };
using ChoiceValues = QList<ChoiceValue>;
QDBusArgument &operator<<(QDBusArgument &, const ChoiceOption &);
const QDBusArgument &operator>>(const QDBusArgument &, ChoiceOption &);
QDBusArgument &operator<<(QDBusArgument &, const AccessChoice &);
const QDBusArgument &operator>>(const QDBusArgument &, AccessChoice &);
QDBusArgument &operator<<(QDBusArgument &, const ChoiceValue &);
const QDBusArgument &operator>>(const QDBusArgument &, ChoiceValue &);
void registerAccessTypes();
struct AccessQuestion {
    QString appId, parentWindow, title, subtitle, body, denyLabel, grantLabel, icon;
    bool modal = true;
    AccessChoices choices;
};
// Pure bounded wire policy: no actor admission, filesystem or presentation.
// Text is plain text; choices are at most 16, options at most 32 each. Unknown
// option keys are ignored for standard forward compatibility; known keys must
// have exactly the spec's type. Result selections must match offered values.
std::optional<AccessQuestion> accessQuestion(const QString &app, const QString &parent,
    const QString &title, const QString &subtitle, const QString &body, const QVariantMap &options);
bool validChoiceValues(const AccessQuestion &, const ChoiceValues &);
// Borrowed asynchronous UI port; one Qt thread. Implementations own helper
// lifetime, and cancel is synchronous retirement: subsequent completion may
// arrive but may never authorize a retired token. No success without admission.
class AccessConsent : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool admitted() const = 0;
    virtual void ask(RequestToken, const AccessQuestion &) = 0;
    virtual void cancel(RequestToken) = 0;
Q_SIGNALS:
    void completed(RequestToken token, RequestResponse response, const ChoiceValues &choices);
    void authorityLost();
};
} // namespace QindaQt::Services::Portal
Q_DECLARE_METATYPE(QindaQt::Services::Portal::ChoiceOption)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::ChoiceOptions)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::AccessChoice)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::AccessChoices)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::ChoiceValue)
Q_DECLARE_METATYPE(QindaQt::Services::Portal::ChoiceValues)
