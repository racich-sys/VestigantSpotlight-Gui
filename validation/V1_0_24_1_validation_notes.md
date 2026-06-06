# V1.0.24.1 Validation Notes

## Scope

V1.0.24.1 is a build hotfix for the V1.0.24 GUI helper modularization.

## Build failure addressed

Uploaded `V1_0_24_build.log` reported:

```text
src\gui\win32_gui.cpp(1619): error C2668: '`anonymous-namespace'::buildWhere': ambiguous call to overloaded function
src\gui\win32_gui.cpp(781): note: could be 'std::string `anonymous-namespace'::buildWhere(const vestigant::spotlight::ViewSpec &,const std::string &)'
src\gui/gui_view_helpers.h(11): note: or       'std::string vestigant::spotlight::buildWhere(const vestigant::spotlight::ViewSpec &,const std::string &,int,const std::string &)'
```

## Fix

- Removed the stale anonymous-namespace `buildWhere(const ViewSpec&, const std::string&)` wrapper from `src/gui/win32_gui.cpp`.
- Changed the review-page SQL construction to call the shared helper explicitly:
  `vestigant::spotlight::buildWhere(v, search, capturedFilterColumn, capturedFilterValue)`.

## Unchanged areas

- APFS/AFF4 traversal and extraction.
- Store-V2 parsing.
- iOS CoreSpotlight parsing.
- SQLite schema.
- GUI platform separation.
- GUI export worker backend logic, except for compile disambiguation in the caller.

## Local validation

- Confirmed no anonymous-namespace `buildWhere` wrapper remains in `src/gui/win32_gui.cpp`.
- Confirmed `buildWhere` helper implementation exists only in `src/gui/gui_view_helpers.cpp`.
- Confirmed the former ambiguous call site now explicitly qualifies the shared helper.
- Rebuilt and ran the controlled SQLite backend smoke test against V1.0.24.1 `gui_export_worker.cpp`, `gui_view_helpers.cpp`, and `view_registry.cpp`; Export Current Page and Export Filtered backend paths passed.
- `g++ -std=c++20 -fsyntax-only` passed for `src/core/app_info.cpp`.
- Generated unified patch and release package.

## Required external validation

- Windows/MSVC build using `scripts\Build-V1_0_24_1.ps1`.
- GUI smoke test for Export Page and Export Filtered.
