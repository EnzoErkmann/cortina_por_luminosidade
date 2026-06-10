/* Módulo: Main / Programa Principal (LOOPBACK) */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

#define ADC_CHANNEL_LDR  0
#define ADC_CHANNEL_TEMP 1

// Limites padrão
#define LIM_LUM_H_DEFAULT 70
#define LIM_LUM_L_DEFAULT 30
#define LIM_TEMP_DEFAULT  35

typedef struct {
    uint8_t lim_lum_h;
    uint8_t lim_lum_l;
    uint8_t lim_temp;
    uint8_t saidas;
} ConfigSistema;

// Protótipos
void adc_sensores_init(void);
uint16_t adc_ler_canal(uint8_t canal);
float adc_raw_para_temperatura(uint16_t raw);
uint8_t adc_raw_para_luminosidade_pct(uint16_t raw);

void uart_protocolo_init(void);
void uart_enviar_telemetria(float temp, uint8_t lum_pct, bool cortina_aberta);
void uart_ler_loopback_e_imprimir(void); // <-- Novo protótipo do loopback

uint32_t mapear_adc(uint32_t raw, uint32_t raw_max, uint32_t out_max) {
    return (raw * out_max) / raw_max;
}

bool aplicar_histerese(uint8_t valor, uint8_t lim_h, uint8_t lim_l, bool estado_anterior) {
    if (valor > lim_h) return true;
    if (valor < lim_l) return false;
    return estado_anterior;
}

int main() {
    // Inicializa a porta USB para comunicação com o VS Code (Canal PC)
    stdio_init_all();
    
    adc_sensores_init();
    uart_protocolo_init(); // Inicializa os pinos de hardware TX e RX
    
    ConfigSistema cfg = {
        .lim_lum_h = LIM_LUM_H_DEFAULT,
        .lim_lum_l = LIM_LUM_L_DEFAULT,
        .lim_temp  = LIM_TEMP_DEFAULT,
        .saidas    = 0x00
    };
    
    bool cortina_aberta = false; 

    sleep_ms(2000); 
    printf("=== AP2 Projeto 19 - TESTE DE LOOPBACK FISICO ===\n");
    printf("Ligue um fio jumper do pino GPIO0 (TX) ao GPIO1 (RX)\n");
    printf("---------------------------------------------------\n");

    while (true) {
        // Leituras dos sensores
        uint16_t ldr_raw = adc_ler_canal(ADC_CHANNEL_LDR);
        uint8_t lum_pct = adc_raw_para_luminosidade_pct(ldr_raw); 
        
        uint16_t lm35_raw = adc_ler_canal(ADC_CHANNEL_TEMP);
        float temp_c = adc_raw_para_temperatura(lm35_raw);
        
        cortina_aberta = aplicar_histerese(lum_pct, cfg.lim_lum_h, cfg.lim_lum_l, cortina_aberta);
        
        // Pico emite o dado pelo pino físico TX
        uart_enviar_telemetria(temp_c, lum_pct, cortina_aberta);
        
        // Espera o dado viajar pelo jumper até o pino RX
        sleep_ms(50);
        
        // Lê o pino RX e imprime na tela
        uart_ler_loopback_e_imprimir();
        
        // Tempo restante da amostragem total do sistema
        sleep_ms(950);
    }
    return 0;
}
