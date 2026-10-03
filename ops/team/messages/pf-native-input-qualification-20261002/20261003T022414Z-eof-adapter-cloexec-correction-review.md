# EOF adapter same-descriptor correction — ACCEPT

- Exact reviewed adapter SHA256: d9162a65bda6a7c32fc8e6ddba9a124301b265721dd1cd9166963acb1b476b46.
- Unchanged driver SHA256: 05bb16c92d68bba5909d9169275338470925925d923ddd405ab856843b0264be.
- Root-owned operational files: credential-migration/diagnostic_empty_pipe.py and run_laptop_import.py under ignored build/install-final-20261002.
- Product source base remains5917da5c054ccd8ed9fd0be5a2ac9db549a4bb1c; production is unchanged.
- Independent reviewer: qinda_icon_brand_audit. Exact local helper bytes read/hashed; no execution, provider calls, tests, secrets, UI, build or product edits.

ACCEPT this operational correction. Immediately after os.dup2(read_fd,3,inheritable=True), explicit os.set_inheritable(3,True) now clears CLOEXEC even when read_fd already equals3. The anonymous read pipe remains owned/validated, its writer is already closed, any distinct original read descriptor closes after duplication, and execv receives the same unchanged importer with FD3. No password bytes, synthetic admission or source-policy change occurs.

Prior review missed the same-FD CLOEXEC case; execution acceptance for adaptere430 is superseded by this exactd916 binding. Root's direct descriptor metadata demonstrated dup2(3,3) retained non-inheritability, and actual GNOME trace completed two full GetSecrets/ReadAlias/Close passes before code7. The source7 observation alone was not proof of a GNOME acquisition defect: Passwords construction also occurs after planning and before catalog opening. The missing transferred pipe is consistent with that boundary. No GNOME production fix is justified from this evidence.

The fresh per-source scratch requirement remains unchanged. Root must not reuse a prior bootstrap/catalog destination; keep previous attempt evidence immutable. One actual GNOME scratch diagnostic is the next gate. Continuing genuine native admission, successful nonempty acquisition and batch validation should reach Cancelled4 at EOF with scratch bootstrap metadata and zero imported encrypted .qkr files. Code4 is cancellation, not sealed migration. Any different actual outcome remains a new receipt, not permission for an unchanged retry.

Checks: exact adapter/driver SHA256 independently matched. No runtime/tests executed by reviewer. Only own board/new timestamped reply changed; stop available after verdict preservation.
