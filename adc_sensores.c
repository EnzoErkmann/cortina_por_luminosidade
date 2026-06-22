/* Módulo: ADC Sensores
 * Leitura dos sensores analógicos de luminosidade (LDR) e temperatura (LM35).
 * Referência de tensão: 3,3 V | Resolução: 12 bits (0–4095).
 */
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdint.h>

#define ADC_CHANNEL_LDR  0   // GPIO 26
#define ADC_CHANNEL_TEMP 1   // GPIO 27

/* Habilita o ADC e coloca os pinos em modo analógico (desativa pulls e função digital). */
void adc_sensores_init(void) {
    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);
}

/* Seleciona o canal, aguarda 200 µs para o multiplexador estabilizar e descarta
 * uma amostra para eliminar resíduo de carga do canal anterior. */
uint16_t adc_ler_canal(uint8_t canal) {
    adc_select_input(canal);
    sleep_us(200);
    (void)adc_read();   // descarta amostra residual
    return adc_read();
}

/* LM35: sensibilidade de 10 mV/°C. Reconstrói a tensão em mV e divide por 10. */
float adc_raw_para_temperatura(uint16_t raw) {
    return (raw * 3300.0f / 4095.0f) / 10.0f;
}

/* Mapeamento linear 0–4095 → 0–100 %. Tensão maior = mais luz = percentual maior. */
uint8_t adc_raw_para_luminosidade_pct(uint16_t raw) {
    return (uint8_t)((raw * 100) / 4095);
}
