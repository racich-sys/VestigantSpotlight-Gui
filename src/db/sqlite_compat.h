#pragma once

#if defined(_WIN32)
  #include <winsqlite/winsqlite3.h>
  #pragma comment(lib, "winsqlite3.lib")
#else
  #include <sqlite3.h>
#endif
