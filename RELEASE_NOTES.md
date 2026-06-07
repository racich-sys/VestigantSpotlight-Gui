# Vestigant Spotlight V1.2.1 Release Notes

V1.2.1 is a stability-focused APFS/AFF4 worker update on top of the V1.2.0 virtual ListView release.

## Reviewed before change

- V1.2.0 source package.
- V1.2.0 Windows/MSVC build log.
- The build log shows CLI, tests, and GUI linked successfully and the compiled CLI reported `Vestigant Spotlight v1.2.0`.

## Changed

- Hoisted reusable guided APFS B-tree node buffers inside `src/parsers/aff4_probe_worker.cpp`.
- Reused the caller-provided horizontal next-leaf buffer in `aff4ApfsLoadNextLeafForProbe(...)` instead of allocating a temporary candidate-node vector.
- Reused vector backing storage in guided target inode and file-extent lookups to reduce repeated heap allocation in tight APFS traversal loops.
- Swapped resolved child node buffers into the reusable node buffer instead of copy-assigning them.

## Not changed

- No APFS copy-out interpretation changes.
- No Store-V2 parser changes.
- No iOS parser changes.
- No SQLite schema changes.
- No new exfiltration/destruction classification output was added.

## Test scope decision

- AFF4/APFS: thin test required.
- iOS: not required.
- Reason: the change touches AFF4/APFS guided lookup internals but is intended to preserve output semantics.
- Escalate to full AFF4/APFS test if thin output changes staged file counts, external comparison counts, mismatch counts, copy statuses, or APFS traversal stop reasons.

## Thin-create PowerShell command

```powershell
powershell -ExecutionPolicy Bypass -File T:\VestigantSpotlightInv_V1_2_1\scripts\Run-V1_2_1-macOS-AFF4-Probe-AndZip.ps1 -CleanOut
```

