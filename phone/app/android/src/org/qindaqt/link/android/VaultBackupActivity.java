// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.android;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.WindowManager;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Invisible helper that hosts the Storage Access Framework picker for backups, so the Qt activity
 * needs no onActivityResult plumbing. It carries no secrets in its intent: the encrypted export
 * bytes and the import passphrase wait in {@link VaultBackupFlow}.
 *
 * AGENT-GUARD: keep FLAG_SECURE and excludeFromRecents (manifest). Reads are capped at 64 MiB so a
 * hostile picked file cannot exhaust memory.
 */
public final class VaultBackupActivity extends Activity {
    private static final int REQ = 77;
    private static final int MAX_READ = 64 * 1024 * 1024;
    private String op = "import";

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        op = String.valueOf(getIntent().getStringExtra("op"));
        if (b != null) return;   // re-created while the picker is up: wait for its result
        Intent i;
        if (op.equals("export")) {
            i = new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
                .setType("application/octet-stream").putExtra(Intent.EXTRA_TITLE, "qindalink-vault.qlvb");
        } else {
            i = new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*");
        }
        startActivityForResult(i, REQ);
    }

    @Override protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        VaultBackupFlow flow = VaultController.get(this).backup;
        Uri uri = data == null ? null : data.getData();
        if (req != REQ || res != RESULT_OK || uri == null) {
            if (op.equals("export")) { flow.takeExport(); flow.exportFinished(false, true, 0); } else flow.importCancelled();
            finish();
            return;
        }
        new Thread(() -> {
            try {
                if (op.equals("export")) {
                    byte[] bytes = flow.takeExport();
                    if (bytes == null) throw new IOException("nothing to write");
                    try (OutputStream out = getContentResolver().openOutputStream(uri, "wt")) {
                        if (out == null) throw new IOException("no stream");
                        out.write(bytes);
                    }
                    flow.exportFinished(true, false, 0);
                } else {
                    flow.applyImport(readAll(uri));
                }
            } catch (IOException | RuntimeException e) {
                if (op.equals("export")) flow.exportFinished(false, false, 0); else flow.importCancelled();
            }
            runOnUiThread(this::finish);
        }, "vault-backup-io").start();
    }

    private byte[] readAll(Uri uri) throws IOException {
        try (InputStream in = getContentResolver().openInputStream(uri)) {
            if (in == null) throw new IOException("no stream");
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[16384];
            int r;
            while ((r = in.read(buf)) > 0) {
                out.write(buf, 0, r);
                if (out.size() > MAX_READ) throw new IOException("file too large");
            }
            return out.toByteArray();
        }
    }
}
