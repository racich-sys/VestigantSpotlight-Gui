# V1.0.18 Deleted Files Manifest

No production files were deleted in V1.0.18.

The cleanup was code-path and packaging focused:
- app-runner iOS parsing now delegates through `IosAppDbParser`.
- filtered-view GUI export moved off the UI thread.
- normal thin-upload packaging no longer includes structural APFS diagnostics unless requested.
