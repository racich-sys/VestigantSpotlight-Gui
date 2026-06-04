# V0_9_53 Review Notes

## Scope
GUI-only V1 usability hotfix after V0_9_51 built and the bottom details pane was visible, but the field/value rendering was still not a true left/right table.

## Input reviewed
- User screenshot of V0_9_51 GUI showing the bottom details pane on iOS Investigator Super Timeline.
- User feedback: field identifier in the bottom window is not a good UI; field name should be on the left and metadata on the right; the pane should be resizable; row-viewing pane is not needed on Case Information.

## Changes
- Replaced the selected-row details RichEdit-style text pane with a true Win32 ListView report table.
- Details pane now has two columns: `Field` and `Metadata / Value`.
- Section dividers are represented as full-width rows with lightly different background colors via ListView custom draw.
- Preserved investigator grouping order: text first, dates second, then paths, people/apps/context, status/warnings, identifiers/provenance, counts/sizes, and other fields.
- Kept the draggable splitter and automatic column resizing.
- Added explicit hiding of details-pane controls when the Case Information or Tags/Notes tabs are active.
- Added RichEdit header compatibility guard and removed PARAFORMAT2W dependency from this path by no longer using RichEdit for the details table.

## Forensic behavior
No parser, ingest, cache, ZIP, FFS inventory, app database, SQLite export, or evidence interpretation behavior was intentionally changed.
