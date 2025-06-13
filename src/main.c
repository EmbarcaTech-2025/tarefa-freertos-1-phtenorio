#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// =================================================================================
// --- Configurações Gerais ---
// =================================================================================
#define LED_R_PIN 13
#define LED_G_PIN 11
#define LED_B_PIN 12
#define LED_DELAY_MS 500

#define BUZZER_PIN 10
#define BUZZER_FREQ 2700
#define BUZZER_VOLUME_PERCENT 50.0f
#define BUZZER_BEEP_DURATION_MS 300
#define BUZZER_BEEP_PERIOD_MS 1000

#define BTN_A_PIN 6 // Controla o LED e MUTE do buzzer
#define BTN_B_PIN 5 // Controla o ON/OFF do buzzer
#define BUTTON_POLL_DELAY_MS 100

// =================================================================================
// --- Tipos e Handles Globais ---
// =================================================================================
typedef enum {
    CMD_TOGGLE_BEEP,
    CMD_MUTE_BEEP
} buzzer_cmd_t;

TaskHandle_t xLedTaskHandle = NULL;
QueueHandle_t xBuzzerQueue = NULL;

// =================================================================================
// --- Funções de Inicialização de Hardware ---
// =================================================================================

// NOVO: Função para inicializar os pinos dos LEDs
void leds_init() {
    gpio_init(LED_R_PIN);
    gpio_set_dir(LED_R_PIN, GPIO_OUT);
    gpio_init(LED_G_PIN);
    gpio_set_dir(LED_G_PIN, GPIO_OUT);
    gpio_init(LED_B_PIN);
    gpio_set_dir(LED_B_PIN, GPIO_OUT);
}

// NOVO: Função para inicializar os pinos dos botões
void buttons_init() {
    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);

    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
}

// Função para inicializar o PWM do buzzer
void buzzer_pwm_init() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    uint chan_num = pwm_gpio_to_channel(BUZZER_PIN);

    pwm_config config = pwm_get_default_config();
    uint32_t wrap = 65535;
    float div = (float)clock_get_hz(clk_sys) / (wrap * BUZZER_FREQ);
    pwm_config_set_clkdiv(&config, div);
    pwm_config_set_wrap(&config, wrap);
    pwm_init(slice_num, &config, false); 
    uint16_t level = (uint16_t)(wrap * (BUZZER_VOLUME_PERCENT / 100.0f));
    pwm_set_chan_level(slice_num, chan_num, level);
}

// =================================================================================
// --- Definições das Tarefas ---
// =================================================================================

// Tarefa do LED agora não contém mais inicialização
void led_task(void *pvParameters) {
    while (true) {
        gpio_put(LED_R_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(LED_DELAY_MS));
        gpio_put(LED_R_PIN, 0);

        gpio_put(LED_G_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(LED_DELAY_MS));
        gpio_put(LED_G_PIN, 0);

        gpio_put(LED_B_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(LED_DELAY_MS));
        gpio_put(LED_B_PIN, 0);
    }
}

// Tarefa do Buzzer (sem alterações)
void buzzer_task(void *pvParameters) {
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    buzzer_cmd_t received_cmd;
    bool beeping_enabled = false;

    while (true) {
        if (xQueueReceive(xBuzzerQueue, &received_cmd, 0) == pdPASS) {
            if (received_cmd == CMD_TOGGLE_BEEP) {
                beeping_enabled = !beeping_enabled;
            } else if (received_cmd == CMD_MUTE_BEEP) {
                beeping_enabled = false;
            }
        }

        if (beeping_enabled) {
            pwm_set_enabled(slice_num, true);
            vTaskDelay(pdMS_TO_TICKS(BUZZER_BEEP_DURATION_MS));
            pwm_set_enabled(slice_num, false);
            vTaskDelay(pdMS_TO_TICKS(BUZZER_BEEP_PERIOD_MS - BUZZER_BEEP_DURATION_MS));
        } else {
            pwm_set_enabled(slice_num, false);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

// Tarefa dos Botões agora não contém mais inicialização
void button_task(void *pvParameters) {
    static bool btn_a_last_state = false;
    static bool btn_b_last_state = false;
    static bool is_led_suspended = false;

    while (true) {
        // Lógica do Botão A
        bool btn_a_current_state = !gpio_get(BTN_A_PIN); 
        if (btn_a_current_state && !btn_a_last_state) {
            is_led_suspended = !is_led_suspended;
            if (is_led_suspended) {
                vTaskSuspend(xLedTaskHandle);
            } else {
                vTaskResume(xLedTaskHandle);
            }
            buzzer_cmd_t cmd = CMD_MUTE_BEEP;
            xQueueSend(xBuzzerQueue, &cmd, 0);
        }
        btn_a_last_state = btn_a_current_state;

        // Lógica do Botão B
        bool btn_b_current_state = !gpio_get(BTN_B_PIN);
        if (btn_b_current_state && !btn_b_last_state) {
            buzzer_cmd_t cmd = CMD_TOGGLE_BEEP;
            xQueueSend(xBuzzerQueue, &cmd, 0);
        }
        btn_b_last_state = btn_b_current_state;
        
        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_DELAY_MS));
    }
}

// =================================================================================
// --- Função Principal ---
// =================================================================================
int main() {
    stdio_init_all();
    
    // Bloco de inicialização de hardware centralizado
    printf("Inicializando hardware...\n");
    leds_init();
    buttons_init();
    buzzer_pwm_init();
    
    // Cria a Fila de Mensagens para comunicação com a tarefa do buzzer
    xBuzzerQueue = xQueueCreate(5, sizeof(buzzer_cmd_t));
    if (xBuzzerQueue == NULL) {
        printf("ERRO: Falha ao criar a fila do buzzer\n");
        while(true);
    }
    
    printf("Iniciando Tarefas FreeRTOS...\n");

    // Cria as tarefas do sistema
    xTaskCreate(led_task, "LED_Task", 256, NULL, 1, &xLedTaskHandle);
    xTaskCreate(buzzer_task, "Buzzer_Task", 256, NULL, 1, NULL); 
    xTaskCreate(button_task, "Button_Task", 256, NULL, 2, NULL);

    // Inicia o escalonador do FreeRTOS
    vTaskStartScheduler();

    // O código nunca deve chegar aqui
    while(1){};
    return 0;
}