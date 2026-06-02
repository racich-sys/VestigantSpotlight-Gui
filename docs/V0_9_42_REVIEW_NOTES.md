# V0_9_42 Review Notes

## Inputs reviewed

- V0_9_41 Windows/MSVC build log.
- V0_9_41 reuse-cache thin upload.
- Previously supplied V1-readiness performance recommendations regarding CSV export and iOS FFS ZIP inventory bottlenecks.

## Findings

V0_9_41 built successfully and the reuse-cache run completed successfully.  The compact iOS CoreSpotlight counts remained stable and the referenced-only FFS lookup path avoided the prior DB-full failure.

## Changes in V0_9_42

- Preserves the verified fast CSV exporter path from V0_9_41: raw SQLite column pointers, on-the-fly CSV escaping, and a 1 MiB output buffer.
- Changes the iOS focused ZIP inventory path so 7-Zip still dumps `7z l -slt` output to a raw text file, but the inventory CSVs are now rebuilt by native C++ parsing after the PowerShell extraction script returns.
- The generated PowerShell script no longer parses the raw 7-Zip listing with `System.IO.File.ReadLines()` for the 7-Zip fast path. It only creates the raw listing and performs focused extraction; .NET ZIP inventory remains as fallback when 7-Zip is unavailable.
- Adds run-status markers for native C++ ZIP inventory parsing: `ios_ffs_inventory_cpp_parser_complete` and `ios_ffs_inventory_native_cpp_ready`.
- Keeps normal iOS mode compact and Spotlight-first.

## Validation limits

The native C++ raw 7-Zip parser was compiled and unit/self-test paths were run in the Linux environment.  The actual speed benefit must be validated on the Windows fresh-ZIP Stage B run because this environment does not contain the large iOS FFS ZIP.
