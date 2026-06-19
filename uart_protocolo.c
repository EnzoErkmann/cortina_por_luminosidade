/* Módulo: UART Protocolo (MÓDULO EXTERNO via UART1 + AP3 + LOG TX/RX UNIFICADO) */
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 8
#define UART_RX_PIN 9
#define UART_BUF_SIZE 128

static char ultimo_tx[UART_BUF_SIZE] = {0};
static bool aguardando_eco = false;

void uart_protocolo_init(void) {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

void uart_enviar_telemetria(float temp, uint8_t lum_pct, bool cortina_aberta, bool modo_auto, bool fan_ligado) {
    char tx_buffer[UART_BUF_SIZE];

    snprintf(tx_buffer, sizeof(tx_buffer), "TEMP:%.1f,LUM:%d,CORTINA:%s,MODO:%s,VENT:%s",
             temp,
             lum_pct,
             cortina_aberta ? "ABERTA" : "FECHADA",
             modo_auto ? "AUTO" : "MANUAL",
             fan_ligado ? "ON" : "OFF");

    // Guarda o que foi mandado, para sabermos identificar o eco depois
    strncpy(ultimo_tx, tx_buffer, sizeof(ultimo_tx) - 1);
    ultimo_tx[sizeof(ultimo_tx) - 1] = '\0';
    aguardando_eco = true;

    char log_tx[UART_BUF_SIZE + 16];
    snprintf(log_tx, sizeof(log_tx), "[TX] %s\r\n", tx_buffer);
    uart_puts(UART_ID, log_tx);
}

void uart_ler_loopback_e_imprimir(void) {
    char rx_buffer[UART_BUF_SIZE];
    int i = 0;

    while (uart_is_readable(UART_ID) && i < (int)(sizeof(rx_buffer) - 1)) {
        rx_buffer[i++] = uart_getc(UART_ID);
    }
    rx_buffer[i] = '\0';

    if (i > 0 && aguardando_eco) {
        // Isso é o eco do que acabamos de mandar (chegou via jumper TXD-RXD)
        char log_rx[UART_BUF_SIZE + 16];
        snprintf(log_rx, sizeof(log_rx), "[RX] %s\r\n\n", rx_buffer);
        uart_puts(UART_ID, log_rx);   // escreve na MESMA UART1, mas só esta vez

        aguardando_eco = false;       // marca como já tratado, não reage de novo
    }
    // Se aguardando_eco já é false, qualquer coisa lida aqui é IGNORADA
    // (evita reagir ao próprio log [RX] que acabamos de mandar, quebrando o loop)
}
