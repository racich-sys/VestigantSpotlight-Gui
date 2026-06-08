# V1.3.2.3 Local Validation Notes

- Prepared as an iOS stack-overflow hotfix after V1.3.2 crashed with Windows structured exception `0xc00000fd` after native iOS ZIP inventory parsing.
- Changed Windows linker stack reserve to `/STACK:33554432` for CLI, tests, and GUI in the no-CMake MSVC build and CMake/MSVC build paths.
- Added iOS app DB record-inventory progress/status markers to help identify the next failing stage if the run still fails.
- Added failed iOS thin ZIP renaming to `_FAILED.zip`.
- Local Linux CMake/build/self-test validation required before packaging. Windows/MSVC and iOS thin required after packaging.
