#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_task_wdt.h"


#define EXAMPLE_ESP_WIFI_SSID      "ESP32_TEST_HEE"
#define EXAMPLE_ESP_WIFI_PASS      "1234567890"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       4

#define MAX_FILE_SIZE 1024

#define CHUNK_SIZE 512


static const char* TAG = "AP_Server";

#define FILENAME "/spiffs/my_file.bin"

esp_err_t read_file_and_execute() {
    FILE* fileRead = fopen(FILENAME, "rb");
    if (!fileRead) {
        ESP_LOGI(TAG, "Failed to open the file for reading");
        return ESP_FAIL;
    }

    fseek(fileRead, 0, SEEK_END);
    long file_size = ftell(fileRead);
    fseek(fileRead, 0, SEEK_SET);
    ESP_LOGI(TAG, "File size: %ld bytes", file_size);

    if (file_size > 0) {
        char buffer[CHUNK_SIZE];
        char* file_data = (char*)malloc(file_size);

        if (!file_data) {
            ESP_LOGE(TAG, "메모리 할당 실패");
            fclose(fileRead);
            return ESP_ERR_NO_MEM;
        }

        size_t totalBytesRead = 0;
        size_t bytesRead;

        while (totalBytesRead < file_size) {
            size_t bytesToRead = (file_size - totalBytesRead) < CHUNK_SIZE
                ? (file_size - totalBytesRead) : CHUNK_SIZE;

            bytesRead = fread(buffer, 1, bytesToRead, fileRead);

            if (bytesRead != bytesToRead) {
                ESP_LOGE(TAG, "파일 읽기 실패");
                free(file_data);
                fclose(fileRead);
                return ESP_FAIL;
            }

            // 읽은 데이터를 file_data에 복사
            memcpy(file_data + totalBytesRead, buffer, bytesRead);
            totalBytesRead += bytesRead;
        }

        fclose(fileRead);

        // 파일 데이터가 메모리에 적재되었으므로 이제 실행 가능한 코드로 캐스팅 및 실행
        void (*app_entry)(void) = (void (*)(void))file_data;
        // 캐스팅된 함수 포인터를 호출하여 프로그램 실행을 시작합니다.
        app_entry();

        // 프로그램이 실행되면 여기로 돌아오지 않을 것임
        free(file_data);
    }
    else {
        ESP_LOGE(TAG, "File size is not valid or exceeds MAX_FILE_SIZE");
        fclose(fileRead);
        return ESP_FAIL;
    }

    return ESP_OK;
}



esp_err_t write_file(httpd_req_t* req) {
    FILE* file = fopen(FILENAME, "wb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    char buf[64];
    int len;
    int content_length = req->content_len;
    while (content_length > 0) {
        len = httpd_req_recv(req, buf, sizeof(buf));
        if (len <= 0) {
            if (len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            break;
        }
        fwrite(buf, 1, len, file);
        content_length -= len; // 받은 데이터 길이를 빼줍니다.
        ESP_LOGI(TAG, "writing to SPIFFS : %d", content_length);
    }
    fclose(file);


    return ESP_OK;
}



esp_err_t upload_handler(httpd_req_t* req) {
    if (req->method == HTTP_POST) {
        char content[100];
        esp_err_t err;

        err = write_file(req);
        if (err != ESP_OK) {
            return err;
        }

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "File %s uploaded and executed successfully", FILENAME);
        }
        else {
            ESP_LOGE(TAG, "Failed file %s uploading", FILENAME);
        }
      

        xTaskCreate(read_file_and_execute, "read_file_and_execute_task", 4096, NULL, 0, NULL);

        ESP_LOGI(TAG, "file upload response: %s", content);
        httpd_resp_send(req, content, strlen(content));

        return ESP_OK;
    }
    else {
        // POST 요청이 아닌 경우 처리
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method Not Allowed");
        return ESP_FAIL;
    }
}


void start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    esp_err_t start_ret = httpd_start(&server, &config);
    if (start_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start the web server, error: %s", esp_err_to_name(start_ret));
        return;
    }

    httpd_uri_t uri = {
        .uri = "/upload",
        .method = HTTP_POST,
        .handler = upload_handler,
        .user_ctx = NULL
    };
    esp_err_t uri_ret = httpd_register_uri_handler(server, &uri);
    if (uri_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register URI handler, error: %s", esp_err_to_name(uri_ret));
        return;
    }
}

void stop_webserver(httpd_handle_t server) {
    httpd_stop(server);
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
            //ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
            //   MAC2STR(event->mac), event->aid);
            ESP_LOGI(TAG, "WIFI connected");
            // 클라이언트가 연결될 때 실행하려는 코드를 추가할 수 있습니다.
           
            start_webserver();
        }

        else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
            //ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d",
            //    MAC2STR(event->mac), event->aid);
            ESP_LOGI(TAG, "WIFI disconnected");
            // 클라이언트가 연결 해제될 때 실행하려는 코드를 추가할 수 있습니다.
        }
    }
}





void init_spiffs() {
    
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
       .base_path = "/spiffs",
       .partition_label = NULL,
       .max_files = 5,
       .format_if_mount_failed = true
    };

    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
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
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
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



void delete_file() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    // 파일 시스템 초기화
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format filesystem");
    }

    // 파일 삭제
    esp_err_t remove_result = remove(FILENAME);
    if (remove_result == ESP_OK) {
        ESP_LOGI(TAG, "File deleted successfully: %s", FILENAME);
    }
    else {
        ESP_LOGE(TAG, "Failed to delete file: %s", FILENAME);
    }
    // 파일 시스템 해제
    esp_vfs_spiffs_unregister(NULL);
}



void app_main() {
    delete_file();
    init_spiffs();
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
    
}
