# SPDX-License-Identifier: MIT

target_sources_ifdef(CONFIG_TOUCAN_INPUT_FEEDER app PRIVATE src/input_feeder.c)
target_sources_ifdef(CONFIG_TOUCAN_INPUT_PROCESSOR_TAP_GUARD app PRIVATE src/input_processor_tap_guard.c)
