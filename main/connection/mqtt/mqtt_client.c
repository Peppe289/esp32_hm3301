#include "mqtt_client.h"
#include "esp_event_base.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "conn_mqtt_client.h"
#include "esp_log.h"

#ifndef MQTT_BROKER
#warning "Missing MQTT_BROKER env variable. Using default."
#define MQTT_BROKER "0.0.0.0"
#endif

#ifndef MQTT_PORT
#warning "Missing MQTT_BROKER env variable. Using default."
#define MQTT_PORT 8883
#endif

#ifndef MQTT_USERNAME
#warning "Missing MQTT_USERNAME env variable. Using default."
#define MQTT_USERNAME "guest"
#endif

#ifndef MQTT_PASSWORD
#warning "Missing MQTT_PASSWORD env variable. Using default."
#define MQTT_PASSWORD "1234"
#endif

static esp_mqtt_client_handle_t client = NULL;
static const esp_mqtt_client_config_t mqtt_cfg = {
    .broker =
        {
            .address =
                {
                    .transport = MQTT_TRANSPORT_OVER_SSL,
                    .hostname = MQTT_BROKER,
                    .port = MQTT_PORT,
                },
            .verification =
                {
                    .certificate = NULL,
                    .certificate_len = 0,
                    .psk_hint_key = NULL,
                    .crt_bundle_attach = NULL,
                    .use_global_ca_store = false,
                    .skip_cert_common_name_check = true,
                    .common_name = NULL,
                },
        },
    .credentials =
        {
            .username = MQTT_USERNAME,
            .authentication =
                {
                    .password = MQTT_PASSWORD,
                    .certificate = NULL,
                    .certificate_len = 0,
                },
        },
};

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    esp_mqtt_client_subscribe(event->client, "/topic/qos0", 0);
    break;
  case MQTT_EVENT_DISCONNECTED:
    break;
  case MQTT_EVENT_SUBSCRIBED:
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    break;
  case MQTT_EVENT_PUBLISHED:
    break;
  case MQTT_EVENT_DATA:
    break;
  case MQTT_EVENT_ERROR:
    break;
  default:
    break;
  }
}

void publish(const char *data) {
  if (client)
    esp_mqtt_client_publish(client, "/topic/qos0", data, 0, 0, 0);
}

void mqtt_app_start(void) {
  client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler,
                                 NULL);
  esp_mqtt_client_start(client);
}