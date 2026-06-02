# V0_9_38 Review Notes

## Inputs reviewed

- `V0_9_37_build.log`
- `Upload_Thin_iOS_GUI_V0_9_37_ReusedCache_Check.zip`

## Findings

V0_9_37 built successfully on Windows/MSVC, including GUI, CLI, and self-test binaries.  The reuse-cache iOS run did not complete successfully.  It failed during native parsing after the SQLite guardrail was reached:

- stage: `failed_sqlite_size_guardrail`
- context: `native_parse`
- parsed items at guardrail: approximately `340,000`
- raw key/value rows at guardrail: approximately `978,712`
- raw date candidates at guardrail: approximately `332,422`
- database size at guardrail: approximately `5,464,506,368` bytes
- WAL size at guardrail: `0` bytes

The failure class is not broad row-count explosion.  The row counts were near prior stable compact-mode values.  The most likely cause is that V0_9_37 increased same-record Spotlight text context storage from the earlier compact level to 4,096 bytes / 12 fields per reference-bearing iOS record, which made the SQLite database exceed the 5 GiB safety guardrail before parsing completed.

A secondary failure also occurred: `sqlite exec failed: cannot commit - no transaction is active`.  This was caused by the fatal guardrail exception being swallowed by an intermediate metadata-parse catch block after a commit/checkpoint cycle.

## V0_9_38 corrective action

V0_9_38 keeps Missing From FFS text visibility, but restores a bounded text-context budget suitable for normal investigator mode:

- compact text context max bytes: `1,800`
- max same-record text fields retained: `8`
- max bytes per individual text field sample: `320`

This should keep content visible in Missing From FFS detail reports while avoiding the V0_9_37 database-size regression.  If full raw text/property values are required, they remain a diagnostic/support workflow rather than a normal investigator default.

V0_9_38 also fixes fatal guardrail propagation so SQLite guardrail hits stop cleanly and do not continue into secondary COMMIT errors.
