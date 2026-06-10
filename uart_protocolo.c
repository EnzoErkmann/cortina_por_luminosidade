/* Módulo: UART Protocolo (LOOPBACK) */
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define UART_BUF_SIZE 128

void uart_protocolo_init(void) {
    // Inicializa a porta UART física na placa (Canal do Hardware)
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

void uart_enviar_telemetria(float temp, uint8_t lum_pct, bool cortina_aberta) {
    char tx_buffer[UART_BUF_SIZE];
    
    // Monta a string em texto na memória
    snprintf(tx_buffer, sizeof(tx_buffer), "TEMP:%.1f,LUM:%d,CORTINA:%s\r\n", 
             temp, lum_pct, cortina_aberta ? "ABERTA" : "FECHADA");
             
    // Envia os dados fisicamente pelo pino GPIO0 (TX)
    uart_puts(UART_ID, tx_buffer);
}

void uart_ler_loopback_e_imprimir(void) {
    char rx_buffer[UART_BUF_SIZE];
    int i = 0;
    
    // Escuta o pino GPIO1 (RX) para ver se chegou eletricidade do jumper
    while (uart_is_readable(UART_ID) && i < (int)(sizeof(rx_buffer) - 1)) {
        rx_buffer[i++] = uart_getc(UART_ID);
    }
    rx_buffer[i] = '\0'; // Finaliza a string
    
    // Se ouviu algo, usa o printf para mandar pro Monitor via USB
    if (i > 0) {
        printf("Loopback Recebido -> %s", rx_buffer);
    }
}
