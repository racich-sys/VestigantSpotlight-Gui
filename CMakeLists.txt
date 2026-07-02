cmake_minimum_required(VERSION 3.20)
project(VestigantSpotlightInv VERSION 1.6.119 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /EHa)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(ENABLE_SANITIZERS "Enable ASan/UBSan for local forensic parser test builds" OFF)
if(ENABLE_SANITIZERS)
  if(MSVC)
    message(WARNING "ENABLE_SANITIZERS is supported only for Clang/GNU builds in this project; MSVC release build is unchanged.")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

option(VESTIGANT_ENABLE_LZFSE "Enable Apple lzfse/lzvn reference decoder when third_party/lzfse is vendored" ON)
set(VESTIGANT_LZFSE_SOURCES)
if(VESTIGANT_ENABLE_LZFSE AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/lzfse/src/lzfse.h")
  message(STATUS "Apple lzfse source detected; enabling VESTIGANT_HAS_LZFSE")
  list(APPEND VESTIGANT_LZFSE_SOURCES
    third_party/lzfse/src/lzfse_decode.c
    third_party/lzfse/src/lzfse_decode_base.c
    third_party/lzfse/src/lzfse_fse.c
    third_party/lzfse/src/lzvn_decode_base.c
  )
  if(MSVC)
    set_source_files_properties(${VESTIGANT_LZFSE_SOURCES} PROPERTIES COMPILE_OPTIONS "/W0")
  else()
    set_source_files_properties(${VESTIGANT_LZFSE_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
  endif()
endif()

set(CORE_SOURCES
  src/core/app_info.cpp
  src/core/csv.cpp
  src/core/hash.cpp
  src/core/logger.cpp
  src/core/path_utils.cpp
  src/db/case_db.cpp
  src/ingest/source_profiles.cpp
  src/ingest/store_discovery.cpp src/ingest/evidence_preservation.cpp
  src/ingest/evidence_intake.cpp
  src/parsers/native_storedb_parser.cpp
  src/parsers/ios_app_db_parser.cpp
  src/parsers/apfs_volume_reader.cpp
  src/parsers/apfs_aff4_reader.cpp
  src/parsers/aff4_probe_worker.cpp
  src/parsers/apfs_diagnostic_exporter.cpp
  src/codec/lzfse_codec.cpp
  src/enrich_sql/sqlite_enrichment.cpp
  src/export_sql/sqlite_exporter.cpp
  src/app/case_store.cpp
  src/app/app_runner.cpp
  ${VESTIGANT_LZFSE_SOURCES}
)

add_library(vestigant_spotlight_core STATIC ${CORE_SOURCES})
target_include_directories(vestigant_spotlight_core PUBLIC src)
if(VESTIGANT_LZFSE_SOURCES)
  target_include_directories(vestigant_spotlight_core PUBLIC third_party/lzfse/src)
  target_compile_definitions(vestigant_spotlight_core PUBLIC VESTIGANT_HAS_LZFSE=1)
endif()

if(WIN32)
  target_link_libraries(vestigant_spotlight_core PUBLIC winsqlite3)
else()
  target_link_libraries(vestigant_spotlight_core PUBLIC sqlite3)
endif()

add_executable(VestigantSpotlightCli src/cli/main.cpp)
target_link_libraries(VestigantSpotlightCli PRIVATE vestigant_spotlight_core)

add_executable(VestigantSpotlightTests tests/main.cpp tests/sql_recursion_test.cpp)
target_link_libraries(VestigantSpotlightTests PRIVATE vestigant_spotlight_core)
if(MSVC)
  target_link_options(VestigantSpotlightCli PRIVATE /STACK:33554432)
  target_link_options(VestigantSpotlightTests PRIVATE /STACK:33554432)
endif()


if(WIN32)
  add_executable(VestigantSpotlight WIN32 src/gui/win32_gui.cpp src/gui/view_registry.cpp src/gui/gui_view_helpers.cpp src/gui/gui_export_worker.cpp)
  target_link_libraries(VestigantSpotlight PRIVATE vestigant_spotlight_core comctl32 shell32 ole32 user32 comdlg32 gdi32)
  target_link_options(VestigantSpotlight PRIVATE /STACK:33554432)
  add_custom_command(TARGET VestigantSpotlight POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:VestigantSpotlight>/resources)
endif()
