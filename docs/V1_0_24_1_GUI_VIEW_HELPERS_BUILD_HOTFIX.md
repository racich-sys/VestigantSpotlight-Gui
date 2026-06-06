# V1.0.24.1 GUI View Helpers Build Hotfix

V1.0.24.1 is a narrow Windows/MSVC build hotfix for V1.0.24.

## Issue

The V1.0.24 GUI helper modularization left a local anonymous-namespace `buildWhere(const ViewSpec&, const std::string&)` wrapper in `win32_gui.cpp` while also adding `vestigant::spotlight::buildWhere(...)` in `gui_view_helpers.h/.cpp`.

On MSVC, the unqualified call in the review-page loader was ambiguous because argument-dependent lookup could see the shared helper while normal unqualified lookup could see the anonymous-namespace wrapper.

## Fix

- Removed the obsolete local `buildWhere` wrapper from `win32_gui.cpp`.
- Changed the review-page SQL assembly call to explicitly call `vestigant::spotlight::buildWhere(v, search, capturedFilterColumn, capturedFilterValue)`.
- Left GUI export behavior, SQLite schema, APFS/AFF4 code, Store-V2 parsing, and iOS parsing unchanged.

## Validation

Local static checks confirmed the obsolete wrapper is removed and only the shared helper implementation remains. Windows/MSVC validation is still required.
