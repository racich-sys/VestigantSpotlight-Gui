cmake_minimum_required(VERSION 3.20)
project(VestigantSpotlightInv VERSION 0.9.53 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /EHa)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
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
  src/parsers/native_storedb_parser.cpp
  src/parsers/v7_output_importer.cpp
  src/enrich_sql/sqlite_enrichment.cpp
  src/export_sql/sqlite_exporter.cpp
  src/app/case_store.cpp
  src/app/app_runner.cpp
)

add_library(vestigant_spotlight_core STATIC ${CORE_SOURCES})
target_include_directories(vestigant_spotlight_core PUBLIC src)

if(WIN32)
  target_link_libraries(vestigant_spotlight_core PUBLIC winsqlite3)
else()
  target_link_libraries(vestigant_spotlight_core PUBLIC sqlite3)
endif()

add_executable(VestigantSpotlightCli src/cli/main.cpp)
target_link_libraries(VestigantSpotlightCli PRIVATE vestigant_spotlight_core)

add_executable(VestigantSpotlightTests tests/main.cpp)
target_link_libraries(VestigantSpotlightTests PRIVATE vestigant_spotlight_core)

if(WIN32)
  add_executable(VestigantSpotlight WIN32 src/gui/win32_gui.cpp)
  target_link_libraries(VestigantSpotlight PRIVATE vestigant_spotlight_core comctl32 shell32 ole32 user32 comdlg32 gdi32)
endif()
