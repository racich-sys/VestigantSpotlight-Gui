# V1.6.41.1 macOS Store-V2 External dbStr Map Loading

## Triggering evidence

The user uploaded `store.db`, `.store.db`, and a small Store-V2 component package from the macOS Spotlight database that had produced GUI rows with placeholder names.

Direct inspection showed:

- both `store.db` and `.store.db` use the native Store-V2 signature `8tsd`;
- both headers have zero in-header dictionary block pointers (`idx11`, `idx21`, `idx41`, `idx81_1`, `idx81_2` all zero);
- adjacent external `dbStr-*` map files are present;
- `dbStr-1.map.*` contains property definitions such as `kMDItemContentTypeTree`, `kMDItemContentType`, and `_kMDItemGroupId`;
- `dbStr-2.map.*` contains category/content type strings such as `public.message`, `com.apple.mail.emlx`, and `public.contact`.

## Root cause

The native parser already had code for external `dbStr-*` map loading, but `shouldLoadExternalDbStrMaps()` only enabled it for iOS/CoreSpotlight-like paths or inferred iOS stores. A macOS `.Spotlight-V100/Store-V2/<GUID>/store.db` with zero in-header dictionary pointers therefore skipped the adjacent external maps and proceeded with empty property/category dictionaries.

That caused weak structured decoding and pushed the GUI to placeholder names even though string/path evidence existed in the Store-V2 sidecar files.

## Fix

V1.6.41.1 changes external dbStr map detection to be component-driven instead of iOS-path-driven.

The parser now loads external `dbStr-*` maps when:

- the native Store-V2 header is version 2;
- `idx11 == 0`; and
- required adjacent map components exist, including `dbStr-1.map.data`, `dbStr-1.map.offsets`, `dbStr-1.map.header`, `dbStr-2.map.data`, `dbStr-2.map.offsets`, and `dbStr-2.map.header`.

The log message was also changed from an iOS-specific message to a generic Store-V2 message:

- `External Store-V2 dbStr map load: ...`

The failure phase was similarly renamed:

- `external_dbstr_maps`

## Expected result

On the uploaded macOS Store-V2 dataset, native parse attempts should no longer show `properties=0 categories=0` when the external maps are present. This should improve structured key/value decoding and reduce placeholder GUI rows together with the V1.6.34 native path-probe promotion.

## Guardrails

This does not infer active filesystem existence. It only uses adjacent Store-V2 sidecar map files for native Spotlight metadata interpretation.
