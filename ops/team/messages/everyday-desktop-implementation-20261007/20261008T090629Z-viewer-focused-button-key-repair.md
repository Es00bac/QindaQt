# Viewer focused-key source repair

Actual predecessor d1ef28682868c558f75b36aea5d4a61a5f4e8e0f built all seven strict targets0/11.158s. The eight owning rows ran: six pass, normal and2x text UI fail, exit8/40.602s. Both observe no stateChanged from focused Previous Return (test39); parent continues after helper failure and later Escape fails(test134). These raw results are failures, not keyboard qualification.

The installed public Tk.Button composes T.Button and exposes guarded click; it supplies no Return/Enter handler. The Viewer-private inline TextActionButton uses public enabled and click with Keys.BeforeItem, consumes both keys, and preserves toolkit availability/busy, Space and accessible press. Dialog primary remains gated and every existing native assertion stays. Actual rerun will determine whether Escape is a separate failure; no speculative Escape repair.

Changed paths: ViewerTextDialog.qml, owning Viewer wiki, own board and this receipt. All native eight-row, screenshots and exact independent review gates remain. R18 installed payload is unchanged; no ED milestone closes.
