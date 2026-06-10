/* Módulo: ADC Sensores */
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdint.h>

// Pinos ADC
#define ADC_CHANNEL_LDR  0   // GPIO26
#define ADC_CHANNEL_TEMP 1   // GPIO27

void adc_sensores_init(void) {
    adc_init();
    adc_gpio_init(26); // LDR (ADC0)
    adc_gpio_init(27); // LM35 (ADC1)
}

uint16_t adc_ler_canal(uint8_t canal) {
    adc_select_input(canal);
    sleep_us(200); 
    
    // Leitura dummy para descarregar capacitor do canal anterior
    (void)adc_read(); 
    
    // Leitura verdadeira
    return adc_read();
}

float adc_raw_para_temperatura(uint16_t raw) {
    return (raw * 3300.0f / 4095.0f) / 10.0f;
}

uint8_t adc_raw_para_luminosidade_pct(uint16_t raw) {
    return (uint8_t)((raw * 100) / 4095);
}
