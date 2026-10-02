// SPDX-License-Identifier: LGPL-3.0-or-later
#include "print_job.h"
#include <QFile>
#include <QSaveFile>
#include <QProcess>
#include <QStandardPaths>
#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>
namespace QindaQt::Services::Portal {
namespace {
class DescriptorInput final : public QIODevice {
public:
    explicit DescriptorInput(int fd) : m_fd(fd) { open(QIODevice::ReadOnly); }
    bool isSequential() const override { return true; }
protected:
    qint64 readData(char *data, qint64 max) override {
        if (m_offset >= 536870912) return 0;
        const auto n = pread(m_fd, data, static_cast<size_t>(qMin(max, 536870912 - m_offset)), m_offset);
        if (n >= 0) m_offset += n; return n;
    }
    qint64 writeData(const char *, qint64) override { return -1; }
private: int m_fd; qint64 m_offset = 0;
};
bool transfer(QIODevice &input, QIODevice &output) {
    QByteArray buffer(65536, Qt::Uninitialized);
    while (true) {
        const auto count = input.read(buffer.data(), buffer.size()); if (count < 0) return false; if (!count) return true;
        qint64 written = 0; while (written < count) { const auto n = output.write(buffer.constData() + written, count - written); if (n <= 0) return false; written += n; }
        if (auto *process = qobject_cast<QProcess *>(&output); process && process->bytesToWrite() > 262144 && !process->waitForBytesWritten(10000)) return false;
    }
}
}
bool runPrintCommand(const QString &command, const QStringList &arguments, QIODevice &input) {
    QProcess process;
    // Close/owner loss kills the ordinary helper. Its spool child must not
    // survive that retirement; already accepted physical jobs cannot be recalled.
    const pid_t parent = getpid();
    process.setUnixProcessParameters({QProcess::UnixProcessFlag::CloseFileDescriptors, 3});
    process.setChildProcessModifier([parent] { if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || getppid() != parent) _exit(2); });
    process.start(command, arguments); if (!process.waitForStarted(5000)) return false;
    const bool copied = transfer(input, process); process.closeWriteChannel();
    if (!copied || !process.waitForFinished(45000)) { process.kill(); process.waitForFinished(1000); return false; }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
bool submitPrint(QPrinter &printer, int fd, const PrintRunner &runner) {
    if (fd < 0) return false; DescriptorInput input(fd);
    if (!printer.outputFileName().isEmpty()) {
        QSaveFile output(printer.outputFileName()); if (!output.open(QIODevice::WriteOnly) || !transfer(input, output)) return false;
        return output.commit();
    }
    for (const auto *name : {"lpr-cups", "lpr.cups", "lpr", "lp"}) {
        const auto executable = QStandardPaths::findExecutable(QString::fromLatin1(name)); if (executable.isEmpty()) continue;
        const auto args = PrintArguments::arguments(&printer, printer.supportsMultipleCopies(), QString::fromLatin1(name), printer.pageLayout().orientation());
        return runner(executable, args, input);
    }
    return false;
}
}
