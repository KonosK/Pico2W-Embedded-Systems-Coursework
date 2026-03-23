/**
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdio.h"
#include "pico/cyw43_arch.h"
#include "pico/async_context.h"
#include "lwip/altcp_tls.h"
#include "example_http_client_util.h"
#include <tusb.h>

#define HOST "api.open-meteo.com"
#define URL_REQUEST "/v1/forecast?latitude=50.9&longitude=-1.4&daily=sunrise,sunset&timezone=Europe/London&forecast_days=7"

char *extract_string_value(const char *json, const char *key){
    const char *p = json;
    size_t key_len = strlen(key);
    char *match = malloc(64);

    while (*p) {
        if (*p == '"'){
            p++;
            if (strncmp(p, key, key_len) == 0 && p[key_len] == '"') {
                p += key_len + 1;
                while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' )) p++;
                if (*p == ':') p++;

                if (*p == '{') continue;
                if (*p == ',') continue;
                
                while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' )) p++;

                if (*p == '"') {
                    p++;
                    int i = 0;
                    while (*p && (*p != '"'))
                        match[i++] = *p++;

                    match[i] = '\0';
                    return match;
                }
            }
        }

        p++;
    }

    return NULL;
}

int extract_7_elems_array_value(const char *json, const char *key, char match[7][64]){
    const char *p = json;
    size_t key_len = strlen(key);

    while (*p) {
        if (*p == '"'){
            p++;
            if (strncmp(p, key, key_len) == 0 && p[key_len] == '"') {
                p += key_len + 1;
                while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ':')) p++;

                if (*p == '[') {
                    p++;
                    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
                    if (*p == '"'){
                        int i;
                        for (i = 0; i < 7; i++){ 
                            p++; //skips opening "
                            int j = 0;
                            while(*p && (*p != '"')){
                                match[i][j++] = *p++;
                            }
                            match[i][j] = '\0';
                            p++; //skips closing "
                            while (*p && (*p == ',' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
                        }
                    }

                    return 0;
                }
            }
        }

        p++;
    }
    
    return -1; //fyi you CANT do the following with arrays, even tho they are very similar to pointers: match =  NULL;
}

err_t http_client_receive_print_fn_Custom(__unused void *arg,
__unused struct altcp_pcb *conn, struct pbuf *p, err_t err) {

    printf("\ncontent err %d\n", err);

    char *json = malloc (4096);
    char *json_initial = json;

    u16_t offset = 0;
    while (offset < p->tot_len) {
        char c = (char)pbuf_get_at(p, offset++);
        *json++ = c;
    }
    *json = '\0';

    printf("\n====London Sunrise and Sunset times for the next week====\n\n");
    char times[7][64];
    char sunrise[7][64];
    char sunset[7][64];

    if (extract_7_elems_array_value(json_initial, "time", times) == -1) printf("Dates extraction failed!");
    if (extract_7_elems_array_value(json_initial, "sunrise", sunrise) == -1) printf("Sunrise times extraction failed!");
    if (extract_7_elems_array_value(json_initial, "sunset", sunset) == -1) printf("Sunset extraction failed!");

    for(int i = 0; i < 7; i++)
        printf("\nOn %s, Sunrise is at %s and Sunset is at %s\n", times[i], sunrise[i], sunset[i]);

    free(json_initial);
    altcp_recved(conn, p->tot_len); //signals to IwP how many bytes have been processed and that it can send more
    pbuf_free(p); //frees the pbuf struct --> necessary to allow space for more bytes to come (to avoid overflow)
    return ERR_OK;
}

int main() {
    stdio_init_all();

    while (!tud_cdc_connected()) { sleep_ms(100); }
    printf("TTY serial terminal connected!\n");

    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect\n");
        return 1;
    }

    EXAMPLE_HTTP_REQUEST_T req1 = {0};
    req1.hostname = HOST;
    req1.url = URL_REQUEST;
    req1.headers_fn = http_client_header_print_fn;
    req1.recv_fn = http_client_receive_print_fn_Custom;
    int result = http_client_request_sync(cyw43_arch_async_context(), &req1);

    if (result != 0) {
        panic("test failed");
    }
    cyw43_arch_deinit();
    printf("Test passed\n");
    sleep_ms(100);
    return 0;
}