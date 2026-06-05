cmake_minimum_required(VERSION 3.13)
project(wensousou VERSION 1.0.20 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Threads REQUIRED)

set(SQLITE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite")
if(NOT EXISTS "${SQLITE_DIR}/sqlite3.c" OR NOT EXISTS "${SQLITE_DIR}/sqlite3.h")
  message(FATAL_ERROR "SQLite amalgamation is missing. Run scripts/prepare-deps.sh first.")
endif()

add_library(wensousou_sqlite STATIC "${SQLITE_DIR}/sqlite3.c")
target_include_directories(wensousou_sqlite PUBLIC "${SQLITE_DIR}")
target_compile_definitions(wensousou_sqlite PRIVATE
  SQLITE_ENABLE_FTS5=1
  SQLITE_ENABLE_LOAD_EXTENSION=1
  SQLITE_THREADSAFE=1
)
target_link_libraries(wensousou_sqlite PUBLIC
  Threads::Threads
  ${CMAKE_DL_LIBS}
  $<$<PLATFORM_ID:Linux>:m>
)

set(WENSOUSOU_CORE_SOURCES
  src/app_paths.cpp
  src/database.cpp
  src/logging.cpp
  src/root_policy.cpp
  src/search_worker.cpp
  src/text_normalizer.cpp
  src/tika_worker.cpp
  src/index_worker.cpp
  src/index_diagnostics.cpp
)

add_library(wensousou_core STATIC ${WENSOUSOU_CORE_SOURCES})
target_include_directories(wensousou_core PUBLIC src)
target_link_libraries(wensousou_core PUBLIC Qt5::Core wensousou_sqlite)

add_executable(wensousou
  src/main.cpp
  src/action_icons.cpp
  src/main_window.cpp
  src/index_manager_dialog.cpp
  src/settings_dialog.cpp
  src/self_check.cpp
)
target_link_libraries(wensousou PRIVATE wensousou_core Qt5::Gui Qt5::Widgets)

install(TARGETS wensousou RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
  find_package(Qt5 5.15 REQUIRED COMPONENTS Test)
  add_executable(root_policy_test tests/root_policy_test.cpp src/root_policy.cpp)
  target_include_directories(root_policy_test PRIVATE src)
  target_link_libraries(root_policy_test PRIVATE Qt5::Core Qt5::Test)
  add_test(NAME root_policy_test COMMAND root_policy_test)
  add_executable(search_meta_type_test tests/search_meta_type_test.cpp)
  target_include_directories(search_meta_type_test PRIVATE src)
  target_link_libraries(search_meta_type_test PRIVATE Qt5::Core Qt5::Test wensousou_core)
  add_test(NAME search_meta_type_test COMMAND search_meta_type_test)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(database_search_test
      tests/database_search_test.cpp
      src/database.cpp
      src/root_policy.cpp
      src/text_normalizer.cpp
    )
    target_include_directories(database_search_test PRIVATE src)
    target_compile_definitions(database_search_test PRIVATE
      WENSOUSOU_TEST_SIMPLE_LIB="${CMAKE_CURRENT_SOURCE_DIR}/resources/libsimple-linux-aarch64.so"
    )
    target_link_libraries(database_search_test PRIVATE Qt5::Core Qt5::Test wensousou_sqlite)
    add_test(NAME database_search_test COMMAND database_search_test)
    add_executable(index_manager_async_test
      tests/index_manager_async_test.cpp
      src/index_manager_dialog.cpp
    )
    target_include_directories(index_manager_async_test PRIVATE src)
    target_compile_definitions(index_manager_async_test PRIVATE
      WENSOUSOU_TEST_SIMPLE_LIB="${CMAKE_CURRENT_SOURCE_DIR}/resources/libsimple-linux-aarch64.so"
      WENSOUSOU_ENABLE_TEST_HOOKS=1
    )
    target_link_libraries(index_manager_async_test PRIVATE wensousou_core Qt5::Widgets Qt5::Test)
    add_test(NAME index_manager_async_test COMMAND index_manager_async_test)
    set_tests_properties(index_manager_async_test PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
    add_executable(action_icons_test
      tests/action_icons_test.cpp
      src/action_icons.cpp
    )
    target_include_directories(action_icons_test PRIVATE src)
    target_link_libraries(action_icons_test PRIVATE Qt5::Gui Qt5::Test)
    add_test(NAME action_icons_test COMMAND action_icons_test)
    set_tests_properties(action_icons_test PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
  endif()
endif()
