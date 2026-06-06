# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Workaround: ESP32-Zephyr-Test wird als Zephyr-Modul erkannt.
# ZEPHYR_MODULES explizit setzen, Projektpfad herausfiltern.
get_filename_component(_project_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
execute_process(
  COMMAND west list -f "{abspath}"
  WORKING_DIRECTORY "${_project_root}"
  OUTPUT_VARIABLE _all_modules
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
string(REPLACE "\n" ";" _modules_list "${_all_modules}")
list(REMOVE_ITEM _modules_list "${_project_root}")
set(ZEPHYR_MODULES ${_modules_list} CACHE STRING "Zephyr module paths" FORCE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bthome_ranging)

# BThome V2 Bibliothek (aus workspace-root/lib/)
add_subdirectory(
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/bthome_v2
  ${CMAKE_CURRENT_BINARY_DIR}/bthome_v2
)

target_link_libraries(app PRIVATE bthome_v2)
target_sources(app PRIVATE src/main.c)
