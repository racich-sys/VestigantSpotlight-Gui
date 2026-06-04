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

# Troubleshooting

Current version: 0.9.59

## Build fails before CLI/GUI link

Upload the build log.  Recent MSVC-specific failure classes included oversized raw string literals (`C2026`) and GUI SQL helper scope mistakes.  V0_9_37 keeps raw-string static checks in validation and continues consolidating SQL/view ownership.

## Run stalls or stops writing

Collect state before rerunning:

```powershell
powershell -ExecutionPolicy Bypass -File "T:\VestigantSpotlightInv_V0_9_59\scripts\Collect-V0_9_59-DBBloat-State.ps1" `
  -CaseRoot "Q:\SpotlightCase\TestiOS_WhatsApp_V0_9_59_ReusedCache" `
  -OutZip "D:\Downloads\Upload_State_V0_9_37_NoWrites_Stopped_Check.zip" `
  -StopVestigant
```

## Even-looking counts

Check `exports/parser_limits_and_suppression_summary.csv`.  Native record count should be unlimited unless a max-record option is explicitly set.  Compact key/value and date counts are expected in normal iOS investigator mode.

## V0_9_59 - Windows MSVC batch-label build hotfix

V0_9_59 is a focused Windows build-stability hotfix after V0_9_55 failed with `The system cannot find the batch label specified - CompileCommon`. The no-CMake MSVC build script no longer uses `CALL :CompileCommon` batch subroutine labels. Common object compilation is now manifest-driven with a `FOR /F` loop and explicit object-existence checks. The batch file is packaged with CRLF line endings.

No parser, ingest, GUI workflow, cache, ZIP, FFS inventory, app database classification, export, or forensic interpretation behavior was intentionally changed from V0_9_55.

