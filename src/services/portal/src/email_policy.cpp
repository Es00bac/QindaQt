// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/email_policy.h>
#include <QDBusArgument>
#include <QDBusMetaType>
namespace QindaQt::Services::Portal {
namespace {
bool text(const QString &s, qsizetype limit, bool multiline = false) {
    if (s.size() > limit) return false;
    for (const auto c : s) if (c.isNull() || (c.category() == QChar::Other_Control
        && !(multiline && (c == QLatin1Char('\n') || c == QLatin1Char('\r') || c == QLatin1Char('\t'))))) return false;
    return true;
}
std::optional<QStringList> strings(const QVariant &v) {
    if (!v.isValid()) return QStringList{};
    if (v.metaType() == QMetaType::fromType<QStringList>()) return v.toStringList();
    if (v.metaType() == QMetaType::fromType<QDBusArgument>() && v.value<QDBusArgument>().currentSignature() == QStringLiteral("as")) return qdbus_cast<QStringList>(v);
    return {};
}
}
std::optional<EmailDraft> emailDraft(const QString &parent, const QVariantMap &options) {
    if (options.size() > 32 || !text(parent, 512) || (!parent.isEmpty() && (!parent.startsWith(QStringLiteral("wayland:")) || parent.size() <= 8))) return {};
    for (const auto &key : {QStringLiteral("address"), QStringLiteral("subject"), QStringLiteral("body"), QStringLiteral("activation_token")})
        if (options.contains(key) && options[key].metaType() != QMetaType::fromType<QString>()) return {};
    auto recipients = strings(options.value(QStringLiteral("addresses")));
    const auto cc = strings(options.value(QStringLiteral("cc"))); const auto bcc = strings(options.value(QStringLiteral("bcc")));
    const auto attachments = strings(options.value(QStringLiteral("attachments")));
    if (!recipients || !cc || !bcc || !attachments || !attachments->isEmpty()) return {};
    if (recipients->isEmpty() && options.contains(QStringLiteral("address"))) recipients->append(options[QStringLiteral("address")].toString());
    for (const auto &list : {*recipients, *cc, *bcc}) {
        if (list.size() > 16) return {};
        for (const auto &address : list) if (address.isEmpty() || !text(address, 255) || address.contains(QLatin1Char(','))) return {};
    }
    const auto subject = options.value(QStringLiteral("subject")).toString(); const auto body = options.value(QStringLiteral("body")).toString();
    const auto activation = options.value(QStringLiteral("activation_token")).toString();
    if (!text(subject, 512) || !text(body, 4096, true) || !text(activation, 512)) return {};
    QByteArray uri = "mailto:" + QUrl::toPercentEncoding(recipients->join(QLatin1Char(',')), QByteArray("@,+."));
    bool query = false;
    const auto add = [&](const QByteArray &key, const QString &value) {
        if (value.isEmpty()) return;
        uri += query ? '&' : '?'; query = true;
        // Encode percent signs too: a literal %0D must remain literal user text
        // and can never become an injected mail header through URI parsing.
        uri += key + '=' + QUrl::toPercentEncoding(value);
    };
    add("cc", cc->join(QLatin1Char(','))); add("bcc", bcc->join(QLatin1Char(','))); add("subject", subject); add("body", body);
    if (uri.size() > 8192) return {};
    const auto value = QUrl::fromEncoded(uri, QUrl::StrictMode); if (!value.isValid()) return {};
    return EmailDraft{value, activation};
}
} // namespace QindaQt::Services::Portal
