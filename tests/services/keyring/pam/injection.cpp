// SPDX-License-Identifier: GPL-3.0-or-later
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
// Prove injected startup code runs inside the actual binary. Its untrusted
// invocation must still be refused before any token payload.
__attribute__((constructor)) static void mark() {
    const char *path=std::getenv("QINDAQT_FIXTURE_INJECTION_MARK");
    if (!path) return;
    const int fd=open(path,O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW,0600);
    if (fd>=0) { const char value='1';write(fd,&value,1);close(fd); }
}
