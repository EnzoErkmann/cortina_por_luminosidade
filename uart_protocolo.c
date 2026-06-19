/* Módulo: UART Protocolo (MÓDULO EXTERNO via UART1 + AP3) */
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// =========================================================================
// UART1 (GPIO 8/9 - default), dedicada ao módulo externo.
// UART0 (GPIO 0/1) continua livre para o stdio/debug via adaptador.
// =========================================================================
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 8
#define UART_RX_PIN 9
#define UART_BUF_SIZE 128

void uart_protocolo_init(void) {
    // Inicializa a porta UART1 física na placa (Canal dedicado ao módulo externo)
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

// recebe modo_auto e fan_ligado
void uart_enviar_telemetria(float temp, uint8_t lum_pct, bool cortina_aberta, bool modo_auto, bool fan_ligado) {
    char tx_buffer[UART_BUF_SIZE];
    // Monta a string em texto na memória com as novas informações
    snprintf(tx_buffer, sizeof(tx_buffer), "TEMP:%.1f,LUM:%d,CORTINA:%s,MODO:%s,VENT:%s\r\n",
             temp, 
             lum_pct, 
             cortina_aberta ? "ABERTA" : "FECHADA",
             modo_auto ? "AUTO" : "MANUAL",
             fan_ligado ? "ON" : "OFF");
    // Envia os dados fisicamente pelo pino GPIO8 (TX) para o módulo externo
    uart_puts(UART_ID, tx_buffer);
    // Mostra no monitor de debug (stdio/UART0) o que foi enviado pela UART1
    printf("Enviado -> %s\r", tx_buffer);
    fflush(stdout);
}

void uart_ler_loopback_e_imprimir(void) {
    char rx_buffer[UART_BUF_SIZE];
    int i = 0;
    // Escuta o pino GPIO9 (RX) para ver se chegou algo do módulo externo
    while (uart_is_readable(UART_ID) && i < (int)(sizeof(rx_buffer) - 1)) {
        rx_buffer[i++] = uart_getc(UART_ID);
    }
    rx_buffer[i] = '\0';
    // Se ouviu algo, usa o printf (stdio/UART0) para mandar pro monitor de debug
    if (i > 0) {
        printf("Loopback Recebido -> %s\r\n\n", rx_buffer);
        fflush(stdout);
    }
}
