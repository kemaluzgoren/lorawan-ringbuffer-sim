#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include "lwrb.h"

#define MAX_DEVICES         10
#define MAX_PAYLOAD         32
#define RB_SIZE             512
#define DEVEUI_LEN          17
#define PRODUCER_DELAY_US   800000
#define CONSUMER_DELAY_US   1000000

typedef struct {
    char deveui[DEVEUI_LEN];    // DEVEUI
    uint8_t joined;
} device_t;

typedef struct {
    char deveui[DEVEUI_LEN];
    uint8_t fport;
    uint8_t payload[MAX_PAYLOAD];
    uint8_t payload_len;
} lorawan_message_t;

typedef struct {
    char deveui[DEVEUI_LEN];
} join_request_t;

device_t device_db[MAX_DEVICES];
int device_count = 0;

lwrb_t rb_join;
uint8_t rb_join_mem[RB_SIZE];

lwrb_t rb_msg;
uint8_t rb_msg_mem[RB_SIZE];

pthread_mutex_t print_mtx = PTHREAD_MUTEX_INITIALIZER;

char hexchar(void) {
    const char* hex = "0123456789ABCDEF";
    return hex[rand() % 16];
}

int is_device_joined(const char* deveui) {
    for (int i = 0; i < device_count; ++i) {
        if (strcmp(device_db[i].deveui, deveui) == 0 && device_db[i].joined) {
            return 1;
        }
    }
    return 0;
}

void register_device(const char* deveui) {
    if (device_count < MAX_DEVICES) {
        strcpy(device_db[device_count].deveui, deveui);
        device_db[device_count].joined = 1;

        pthread_mutex_lock(&print_mtx);
        printf("[JOIN] Device registered: %s\n", deveui);
        pthread_mutex_unlock(&print_mtx);

        device_count++;
    } else {
        pthread_mutex_lock(&print_mtx);
        printf("[JOIN] Device DB full. Could not register: %s\n", deveui);
        pthread_mutex_unlock(&print_mtx);
    }
}

void* join_request_thread(void* arg) {
    (void)arg; 

    while (1) {
        join_request_t req;
        for (int i = 0; i < 16; i++) {
            req.deveui[i] = hexchar();
        }
        req.deveui[16] = '\0';

        if (lwrb_get_free(&rb_join) >= sizeof(join_request_t)) {
            lwrb_write(&rb_join, &req, sizeof(join_request_t));
            pthread_mutex_lock(&print_mtx);
            printf("[Device] Sent JOIN REQUEST → %s\n", req.deveui);
            pthread_mutex_unlock(&print_mtx);
        }

        usleep(6 * PRODUCER_DELAY_US); // Sparse join
    }
    return NULL;
}

void* join_handler_thread(void* arg) {
    (void)arg; 

    while (1) {
        if (lwrb_get_full(&rb_join) >= sizeof(join_request_t)) {
            join_request_t req;
            lwrb_read(&rb_join, &req, sizeof(req));
            register_device(req.deveui);
        }
        usleep(200000);
    }
    return NULL;
}

void* random_packet_thread(void* arg) {
    (void)arg; 

    while (1) {
        lorawan_message_t msg;
        for (int i = 0; i < 16; i++) msg.deveui[i] = hexchar();
        msg.deveui[16] = '\0';
        msg.fport = (rand() % 222) + 1;
        msg.payload_len = (rand() % MAX_PAYLOAD) + 1;
        for (int i = 0; i < msg.payload_len; i++) {
            msg.payload[i] = rand() % 256;
        }

        if (lwrb_get_free(&rb_msg) >= sizeof(lorawan_message_t)) {
            lwrb_write(&rb_msg, &msg, sizeof(lorawan_message_t));
            pthread_mutex_lock(&print_mtx);
            printf("[LoRa RX] UNREGISTERED DevEUI: %s sent FPort %d\n", msg.deveui, msg.fport);
            pthread_mutex_unlock(&print_mtx);
        }
        usleep(PRODUCER_DELAY_US);
    }
    return NULL;
}

void* uplink_handler_thread(void* arg) {
    (void)arg; 

    while (1) {
        if (lwrb_get_full(&rb_msg) >= sizeof(lorawan_message_t)) {
            lorawan_message_t msg;
            lwrb_read(&rb_msg, &msg, sizeof(msg));

            if (is_device_joined(msg.deveui)) {
                pthread_mutex_lock(&print_mtx);
                printf("[APP] UPLINK from JOINED DevEUI: %s, FPort: %d, Payload: ", msg.deveui, msg.fport);
                for (int i = 0; i < msg.payload_len; i++) {
                    printf("%02X ", msg.payload[i]);
                }
                printf("\n");
                pthread_mutex_unlock(&print_mtx);
            } else {
                pthread_mutex_lock(&print_mtx);
                printf("[REJECTED] Message from unknown DevEUI: %s\n", msg.deveui);
                pthread_mutex_unlock(&print_mtx);
            }
        }
        usleep(CONSUMER_DELAY_US);
    }
    return NULL;
}

void* device_uplink_simulator(void* arg) {
    (void)arg; 

    while (1) {
        for (int i = 0; i < device_count; ++i) {
            if (device_db[i].joined) {
                lorawan_message_t msg = {0};
                strcpy(msg.deveui, device_db[i].deveui);
                msg.fport = 1;
                msg.payload_len = 4;
                for (int j = 0; j < msg.payload_len; j++) {
                    msg.payload[j] = rand() % 256;
                }

                if (lwrb_get_free(&rb_msg) >= sizeof(msg)) {
                    lwrb_write(&rb_msg, &msg, sizeof(msg));
                    pthread_mutex_lock(&print_mtx);
                    printf("[Device] UPLINK from %s → ", msg.deveui);
                    for (int j = 0; j < msg.payload_len; j++) {
                        printf("%02X ", msg.payload[j]);
                    }
                    printf("\n");
                    pthread_mutex_unlock(&print_mtx);
                }
            }
        }

        usleep(5 * 1000000); // try every 5 seconds
    }
    return NULL;
}


int main(void) {
    srand(time(NULL));
    lwrb_init(&rb_join, rb_join_mem, sizeof(rb_join_mem));
    lwrb_init(&rb_msg, rb_msg_mem, sizeof(rb_msg_mem));

    pthread_t tid1, tid2, tid3, tid4, tid5;
    pthread_create(&tid1, NULL, join_request_thread, NULL);
    pthread_create(&tid2, NULL, join_handler_thread, NULL);
    pthread_create(&tid3, NULL, random_packet_thread, NULL);
    pthread_create(&tid4, NULL, uplink_handler_thread, NULL);
    pthread_create(&tid5, NULL, device_uplink_simulator, NULL);
    
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    pthread_join(tid4, NULL);
    pthread_join(tid5, NULL);

    return 0;
}
