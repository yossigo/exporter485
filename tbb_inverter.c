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

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <modbus/modbus.h>
#include "exporter485.h"

static uint16_t crc16(const char *data, size_t len) {
    static const uint16_t crc_table[] = {
            0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
            0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
            0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
            0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
            0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
            0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
            0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
            0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
            0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
            0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
            0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
            0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
            0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
            0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
            0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
            0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
            0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
            0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
            0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
            0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
            0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
            0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
            0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
            0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
            0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
            0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
            0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
            0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
            0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
            0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
            0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
            0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040};

    uint8_t xor = 0;
    uint16_t crc = 0xffff;

    while (len--) {
        xor = (*data++) ^ crc;
        crc >>= 8;
        crc ^= crc_table[xor];
    }

    return crc;
}

static ssize_t read_payload(modbus_t *modbus, char *buf, ssize_t read_len)
{
    ssize_t nread = 0;

    uint response_timeout_sec, response_timeout_usec;
    modbus_get_response_timeout(modbus, &response_timeout_sec, &response_timeout_usec);

    int fd = modbus_get_socket(modbus);

    struct timeval tv = { .tv_sec = response_timeout_sec, tv.tv_usec = response_timeout_sec };

    while (nread < read_len) {
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(fd, &rset);

        ssize_t ret = select(1, &rset, NULL, NULL, &tv);
        if (ret == -1) {
            return -1;
        }

        ret = read(fd, buf + nread, read_len - nread);
        if (ret < 0) {
            return -1;
        }

        nread += ret;

        uint32_t byte_timeout_sec, byte_timeout_usec;
        modbus_get_byte_timeout(modbus, &byte_timeout_sec, &byte_timeout_usec);
        tv.tv_sec = byte_timeout_sec;
        tv.tv_usec = byte_timeout_usec;
    }

    return nread;
}

static int check_payload_crc(const char *data, size_t len)
{
    if (len < 2)
        return 0;  /* Too short */

    uint16_t crc = crc16(data, len - 2);
    uint16_t *payload_crc = (uint16_t *) (data + len - 2);

    return (crc == *payload_crc);
}

int tbb_get_payload(exporter_t *exporter, tbb_payload_t *payload) {
    const char request_payload[] = { 0x7e, 0xff, 0x11, 0x03, 0xa0, 0x08, 0x92, 0xeb };
    int fd = modbus_get_socket(exporter->modbus);

    if (write(fd, request_payload, sizeof(request_payload)) < 0) {
        printf("Failed to write payload: %s\n", strerror(errno));
        return -1;
    }

    if (read_payload(exporter->modbus, payload->data, sizeof(payload->data)) != sizeof(payload->data)) {
        printf("Failed to read payload.\n");
        return -1;
    }

    if (!check_payload_crc(payload->data, sizeof(payload->data))) {
        printf("Failed to validate payload CRC.\n");
        return -1;
    }

    return 0;
}

#ifdef TBBDUMP
int main(int argc, char *argv[])
{
    exporter_t exporter;

    exporter.modbus = modbus_new_rtu("/dev/ttyUSB0", 9600, 'N', 8, 1);
    if (modbus_connect(exporter.modbus) == -1) {
        printf("modbus_connect failed.\n");
        exit(1);
    }

    tbb_payload_t payload;
    if (tbb_get_payload(&exporter, &payload) < 0) {
        printf("tbb_get_payload failed.\n");
        exit(1);
    }

    printf("Payload:\n");
    for (int i = 0; i < sizeof(payload.data); i++) {
        printf("%02x%c", payload.data[i], i % 16 == 15 ? '\n' : ' ');
    }
    printf("\n");
    exit(0);
}
#endif

#ifdef TEST
int main(int argc, char *argv[])
{
    const char request1[] = { 0x7e, 0xff, 0x11, 0x03, 0xa0, 0x08, 0x92, 0xeb };
    const char bad_request1[] = { 0x7e, 0xff, 0x11, 0x03, 0xa0, 0x08, 0x92, 0xec };
    const char request2[] = { 0x7e, 0xff, 0x11, 0x03, 0x33, 0x0c, 0x00, 0x65, 0x00, 0x64, 0xa1, 0xdb };

    int ret;

    ret = check_payload_crc(request1, sizeof(request1));
    printf("request1 check_payload_crc: %d\n", ret);
    if (!ret) exit(1);

    ret = check_payload_crc(bad_request1, sizeof(bad_request1));
    printf("bad_request1 check_payload_crc: %d\n", ret);
    if (ret) exit(1);

    ret = check_payload_crc(request2, sizeof(request2));
    printf("request2 check_payload_crc: %d\n", ret);
    if (!ret) exit(1);

    exit(0);
}
#endif