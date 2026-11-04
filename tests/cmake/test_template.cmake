FILE(GLOB app_sources src/*.c)
FILE(GLOB app_sources_cpp src/*.cpp)
target_sources(app PRIVATE
  ${app_sources} ${app_sources_cpp}
  )
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../include)

if (VERBOSE)
  zephyr_compile_definitions(ZCBOR_VERBOSE)
endif()

if (ASSERTS)
  zephyr_compile_definitions(ZCBOR_ASSERTS)
endif()

if (CANONICAL)
  zephyr_compile_definitions(ZCBOR_CANONICAL)
endif()

if (CONFIG_BIG_ENDIAN OR BIG_ENDIAN)
  zephyr_compile_definitions(ZCBOR_BIG_ENDIAN)
endif()

if (MAP_SMART_SEARCH)
  zephyr_compile_definitions(ZCBOR_MAP_SMART_SEARCH)
endif()

if (DEFINED MAX_SKIP_DEPTH)
  zephyr_compile_definitions(ZCBOR_MAX_SKIP_DEPTH=${MAX_SKIP_DEPTH})
endif()

zephyr_compile_options(-Werror)

if (CONFIG_64BIT)
  set(bit_arg --default-bit-size 64)
endif()
