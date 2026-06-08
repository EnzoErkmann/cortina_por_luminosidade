/* Módulo: Main / Programa Principal */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

// --- CONSTANTES NECESSÁRIAS NO MAIN ---
#define LED_PIN 25
#define ADC_CHANNEL_LDR  0
#define ADC_CHANNEL_TEMP 1
#define UART_ID uart0

// Limites padrão
#define LIM_LUM_H_DEFAULT 70
#define LIM_LUM_L_DEFAULT 30
#define LIM_TEMP_DEFAULT  35

// --- ESTRUTURAS ---
typedef struct {
    uint8_t lim_lum_h;
    uint8_t lim_lum_l;
    uint8_t lim_temp;
    uint8_t saidas;
} ConfigSistema;

// --- PROTÓTIPOS DE FUNÇÕES (Módulos C Externos) ---
void adc_sensores_init(void);
uint16_t adc_ler_canal(uint8_t canal);
float adc_raw_para_temperatura(uint16_t raw);

void uart_protocolo_init(void);
void uart_enviar_telemetria(float temp, uint8_t lum_pct, bool cortina_aberta);
bool uart_processar_recepcao(ConfigSistema *cfg);

// --- FUNÇÕES AUXILIARES (Substituindo o Assembly) ---
uint32_t mapear_adc(uint32_t raw, uint32_t raw_max, uint32_t out_max) {
    return (raw * out_max) / raw_max;
}

bool aplicar_histerese(uint8_t valor, uint8_t lim_h, uint8_t lim_l, bool estado_anterior) {
    if (valor > lim_h) {
        return true;  // Abre a cortina
    }
    if (valor < lim_l) {
        return false; // Fecha a cortina
    }
    return estado_anterior; // Mantém o estado (zona morta)
}

int main() {
    stdio_init_all();
    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    adc_sensores_init();
    uart_protocolo_init();
    
    ConfigSistema cfg = {
        .lim_lum_h = LIM_LUM_H_DEFAULT,
        .lim_lum_l = LIM_LUM_L_DEFAULT,
        .lim_temp  = LIM_TEMP_DEFAULT,
        .saidas    = 0x00
    };
    
    bool cortina_aberta = false; 

    uart_puts(UART_ID, "=== AP2 Projeto 19 - Cortina por Luminosidade ===\r\n");
    uart_puts(UART_ID, "UART0: 115200 baud | GPIO0=TX | GPIO1=RX\r\n");
    uart_puts(UART_ID, "ADC0=GPIO26(LDR) | ADC1=GPIO27(LM35)\r\n");
    uart_puts(UART_ID, "-------------------------------------------------\r\n");

    while (true) {
        uart_processar_recepcao(&cfg);
        
        uint16_t ldr_raw = adc_ler_canal(ADC_CHANNEL_LDR);
        // Usando a função implementada em C agora
        uint8_t lum_pct = (uint8_t)mapear_adc(ldr_raw, 4095, 100);
        
        uint16_t lm35_raw = adc_ler_canal(ADC_CHANNEL_TEMP);
        float temp_c = adc_raw_para_temperatura(lm35_raw);
        
        // Aplicando a histerese em C
        cortina_aberta = aplicar_histerese(lum_pct, cfg.lim_lum_h, cfg.lim_lum_l, cortina_aberta);
        
        uart_enviar_telemetria(temp_c, lum_pct, cortina_aberta);
        
        gpio_put(LED_PIN, 1);
        sleep_ms(50); 
        gpio_put(LED_PIN, 0);
        
        sleep_ms(950);
    }

    return 0;
}