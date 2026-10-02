// SPDX-License-Identifier: LGPL-3.0-or-later
#include "misc_policy.h"
#include <QDBusMetaType>
#include <QJsonArray>
#include <QSet>
namespace QindaQt::Services::Portal {
bool boundedText(const QString &text, qsizetype limit) {
    if (text.size() > limit) return false;
    for (const auto c : text) if (c.isNull() || (c.category() == QChar::Other_Control && c != QLatin1Char('\n') && c != QLatin1Char('\t'))) return false;
    return true;
}
std::optional<QJsonObject> miscFrame(const QString &kind, const QString &app,
    const QString &parent, const QString &title, const QVariantMap &options) {
    const auto question = accessQuestion(app, parent, title, {}, {}, options);
    if (!question) return {};
    return QJsonObject{{"type", kind}, {"app", app}, {"parent", parent}, {"title", title}, {"modal", question->modal}};
}
void registerMiscTypes() {
    qDBusRegisterMetaType<UsbDevices>(); qDBusRegisterMetaType<UsbSelections>();
    qDBusRegisterMetaType<LauncherIcon>();
}
}
