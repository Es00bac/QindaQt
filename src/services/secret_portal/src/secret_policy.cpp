// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/secret_portal/secret_policy.h>
#include <QCryptographicHash>
#include <QDateTime>
#include <QRegularExpression>
#include <openssl/rand.h>
namespace QindaQt::Services::SecretPortal {
bool validApplicationId(const QString &value) {
    static const QRegularExpression pattern("^[A-Za-z0-9_-]+(?:\\.[A-Za-z0-9_-]+)+$");
    return !value.isEmpty() && value.size()<=255 && pattern.match(value).capturedLength()==value.size();
}
bool validRequestHandle(const QString &value) {
    static const QRegularExpression pattern("^/org/freedesktop/portal/desktop/request/[A-Za-z0-9_]+/[A-Za-z0-9_]+$");
    return value.size()<=512 && pattern.match(value).capturedLength()==value.size();
}
bool validOptions(const QVariantMap &options) {
    if(options.isEmpty()) return true;
    if(options.size()!=1 || !options.contains("token")) return false;
    const auto token=options.value("token");
    return token.metaType().id()==QMetaType::QString && token.toString().toUtf8().size()<=1024 && !token.toString().contains(QChar(0));
}
QString applicationItemId(const QString &app) {
    if(!validApplicationId(app)) return {};
    QByteArray domain=QByteArrayLiteral("QindaQt.SecretPortal1");domain.append(char(0));domain.append(app.toUtf8());
    return QString::fromLatin1(QCryptographicHash::hash(domain,QCryptographicHash::Sha256).toHex());
}
qindaqt::keyring::Item newApplicationSecret(const QString &app) {
    using namespace qindaqt::keyring;
    if(!validApplicationId(app)) throw std::runtime_error("Invalid application identity");
    Item item;item.id=applicationItemId(app).toStdString();item.secret=SecureBuffer(SecretSize);
    if(RAND_priv_bytes(item.secret.bytes().data(),static_cast<int>(SecretSize))!=1) throw std::runtime_error("Secret generation unavailable");
    item.attributes={{"qindaqt.portal.application",app.toStdString()},{"qindaqt.portal.version","fresh-native-32"}};
    item.metadata.label="Application secret";item.metadata.contentType="application/vnd.qindaqt.portal-secret";
    item.metadata.creator="QindaQt Secret portal";
    item.metadata.created=item.metadata.modified=static_cast<std::uint64_t>(QDateTime::currentSecsSinceEpoch());
    return item;
}
bool matchesApplicationSecret(const qindaqt::keyring::Item &item,const QString &app) {
    const qindaqt::keyring::Attributes attributes{{"qindaqt.portal.application",app.toStdString()},{"qindaqt.portal.version","fresh-native-32"}};
    return validApplicationId(app) && item.id==applicationItemId(app).toStdString() && item.attributes==attributes
        && item.metadata.contentType=="application/vnd.qindaqt.portal-secret" && item.secret.size()==SecretSize;
}
}
