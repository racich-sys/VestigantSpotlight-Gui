# V1_6_38 Release Preflight Hardening

## Purpose

V1_6_38 fixes the recurring build-blocking release-readiness failure pattern observed during the 1.6.29.x and 1.6.31 build attempts.

## Problem

Release-readiness checks were being executed as fatal preflight gates before MSVC. Those checks included brittle version-token and documentation marker assertions, so a stale escaped regex or missing prose/document marker could prevent compilation from starting even when the source tree was otherwise buildable.

## Change

- Fatal preflight checks are now limited to build-safety checks that can directly affect compilation or wrapper execution.
- Release-readiness now runs as an advisory check from the build wrapper.
- The build wrapper now reads the expected version dynamically from the root `VERSION` file instead of embedding a stale literal version regex.
- The post-build CLI version check now compares against the dynamic `VERSION` value.
- Documentation marker checks are advisory and no longer block MSVC from running.

## Validation target

The next build log should show wrapper compatibility and raw-string checks first, then continue to MSVC compilation even if a documentation/advisory release-readiness issue is found.
