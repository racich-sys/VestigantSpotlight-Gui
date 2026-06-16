# V1.6.40.1.1 macOS Native Probe Promotion SQL Filter Fix

## Triggering evidence

The V1.6.35 macOS zipped Spotlight thin run confirmed that external dbStr maps were loaded successfully: `dbStr-1=PARSED:454/454` and `dbStr-2=PARSED:9981/9982`, with native decode attempts reporting `properties_count=454` and `categories_count=9981`.

However, the same thin output showed `native_path_probe_artifacts_updated=0` even though `native_key_values_high_value_sample.csv` contained native file path probe candidates such as `/Users/luducrey/Library/Containers/com.microsoft.teams2/Data/Library/Application` for records whose current raw path was only `/Application`.

## Root cause

The V1.6.34 native path probe promotion added a C++ safety decision that can recognize and replace weak basename-only paths, but the SQL pre-filter excluded rows that already had any absolute path and non-placeholder name. That prevented the C++ safety logic from ever seeing `/Application` -> `/Users/.../Application` candidate pairs.

## Fix

V1.6.40.1.1 removes the premature weak-path SQL pre-filter for native file path probe rows. SQL now selects absolute native path probe candidates, and C++ performs the final safety decision with `shouldApplyNativeProbePath()`.

V1.6.40.1.1 also adds a second bounded promotion pass for `__native_probe_basename_candidate_%` values. This pass only updates artifact names/display names when the current name/display value is blank or a known placeholder, and it rejects URLs, paths, control bytes, tiny/noisy values, and overlong names.

## New metrics/status

- `native_path_probe_artifacts_updated`
- `native_basename_probe_artifacts_updated`
- `enrichment_native_basename_probe_apply_complete`

## Expected result

The macOS zipped Spotlight dataset should now show native probe updates above zero where safe longer path candidates exist. GUI rows should improve for records that have native path or basename candidates, while records without any usable name/path evidence remain `------NONAME------`.
