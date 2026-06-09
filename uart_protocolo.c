/* Módulo: UART Protocolo (VERSÃO TESTE USB) */
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define UART_BUF_SIZE 64

typedef struct {
    uint8_t lim_lum_h;
    uint8_t lim_lum_l;
    uint8_t lim_temp;
    uint8_t saidas;
} ConfigSistema;

static char rx_buffer[UART_BUF_SIZE];
static uint8_t rx_idx = 0;

void uart_protocolo_init(void) {
    // Vazio no modo USB, pois a inicialização foi feita pelo stdio_init_all()
}

void uart_enviar_telemetria(float temp, uint8_t lum_pct, bool cortina_aberta) {
    // Envia direto pro cabo USB
    printf("TEMP:%.1f,LUM:%d,CORTINA:%s\n", temp, lum_pct, cortina_aberta ? "ABERTA" : "FECHADA");
}

bool uart_processar_recepcao(ConfigSistema *cfg) {
    bool alterado = false;
    
    // Lê caracteres do cabo USB (não trava o código se não tiver nada)
    int c = getchar_timeout_us(0);
    
    while (c != PICO_ERROR_TIMEOUT) {
        if (c == '\r') {
            c = getchar_timeout_us(0);
            continue;
        }
        
        if (c == '\n') {
            rx_buffer[rx_idx] = '\0'; 
            int valor;
            
            if (sscanf(rx_buffer, "LIM_LUM_H:%d", &valor) == 1) {
                cfg->lim_lum_h = (uint8_t)valor;
                printf("OK:LIM_LUM_H=%d\n", valor);
                alterado = true;
            } 
            else if (sscanf(rx_buffer, "LIM_LUM_L:%d", &valor) == 1) {
                cfg->lim_lum_l = (uint8_t)valor;
                printf("OK:LIM_LUM_L=%d\n", valor);
                alterado = true;
            } 
            else if (sscanf(rx_buffer, "LIM_TEMP:%d", &valor) == 1) {
                cfg->lim_temp = (uint8_t)valor;
                printf("OK:LIM_TEMP=%d\n", valor);
                alterado = true;
            } 
            else if (strncmp(rx_buffer, "SAIDAS:", 7) == 0) {
                char* bits = rx_buffer + 7;
                uint8_t out_val = 0;
                bool valid = true;
                for (int i = 0; i < 8; i++) {
                    if (bits[i] == '1') out_val |= (1 << (7 - i));
                    else if (bits[i] != '0') { valid = false; break; }
                }
                if (valid) {
                    cfg->saidas = out_val;
                    printf("OK:SAIDAS=%.8s\n", bits);
                    alterado = true;
                } else {
                    printf("ERRO:COMANDO_INVALIDO\n");
                }
            } 
            else if (rx_idx > 0) {
                printf("ERRO:COMANDO_INVALIDO\n");
            }
            rx_idx = 0;
        } 
        else {
            if (rx_idx < UART_BUF_SIZE - 1) rx_buffer[rx_idx++] = (char)c;
            else rx_idx = 0; 
        }
        c = getchar_timeout_us(0); // Lê o próximo caractere
    }
    return alterado;
}