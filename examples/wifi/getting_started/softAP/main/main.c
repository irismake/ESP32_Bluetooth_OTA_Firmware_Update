/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_spiffs.h" 
#include "lwip/err.h"
#include "lwip/sys.h"
#include "string.h"
#include "stdio.h"
#include "driver/gpio.h"

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "ESP32_TEST_HEE"
#define EXAMPLE_ESP_WIFI_PASS      "1234567890"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       4

static const char *TAG = "wifi softAP";

#define BLINK_GPIO 32

const esp_partition_t* get_running_partition(void) {
    return esp_ota_get_running_partition();
}

void print_partition_info(const esp_partition_t* partition) {
    switch (partition->address) {
    case 0x00010000:
        ESP_LOGI(TAG, "Running partition: factory");
        break;
    case 0x00110000:
        ESP_LOGI(TAG, "Running partition: ota_0");
        break;
    case 0x00210000:
        ESP_LOGI(TAG, "Running partition: ota_1");
        break;
    default:
        ESP_LOGI(TAG, "Running partition: unknown");
        break;
    }
}

void saveCodeToSPIFFS() {

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }


    ESP_LOGI(TAG, "Opening file");
    FILE* f = fopen("/spiffs/hello.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "void setup() {\n");
    fprintf(f, "  pinMode(%d, OUTPUT);\n", BLINK_GPIO);
    fprintf(f, "}\n");
    fprintf(f, "void loop() {\n");
    fprintf(f, "  on\n");
    fprintf(f, "  wait.1\n");
    fprintf(f, "  off\n");
    fprintf(f, "  wait.5\n");
    fprintf(f, "}\n");
    fclose(f);
    ESP_LOGI(TAG, "File written");

    /*f = fopen("/spiffs/hello.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
    // Read and log the content
    char line[64];
    while (fgets(line, sizeof(line), f) != NULL) {
        // strip newline
        char* pos = strchr(line, '\n');
        if (pos) {
            *pos = '\0';
        }
        ESP_LOGI(TAG, "Read from file: '%s'", line);
    }
    fclose(f);

    // Unmount SPIFFS
    esp_vfs_spiffs_unregister(NULL);
    ESP_LOGI(TAG, "SPIFFS unmounted");*/


    //Set the GPIO as a push/pull output 
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);


    for (;;)
    {

        FILE* f = fopen("/spiffs/hello.txt", "r");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading. Error: %s", strerror(errno));
            return;
        }
        char line[64];
        while (fgets(line, sizeof(line), f) != NULL) {
            // strip newline
            char* pos = strchr(line, '\n');
            if (pos) {
                *pos = '\0';
            }

            ESP_LOGI(TAG, "Read from file: '%s'", line);

            if (strstr(line, "on") != NULL) {
                gpio_set_level(BLINK_GPIO, 0);
                ESP_LOGI(TAG, "LED ON");
            }
            else if (strstr(line, "off") != NULL) {
                gpio_set_level(BLINK_GPIO, 1);
                ESP_LOGI(TAG, "LED OFF");
            }
            else if (strstr(line, "wait") != NULL) {
                char* duration_str = strchr(line, '.') + 1;
                int duration = atoi(duration_str);
                ESP_LOGI(TAG, "WAIT for %d seconds", duration);
                vTaskDelay(duration * 1000 / portTICK_PERIOD_MS);  // 초를 밀리초로 변환
                ESP_LOGI(TAG, "WAIT completed");
            }
        }

        fclose(f);
        ESP_LOGI(TAG, "code is executing");
        
    }
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
            MAC2STR(event->mac), event->aid);
        // 진행중인 파티션 정보
        const esp_partition_t* partition = get_running_partition();
        print_partition_info(partition);
        // 코드 실행
        saveCodeToSPIFFS();
        //executeCodeFromSPIFFS();

    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
            MAC2STR(event->mac), event->aid);
    }
}



void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

void app_main(void)
{
    

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
}
