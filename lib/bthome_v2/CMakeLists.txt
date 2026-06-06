# SPDX-License-Identifier: MIT
# lib/bthome_v2/CMakeLists.txt

add_library(bthome_v2 STATIC src/bthome_v2.c)
target_include_directories(bthome_v2 PUBLIC include)
target_link_libraries(bthome_v2 PUBLIC zephyr_interface)
