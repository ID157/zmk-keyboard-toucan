/*
 * SPDX-License-Identifier: MIT
 *
 * Holds back the start of each relative movement until it exceeds a small
 * distance, so the finger wobble of a tap does not move the cursor.
 * See dts/bindings/toucan,input-processor-tap-guard.yaml.
 */

#define DT_DRV_COMPAT toucan_input_processor_tap_guard

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>

struct tap_guard_config {
    int32_t threshold;
    int32_t idle_ms;
};

struct tap_guard_data {
    int64_t last_ts;
    int32_t acc_x;
    int32_t acc_y;
    bool flowing;
};

static int tap_guard_handle_event(const struct device *dev, struct input_event *event,
                                  uint32_t param1, uint32_t param2,
                                  struct zmk_input_processor_state *state) {
    const struct tap_guard_config *cfg = dev->config;
    struct tap_guard_data *data = dev->data;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int64_t now = k_uptime_get();
    if (now - data->last_ts > cfg->idle_ms) {
        data->flowing = false;
        data->acc_x = data->acc_y = 0;
    }
    data->last_ts = now;

    int32_t *acc = event->code == INPUT_REL_X ? &data->acc_x : &data->acc_y;

    if (!data->flowing) {
        *acc += event->value;
        if (abs(data->acc_x) + abs(data->acc_y) < cfg->threshold) {
            // Still within the tap radius: hold the motion back. Keep sync events
            // (with a zero value) so reports downstream stay correctly framed.
            event->value = 0;
            return event->sync ? ZMK_INPUT_PROC_CONTINUE : ZMK_INPUT_PROC_STOP;
        }
        data->flowing = true;
        event->value = 0;
    }

    // Release whatever is still held back for this axis along with the event
    event->value += *acc;
    *acc = 0;
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api tap_guard_driver_api = {
    .handle_event = tap_guard_handle_event,
};

#define TAP_GUARD_INST(n)                                                                          \
    static struct tap_guard_data tap_guard_data_##n;                                               \
    static const struct tap_guard_config tap_guard_config_##n = {                                  \
        .threshold = DT_INST_PROP(n, threshold),                                                   \
        .idle_ms = DT_INST_PROP(n, idle_ms),                                                       \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &tap_guard_data_##n, &tap_guard_config_##n, POST_KERNEL,  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tap_guard_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TAP_GUARD_INST)
