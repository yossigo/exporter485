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

#ifndef EXPORTER485_H
#define EXPORTER485_H

#include <event2/http.h>

/* Type of modbus input  */
typedef enum input_type {
    INPUT_TYPE_HOLDING_REGISTER,
    INPUT_TYPE_INPUT_REGISTER
} input_type_t;

/* Prometheus metric type */
typedef enum metric_type {
    METRIC_TYPE_UNTYPED,
    METRIC_TYPE_COUNTER,
    METRIC_TYPE_GAUGE
} metric_type_t;

/* Data type: this controls two things:
 * 1. The number of modbus registers read (currently 1 or 2).
 * 2. How the data is exported when scraped.
 */
typedef enum data_type {
    DATA_TYPE_INT16,
    DATA_TYPE_INT32,
    DATA_TYPE_UINT16,
    DATA_TYPE_UINT32,
    DATA_TYPE_FLOAT16,
    DATA_TYPE_FLOAT32
} data_type_t;

typedef enum word_order {
    WORD_ORDER_LOW_HIGH = 0,
    WORD_ORDER_HIGH_LOW
} word_order_t;

/* Exported metric type */
typedef struct metric {
    input_type_t input_type;
    metric_type_t metric_type;
    data_type_t data_type;
    word_order_t word_order;
    unsigned int address;
    char *name;
    char *help;
    float factor;
} metric_t;

/* A device class is a collection of metrics */
typedef struct module {
    char *name;
    metric_t **metrics;
    unsigned int metrics_count;
} module_t;

typedef struct modules {
    module_t **modules;
    unsigned int modules_count;
} modules_t;

typedef union metric_value {
    int int_value;
    unsigned int uint_value;
    float float_value;
} metric_value_t;

typedef struct metrics_value_set {
    metric_value_t *values;
    unsigned int values_count;
} metrics_value_set_t;

typedef struct options {
    char *config_file;
    int port;
    char *bind_addr;
    char *device;
    int baud_rate;
    char parity;
    int data_bits;
    int stop_bits;
    int dry_run;
} options_t;

typedef struct _modbus modbus_t;

typedef struct exporter {
    modbus_t *modbus;
    modules_t *modules;
    options_t options;
} exporter_t;

/* modules.c */
module_t *modules_get_module(modules_t *modules, const char *name);
modules_t *modules_load(const char *filename);
int modules_dump(modules_t *modules, char **output, size_t *len);
const char *get_metric_type_str(metric_type_t metric_type);

/* collect.c */
metrics_value_set_t *metrics_value_set_collect(exporter_t *exporter, module_t *module, int target);
void metrics_value_set_free(metrics_value_set_t *values);

/* http.c */
void handle_config(struct evhttp_request *req, void *arg);
void handle_metrics(struct evhttp_request *req, void *arg);

#endif  /* EXPORTER485_H */
