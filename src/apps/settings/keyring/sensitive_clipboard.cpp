// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_keyring/sensitive_clipboard.h>
#include <QClipboard>
#include <QGuiApplication>
namespace QindaQt::Apps::SettingsKeyring {
namespace {
class SecretMime final : public QMimeData {
public:
    explicit SecretMime(std::shared_ptr<qindaqt::keyring::SecureBuffer> data):bytes(std::move(data)) {}
    ~SecretMime() override{clear();}
    void clear(){if(bytes) bytes->clear();bytes.reset();}
    QStringList formats() const override{return {"text/plain","application/octet-stream","application/x-qindaqt-secret"};}
protected:
    QVariant retrieveData(const QString &type,QMetaType preferred) const override {
        if(type=="application/x-qindaqt-secret") return QByteArray();
        if(!bytes || (type!="text/plain" && type!="application/octet-stream")) return {};
        const auto value=bytes->bytes();
        const auto *data=reinterpret_cast<const char *>(value.data());const auto size=static_cast<qsizetype>(value.size());
        // Framework/requester allocations are created only on actual selection
        // transfer. The retained provider stores no ordinary plaintext byte array.
        if(preferred==QMetaType::fromType<QString>()) return QString::fromUtf8(data,size);
        return QByteArray(data,size);
    }
private:
    std::shared_ptr<qindaqt::keyring::SecureBuffer> bytes;
};
}
SensitiveClipboard::SensitiveClipboard(QObject *parent,int lifetimeMilliseconds):QObject(parent){
    m_timeout.setSingleShot(true);m_timeout.setInterval(lifetimeMilliseconds>0 && lifetimeMilliseconds<=30000?lifetimeMilliseconds:30000);
    connect(&m_timeout,&QTimer::timeout,this,&SensitiveClipboard::clear);
}
SensitiveClipboard::~SensitiveClipboard(){clear();}
bool SensitiveClipboard::copy(std::shared_ptr<qindaqt::keyring::SecureBuffer> bytes){
    if(!qobject_cast<QGuiApplication *>(QCoreApplication::instance()) || !bytes || bytes->size()>1024*1024) return false;
    auto *clipboard=QGuiApplication::clipboard();if(!clipboard) return false;
    clear();auto *mime=new SecretMime(std::move(bytes));m_owned=mime;
    clipboard->setMimeData(mime,QClipboard::Clipboard);
    if(clipboard->mimeData(QClipboard::Clipboard)!=m_owned) {clear();return false;}
    m_timeout.start();return true;
}
void SensitiveClipboard::clear(){
    m_timeout.stop();if(!m_owned) return;
    auto *mime=static_cast<SecretMime *>(m_owned.data());mime->clear();
    if(qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        auto *clipboard=QGuiApplication::clipboard();
        if(clipboard && clipboard->mimeData(QClipboard::Clipboard)==mime) clipboard->clear(QClipboard::Clipboard);
    }
    m_owned=nullptr;
}
}
