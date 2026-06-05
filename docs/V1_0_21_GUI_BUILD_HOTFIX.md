# V1.0.22 GUI Build Hotfix

V1.0.20 intentionally moved export/database work out of `win32_gui.cpp`, but the small Win32 ListView rendering helpers for the Selected Row Details pane were accidentally removed from the GUI translation unit.

V1.0.22 restores only these local UI rendering helpers:

- `ensureDetailsListColumns()`
- `resizeDetailsListColumns()`
- `clearDetailsList()`
- `addDetailsListRow()`

These functions are not database/export business logic. They operate directly on the `gRowDetails` ListView handle and therefore remain appropriate in the GUI file until a future `MainWindow`/detail-pane class extraction.

No macOS AFF4/APFS extraction, iOS CoreSpotlight extraction, Apple/lzfse codec behavior, Store-V2 parsing, or database schema behavior changed.
