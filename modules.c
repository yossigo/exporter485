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

#include <stdio.h>
#include <string.h>
#include <cyaml/cyaml.h>
#include "exporter485.h"

static const cyaml_strval_t input_type_strings[] = {
    { "holdingRegister", INPUT_TYPE_HOLDING_REGISTER },
    { "inputRegister", INPUT_TYPE_INPUT_REGISTER }
};

static const cyaml_strval_t metric_type_strings[] = {
    { "untyped", METRIC_TYPE_UNTYPED },
    { "counter", METRIC_TYPE_COUNTER },
    { "gauge", METRIC_TYPE_GAUGE }
};

const char *get_metric_type_str(metric_type_t metric_type)
{
   switch (metric_type) {
       case METRIC_TYPE_UNTYPED:
           return "untyped";
       case METRIC_TYPE_COUNTER:
           return "counter";
       case METRIC_TYPE_GAUGE:
           return "gauge";
       default:
           return "unknown";
   }
}

static const cyaml_strval_t data_type_strings[] = {
        { "int16", DATA_TYPE_INT16 },
        { "int32", DATA_TYPE_INT32 },
        { "uint16", DATA_TYPE_UINT16 },
        { "uint32", DATA_TYPE_UINT32 },
        { "float16", DATA_TYPE_FLOAT16 },
        { "float32", DATA_TYPE_FLOAT32 }
};

static const cyaml_strval_t word_order_strings[] = {
        { "big", WORD_ORDER_LOW_HIGH },
        { "little", WORD_ORDER_HIGH_LOW }
};

static const cyaml_schema_field_t metric_fields[] = {
    CYAML_FIELD_ENUM(
        "metricType", CYAML_FLAG_DEFAULT,
        struct metric, metric_type, metric_type_strings,
        CYAML_ARRAY_LEN(metric_type_strings)),
    CYAML_FIELD_ENUM(
        "inputType", CYAML_FLAG_DEFAULT,
        struct metric, input_type, input_type_strings,
        CYAML_ARRAY_LEN(input_type_strings)),
    CYAML_FIELD_ENUM(
        "dataType", CYAML_FLAG_DEFAULT,
        struct metric, data_type, data_type_strings,
        CYAML_ARRAY_LEN(data_type_strings)),
    CYAML_FIELD_ENUM(
        "wordOrder", CYAML_FLAG_DEFAULT|CYAML_FLAG_OPTIONAL,
        struct metric, word_order, word_order_strings,
        CYAML_ARRAY_LEN(word_order_strings)),
    CYAML_FIELD_UINT(
        "address", CYAML_FLAG_DEFAULT,
        struct metric, address),
    CYAML_FIELD_STRING_PTR(
        "name", CYAML_FLAG_POINTER,
        struct metric, name, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR(
        "help", CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
        struct metric, help, 0, CYAML_UNLIMITED),
    CYAML_FIELD_FLOAT(
        "factor", CYAML_FLAG_OPTIONAL,
        struct metric, factor),
    CYAML_FIELD_END
};

static const cyaml_schema_value_t metric_schema = {
    CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER, struct metric, metric_fields)
};

static const cyaml_schema_field_t module_fields[] = {
    CYAML_FIELD_STRING_PTR(
        "name", CYAML_FLAG_POINTER,
        struct module, name, 0, CYAML_UNLIMITED),
    CYAML_FIELD_SEQUENCE(
        "metrics", CYAML_FLAG_POINTER,
        struct module, metrics,
        &metric_schema, 0, CYAML_UNLIMITED),
    CYAML_FIELD_END
};

static const cyaml_schema_value_t module_schema = {
    CYAML_VALUE_MAPPING(
        CYAML_FLAG_POINTER,
        struct module, module_fields)
};

static const cyaml_schema_field_t modules_fields[] = {
        CYAML_FIELD_SEQUENCE(
                "modules", CYAML_FLAG_POINTER,
                struct modules, modules,
                &module_schema, 0, CYAML_UNLIMITED),
        CYAML_FIELD_END
};

static const cyaml_schema_value_t modules_schema = {
        CYAML_VALUE_MAPPING(
                CYAML_FLAG_POINTER,
                struct modules, modules_fields)
};

static cyaml_config_t cyaml_config = {
        .log_fn = cyaml_log,
        .mem_fn = cyaml_mem,
        .log_level = CYAML_LOG_WARNING
};

modules_t *modules_load(const char *filename)
{
    modules_t *modules;

    cyaml_err_t err = cyaml_load_file(filename, &cyaml_config,
                                      &modules_schema, (void **) &modules, NULL);
    if (err != CYAML_OK) {
        fprintf(stderr, "%s: %s\n", filename, cyaml_strerror(err));
        return NULL;
    }

    return modules;
}

int modules_dump(modules_t *modules, char **output, size_t *len)
{
    cyaml_err_t err = cyaml_save_data(output, len, &cyaml_config,
                    &modules_schema, modules, 0);
    if (err != CYAML_OK)
        return -1;
    return 0;
}

module_t *modules_get_module(modules_t *modules, const char *name)
{
    for (int i = 0; i < modules->modules_count; i++) {
        if (!strcmp(modules->modules[i]->name, name))
            return modules->modules[i];
    }

    return NULL;
}
