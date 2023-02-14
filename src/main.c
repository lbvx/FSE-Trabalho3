#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/semphr.h"

#include "wifi.h"
#include "mqtt.h"
#include "dht11.h"

#define DHT_GPIO 18
#define BALL_GPIO 5
#define BOTAO_GPIO 21
#define BOTAO_PLACA_GPIO 0
#define LED_PLACA_GPIO 2
#define LED_VERDE_GPIO 3
#define LED_VERMELHO_GPIO 23

SemaphoreHandle_t conexaoWifiSemaphore;
SemaphoreHandle_t conexaoMQTTSemaphore;

void conectadoWifi(void *params)
{
    while (true)
    {
        if (xSemaphoreTake(conexaoWifiSemaphore, portMAX_DELAY))
        {
            // Processamento Internet
            mqtt_start();
        }
    }
}

void comunicaTemperaturaUmidade(void *params)
{
    char mensagem[50];
    char tag[] = "DHT";
    while (true)
    {
        xSemaphoreTake(conexaoMQTTSemaphore, portMAX_DELAY);
        struct dht11_reading leitura = DHT11_read();
        if (leitura.status == 0)
        {
            // DHT
            sprintf(mensagem, "{\"temperatura1\": \"%d\"}", leitura.temperature);
            mqtt_envia_mensagem("v1/devices/me/telemetry", mensagem);
            ESP_LOGD(tag, "Temperatura enviada:%d", leitura.temperature);
            sprintf(mensagem, "{\"umidade1\": \"%d\"}", leitura.humidity);
            mqtt_envia_mensagem("v1/devices/me/telemetry", mensagem);
            ESP_LOGD(tag, "Umidade enviada:%d", leitura.humidity);
        }
        xSemaphoreGive(conexaoMQTTSemaphore);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    
}

void comunicaSensores(void *params)
{
    char mensagem[50];
    char tag[] = "Sensores";
    
    while (true)
    {
        xSemaphoreTake(conexaoMQTTSemaphore, portMAX_DELAY);
        // Ball Switch
        int ball_switch = gpio_get_level(BALL_GPIO);
        sprintf(mensagem, "{\"ballswitch1\": %d}", ball_switch);
        mqtt_envia_mensagem("v1/devices/me/attributes", mensagem);
        ESP_LOGD(tag, "Ballswitch enviado:%d", ball_switch);
        // Botao Placa
        int botaoPlaca = gpio_get_level(BOTAO_PLACA_GPIO);
        sprintf(mensagem, "{\"botaoPlaca1\": %d}", botaoPlaca);
        mqtt_envia_mensagem("v1/devices/me/attributes", mensagem);
        ESP_LOGD(tag, "BotaoPlaca enviado:%d", botaoPlaca);
        // Botao
        int botao = gpio_get_level(BOTAO_GPIO);
        sprintf(mensagem, "{\"button1\": %d}", botao);
        mqtt_envia_mensagem("v1/devices/me/attributes", mensagem);
        ESP_LOGD(tag, "Botao enviado:%d", botao);

        xSemaphoreGive(conexaoMQTTSemaphore);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    
}

void gerenciaEntrada(void *params)
{
    while (true)
    {
        int estado_botao = gpio_get_level(BOTAO_PLACA_GPIO);
        gpio_set_level(LED_PLACA_GPIO, estado_botao);
        gpio_set_level(LED_VERDE_GPIO, !estado_botao);

        int estado_ballswitch = gpio_get_level(BALL_GPIO);
        gpio_set_level(LED_VERMELHO_GPIO, estado_ballswitch);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void setupGPIO()
{
    // DHT
    DHT11_init(DHT_GPIO);
    // Ball Switch
    esp_rom_gpio_pad_select_gpio(BALL_GPIO);
    gpio_set_direction(BALL_GPIO, GPIO_MODE_INPUT);
    // Botao Placa
    esp_rom_gpio_pad_select_gpio(BOTAO_PLACA_GPIO);
    gpio_set_direction(BOTAO_PLACA_GPIO, GPIO_MODE_INPUT);
    // LED Placa
    esp_rom_gpio_pad_select_gpio(LED_PLACA_GPIO);
    gpio_set_direction(LED_PLACA_GPIO, GPIO_MODE_OUTPUT);
    // LED Verde
    esp_rom_gpio_pad_select_gpio(LED_VERDE_GPIO);
    gpio_set_direction(LED_VERDE_GPIO, GPIO_MODE_OUTPUT);
    // LED VERMELHO
    esp_rom_gpio_pad_select_gpio(LED_VERMELHO_GPIO);
    gpio_set_direction(LED_VERMELHO_GPIO, GPIO_MODE_OUTPUT);
    // Botao
    esp_rom_gpio_pad_select_gpio(BOTAO_GPIO);
    gpio_set_direction(BOTAO_GPIO, GPIO_MODE_INPUT);
}

void app_main(void)
{
    // Inicializa o NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    setupGPIO();

    conexaoWifiSemaphore = xSemaphoreCreateBinary();
    conexaoMQTTSemaphore = xSemaphoreCreateBinary();
    wifi_start();

    xTaskCreate(&conectadoWifi, "Conexão ao MQTT", 4096, NULL, 1, NULL);
    xTaskCreate(&comunicaTemperaturaUmidade, "Temp/Umid", 4096, NULL, 1, NULL);
    xTaskCreate(&comunicaSensores, "Sensores e Leds", 4096, NULL, 1, NULL);
    xTaskCreate(&gerenciaEntrada, "Gerencia Entrada", 4096, NULL, 1, NULL);
}
