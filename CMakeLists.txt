# SPDX-License-Identifier: MIT

zephyr_include_directories(include)

target_sources_ifdef(CONFIG_TOUCAN_INPUT_FEEDER app PRIVATE src/input_feeder.c)
target_sources_ifdef(CONFIG_TOUCAN_INPUT_PROCESSOR_TAP_GUARD app PRIVATE src/input_processor_tap_guard.c)
target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_CIRQUE app PRIVATE src/input_processor_cirque.c)
