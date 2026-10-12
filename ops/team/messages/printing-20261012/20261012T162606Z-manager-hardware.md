# Samsung hardware and queue evidence

Normal Portage install: `emerge --oneshot --noreplace net-print/splix net-print/cups-filters app-admin/system-config-printer`, exit 0, 17 new packages. Host MAKEOPTS remained `-j32 -l16`; remote-helper delays resolved by its normal fallback. No unrelated pending config update was adopted.

Targeted USB add replay invoked upstream `configure-printer@` and auto-created/defaulted `ML-2160-Series` using `samsung/ml2160.ppd.gz`. Installed Qt CUPS plugin enumerates that same queue and 23 paper sizes. Installed Gentoo printer desktop entry validates (only an upstream multiple-category hint).

Initial test submission failed unsupported `text/plain` because CUPS was already running before filters merged. Reloading the scheduler loaded the new MIME/conversion data. CUPS job 2 then reached state 9 / `job-completed-successfully`, one sheet; printer returned to state 3 / idle / no reasons. Known held fixture job 3 reached state 4, then ordinary-user cancel reached state 7 with zero sheets. No user document was opened or printed. Physical readable-paper confirmation remains pending.

Next: exact Office candidate and generated-document hardware fixture, independent overlay/Office review, Portage-owned default stack and Office package adoption, source integration/preservation on qinda. Root exclusively owns physical printer actions.
