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
#include <string.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include "exporter485.h"

static struct evbuffer *render_metrics(module_t *module, metrics_value_set_t *vals)
{
    struct evbuffer *buf = evbuffer_new();

    for (int i = 0; i < module->metrics_count; i++) {
        metric_t *metric = module->metrics[i];

        if (metric->help)
            evbuffer_add_printf(
                    buf,
                    "# HELP %s_%s %s\n",
                    module->name,
                    metric->name,
                    metric->help);
        evbuffer_add_printf(
                buf,
                "# TYPE %s_%s %s\n",
                module->name,
                metric->name,
                get_metric_type_str(metric->metric_type));

        switch (metric->data_type) {
            case DATA_TYPE_FLOAT16:
            case DATA_TYPE_FLOAT32:
                evbuffer_add_printf(buf, "%s_%s %f\n", module->name,
                        metric->name, vals->values[i].float_value);
                break;
            case DATA_TYPE_INT16:
            case DATA_TYPE_INT32:
                evbuffer_add_printf( buf, "%s_%s %d\n", module->name,
                        metric->name, vals->values[i].int_value);
                break;
            case DATA_TYPE_UINT16:
            case DATA_TYPE_UINT32:
                evbuffer_add_printf( buf, "%s_%s %u\n", module->name,
                                     metric->name, vals->values[i].uint_value);
                break;
        }
    }

    return buf;
}

void handle_metrics(struct evhttp_request *req, void *arg) {
    exporter_t *exporter = (exporter_t *) arg;
    struct evkeyvalq params;
    evhttp_parse_query(evhttp_request_get_uri(req), &params);

    const char *module_name;
    if (!(module_name = evhttp_find_header(&params, "module"))) {
        /* TODO: Exporter metrics here */
        evhttp_send_error(req, HTTP_NOTIMPLEMENTED, NULL);
        return;
    }

    module_t *module = modules_get_module(exporter->modules, module_name);
    if (!module) {
        evhttp_send_error(req, HTTP_NOTFOUND, "Module not found");
        return;
    }

    const char *target_param = evhttp_find_header(&params, "target");
    if (!target_param) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Missing target");
        return;
    }

    int target = atoi(target_param);
    if (target < 1 || target > 247) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Invalid target id");
        return;
    }

    metrics_value_set_t *values = metrics_value_set_collect(exporter, module, target);
    if (!values) {
        evhttp_send_error(req, HTTP_INTERNAL, "Failed to collect metrics");
        return;
    }
    struct evbuffer *buf = render_metrics(module, values);
    metrics_value_set_free(values);

    evhttp_add_header (evhttp_request_get_output_headers (req),
                       "Content-Type", "text/plain");
    evhttp_send_reply(req, HTTP_OK, NULL, buf);
    evbuffer_free(buf);
}

void handle_config(struct evhttp_request *req, void *arg)
{
    exporter_t *exporter = (exporter_t *) arg;

    char *data = NULL;
    size_t len;

    if (modules_dump(exporter->modules, &data, &len) < 0) {
        evhttp_send_reply(req, HTTP_INTERNAL, "Failed", NULL);
    } else {
        struct evbuffer *buf = evbuffer_new();
        evbuffer_add(buf, data, len);
        free(data);

        evhttp_send_reply(req, 200, "OK", buf);
        evbuffer_free(buf);
    }
}

