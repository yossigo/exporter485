/*
 * Copyright (c) 2023, Yossi Gottlieb <yossigo@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the copyright holder nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <modbus/modbus.h>
#include "exporter485.h"

void metrics_value_set_free(metrics_value_set_t *values)
{
    free(values->values);
    free(values);
}

metrics_value_set_t *metrics_value_set_collect(exporter_t *exporter, module_t *module, int target)
{
    metrics_value_set_t *values = calloc(1, sizeof(metrics_value_set_t));
    values->values_count = module->metrics_count;
    values->values = calloc(values->values_count, sizeof(metric_value_t));

    tbb_payload_t payload;

    switch (module->module_type) {
        case MODULE_TYPE_MODBUS:
            if (!exporter->options.dry_run)
                modbus_set_slave(exporter->modbus, target);
            break;
        case MODULE_TYPE_TBB_INVERTER:
            if (tbb_get_payload(exporter, &payload) < 0)
                goto error;
            break;
        default:
            goto error;
    }

    for (int i = 0; i < values->values_count; i++) {
        metric_t *metric = module->metrics[i];
        uint16_t reg[2];

        /* Read one or two registers */
        int nregs = 1;
        int low_reg = 0;
        int high_reg = 0;
        if (metric->data_type == DATA_TYPE_INT32 ||
            metric->data_type == DATA_TYPE_UINT32 ||
            metric->data_type == DATA_TYPE_FLOAT32) {

            low_reg = (metric->word_order == WORD_ORDER_LOW_HIGH ? 0 : 1);
            high_reg = (metric->word_order == WORD_ORDER_LOW_HIGH ? 1 : 0);
            nregs = 2;
        }

        if (exporter->options.dry_run) {
            static uint16_t counter = 0;
            reg[0] = counter++;
            if (nregs == 2) reg[1] = counter++;
        } else {
            switch (metric->input_type) {
                case INPUT_TYPE_INPUT_REGISTER:
                    if (modbus_read_input_registers(exporter->modbus, metric->address, nregs, reg) < 0)
                        goto error;
                    break;
                case INPUT_TYPE_HOLDING_REGISTER:
                    if (modbus_read_registers(exporter->modbus, metric->address, nregs, reg) < 0)
                        goto error;
                    break;
                case INPUT_TYPE_PAYLOAD_OFFSET:
                    /* Only supporting 16 bit values */
                    reg[0] = (uint16_t) payload.data[metric->address] << 8 | payload.data[metric->address+1];
                    break;
            }
        }

        switch (metric->data_type) {
            case DATA_TYPE_INT16:
                values->values[i].int_value = reg[low_reg];
                break;
            case DATA_TYPE_UINT16:
                values->values[i].uint_value = reg[low_reg];
                break;
            case DATA_TYPE_INT32:
                values->values[i].int_value = (reg[high_reg] << 16) | reg[low_reg];
                break;
            case DATA_TYPE_UINT32:
                values->values[i].uint_value = (reg[high_reg] << 16) | reg[low_reg];
                break;
            case DATA_TYPE_FLOAT16:
                values->values[i].float_value = (int16_t) reg[low_reg];
                break;
            case DATA_TYPE_FLOAT32:
                values->values[i].float_value = (int32_t) ((reg[high_reg] << 16) | reg[low_reg]);
                break;
            default:
                goto error;
        }

        if (metric->factor) {
            switch (metric->data_type) {
                case DATA_TYPE_INT16:
                case DATA_TYPE_INT32:
                    values->values[i].int_value *= metric->factor;
                    break;
                case DATA_TYPE_UINT32:
                case DATA_TYPE_UINT16:
                    values->values[i].uint_value *= metric->factor;
                    break;
                case DATA_TYPE_FLOAT16:
                case DATA_TYPE_FLOAT32:
                    values->values[i].float_value *= metric->factor;
                    break;
                default:
                    break;
            }
        }
    }
    return values;

error:
    metrics_value_set_free(values);
    return NULL;
}
