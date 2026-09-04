// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "../common/shelltokenpublisher.h"

#include <QCoreApplication>
#include <QDebug>

namespace QindaQt::Shell {

bool ShellRuntimeApplication::initializeTokens(QString *error)
{
    m_tokenPublisher =
        std::make_unique<ShellTokenPublisher>(m_engine, m_themes, this);
    connect(m_tokenPublisher.get(), &ShellTokenPublisher::publicationFailed,
            this, [](const QString &message) {
                qCritical().noquote()
                    << "QindaQt shell token publication failed:" << message;
                QCoreApplication::exit(4);
            });
    if (m_tokenPublisher->start(error)) {
        return true;
    }
    if (error != nullptr) {
        *error = QStringLiteral("QindaQt shell token publication failed: %1")
                     .arg(*error);
    }
    return false;
}

} // namespace QindaQt::Shell
