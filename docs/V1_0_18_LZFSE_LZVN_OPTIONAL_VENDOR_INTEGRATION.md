# V1.0.18 LZFSE/LZVN Optional Vendor Integration

## Source decision

V1.0.18 recognizes Apple/lzfse as the vetted codec source for future APFS decmpfs LZFSE/LZVN reconstruction. The project does not fetch source during normal builds; it requires the source to be explicitly vendored under:

```text
third_party/lzfse/
```

This keeps forensic builds reproducible and reviewable.

## Build behavior

If `third_party/lzfse/src/lzfse.h` and the expected decoder C files are present, both CMake and `build_windows_msvc.bat` define:

```text
VESTIGANT_HAS_LZFSE=1
```

and compile the Apple decoder sources into the project.

If the source is absent, the code still builds. LZFSE/LZVN decode attempts return:

```text
LZFSE_CODEC_NOT_COMPILED
```

and emit no reconstructed bytes.

## Vendor helper

Use:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\Prepare-LzfseThirdParty.ps1 -LzfseZip D:\Downloads\lzfse-master.zip -ExpectedSha256 <sha256>
```

The helper unpacks the source tree, verifies required files, and writes a vendor manifest. The `-ExpectedSha256` parameter should be used for final forensic builds.

## Runtime integration point

The existing APFS decmpfs resource-fork reconstruction path now calls `decodeAppleLzfseOrLzvnChunk()` for compression types 8 and 12 when the codec is compiled in. Decode failures are explicit and non-fatal to the process; they are recorded as skipped/failed reconstruction statuses.

## Remaining benchmark before production claim

Before claiming complete LZFSE/LZVN support, run a codec-enabled build against known-good compressed/decompressed vectors and against the external Spotlight reference comparison. The required evidence is:

- codec source SHA-256/vendor manifest;
- MSVC and Linux build pass;
- LZFSE vector pass;
- LZVN vector pass;
- decmpfs inline/resource-fork decode tests;
- AFF4/APFS external comparison hash parity improvement.
