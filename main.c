/* Módulo: Main / Programa Principal (Foco AP2: ADC + UART) */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

// --- CONSTANTES NECESSÁRIAS NO MAIN ---
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
    uint8_t saidas; // Armazenado na memória via UART (Implementação física no projeto final)
} ConfigSistema;

// --- PROTÓTIPOS DE FUNÇÕES (Módulos C Externos) ---
void adc_sensores_init(void);
uint16_t adc_ler_canal(uint8_t canal);
float adc_raw_para_temperatura(uint16_t raw);

void uart_protocolo_init(void);
void uart_enviar_telemetria(float temp, uint8_t lum_pct, bool cortina_aberta);
bool uart_processar_recepcao(ConfigSistema *cfg);

// --- FUNÇÕES AUXILIARES (Lógica da Cortina em C) ---
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
        // A função abaixo processa a UART. Se o comando SAIDAS for recebido, 
        // ele será validado e guardado em cfg.saidas, cumprindo o requisito da AP2.
        uart_processar_recepcao(&cfg);
        
        // Leituras ADC
        uint16_t ldr_raw = adc_ler_canal(ADC_CHANNEL_LDR);
        uint8_t lum_pct = (uint8_t)mapear_adc(ldr_raw, 4095, 100);
        
        uint16_t lm35_raw = adc_ler_canal(ADC_CHANNEL_TEMP);
        float temp_c = adc_raw_para_temperatura(lm35_raw);
        
        // Lógica
        cortina_aberta = aplicar_histerese(lum_pct, cfg.lim_lum_h, cfg.lim_lum_l, cortina_aberta);
        
        // Envia feedback para o terminal PC via UART
        uart_enviar_telemetria(temp_c, lum_pct, cortina_aberta);
        
        // Tempo de amostragem
        sleep_ms(1000);
    }

    return 0;
}