/* Módulo: Main
 * Loop principal do sistema de automação de cortina e ventilação.
 * Integra leitura ADC, controle PWM, 7 entradas digitais,
 * interrupção de manutenção e telemetria UART1.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"

#define ADC_CHANNEL_LDR  0
#define ADC_CHANNEL_TEMP 1

#define SERVO_PIN        18   // Servo motor da cortina
#define FAN_PIN          16   // Ventilador via ponte H / transistor
#define BOTAO_MANUAL_PIN 15   // Trava de manutenção (interrupção externa)

// GPIOs 8 e 9 reservados para UART1 — não usar aqui
#define PIN_FIM_ABERTO   2    // Fim de curso: cortina 100% aberta
#define PIN_FIM_FECHADO  3    // Fim de curso: cortina 100% fechada
#define PIN_JANELA       4    // Sensor magnético de janela
#define PIN_PIR          5    // Sensor de presença
#define PIN_CHUVA        6    // Sensor de chuva
#define PIN_CINEMA       7    // Modo cinema
#define PIN_VENT_MAX     14   // Força ventilação máxima

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
void uart_enviar_telemetria(float temp, uint8_t lum_pct, bool cortina_aberta, bool modo_auto, bool fan_ligado);
void uart_ler_loopback_e_imprimir(void);

// Compartilhadas entre ISR e timer — volatile obrigatório
volatile bool     modo_manual          = false;
volatile bool     alvo_aberta          = false;
volatile bool     cortina_estado_fisico = false;
volatile bool     cortina_em_movimento  = false;
volatile uint8_t  timer_10s_counter     = 0;
volatile uint16_t servo_pulse_us        = 1000;

/* ISR do botão de manutenção: alterna modo manual/automático a cada borda de descida. */
void botao_isr(uint gpio, uint32_t events) {
    if (gpio == BOTAO_MANUAL_PIN)
        modo_manual = !modo_manual;
}

/* Timer de 100 ms: controla a rampa do servo e os fins de curso.
 * Lê os sensores de fim de curso diretamente aqui para reação imediata,
 * sem depender do ciclo principal. O debounce de 10 s evita acionamentos
 * espúrios por oscilação do sinal do LDR. */
bool timer_callback(struct repeating_timer *t) {
    if (modo_manual) return true;

    bool st_fim_aberto  = !gpio_get(PIN_FIM_ABERTO);
    bool st_fim_fechado = !gpio_get(PIN_FIM_FECHADO);

    if (cortina_em_movimento) {
        if (alvo_aberta) {
            if (!st_fim_aberto) servo_pulse_us += 20;
            if (servo_pulse_us >= 2000 || st_fim_aberto) {
                if (servo_pulse_us > 2000) servo_pulse_us = 2000;
                cortina_em_movimento = false;
                cortina_estado_fisico = true;
            }
        } else {
            if (!st_fim_fechado) servo_pulse_us -= 20;
            if (servo_pulse_us <= 1000 || st_fim_fechado) {
                if (servo_pulse_us < 1000) servo_pulse_us = 1000;
                cortina_em_movimento = false;
                cortina_estado_fisico = false;
            }
        }
        pwm_set_gpio_level(SERVO_PIN, servo_pulse_us);
    } else {
        // Aguarda 10 s de estado divergente antes de iniciar o movimento
        if (alvo_aberta != cortina_estado_fisico) {
            if (++timer_10s_counter >= 100) {
                cortina_em_movimento = true;
                timer_10s_counter = 0;
            }
        } else {
            timer_10s_counter = 0;
        }
    }
    return true;
}

/* Servo: clkdiv 125 → tick de 1 µs, wrap 20000 → período de 20 ms (50 Hz).
 * Ventilador: clkdiv 125, wrap 1000 → resolução de duty de 0,1%. */
void pwm_atuadores_init() {
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    uint slice_servo = pwm_gpio_to_slice_num(SERVO_PIN);
    pwm_config cfg_servo = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg_servo, 125.0f);
    pwm_config_set_wrap(&cfg_servo, 20000);
    pwm_init(slice_servo, &cfg_servo, true);
    pwm_set_gpio_level(SERVO_PIN, servo_pulse_us);

    gpio_set_function(FAN_PIN, GPIO_FUNC_PWM);
    uint slice_fan = pwm_gpio_to_slice_num(FAN_PIN);
    pwm_config cfg_fan = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg_fan, 125.0f);
    pwm_config_set_wrap(&cfg_fan, 1000);
    pwm_init(slice_fan, &cfg_fan, true);
    pwm_set_gpio_level(FAN_PIN, 0);
}

void hardware_entradas_init() {
    // Botão de manutenção com IRQ na borda de descida
    gpio_init(BOTAO_MANUAL_PIN);
    gpio_set_dir(BOTAO_MANUAL_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_MANUAL_PIN);
    gpio_set_irq_enabled_with_callback(BOTAO_MANUAL_PIN, GPIO_IRQ_EDGE_FALL, true, &botao_isr);

    // 7 entradas digitais com pull-up (lógica invertida: LOW = ativo)
    int pinos_entrada[] = { PIN_FIM_ABERTO, PIN_FIM_FECHADO, PIN_JANELA,
                             PIN_PIR, PIN_CHUVA, PIN_CINEMA, PIN_VENT_MAX };
    for (int i = 0; i < 7; i++) {
        gpio_init(pinos_entrada[i]);
        gpio_set_dir(pinos_entrada[i], GPIO_IN);
        gpio_pull_up(pinos_entrada[i]);
    }
}

/* Histerese de dois limiares: evita chaveamento por pequenas oscilações do sensor. */
bool aplicar_histerese(uint8_t valor, uint8_t lim_h, uint8_t lim_l, bool estado_anterior) {
    if (valor > lim_h) return true;
    if (valor < lim_l) return false;
    return estado_anterior;
}

int main() {
    stdio_init_all();
    adc_sensores_init();
    uart_protocolo_init();
    pwm_atuadores_init();
    hardware_entradas_init();

    struct repeating_timer timer_cortina;
    add_repeating_timer_ms(100, timer_callback, NULL, &timer_cortina);

    ConfigSistema cfg = {
        .lim_lum_h = LIM_LUM_H_DEFAULT,
        .lim_lum_l = LIM_LUM_L_DEFAULT,
        .lim_temp  = LIM_TEMP_DEFAULT,
        .saidas    = 0x00
    };

    sleep_ms(2000);
    printf("=== AP3 Projeto 19 - AUTOMAÇÃO AVANÇADA (7 ENTRADAS + INTERRUPÇÃO) ===\n");

    while (true) {
        uint16_t ldr_raw  = adc_ler_canal(ADC_CHANNEL_LDR);
        uint8_t  lum_pct  = adc_raw_para_luminosidade_pct(ldr_raw);
        uint16_t lm35_raw = adc_ler_canal(ADC_CHANNEL_TEMP);
        float    temp_c   = adc_raw_para_temperatura(lm35_raw);

        // Lógica invertida: pull-up interno → pino LOW quando sensor ativo
        bool st_janela   = !gpio_get(PIN_JANELA);
        bool st_pir      = !gpio_get(PIN_PIR);
        bool st_chuva    = !gpio_get(PIN_CHUVA);
        bool st_cinema   = !gpio_get(PIN_CINEMA);
        bool st_vent_max = !gpio_get(PIN_VENT_MAX);

        // Cascata de decisão da cortina (prioridade crescente):
        // 1) luminosidade com histerese
        alvo_aberta = aplicar_histerese(lum_pct, cfg.lim_lum_h, cfg.lim_lum_l, alvo_aberta);
        // 2) chuva ou cinema sobrescrevem para fechar
        if (st_chuva || st_cinema)
            alvo_aberta = false;
        // 3) janela aberta bloqueia o fechamento para proteger o vidro
        if (st_janela && !alvo_aberta)
            alvo_aberta = true;

        // Cascata de decisão do ventilador
        uint16_t fan_duty = 0;
        if (st_vent_max) {
            fan_duty = 1000;  // forçado em 100% independente de qualquer condição
        } else if (!st_janela && st_pir && temp_c > 25.0f && !modo_manual) {
            // Offset de 35% garante torque de partida no motor 12 V
            fan_duty = 350 + (uint16_t)((temp_c - 25.0f) * 65.0f);
            if (fan_duty > 1000) fan_duty = 1000;
        }
        pwm_set_gpio_level(FAN_PIN, fan_duty);

        uart_enviar_telemetria(temp_c, lum_pct, cortina_estado_fisico, !modo_manual, fan_duty > 0);

        sleep_ms(50);
        uart_ler_loopback_e_imprimir();
        sleep_ms(950);
    }
    return 0;
}
