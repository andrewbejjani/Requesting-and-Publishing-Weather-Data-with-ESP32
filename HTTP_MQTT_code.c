// EECE 425 // Course Project // Weather Data using HTTP and MQTT Clients
// Andrew Bejjani ; ID: 202100407 // Hadil Abdel Samad ; ID: 202101780
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_http_client.h"

#include <cJSON.h>
#include <time.h>
#include <math.h>

static const char *TAG = "MQTT_TCP";

// WiFi functions

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi connecting ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        break;
    default:
        break;
    }
}

void wifi_connection()
{
    // 1 - Wi-Fi/LwIP Init Phase
    esp_netif_init();                    // TCP/IP initiation                   s1.1
    esp_event_loop_create_default();     // event loop                          s1.2
    esp_netif_create_default_wifi_sta(); // WiFi station                        s1.3
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation); //                                         s1.4
    // 2 - Wi-Fi Configuration Phase
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "PUT_WIFI_NAME_HERE", // wifi name
            .password = "PUT_WIFI_PASSWORD_HERE"}}; // wifi password
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    // 3 - Wi-Fi Start Phase
    esp_wifi_start();
    // 4- Wi-Fi Connect Phase
    esp_wifi_connect();
}

static void wifi_init() {
    //wifi initialization
    nvs_flash_init();
    wifi_connection();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    printf("WIFI was initiated ...........\n");
}

//HTTP functions

char dest[1024];    //holds the entire data of the json file from the weather map

esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        strncat(dest, (char*)evt->data, evt->data_len); //concatinate the content of the data to the dest array
        break;

    default:
        break;
    }
    return ESP_OK;
}

static void rest_get()
{
    esp_http_client_config_t config_get =
    {   //get the data from the weather app through the following url
        .url = "http://api.openweathermap.org/data/2.5/weather?q=Beirut,LB&units=metric&appid=d0afc4eefe171b18b64c30ee88867366",
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .event_handler = client_event_get_handler
    };
       
    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

char loc[128];
void weatherConditions()
{   //getting the weather location and storing it in the loc buffer
    char *dest_1 = dest;
    cJSON *root = cJSON_Parse(dest_1);
    char *name = cJSON_GetObjectItem(root, "name")->valuestring; //name of city
    cJSON *sys = cJSON_GetObjectItem(root, "sys");
    char *country = cJSON_GetObjectItem(sys, "country")->valuestring; //name of country
    sprintf(loc,"Weather conditions in %s, %s\n",(char*) name, ( char *)country);
}

char date[128];
void dt()
{   //getting the weather date and time and storing it in the date buffer
    char *dest_1 = dest;
    cJSON *root = cJSON_Parse(dest_1);
    int dt = cJSON_GetObjectItem(root, "dt")->valueint; //dt value
    int timezone = cJSON_GetObjectItem(root, "timezone")->valueint; //timezone difference
    time_t epoch1 = dt;
    time_t epoch2 = timezone;
    time_t epoch = epoch1 + epoch2;
    struct tm ts ;
    ts = * localtime (& epoch ); //converting our epoch to a local date and time
    strftime ( date , sizeof ( date ), "%A, %B %d, %Y %I:%M:%S %p\n" , &ts ); //proper display for date and time
}

char temperature[128];
void temp()
{  //getting the weather temperature and storing it in the temperature buffer
    char *dest_1 = dest;
    cJSON *root = cJSON_Parse(dest_1);
    cJSON *main_1 = cJSON_GetObjectItem(root, "main");
    double temp = cJSON_GetObjectItem(main_1, "temp")->valuedouble; //getting the temperature value
    sprintf(temperature,"%.2f\n",(double)temp);
}

char hum[128];
void humidity()
{  //getting the weather humidity and storing it in the hum buffer
    char *dest_1 = dest;
    cJSON *root = cJSON_Parse(dest_1);
    cJSON *main_2 = cJSON_GetObjectItem(root, "main");
    int humidity = cJSON_GetObjectItem(main_2, "humidity")->valueint; //getting the humidity value
    sprintf(hum,"%d%%\n",(int)humidity);
}

char cond[128];
void conditions()
{   //getting the weather conditions and appending it to the data buffer
    char *dest_1 = dest;
    cJSON *root = cJSON_Parse(dest_1);
    cJSON *weather = cJSON_GetObjectItem(root, "weather");
    cJSON *w = cJSON_GetArrayItem(weather, 0); //accessing first element of array which gives a json type object
    char *description = cJSON_GetObjectItem(w, "description")->valuestring; //accessing description of weather
    sprintf(cond,"%s\n",(char *)description);
}

char wind_data[128];
void wind_direction()
{   //getting the wind information and storing it in the wind_data buffer
    char *dest_1 = dest;
    cJSON *root = cJSON_Parse(dest_1);
    cJSON *wind = cJSON_GetObjectItem(root, "wind");
    double speed = cJSON_GetObjectItem(wind, "speed")->valuedouble; //getting wind speed
    int c = cJSON_GetArraySize(wind);
    speed = speed*3.6; //converting from m/s to km/h

    int deg = cJSON_GetObjectItem(wind, "deg")->valueint;
    deg= deg/45; //divide by 45 to have the deg value range between 0 and 8 since we have 8 directions
    deg = round(deg); //round it since a value of 1.6 for example is closer to direction 2 than direction 1

    if (c == 3) {   //if we have gust (3 objects in wind indicate gust is the 3rd object)
        double gust = cJSON_GetObjectItem(wind, "gust")->valuedouble; //get gust value
        gust = gust*3.6;    //converting gust from m/s to km/h
        if (deg==0||deg==8){sprintf(wind_data,"%.2f km/h (%.2f km/h gust) from the N\n",speed, gust);}
        else if (deg==1){sprintf(wind_data,"%.2f km/h (%.2f km/h gust) from the NE\n",speed, gust);}
        else if (deg==2){sprintf(wind_data,"%.2f km/h (%.2f km/h gust) from the E\n",speed, gust);}
        else if (deg==3){sprintf(wind_data,"%.2f km/h (%.2f km/h gust) from the SE\n",speed, gust);}
        else if (deg==4){sprintf(wind_data,"%.2f km/h (%.2f km/h gust) from the S\n",speed, gust);}
        else if (deg==5){sprintf(wind_data,"%.2f km/h (%.2f km/h gust) from the SW\n",speed, gust);}
        else if (deg==6){sprintf(wind_data,"%.2f km/h (%.2f km/h gust) from the W\n",speed, gust);}
        else if (deg==7){sprintf(wind_data,"%.2f km/h (%.2f km/h gust) from the NW\n",speed, gust);}
    }
    else if (c == 2) {  //if we don't have gust
        if (deg==0||deg==8){sprintf(wind_data,"%.2f km/h from the N\n",speed);}
        else if (deg==1){sprintf(wind_data,"%.2f km/h from the NE\n",speed);}
        else if (deg==2){sprintf(wind_data,"%.2f km/h from the E\n",speed);}
        else if (deg==3){sprintf(wind_data,"%.2f km/h from the SE\n",speed);}
        else if (deg==4){sprintf(wind_data,"%.2f km/h from the S\n",speed);}
        else if (deg==5){sprintf(wind_data,"%.2f km/h from the SW\n",speed);}
        else if (deg==6){sprintf(wind_data,"%.2f km/h from the W\n",speed);}
        else if  (deg==7){sprintf(wind_data,"%.2f km/h from the NW\n",speed);}
    }
}

static void http_init() {
    //http initialization and getting the needed information from the weather map
    rest_get();
    weatherConditions();
    dt();
    temp();
    humidity();
    conditions();
    wind_direction();
}

// MQTT functions
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED"); //subscribe to topic where node red will publish
        esp_mqtt_client_subscribe(event->client, "weather/request", 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:   //visited when we receive a message on the topic we're subscribed to
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");   //meaning the user has initated a request for weather data
        http_init();    //get the weather data
        esp_mqtt_client_publish(client, "weather/country",loc, 0,1,0);
        esp_mqtt_client_publish(client, "weather/date",date, 0,1,0);
        esp_mqtt_client_publish(client, "weather/temperature",temperature, 0,1,0);
        esp_mqtt_client_publish(client, "weather/humidity",hum, 0,1,0);
        esp_mqtt_client_publish(client, "weather/conditions",cond, 0,1,0);
        esp_mqtt_client_publish(client, "weather/wind",wind_data, 0,1,0);
        break;  //publish the data on six different topics, each corresponding to a weather attribute
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://mqtt.eclipseprojects.io",
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void app_main()
{
    wifi_init();
    mqtt_app_start();
}