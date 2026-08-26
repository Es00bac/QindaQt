// SPDX-License-Identifier: GPL-3.0-or-later
#include "captureenvironment.h"
#include "shellpreviewapplication.h"

#include <QCoreApplication>
#include <QGuiApplication>

int main(int argc, char *argv[])
{
    QindaQt::Shell::configureCaptureEnvironment(argc, argv);

    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-shell-preview"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    QindaQt::Shell::ShellPreviewApplication preview(application);
    return preview.run();
}
