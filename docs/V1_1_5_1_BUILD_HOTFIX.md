# V1.1.5.1 Build Hotfix

V1.1.5.1 fixes the V1.1.5 MSVC compile failure in `src/app/app_runner.cpp`.

The V1.1.5 cancellation propagation inserted a `return false;` inside a lambda returning `ApfsOmapTargetResolution`. V1.1.5.1 returns a populated cancellation-status `ApfsOmapTargetResolution` instead.

No live forensic extraction behavior was otherwise changed.
