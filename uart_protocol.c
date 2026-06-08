/* Módulo: UART Protocolo */
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Configurações da UART
#define UART_ID       uart0
#define BAUD_RATE     115200
#define UART_TX_PIN   0
#define UART_RX_PIN   1
#define UART_BUF_SIZE 64

// Estrutura de configuração
typedef struct {
    uint8_t lim_lum_h;
    uint8_t lim_lum_l;
    uint8_t lim_temp;
    uint8_t saidas;
} ConfigSistema;

static char rx_buffer[UART_BUF_SIZE];
static uint8_t rx_idx = 0;

void uart_protocolo_init(void) {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_fifo_enabled(UART_ID, true);
}

void uart_enviar_telemetria(float temp, uint8_t lum_pct, bool cortina_aberta) {
    char tx_buffer[UART_BUF_SIZE];
    snprintf(tx_buffer, sizeof(tx_buffer), "TEMP:%.1f,LUM:%d,CORTINA:%s\r\n", 
             temp, lum_pct, cortina_aberta ? "ABERTA" : "FECHADA");
    uart_puts(UART_ID, tx_buffer);
}

bool uart_ha_dados_disponiveis(void) {
    return uart_is_readable(UART_ID);
}

bool uart_processar_recepcao(ConfigSistema *cfg) {
    bool alterado = false;
    
    while (uart_is_readable(UART_ID)) {
        char c = uart_getc(UART_ID);
        
        if (c == '\r') continue;
        
        if (c == '\n') {
            rx_buffer[rx_idx] = '\0'; 
            
            int valor;
            char resposta[UART_BUF_SIZE];
            
            if (sscanf(rx_buffer, "LIM_LUM_H:%d", &valor) == 1) {
                cfg->lim_lum_h = (uint8_t)valor;
                snprintf(resposta, sizeof(resposta), "OK:LIM_LUM_H=%d\r\n", valor);
                uart_puts(UART_ID, resposta);
                alterado = true;
            } 
            else if (sscanf(rx_buffer, "LIM_LUM_L:%d", &valor) == 1) {
                cfg->lim_lum_l = (uint8_t)valor;
                snprintf(resposta, sizeof(resposta), "OK:LIM_LUM_L=%d\r\n", valor);
                uart_puts(UART_ID, resposta);
                alterado = true;
            } 
            else if (sscanf(rx_buffer, "LIM_TEMP:%d", &valor) == 1) {
                cfg->lim_temp = (uint8_t)valor;
                snprintf(resposta, sizeof(resposta), "OK:LIM_TEMP=%d\r\n", valor);
                uart_puts(UART_ID, resposta);
                alterado = true;
            } 
            else if (strncmp(rx_buffer, "SAIDAS:", 7) == 0) {
                char* bits = rx_buffer + 7;
                uint8_t out_val = 0;
                bool valid = true;
                
                for (int i = 0; i < 8; i++) {
                    if (bits[i] == '1') {
                        out_val |= (1 << (7 - i));
                    } else if (bits[i] != '0') {
                        valid = false;
                        break;
                    }
                }
                
                if (valid) {
                    cfg->saidas = out_val;
                    snprintf(resposta, sizeof(resposta), "OK:SAIDAS=%.8s\r\n", bits);
                    uart_puts(UART_ID, resposta);
                    alterado = true;
                } else {
                    uart_puts(UART_ID, "ERRO:COMANDO_INVALIDO\r\n");
                }
            } 
            else {
                if (rx_idx > 0) {
                    uart_puts(UART_ID, "ERRO:COMANDO_INVALIDO\r\n");
                }
            }
            rx_idx = 0;
        } 
        else {
            if (rx_idx < UART_BUF_SIZE - 1) {
                rx_buffer[rx_idx++] = c;
            } else {
                rx_idx = 0; 
            }
        }
    }
    return alterado;
}