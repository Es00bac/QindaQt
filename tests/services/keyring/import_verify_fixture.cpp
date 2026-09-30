// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring/collection_store.h>
#include <QCoreApplication>
#include <QTextStream>
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDir>
#include <map>
#include <vector>
#include <algorithm>
using namespace qindaqt::keyring;
namespace {
bool exact(const SecureBuffer &actual,const std::vector<unsigned char> &expected) {
    return actual.size()==expected.size() && std::equal(actual.bytes().begin(),actual.bytes().end(),expected.begin());
}
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if(argc==2 && QString::fromLocal8Bit(argv[1])=="--compositor-service") {
        QTextStream(stdout)<<QString(QindaQt::CompositorNames::service)<<"\n";return 0;
    }
    if(argc!=3) return 2;
    const QString root=QString::fromLocal8Bit(argv[1]);const QString mode=QString::fromLocal8Bit(argv[2]);
    std::size_t items=0,collections=0;bool ss=false,kw=false,portal=false;
    std::vector<unsigned char> portalBytes(64),gogcli(64);
    for(std::size_t n=0;n<64;++n) {portalBytes[n]=static_cast<unsigned char>(n);gogcli[n]=static_cast<unsigned char>((n*17)&255);}
    try {
        for(const auto &file:QDir(root).entryList({"*.qkr"},QDir::Files)) {
            ++collections;CollectionStore store(root.toStdString(),file.chopped(4).toStdString());
            SecureBuffer password(8);std::fill(password.bytes().begin(),password.bytes().end(),0x42);
            if(store.load()!=StoreError::None || !store.locked() || store.unlock(password.bytes())!=StoreError::None) return 3;
            const auto index=store.search({});if(!index.authenticated) return 4;
            for(const auto &id:index.ids) {
                ++items;const auto *item=store.item(id);if(!item) return 5;
                const auto &creator=item->metadata.creator;
                if(creator=="/legacy/login/password") {ss=true;if(!exact(item->secret,{0,255,16}) || item->attributes!=Attributes{{"app","gogcli"}}) return 6;}
                else if(creator=="/legacy/login/empty") {if(item->secret.size()!=0) return 7;}
                else if(creator=="/legacy/gogcli/account") {if(!exact(item->secret,gogcli) || item->attributes!=Attributes{{"service","gogcli"}}) return 8;}
                else if(creator=="same") {
                    kw=true;const auto folder=item->attributes.at("qindaqt.import.kwallet.folder");const auto type=item->attributes.at("qindaqt.import.kwallet.type");
                    if(folder=="Passwords") {if(type!="1" || !exact(item->secret,{0,80,0,119,0})) return 9;}
                    else if(folder=="Maps") {if(type!="3" || !exact(item->secret,{0,0,0,1,255,65})) return 10;}
                    else if(folder=="Streams") {if(type!="2" || !exact(item->secret,{0,255,1,7})) return 11;}
                    else return 12;
                } else if(creator=="org.example.Legacy") {if(!exact(item->secret,portalBytes)) return 13;}
                else if(creator=="QindaQt legacy Secret import") {
                    portal=true;if(!exact(item->secret,portalBytes) || item->attributes.at("qindaqt.portal.version")!="legacy-opaque-64"
                        || item->attributes.at("qindaqt.portal.source.wallet")!="kdewallet") return 14;
                } else return 15;
            }
        }
        if(mode=="both") return collections==4 && items==8 && ss && kw && portal?0:16;
        if(mode=="secret") return collections==3 && items==3 && ss && !kw && !portal?0:17;
        if(mode=="kwallet") return collections==2 && items==5 && !ss && kw && portal?0:18;
        return 19;
    } catch(...) {return 20;}
}
