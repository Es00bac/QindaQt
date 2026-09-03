// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/font_discovery/font_discovery.h>

#include <QFile>
#include <QTemporaryDir>

#include <iostream>

int main()
{
    using namespace QindaQt::Services::FontDiscovery;

    if (!FontDiscoveryRequest::productionDefault().isWellFormed()) {
        std::cerr << "Installed consumer: production default request is malformed\n";
        return 1;
    }

    QTemporaryDir stage;
    if (!stage.isValid()) {
        std::cerr << "Installed consumer: cannot create a temporary stage\n";
        return 2;
    }
    const QString configPath = stage.filePath(QStringLiteral("fonts.conf"));
    {
        QFile file(configPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            std::cerr << "Installed consumer: cannot write injected config\n";
            return 3;
        }
        file.write("<fontconfig/>");
    }

    // An injected empty configuration enumerates exactly the injected
    // directories (none here) and never the host default configuration.
    FontDiscoveryRequest request;
    request.configurationFile = configPath;
    const FontDiscoveryProvider provider(request);
    const FontDiscoveryResult result = provider.discover();
    if (!result.available || !result.facts.isEmpty() || result.truncated) {
        std::cerr << "Installed consumer: unexpected injected-empty discovery outcome\n";
        return 4;
    }

    FontDiscoveryRequest malformed;
    malformed.limits.maximumFacts = -1;
    if (FontDiscoveryProvider(malformed).discover().available) {
        std::cerr << "Installed consumer: malformed request was not rejected\n";
        return 5;
    }

    std::cout << "Installed FontDiscovery consumer verified successfully\n";
    return 0;
}
