/* Módulo: Main / Programa Principal (LOOPBACK + AP3 + TELEMETRIA COMPLETA + 7 ENTRADAS) */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"

// Pinos dos Sensores Analógicos
#define ADC_CHANNEL_LDR  0
#define ADC_CHANNEL_TEMP 1

// Pinos dos Atuadores e Botão de Manutenção (AP3)
#define SERVO_PIN 18          // Pino do Servo Motor da Cortina
#define FAN_PIN 16            // Pino do Ventilador (Ponte H / Transistor)
#define BOTAO_MANUAL_PIN 15   // Botão de Interrupção Externa (Trava de Manutenção / Override)

// =========================================================================
// NOVOS PINOS: AS 7 ENTRADAS DIGITAIS DE AUTOMAÇÃO AVANÇADA
// =========================================================================
#define PIN_FIM_ABERTO    2  // 1. Fim de Curso: Cortina 100% aberta
#define PIN_FIM_FECHADO   3  // 2. Fim de Curso: Cortina 100% fechada
#define PIN_JANELA        4  // 3. Sensor Magnético: Janela aberta
#define PIN_PIR           5  // 4. Sensor de Presença (PIR)
#define PIN_CHUVA         6  // 5. Sensor de Chuva
#define PIN_CINEMA        7  // 6. Modo Cinema
#define PIN_VENT_MAX      8  // 7. Forçar Ventilação Máxima

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
void uart_enviar_telemetria(float temp, uint8_t lum_pct, bool cortina_aberta, bool modo_auto, bool fan_ligado);
void uart_ler_loopback_e_imprimir(void);

// =========================================================================
// VARIÁVEIS GLOBAIS
// =========================================================================
volatile bool modo_manual = false;          
volatile bool alvo_aberta = false;          
volatile bool cortina_estado_fisico = false;
volatile bool cortina_em_movimento = false;
volatile uint8_t timer_10s_counter = 0;     
volatile uint16_t servo_pulse_us = 1000;    

// =========================================================================
// INTERRUPÇÕES E TIMERS (AP3)
// =========================================================================

// === TESTE 3: INTERRUPÇÃO EXTERNA (A nossa verdadeira trava de segurança!) ===
void botao_isr(uint gpio, uint32_t events) {
    if (gpio == BOTAO_MANUAL_PIN) {
        modo_manual = !modo_manual; // Alterna modo manual/manutenção
    }
}

// === TESTE 2: TIMER E PWM DO SERVO MOTOR ===
bool timer_callback(struct repeating_timer *t) {
    // Se a trava de manutenção estiver ativada pela interrupção, aborta qualquer ação autônoma
    if (modo_manual) return true; 

    // LER ENTRADAS CRÍTICAS DIRETAMENTE NO TIMER (Ação imediata para proteção mecânica)
    bool st_fim_aberto  = !gpio_get(PIN_FIM_ABERTO);
    bool st_fim_fechado = !gpio_get(PIN_FIM_FECHADO);

    // LÓGICA DE TRANSIÇÃO SUAVE
    if (cortina_em_movimento) {
        if (alvo_aberta) {
            // ENTRADA 1: Só continua o motor se NÃO tiver batido no fim de curso
            if (!st_fim_aberto) servo_pulse_us += 20; 
            
            // Se atingiu o pulso final OU bateu no sensor físico de fim de curso
            if (servo_pulse_us >= 2000 || st_fim_aberto) {
                if (servo_pulse_us > 2000) servo_pulse_us = 2000;
                cortina_em_movimento = false;
                cortina_estado_fisico = true;
            }
        } else {
            // ENTRADA 2: Só fecha se NÃO tiver batido no fim de curso de fechamento
            if (!st_fim_fechado) servo_pulse_us -= 20; 
            
            if (servo_pulse_us <= 1000 || st_fim_fechado) {
                if (servo_pulse_us < 1000) servo_pulse_us = 1000;
                cortina_em_movimento = false;
                cortina_estado_fisico = false;
            }
        }
        pwm_set_gpio_level(SERVO_PIN, servo_pulse_us);
    } 
    // LÓGICA DE DEBOUNCE (Tolerância)
    else {
        if (alvo_aberta != cortina_estado_fisico) {
            timer_10s_counter++;
            if (timer_10s_counter >= 100) { 
                cortina_em_movimento = true;
                timer_10s_counter = 0;
            }
        } else {
            timer_10s_counter = 0; 
        }
    }
    return true;
}

// INICIALIZAÇÃO DE HARDWARE AP3
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
    // Inicia o botão de segurança AP3 com Interrupção e Pull-Up
    gpio_init(BOTAO_MANUAL_PIN);
    gpio_set_dir(BOTAO_MANUAL_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_MANUAL_PIN); 
    gpio_set_irq_enabled_with_callback(BOTAO_MANUAL_PIN, GPIO_IRQ_EDGE_FALL, true, &botao_isr);

    // Loop para iniciar GPIOs 2 a 8 (Agora são 7 chaves físicas em vez de 8)
    for (int i = 2; i <= 8; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_pull_up(i);
    }
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
    
    pwm_atuadores_init();
    hardware_entradas_init(); // Inicializa as 7 novas chaves e a interrupção
    
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
        // LEITURA DOS SENSORES ANALÓGICOS
        uint16_t ldr_raw = adc_ler_canal(ADC_CHANNEL_LDR);
        uint8_t lum_pct = adc_raw_para_luminosidade_pct(ldr_raw); 
        uint16_t lm35_raw = adc_ler_canal(ADC_CHANNEL_TEMP);
        float temp_c = adc_raw_para_temperatura(lm35_raw);
        
        // LEITURA DAS ENTRADAS DIGITAIS (Lógica invertida devido ao Pull-Up)
        bool st_janela    = !gpio_get(PIN_JANELA);   // Entrada 3
        bool st_pir       = !gpio_get(PIN_PIR);      // Entrada 4
        bool st_chuva     = !gpio_get(PIN_CHUVA);    // Entrada 5
        bool st_cinema    = !gpio_get(PIN_CINEMA);   // Entrada 6
        bool st_vent_max  = !gpio_get(PIN_VENT_MAX); // Entrada 7

        // ==========================================
        // CASCATA DE DECISÃO DA CORTINA
        // ==========================================
        // 1º Passo: O que a luz natural manda fazer?
        alvo_aberta = aplicar_histerese(lum_pct, cfg.lim_lum_h, cfg.lim_lum_l, alvo_aberta);
        
        // 2º Passo: ENTRADAS 5 e 6 (Sobrescrita para Fechar)
        if (st_chuva || st_cinema) {
            alvo_aberta = false; // Ignora o LDR e comanda fechar
        }
        
        // 3º Passo: ENTRADA 3 (Segurança da Janela de Vidro)
        // Se a janela estiver aberta e o sistema quiser fechar, o sistema bloqueia o fechamento!
        if (st_janela && !alvo_aberta) {
            alvo_aberta = true; 
        }

        // ==========================================
        // CASCATA DE DECISÃO DO VENTILADOR
        // ==========================================
        uint16_t fan_duty = 0;
        
        if (st_vent_max) {
            // ENTRADA 7: Botão de Ventilação Máxima crava o PWM em 100%
            fan_duty = 1000;
        } 
        else {
            // MODO AUTOMÁTICO DO VENTILADOR (Depende do calor, presença, janela e interrupção)
            // Só liga se a janela estiver fechada e houver alguém na sala!
            if (!st_janela && st_pir && temp_c > 25.0f && !modo_manual) {
                // Empurrão de 35% adicionado para dar torque de partida no motor 12V
                fan_duty = 350 + (uint16_t)((temp_c - 25.0f) * 65.0f);
                if (fan_duty > 1000) fan_duty = 1000;
            }
        }
        pwm_set_gpio_level(FAN_PIN, fan_duty);
        
        // --- Verifica estados para a telemetria ---
        bool fan_ligado = (fan_duty > 0);     
        bool modo_auto = !modo_manual; 
        
        // Envia telemetria completa
        uart_enviar_telemetria(temp_c, lum_pct, cortina_estado_fisico, modo_auto, fan_ligado);
        
        sleep_ms(50);
        uart_ler_loopback_e_imprimir();
        sleep_ms(950);
    }
    return 0;
}
