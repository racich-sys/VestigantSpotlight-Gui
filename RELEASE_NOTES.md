# V1.0.26.1

V1.0.26.1 is a thin-upload packaging hotfix after the V1.0.26 AFF4/APFS run and external comparison completed but the thin ZIP failed during PowerShell relative-path inventory generation.

## Changed

- Fixed `tools/Create-SourceProbeUploadZip.ps1` so `Get-RelativePathForThinInventory` no longer uses `[char]'\\'`, which Windows PowerShell treats as a two-character string and rejects.
- Reused the robust relative-path helper for `ExtractedSpotlight` copy paths.
- Changed `reader_tools_file_inventory.txt` to use relative paths instead of full local paths.
- Added `scripts/Package-V1_0_26_1-macOS-AFF4-ThinFromExistingCase.ps1` for packaging an already-completed V1.0.26 AFF4/APFS case without rerunning the probe.
- Added `docs/CONTINUATION_HANDOFF.md`, `docs/ROADMAP_CHECKLIST.md`, and `docs/SUGGESTIONS_AND_FIXES_TRACKER.md`.

## Not changed

- No APFS traversal, copy-out, Store-V2 parsing, iOS parsing, database schema, or GUI behavior was intentionally changed.

## Validation

- Reviewed the uploaded V1.0.26 build log; the Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.26`.
- Reviewed the user-reported wrapper output showing the AFF4/APFS probe and external comparison completed before packaging failed.
- Local syntax/text checks were performed for modified C++ and PowerShell packaging files. Windows/MSVC V1.0.26.1 validation remains required.

# V1.0.26

- Fixed the remaining thin-upload raw-log leak in the standalone source-probe upload tool by denying raw tool outputs and full raw file inventories.
- Added matching in-app thin-upload deny-list policy for raw AFF4/iOS tool logs and full file-inventory CSVs.
- Updated thin-upload inventory text files to report relative paths instead of full local paths.
- Added bounded hidden Windows subprocess waits to avoid indefinite hangs from prompted/wedged external tools.
- Updated exact AFF4/ZIP byte reads on Windows to use 64-bit `_fseeki64`.
- No APFS traversal, Store-V2 parsing, iOS parsing, GUI schema, or APFS diagnostic writer movement is included in this version.

# V1.0.25

Thin Upload security and iOS staging performance hardening after the V1.0.24.1 Windows/MSVC build succeeded.

## Changed

- Removed raw AFF4/iOS extraction tool logs and generated extraction helper scripts from Thin Upload copy lists.
- Kept high-level run logs in Thin Upload: `run_status.txt`, `last_stage.txt`, `run_progress.tsv`, `last_progress.tsv`, and `VestigantSpotlight.log`.
- Replaced hardcoded export CSV bundle lists with dynamic copying of regular top-level `exports/*.csv` files.
- Kept the existing `exports/upload_samples` recursive copy.
- Changed `countCsvDataRows()` from line-by-line `std::getline()` allocation to binary chunk newline counting.
- Changed staged iOS app-database output path normalization to use `std::filesystem::path::lexically_normal()` plus per-component sanitization.
- Added direct Windows `CreateProcessW` helpers with stdout/stderr log redirection for selected hidden tool/script execution paths.

## Not changed

- macOS AFF4/APFS traversal, copy-out, staging, Store-V2 parsing, or external-compare behavior.
- APFS diagnostic writer locations.
- iOS CoreSpotlight parser behavior, app DB parser behavior, schema, or GUI views.
- Apple/lzfse codec behavior.
- GUI export worker behavior from V1.0.24.1.

## Validation

- Reviewed uploaded `V1_0_24_1_build.log`; Windows/MSVC build completed and reported `Vestigant Spotlight v1.0.24.1`.
- Reviewed uploaded `Upload_Thin_MacOS_AFF4_V1_0_24_1.zip`; the run reached `complete_source_probe` and staged/parsed the expected AFF4/APFS Store-V2 baseline.
- Ran local C++20 syntax check for `src/app/app_runner.cpp`; it passed.
- Windows/MSVC V1.0.25 validation is still required.
