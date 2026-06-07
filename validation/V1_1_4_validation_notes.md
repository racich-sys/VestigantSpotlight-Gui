# V1.1.4 Validation Notes

## Baseline reviewed

- Source baseline: `VestigantSpotlightInv_V1_1_3.zip`.
- Windows/MSVC baseline build log: `V1_1_3_build.log`.
- Baseline thin ZIP: `Upload_Thin_MacOS_AFF4_V1_1_3.zip`.
- Baseline build status: V1.1.3 build completed and reported `Vestigant Spotlight v1.1.3`.
- Thin ZIP status: generated successfully; denied raw upload filenames were absent.
- AFF4/APFS baseline counts reviewed from thin ZIP:
  - `raw_record_count=25000`
  - `raw_key_value_count=2181`
  - `raw_date_candidate_count=25000`
  - `artifact_count=25000`
  - `external_file_count=4123`
  - `vestigant_file_count=8986`
  - `file_match_rows=2213`
  - `external_only_rows=1424`
  - `vestigant_only_rows=6710`

## V1.1.4 implemented changes

- Added bounded bplist offset-table validation metadata to the existing bplist context summary:
  - `offset_table_bytes`
  - `offset_table_status=parsed`
  - `top_object_offset_rel=<offset|invalid>`
- Added checked-artifact snapshot helpers in the Win32 GUI and used them when constructing review/export requests.
- Strengthened the GUI ingest launch gate using `compare_exchange_strong` to reject repeated `Build / Process Case` starts before a second worker is created.
- Updated workflow ledger, handoff, roadmap, suggestions/fixes tracker, release/history notes, and validation notes.

## Not changed

- No live APFS extraction/traversal behavior changed.
- No Store-V2 parser schema changed.
- No SQLite schema changed.
- No full NSKeyedArchiver UID graph decoding was added.
- No live APFS absolute-path substitution was added.
- The dynamic AFF4/APFS probe monolith remains tracked for a dedicated high-risk refactor.

## Local validation performed

Pass 1:

```text
g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/parsers/native_storedb_parser.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/gui/gui_export_worker.cpp
g++ -std=c++20 -Isrc -fsyntax-only src/core/app_info.cpp
```

Pass 2:

```text
g++ -std=c++20 -DVESTIGANT_HAS_LZFSE=1 -Isrc -Ithird_party/lzfse/src -fsyntax-only src/app/app_runner.cpp
cmake -S . -B build-cmake-validate
cmake --build build-cmake-validate -j2
./build-cmake-validate/VestigantSpotlightCli --version
./build-cmake-validate/VestigantSpotlightTests build-cmake-validate/selftest_out
```

Results:

- Linux CMake configure passed.
- Linux CMake build passed.
- CLI version reported `Vestigant Spotlight v1.1.4`.
- Local self-test passed.

## Not validated here

- Windows/MSVC V1.1.4 build.
- Windows GUI runtime.
- V1.1.4 macOS AFF4/APFS thin run.
- iOS runtime parity after previous EvidenceIntake changes.
