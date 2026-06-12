/* Módulo: Main / Programa Principal (LOOPBACK + AP3) */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"

// Pinos dos Sensores
#define ADC_CHANNEL_LDR  0
#define ADC_CHANNEL_TEMP 1

// Pinos dos Atuadores e Botão (AP3)
#define SERVO_PIN 17          // Pino do Servo Motor da Cortina
#define FAN_PIN 16            // Pino do Ventilador (Ponte H / Transistor)
#define BOTAO_MANUAL_PIN 15   // Botão de Interrupção Externa (Override)

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
void uart_ler_loopback_e_imprimir(void);

// =========================================================================
// VARIÁVEIS GLOBAIS (Compartilhadas com as Interrupções da AP3)
// =========================================================================
volatile bool modo_manual = false;          // Controlado pela Interrupção do Botão
volatile bool alvo_aberta = false;          // O que o sensor LDR "quer" que aconteça
volatile bool cortina_estado_fisico = false;// O estado real atual
volatile bool cortina_em_movimento = false;
volatile uint8_t timer_10s_counter = 0;     // Conta até 100 (100 * 100ms = 10 seg)
volatile uint16_t servo_pulse_us = 1000;    // Posição do servo (1000 = fechado, 2000 = aberto)

// =========================================================================
// INTERRUPÇÕES E TIMERS (AP3)
// =========================================================================
void botao_isr(uint gpio, uint32_t events) {
    if (gpio == BOTAO_MANUAL_PIN) {
        modo_manual = !modo_manual; // Alterna modo manual
    }
}

bool timer_callback(struct repeating_timer *t) {
    if (modo_manual) return true; // Ignora automação se estiver manual

    // LÓGICA DE TRANSIÇÃO SUAVE (5 Segundos)
    if (cortina_em_movimento) {
        if (alvo_aberta) {
            servo_pulse_us += 20; // Aumenta 20us a cada 100ms
            if (servo_pulse_us >= 2000) {
                servo_pulse_us = 2000;
                cortina_em_movimento = false;
                cortina_estado_fisico = true;
            }
        } else {
            servo_pulse_us -= 20; // Diminui 20us a cada 100ms
            if (servo_pulse_us <= 1000) {
                servo_pulse_us = 1000;
                cortina_em_movimento = false;
                cortina_estado_fisico = false;
            }
        }
        pwm_set_gpio_level(SERVO_PIN, servo_pulse_us);
    } 
    // LÓGICA DE DEBOUNCE (Tolerância de 10 Segundos)
    else {
        if (alvo_aberta != cortina_estado_fisico) {
            timer_10s_counter++;
            if (timer_10s_counter >= 100) { // Passou 10 segundos estáveis
                cortina_em_movimento = true;
                timer_10s_counter = 0;
            }
        } else {
            timer_10s_counter = 0; // Alarme falso, reseta
        }
    }
    return true;
}

// INICIALIZAÇÃO DE HARDWARE AP3
void pwm_atuadores_init() {
    // Servo (50Hz)
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    uint slice_servo = pwm_gpio_to_slice_num(SERVO_PIN);
    pwm_config cfg_servo = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg_servo, 125.0f); 
    pwm_config_set_wrap(&cfg_servo, 20000);    
    pwm_init(slice_servo, &cfg_servo, true);
    pwm_set_gpio_level(SERVO_PIN, servo_pulse_us); 

    // Ventilador (1kHz)
    gpio_set_function(FAN_PIN, GPIO_FUNC_PWM);
    uint slice_fan = pwm_gpio_to_slice_num(FAN_PIN);
    pwm_config cfg_fan = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg_fan, 125.0f); 
    pwm_config_set_wrap(&cfg_fan, 1000);       
    pwm_init(slice_fan, &cfg_fan, true);
    pwm_set_gpio_level(FAN_PIN, 0);            
}

void botao_interrupcao_init() {
    gpio_init(BOTAO_MANUAL_PIN);
    gpio_set_dir(BOTAO_MANUAL_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_MANUAL_PIN); 
    gpio_set_irq_enabled_with_callback(BOTAO_MANUAL_PIN, GPIO_IRQ_EDGE_FALL, true, &botao_isr);
}

bool aplicar_histerese(uint8_t valor, uint8_t lim_h, uint8_t lim_l, bool estado_anterior) {
    if (valor > lim_h) return true;
    if (valor < lim_l) return false;
    return estado_anterior;
}

int main() {
    stdio_init_all();
    adc_sensores_init();
    uart_protocolo_init(); 
    
    // Inicia periféricos da AP3
    pwm_atuadores_init();
    botao_interrupcao_init();
    
    // Hardware Timer rodando a cada 100ms
    struct repeating_timer timer_cortina;
    add_repeating_timer_ms(100, timer_callback, NULL, &timer_cortina);
    
    ConfigSistema cfg = {
        .lim_lum_h = LIM_LUM_H_DEFAULT,
        .lim_lum_l = LIM_LUM_L_DEFAULT,
        .lim_temp  = LIM_TEMP_DEFAULT,
        .saidas    = 0x00
    };

    sleep_ms(2000); 
    printf("=== AP3 Projeto 19 - LOOPBACK + PWM/TIMER ===\n");
    printf("Ligue um fio jumper do pino GPIO0 (TX) ao GPIO1 (RX)\n");
    printf("---------------------------------------------------\n");

    while (true) {
        // Leituras dos sensores
        uint16_t ldr_raw = adc_ler_canal(ADC_CHANNEL_LDR);
        uint8_t lum_pct = adc_raw_para_luminosidade_pct(ldr_raw); 
        
        uint16_t lm35_raw = adc_ler_canal(ADC_CHANNEL_TEMP);
        float temp_c = adc_raw_para_temperatura(lm35_raw);
        
        // Atualiza a vontade do sistema (o Timer executará a ação)
        alvo_aberta = aplicar_histerese(lum_pct, cfg.lim_lum_h, cfg.lim_lum_l, alvo_aberta);
        
        // PWM Ventilador (Proporcional à temp > 25)
        uint16_t fan_duty = 0;
        if (temp_c > 25.0f && !modo_manual) {
            fan_duty = (uint16_t)((temp_c - 25.0f) * 66.0f);
            if (fan_duty > 1000) fan_duty = 1000;
        }
        pwm_set_gpio_level(FAN_PIN, fan_duty);
        
        // Envia telemetria usando o estado físico real da cortina
        uart_enviar_telemetria(temp_c, lum_pct, cortina_estado_fisico);
        
        // Espera o dado viajar e imprime (Mantendo a sua estrutura de 1 segundo!)
        sleep_ms(50);
        uart_ler_loopback_e_imprimir();
        sleep_ms(950);
    }
    return 0;
}
