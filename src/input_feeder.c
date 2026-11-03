/*
 * SPDX-License-Identifier: MIT
 *
 * Feeds events from an input device through input processors, without producing
 * any HID output. See dts/bindings/toucan,input-feeder.yaml.
 */

#define DT_DRV_COMPAT toucan_input_feeder

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>

#define INPUT_FEEDER_INST(n)                                                                       \
    static const struct zmk_input_processor_entry input_feeder_processors_##n[] = {               \
        LISTIFY(DT_INST_PROP_LEN(n, input_processors), ZMK_INPUT_PROCESSOR_ENTRY_AT_IDX, (, ),     \
                DT_DRV_INST(n))};                                                                  \
                                                                                                   \
    static void input_feeder_cb_##n(struct input_event *evt, void *user_data) {                    \
        ARG_UNUSED(user_data);                                                                     \
        for (size_t i = 0; i < ARRAY_SIZE(input_feeder_processors_##n); i++) {                    \
            const struct zmk_input_processor_entry *p = &input_feeder_processors_##n[i];           \
            if (zmk_input_processor_handle_event(p->dev, evt, p->param1, p->param2, NULL) !=       \
                ZMK_INPUT_PROC_CONTINUE) {                                                         \
                return;                                                                            \
            }                                                                                      \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_INST_PHANDLE(n, device)), input_feeder_cb_##n, NULL);

DT_INST_FOREACH_STATUS_OKAY(INPUT_FEEDER_INST)
