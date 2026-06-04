# Vestigant Spotlight V0_9_59 Notes

V0_9_59 is a V1 production-readiness cleanup after V0_9_57 compiled and ran on Windows. It improves the processing workflow and review workflow without changing parser interpretation logic.

Key changes:
- The Case Information bottom log is now the live processing log. It clears at run start, timestamps messages, mirrors run/progress status, and emits periodic heartbeat messages while processing continues.
- View loading now shows an explicit marquee progress indicator above the investigation grid and a loading message in the details pane so long SQLite view loads are not mistaken for hangs.
- The V1 GUI source selector now exposes only fully implemented Folder and ZIP intake paths. AFF4/APFS and raw image support remain roadmap items and are not presented as clickable V1 options.
- Legacy V7-only schema tables/indexes were removed from new case initialization.
- CLI/operator self-test mode is deprecated; the automated test executable uses an internal automated self-test path.
- Duplicate AFF4/APFS child/descendant root-tree probe output writers were consolidated into one traversal-output writer.

Validation summary:
- Linux CMake configure/build passed.
- VestigantSpotlightTests passed.
- C++20 syntax checks passed for modified non-Windows translation units.
- Windows/MSVC GUI compile and runtime validation remain required.

# Vestigant Spotlight

## V0_9_59 - Windows GUI forward-declaration compile hotfix

V0_9_59 is a focused Windows/MSVC GUI build hotfix after V0_9_56 reached the GUI compile stage and failed with `C3861: setReviewSummary identifier not found` in `src\gui\win32_gui.cpp`. The fix adds a forward declaration for `setReviewSummary(const std::wstring&)` before the custom view-set helper functions that call it. No parser, ingest, cache, ZIP, FFS inventory, app DB, export, or forensic interpretation behavior was intentionally changed.

Current version: 0.9.59

Vestigant Spotlight is a forensic review tool for macOS Spotlight Store-V2 and iOS CoreSpotlight investigations. The current V0_9 line emphasizes compact, investigator-focused iOS CoreSpotlight review, targeted app database parsing, Missing From FFS analysis, KnowledgeC/CoreDuet interaction context, and GUI review workflows.

## V0_9_59 - Windows MSVC batch-label build hotfix

V0_9_59 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.


## V0_9_55

V0_9_55 is a GUI-focused V1 readiness release. It adds supplied Vestigant logo branding, streamlines the Case Information / Build Processing workflow, uses the bottom Case Information log pane for live processing status messages, adds elapsed-time/throughput telemetry where measurable, adds case-persisted Custom view sets, and repairs tag-management schema availability for older existing case databases.

This version does not intentionally change parser, cache, ZIP staging, FFS inventory, app database classification, or forensic interpretation logic.

## Minimal validation

Build on Windows:

```powershell
Set-Location D:\Downloads
Get-FileHash .\VestigantSpotlightInv_V0_9_55.zip -Algorithm SHA256
Remove-Item -LiteralPath "T:\VestigantSpotlightInv_V0_9_55" -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath .\VestigantSpotlightInv_V0_9_55.zip -DestinationPath T:\ -Force
powershell -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_9_55\scripts\Build-V0_9_55.ps1"
```

Open an existing completed case to test GUI-only changes. Reuse-cache or fresh-ZIP ingest reruns are optional unless the Windows GUI test exposes a runtime issue or parser/staging behavior is changed in a later version.
