# V0_9_42 Review Notes

## Inputs reviewed

- `V0_9_39_build.log`
- `Upload_Thin_iOS_GUI_V0_9_39_ReusedCache_Check.zip`
- External V1-readiness / performance suggestions regarding GUI stability, source ZIP inventory, hashing, CSV export performance, and deferred structural refactors.

## V0_9_39 result

V0_9_39 built successfully on Windows/MSVC, including GUI, CLI, and self-test executables. The iOS reuse-cache run reached `complete_success`.

Observed V0_9_39 case metrics:

- iOS CoreSpotlight stores: 6
- Raw records: 344,445
- Compact raw key/value rows: 982,230
- Compact date candidates: 336,037
- Slim FFS lookup rows imported from cache: 1,592,440
- Full FFS inventory materialized: false
- Broad app DB records materialized: false
- Missing From FFS text detail rows: 23
- Missing From FFS text coverage showed visible text for the candidate groups present in the normal investigator export.

No DB/WAL guardrail failure, export stall, or Windows build failure recurred in the V0_9_39 thin upload.

## V0_9_42 implementation focus

V0_9_42 shifts from reuse-cache parser stability to V1-readiness performance work:

1. Faster CSV export path using direct SQLite text pointers, on-the-fly CSV escaping, and a 1 MiB output stream buffer.
2. Faster Windows evidence hashing reads by increasing the Win32 sequential-read buffer to 4 MiB.
3. Faster actual FFS ZIP inventory preparation by changing the generated 7-Zip inventory workflow from an external-process PowerShell pipeline with per-line regex matching to a raw `7z l -slt` text dump followed by a simple non-regex parser over `System.IO.File.ReadLines()`.
4. Added a dedicated Stage B fresh-ZIP test script for the current large iOS source so the next testing transition can verify actual ZIP enumeration/staging rather than only reuse-cache behavior.

## Intentional deferrals

- Broad Win32 global-state refactor remains deferred.
- Full SQL migration out of the GUI remains deferred unless a build/runtime issue requires it.
- Legacy V7 code removal remains deferred because it is not blocking the iOS V1 workflow and removing it may introduce avoidable regression risk.
- AFF4/APFS work remains on the roadmap after the iOS V1 path is stable, with the next macOS focus still on AFF4/APFS source inventory, Store-V2 extraction, and active file comparison.
