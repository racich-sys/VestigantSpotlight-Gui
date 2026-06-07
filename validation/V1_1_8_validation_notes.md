# V1.1.8 Validation Notes

## Baseline reviewed

- Source baseline: V1.1.7.1.
- Uploaded V1.1.7.1 Windows/MSVC build log: decoded and reviewed; build completed and reported `Vestigant Spotlight v1.1.7.1`.
- Uploaded V1.1.7.1 macOS AFF4/APFS thin ZIP: reviewed; thin ZIP exists and denied raw upload filenames were absent.
- User-provided `BaselineVersionHistory.md` was treated as the append-only version history baseline.

## Changes validated locally

- Version metadata updated to V1.1.8.
- `docs/FULL_VERSION_HISTORY.md`, `docs/BaselineVersionHistory.md`, and `VERSION_HISTORY.md` now use the user-provided baseline with V1.1.8 appended at the top.
- Windows long-path helper API added in `src/core/path_utils.*`.
- APFS/AFF4 Store-V2 copy-out and decmpfs reconstruction file writes now use long-path-capable binary output on Windows.
- Logger writes are mutex-protected.
- SQLite close/checkpoint path now requests WAL truncate.
- Explicit WAL checkpoint/truncate status marker was added before upload packaging.
- APFS decmpfs reconstruction expected-size cap reduced to 256 MiB.

## Local checks performed

- C++20 syntax checks passed for changed/dependent source files:
  - `src/core/path_utils.cpp`
  - `src/core/logger.cpp`
  - `src/parsers/aff4_probe_worker.cpp`
  - `src/app/app_runner.cpp`
  - `src/codec/lzfse_codec.cpp`
  - `src/db/case_db.cpp`
  - `src/core/app_info.cpp`
- Linux CMake configure/build passed.
- CLI version check returned `Vestigant Spotlight v1.1.8`.
- Local self-test passed.

## Not validated here

- Windows/MSVC V1.1.8 build.
- Windows long-path CreateFileW runtime behavior.
- Windows GUI runtime.
- V1.1.8 macOS AFF4/APFS thin run.
- Current iOS runtime parity.
