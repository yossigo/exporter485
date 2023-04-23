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
#include <stdlib.h>
#include <getopt.h>
#include <event2/event.h>
#include <event2/http.h>
#include <modbus/modbus.h>

#include "exporter485.h"

void usage(void)
{
    printf("Usage: exporter485 [options]\n"
           "\n"
           "Options:\n"
           "  -c, --config-file=FILE    Configuration file (default: config.yaml)\n"
           "  -p, --port=PORT           HTTP port to listen on (default: 9485)\n"
           "  -b, --bind-addr=ADDR      Address to listen on (default: 0.0.0.0)\n"
           "  -d, --device=DEV          RS-485 serial device (default: /dev/tty/XRUSB0)\n"
           "      --baud-rate           Serial device baud rate (default: 115200)\n"
           "      --parity              Serial device parity (default: N)\n"
           "      --data-bits           Serial device data bits (default: 8)\n"
           "      --stop-bits           Serial device sop bits (default: 1)\n"
           "      --dry-run             Dry-run mode, produce fake metrics\n"
    );
}

int main(int argc, char *argv[])
{
    struct event_base *base;
    struct evhttp *http;

    enum extended_options {
        o_extended_base = 128,
        o_baud_rate,
        o_parity,
        o_data_bits,
        o_stop_bits,
        o_dry_run
    };
    static struct option long_options[] = {
        {"config-file",     required_argument,  0, 'c' },
        {"port",            required_argument,  0, 'p' },
        {"bind-addr",       required_argument,  0, 'b' },
        {"device",          required_argument,  0, 'd' },
        {"baud-rate",       required_argument,  0, o_baud_rate },
        {"parity",          required_argument,  0, o_parity },
        {"data_bits",       required_argument,  0, o_data_bits },
        {"stop_bits",       required_argument,  0, o_stop_bits },
        {"dry-run",         0,                  0, o_dry_run },
        {"help",            0,                  0, 'h' },
        {0,                 0,                  0, 0 }
    };

    options_t o = {
        .config_file = "config.yaml",
        .port = 9485,
        .bind_addr = "0.0.0.0",
        .device = "/dev/ttyXRUSB0",
        .baud_rate = 115200,
        .parity = 'N',
        .data_bits = 8,
        .stop_bits = 1,
        .dry_run = 0
    };

    while (1) {
        int option_index;
        int c = getopt_long(argc, argv, "hc:d:p:", long_options, &option_index);
        if (c == -1) {
           break;
        }

        switch (c) {
            case 'h':
                usage();
                exit(0);
            case 'c':
                o.config_file = optarg;
                break;
            case 'd':
                o.device = optarg;
                break;
            case 'p':
                o.port = atoi(optarg);
                if (o.port <= 0 || o.port > 65535) {
                    fprintf(stderr, "Error: port must be 1-65535\n");
                    exit(1);
                }
                break;
            case o_baud_rate:
                o.baud_rate = atoi(optarg);
                break;
            case o_parity:
                o.parity = optarg[0];
                break;
            case o_data_bits:
                o.data_bits = atoi(optarg);
                break;
            case o_stop_bits:
                o.stop_bits = atoi(optarg);
                break;
            case o_dry_run:
                o.dry_run = 1;
                break;
            default:
                exit(-1);
        }
    }

    static exporter_t exporter = { 0 };
    exporter.options = o;
    if (!(exporter.modules = modules_load(o.config_file))) {
        exit(1);
    }

    if (!o.dry_run) {
        exporter.modbus = modbus_new_rtu(o.device, o.baud_rate, o.parity, o.data_bits, o.stop_bits);
        if (modbus_connect(exporter.modbus) == -1) {
            fprintf(stderr, "Error: connection failed: %s: %s\n", o.device, modbus_strerror(errno));
            modbus_free(exporter.modbus);
            exit(1);
        }
    }

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Failed to create event base.\n");
        exit(1);
    }

    http = evhttp_new(base);
    if (!http) {
        fprintf(stderr, "Failed to create http server.\n");
        exit(1);
    }

    evhttp_set_cb(http, "/config", handle_config, &exporter);
    evhttp_set_cb(http, "/metrics", handle_metrics, &exporter);
    if (evhttp_bind_socket(http, o.bind_addr, o.port) < 0) {
        fprintf(stderr, "Failed to bind to socket.\n");
        exit(1);
    }

    printf("Running exporter485 on %s:%u\n", o.bind_addr, o.port);
    event_base_dispatch(base);
    return 0;
}
