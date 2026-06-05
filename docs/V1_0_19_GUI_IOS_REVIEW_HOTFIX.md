# V1.0.19 GUI / iOS Review Hotfix

V1.0.19 is a narrow GUI hotfix based on the V1.0.18 dual-platform validation pass. It intentionally does not change macOS AFF4/APFS extraction, Apple/lzfse codec integration, or iOS CoreSpotlight parsing behavior.

## Confirmed V1.0.18 GUI regressions addressed

1. In the iOS Investigation View, the View Set combo box could visually disappear after the first window resize and reappear when its screen area was clicked.
2. The first clicked iOS review row could leave the bottom Selected Row Details grid blank until a second row was clicked.
3. A clipped text/control artifact appeared between the main results grid and the Selected Row Details section. The artifact was caused by text drawn into an 8-pixel splitter static control.

## Fixes

- Moved the View Set combo box farther away from the label and forced its visibility/z-order after review layout.
- Added review-control z-order stabilization after resize/layout on macOS and iOS investigation tabs.
- Removed visible text from the row-detail splitter and changed it to an etched horizontal splitter so clipped text cannot bleed through.
- Increased the vertical gap between the main results grid and selected-row details section.
- Added forced-row selected detail updates so the first selected/clicked row populates details immediately.
- Auto-selects and populates row 1 after a page load when rows are available.
- Changed mouse checkbox selection handling to keep focus/details on the clicked row instead of advancing to the next row.

## Validation target

After building V1.0.19, validate:

1. Open iOS Investigation View.
2. Resize the window.
3. Confirm the View Set combo remains visible.
4. Confirm no clipped/hidden text appears between the result grid and Selected Row Details.
5. Click the first visible iOS row once.
6. Confirm Selected Row Details immediately populates.
7. Switch between macOS and iOS investigation tabs and repeat the resize check.
8. Confirm filtered export remains backgrounded and responsive.
