# V1.0.17 Apple/lzfse Vendor Enablement

## Source accepted for this build

The project now vendors the uploaded Apple/lzfse source package under:

```text
third_party/lzfse/
```

Vendor manifest:

```text
third_party/lzfse/VESTIGANT_VENDOR_MANIFEST.txt
```

Recorded ZIP hash:

```text
23855f54ff38ff2f679f79730d20df970dcd3f6cd5ad33505fcdc4220b3ab158  lzfse-master.zip
```

The repository reviewed for source provenance is:

```text
https://github.com/lzfse/lzfse
```

Observed latest commit from GitHub connector review:

```text
8ca039302ee20ae9ee39d2d00ab0d6f652352a10  Add const to tables in lzfse_internal.h
```

## Build behavior

When `third_party/lzfse/src/lzfse.h` exists, both the CMake and no-CMake MSVC build paths define:

```text
VESTIGANT_HAS_LZFSE=1
```

The build compiles these Apple/lzfse decoder sources into the common object set:

```text
third_party/lzfse/src/lzfse_decode.c
third_party/lzfse/src/lzfse_decode_base.c
third_party/lzfse/src/lzfse_fse.c
third_party/lzfse/src/lzvn_decode_base.c
```

## Runtime behavior

The AFF4/APFS decmpfs resource-fork reconstruction path now uses the Apple decoder adapter for compression types:

```text
8  LZVN_RSRC
12 LZFSE_RSRC
```

Successful rows are expected to use statuses such as:

```text
COPIED_DECOMPFS_RESOURCE_FORK_LZVN
COPIED_DECOMPFS_RESOURCE_FORK_LZFSE
```

Failures remain explicit. The tool must not silently emit uncertain decompressed data. Examples:

```text
DECOMPFS_LZFSE_LZVN_DECODE_FAILED
LZFSE_EXPECTED_OUTPUT_SIZE_UNSAFE
LZFSE_DECODE_OUTPUT_EXCEEDED_EXPECTED_CHUNK_SIZE
```

## Validation benchmark for the next AFF4 run

The next thin upload should be checked for:

1. `aff4_apfs_spotlight_file_copy_out_summary.json` reports `APPLE_LZFSE_REFERENCE_CODEC_ENABLED`.
2. LZVN/LZFSE resource-fork rows appear where compressed cache files are present.
3. `RELATIVE_PATH_SIZE_MISMATCH` and `EXTERNAL_ONLY` cache-file counts fall if the mismatches were caused by compressed resource forks.
4. The external comparison should move from size-only mismatch toward relative-path hash matches for decmpfs cache files.
5. Any failed decode must remain visible in CSV/summary output with inode/file/relative-path provenance.
