// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.h"

#include <QApplication>
#include <QComboBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent>

#include <sys/wait.h>

using namespace QindaQt::Apps::WindowsHandler;

class SetupWindow final : public QWidget {
public:
    explicit SetupWindow(const QString &path)
        : m_dataRoot(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                     + QStringLiteral("/qindaqt/windows-programs")),
          m_session(QProcessEnvironment::systemEnvironment())
    {
        setWindowTitle(QStringLiteral("Open Windows Program")); resize(540, 320);
        auto *layout = new QVBoxLayout(this);
        m_program = new QLabel(this); m_program->setTextFormat(Qt::PlainText); m_program->setWordWrap(true);
        layout->addWidget(m_program);
        auto *scope = new QLabel(QStringLiteral("Each program keeps its own Windows profile and data. File access uses your normal desktop permissions."), this);
        scope->setWordWrap(true); layout->addWidget(scope);
        m_runtime = new QComboBox(this); m_runtime->setAccessibleName(QStringLiteral("Windows runtime")); layout->addWidget(m_runtime);
        m_kind = new QComboBox(this); m_kind->addItems({QStringLiteral("Run a portable application"), QStringLiteral("Run an installer")});
        m_kind->setAccessibleName(QStringLiteral("Application or installer")); layout->addWidget(m_kind);
        m_status = new QLabel(this); m_status->setTextFormat(Qt::PlainText); m_status->setWordWrap(true); layout->addWidget(m_status);
        m_run = new QPushButton(QStringLiteral("Run"), this); m_run->setEnabled(false); layout->addWidget(m_run);
        m_chooseInstalled = new QPushButton(QStringLiteral("Choose installed program"), this); m_chooseInstalled->setVisible(false); layout->addWidget(m_chooseInstalled);
        auto *files = new QPushButton(QStringLiteral("Open Windows profile files"), this); layout->addWidget(files);
        connect(files, &QPushButton::clicked, this, [this] {
            if (!m_profileRoot.isEmpty()) { QDesktopServices::openUrl(QUrl::fromLocalFile(m_profileRoot + QStringLiteral("/prefix/drive_c"))); }
        });
        connect(m_chooseInstalled, &QPushButton::clicked, this, [this] {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Choose installed Windows program"),
                m_profileRoot + QStringLiteral("/prefix/drive_c"), QStringLiteral("Windows programs (*.exe);;All files (*)"));
            if (!selected.isEmpty()) { m_useExistingProfile = true; inspect(selected); }
        });
        connect(m_runtime, &QComboBox::currentIndexChanged, this, [this] { m_run->setEnabled(!m_jobPending && m_pid < 0 && m_inspection && m_inspection->error.isEmpty() && m_runtime->currentIndex() >= 0); });
        connect(m_run, &QPushButton::clicked, this, [this] { launch(); });
        m_processPoll = new QTimer(this); m_processPoll->setInterval(200);
        connect(m_processPoll, &QTimer::timeout, this, [this] {
            int status = 0; const pid_t observed = ::waitpid(m_pid, &status, WNOHANG);
            if (observed <= 0) { return; }
            m_processPoll->stop(); m_pid = -1;
            if (m_installer) {
                show(); m_chooseInstalled->setVisible(true); m_run->setEnabled(true);
                m_runtime->setEnabled(true); m_kind->setEnabled(true);
                m_status->setText(QStringLiteral("When installation finishes, choose the program installed in this Windows profile."));
            } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                show(); m_run->setEnabled(true); m_runtime->setEnabled(true); m_kind->setEnabled(true);
                m_status->setText(QStringLiteral("The Windows runtime command failed. The profile and data were retained. You can explicitly try again."));
            } else { QApplication::quit(); }
        });
        inspect(path);
    }
private:
    void closeEvent(QCloseEvent *event) override
    {
        m_closeRequested = true;
        if (m_cancelled) { m_cancelled->store(true); }
        if (!m_jobPending && m_pid < 0) { QApplication::quit(); }
        event->accept();
    }
    void inspect(const QString &path)
    {
        m_program->setText(QFileInfo(path).fileName()); m_run->setEnabled(false);
        m_chooseInstalled->setEnabled(false); m_runtime->setEnabled(false); m_kind->setEnabled(false);
        m_status->setText(QStringLiteral("Checking the program and installed runtimes…"));
        m_jobPending = true; const quint64 generation = ++m_inspectionGeneration;
        auto *watcher = new QFutureWatcher<ProgramInspection>(this);
        connect(watcher, &QFutureWatcher<ProgramInspection>::finished, this, [this, watcher, generation] {
            if (generation != m_inspectionGeneration) { watcher->deleteLater(); return; }
            m_inspection = std::make_shared<ProgramInspection>(watcher->result()); watcher->deleteLater();
            m_jobPending = false;
            if (m_closeRequested) { QApplication::quit(); return; }
            m_runtime->clear();
            for (const auto &runtime : m_inspection->runtimes) { m_runtime->addItem(runtime->label); }
            m_kind->setCurrentIndex(m_inspection->preferInstaller ? 1 : 0);
            const QString preferredId = m_useExistingProfile
                ? m_profileRuntimeId : m_inspection->preferredRuntimeId;
            if (!preferredId.isEmpty()) {
                int preferred = -1;
                for (qsizetype i = 0; i < m_inspection->runtimes.size(); ++i) {
                    if (m_inspection->runtimes[i]->id == preferredId) { preferred = static_cast<int>(i); }
                }
                m_runtime->setCurrentIndex(preferred);
            }
            m_run->setEnabled(m_inspection->error.isEmpty() && m_runtime->currentIndex() >= 0);
            m_chooseInstalled->setEnabled(true); m_runtime->setEnabled(true); m_kind->setEnabled(true);
            m_status->setText(m_inspection->error.isEmpty()
                ? QStringLiteral("Choose the runtime and whether this is an application or installer. Your existing profile and data are preserved.")
                : m_inspection->error);
        });
        const QString dataRoot = m_dataRoot;
        watcher->setFuture(QtConcurrent::run([path, dataRoot] { return inspectProgram(path, dataRoot); }));
    }
    void launch()
    {
        if (!m_inspection || m_runtime->currentIndex() < 0) { return; }
        m_run->setEnabled(false); m_chooseInstalled->setVisible(false);
        m_runtime->setEnabled(false); m_kind->setEnabled(false);
        m_status->setText(QStringLiteral("Preparing the Windows profile and starting the program…"));
        m_installer = m_kind->currentIndex() == 1;
        const auto inspected = m_inspection;
        const auto runtime = inspected->runtimes[m_runtime->currentIndex()];
        const ProgramKind kind = m_installer ? ProgramKind::Installer : ProgramKind::Portable;
        const QString root = m_dataRoot, reuse = m_useExistingProfile ? m_profileRoot : QString{};
        const QProcessEnvironment session = m_session;
        m_jobPending = true; m_cancelled = std::make_shared<std::atomic_bool>(false);
        const auto cancelled = m_cancelled;
        auto *watcher = new QFutureWatcher<ProfileLaunch>(this);
        connect(watcher, &QFutureWatcher<ProfileLaunch>::finished, this, [this, watcher, runtime] {
            const ProfileLaunch result = watcher->result(); watcher->deleteLater();
            m_jobPending = false;
            m_status->setText(result.message);
            if (!result.profileRoot.isEmpty()) { m_profileRoot = result.profileRoot; }
            m_profileRuntimeId = runtime->id;
            if (!result.submitted || result.processId <= 0) {
                if (m_closeRequested) { QApplication::quit(); } else {
                    m_run->setEnabled(true); m_runtime->setEnabled(true); m_kind->setEnabled(true);
                }
                return;
            }
            m_pid = result.processId; m_processPoll->start();
            if (!m_installer) { hide(); }
        });
        watcher->setFuture(QtConcurrent::run([inspected, runtime, kind, root, session, reuse, cancelled] {
            return launchProgram(*inspected, runtime, kind, root, session, reuse, {}, cancelled);
        }));
    }
    QString m_dataRoot, m_profileRoot, m_profileRuntimeId;
    QProcessEnvironment m_session;
    std::shared_ptr<ProgramInspection> m_inspection;
    QLabel *m_program = nullptr, *m_status = nullptr;
    QComboBox *m_runtime = nullptr, *m_kind = nullptr;
    QPushButton *m_run = nullptr, *m_chooseInstalled = nullptr;
    QTimer *m_processPoll = nullptr;
    int m_pid = -1;
    bool m_installer = false;
    bool m_useExistingProfile = false;
    bool m_jobPending = false, m_closeRequested = false;
    quint64 m_inspectionGeneration = 0;
    std::shared_ptr<std::atomic_bool> m_cancelled;
};

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("qindaqt-windows-runner"));
    QApplication::setOrganizationName(QStringLiteral("QindaQt"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.qindaqt.WindowsRunner"));
    application.setQuitOnLastWindowClosed(false);
    const QStringList arguments = application.arguments();
    if (arguments.size() != 2) { return 2; }
    SetupWindow window(QFileInfo(arguments[1]).absoluteFilePath()); window.show();
    return application.exec();
}
