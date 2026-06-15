# AI Context - Vestigant Spotlight V1.6.29.4

Current baseline: V1.6.29.4.

V1.6.29.3 failed MSVC compile in `aff4_probe_worker.cpp` because OMAP vertical-cycle handling called `appendProbeNote`, which is not available in that context. V1.6.29.4 changes those calls to `aff4ApfsAppendProbeNote`. The build wrapper also now fails before version probing if the CLI executable is missing or if compiler/linker errors appear in the build log.

Next validation: build V1.6.29.4 on Windows/MSVC, upload `V1_6_29_4_build.log`, then run/upload the V1.6.29.4 iOS CoreSpotlight thin.
