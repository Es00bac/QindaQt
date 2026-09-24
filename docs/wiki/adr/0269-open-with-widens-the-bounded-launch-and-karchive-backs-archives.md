# ADR-0269: Open With widens the bounded launch, and KArchive backs Compress and Extract

- **Status:** Accepted
- **Date:** 2026-09-24
- **Owners:** First-party applications (File Manager), with Settings (Default
  Applications store) and the Shell working group (launcher `Exec` grammar)
- **Supersedes:** None. It widens
  [ADR-0029](0029-file-manager-bounded-local-launch.md) for Open With only;
  opening a file with its default application is unchanged.
- **Superseded by:** None

## Context

W10 gives the File Manager the right-click set people expect from Finder.
The item menu has Open, Open With ▸, Open in New Window, Duplicate, Make Link,
Copy Path, Compress, Extract, Add to Sidebar, Get Info, Move to Trash, Delete
Permanently and, inside Trash, Put Back. The background menu has New Folder,
New File ▸, Paste, Open Terminal Here, Select All, Sort By ▸, View ▸, Show
Hidden Files and Get Info for the folder. The same commands appear in the
File and Edit menus with shortcuts.

Four existing decisions constrain this work:

- [ADR-0029](0029-file-manager-bounded-local-launch.md) lets the File Manager
  open a validated local file only through `QDesktopServices::openUrl()`, the
  desktop's default handler. It names an Open With chooser as a later outcome
  that needs its own ADR.
- The shared launcher grammar (`ExecFieldCodeExpander`, ADR-0164) drops
  `%f %F %u %U` because the launcher never hands an application a file.
- Settings → Default Applications
  ([ADR-0245](0245-write-only-supported-default-application-associations.md))
  owns the `mimeapps.list` store. Before this ADR it only read and wrote whole
  categories such as "Image viewer".
- Every local file operation goes through the identity-checked, cancellable
  `MutationController`/`LocalMutationBackend` pipeline, which has one busy
  slot. That pipeline has no archive support.

## Decision

1. **Open With is a bounded launch of an application the user names.**
   `OpenWithLauncher` checks each file the way ADR-0029 does: it must exist,
   a link is resolved once, and the target must be a readable regular file.
   It then plans the launch from the desktop entry the scan kept, using the
   shared `Exec` grammar. That grammar now puts caller-supplied local paths in
   place of `%f %F %u %U`. Each path is always a whole argument. A path is
   never joined to other text and is never the program. The URL codes get
   the local path, which the desktop entry specification allows, so no URL is
   built or guessed. An `Exec` that takes one file (`%f`, `%u`) starts once
   per file. An application whose `Exec` takes no file is refused rather than
   started without the files. A `Terminal=true` entry runs inside QQ_Term's
   `qqterm -e`, the same terminal policy as the shell launcher. A
   D-Bus-activatable entry is refused for now. A request holds at most 32
   files. Processes start detached, with no shell. The ordinary Open and a
   double-click still go through ADR-0029's default-handler launch.
2. **Settings owns file associations; the File Manager only uses its store.**
   `DefaultApplicationsStore` gains two per-type methods:
   `loadMimeTypeHandlers` returns the effective default and the associated
   handlers, in order: default, then Added Associations, then declared
   handlers. `saveMimeTypeDefault` writes one type's default. A new helper,
   `createSessionDefaultApplicationsStore`, builds the session store for both
   Settings and the File Manager, so both read and write the same files.
   - **Recommended applications** are the handlers of the file's type and of
     its parent types, taken from the shared MIME database. For example, a C
     source file also lists plain-text editors.
   - **Always Open With** writes the user's `mimeapps.list`, or a
     desktop-specific user file that already owns the key. If the chosen
     application does not declare the type, it is also recorded as the first
     Added Association, because lookup skips a default that is not associated.
     The change is reported as done only after a fresh lookup confirms the
     new default.
   - **Other Application…** lists the rows of the Applications place
     ([ADR-0262](0262-applications-is-browsed-in-the-file-managers-ordinary-views.md)).
     It therefore shows exactly the applications that place shows.
3. **Opening a folder elsewhere passes argv only.**
   - **Open Terminal Here** starts QQ_Term as
     `qqterm --working-directory <canonical folder>`.
   - **Open in New Window** starts the File Manager through
     `Desktop::FileBoundary::openLocalFolder`. That call first checks that
     the folder is still the one the listing showed.
4. **The new file operations are new kinds in the existing pipeline.**
   Each one is identity-checked, runs in the one busy slot and can be
   cancelled.
   - **New File** makes an empty file, which can be undone into Trash, or
     copies a template. Templates are the top-level, non-hidden regular files
     in `XDG_TEMPLATES_DIR`.
   - **Make Link** makes a relative symbolic link named "Link to …" next to
     the item, pointing at it by name.
   - **Delete Permanently** never goes through Trash and always asks first.
     Deleting an item inside the home Trash also removes its `.trashinfo`
     record.
   - **Compress** and **Extract** are described in decision 5.
   - **Duplicate** reuses Copy with Finder-style "name copy", "name copy 2"
     names.
   - **Put Back** reuses Restore, using the path saved in the item's Trash
     record.
   New names are chosen on the GUI thread as the first free name. Files are
   created exclusively, so an existing name is never replaced.
5. **KArchive is the archive library, behind one seam.** KDE Frameworks'
   KArchive is a tier-1 framework next to the KIO the File Manager already
   links; the Gentoo package is `kde-frameworks/karchive:6`. Only
   `KArchiveCodec` uses it, behind the `ArchiveCodec` interface.
   - **Compress** writes a deflate zip beside the first item: "name.zip" for
     one item, "Archive.zip" for several.
   - **Extract** reads zip and tar files, plain or compressed with gzip,
     bzip2, xz or zstd where the installed KArchive supports them. It always
     creates a new folder named after the archive. If the archive holds one
     top-level folder, that folder's contents go straight into the new folder,
     so the same name is not nested twice.
   - **Safety.**
     - Every file is opened relative to a parent folder that was itself
       opened without following links.
     - The archive is read from the exact file handle whose identity was
       checked.
     - Every entry name must be a single path component. `..`, `.`, empty
       names and names with a slash fail the whole Extract.
     - Links are created after everything else and are never followed.
     - Special files are not created, and setuid, setgid and sticky bits are
       dropped.
     - Limits are 20,000 entries and a depth of 64.
     - Both jobs can be cancelled between entries and between data chunks.
     - A failed or cancelled Extract removes the folder it created. A failed
       Compress removes only the archive file it created itself.
6. **A batch accepts its own writes to a folder.** Earlier items in a batch
   change the modification time of the folder they write into. The folder
   identity recorded when the batch was requested then failed every later
   item going into that folder. The controller now re-reads that folder's
   identity before such an item and accepts it only if the device and inode
   are still the same, so a replaced folder still fails. This also fixes
   multi-item paste into a single folder.
7. **The vocabulary lives in the public menu catalog**
   ([ADR-0260](0260-show-the-file-managers-menu-when-no-application-is-active.md)),
   so the desktop menu can offer the same entries.
   - `file.open` replaces `application.open` everywhere.
   - Properties is renamed **Get Info**. With nothing selected it describes
     the current folder.
   - Sort By uses commands, not checkable actions, like the view choices.
     Choosing the active sort again reverses its direction.
   - The behaviour is in the `FileActions` controller layer, so the QindaTK
     views of W11 inherit it.

## Consequences

- One association authority remains. A choice made in Settings shows up in
  Open With, and Always Open With shows up in Settings. The File Manager has
  no MIME table of its own.
- The launcher's own behaviour does not change: an empty `localFiles` still
  drops the file codes. `ExecPlan::fileArguments` and
  `LaunchPreparation::fileArguments` report how many files a plan took.
- Open With cannot yet use D-Bus-activatable applications, and it cannot
  report whether a started application later failed. This is the same limit
  ADR-0029 documents.
- The right-click set works on local folders only. In smb/sftp locations
  and in the Applications place these items are hidden and disabled. Put Back
  and Delete-in-Trash cover the home Trash only; per-volume Trash remains S4.
- Compress writes zip only, and nothing asks which format to use. Extract has
  no byte limit beyond disk space and Cancel, so a very large archive stops
  at disk-full and is removed.
- Packaging: the ebuild's RDEPEND must gain `kde-frameworks/karchive:6`. At
  build time, CMake requires `KF6Archive`.
- Tests:
  - `qindaqt.launcher-execution` and `application-catalog.launch-support`
    cover files handed to the `Exec` grammar.
  - `qindaqt.settings-default-apps-mime-types` covers per-type reads and
    writes.
  - `qindaqt.file-manager-open-with` covers the launcher and controller over
    fakes.
  - `qindaqt.file-manager-file-actions-mutation` covers each new operation
    and the archive jobs.
  - `qindaqt.file-manager-file-actions-ui` covers the menu rules and the
    catalog-to-controller path.
  - `qindaqt.file-manager-action-catalog` checks that no two actions share a
    shortcut.

## Revisit when

- The File Manager gains a D-Bus activation seam
  (`org.freedesktop.Application.Open`), or launches must go through a portal.
- W11 replaces the views. The actions must stay in `FileActions` and the
  controllers.
- Users ask for another compress format or for 7z/rar extraction.
- Per-volume Trash arrives (S4). Put Back and Delete Permanently would then
  need to cover volume trashes.
