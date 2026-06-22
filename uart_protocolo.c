/* Módulo: UART Protocolo
 * Telemetria serial via UART1 (MAX3232) com confirmação por loopback.
 * O TX envia um pacote delimitado por '\n'; o RX lê o eco de volta para
 * garantir integridade da linha. Baud: 115200, 8N1.
 */
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define UART_ID       uart1
#define BAUD_RATE     115200
#define UART_TX_PIN   8
#define UART_RX_PIN   9
#define UART_BUF_SIZE 128

void uart_protocolo_init(void) {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

void uart_enviar_telemetria(float temp, uint8_t lum_pct, bool cortina_aberta, bool modo_auto, bool fan_ligado) {
    char tx_buffer[UART_BUF_SIZE];
    snprintf(tx_buffer, sizeof(tx_buffer), "TEMP:%.1f,LUM:%d,CORTINA:%s,MODO:%s,VENT:%s",
             temp, lum_pct,
             cortina_aberta ? "ABERTA" : "FECHADA",
             modo_auto      ? "AUTO"   : "MANUAL",
             fan_ligado     ? "ON"     : "OFF");

    // Drena resíduos do ciclo anterior antes de transmitir
    while (uart_is_readable(UART_ID))
        (void)uart_getc(UART_ID);

    // Envia o pacote com delimitador de fim de mensagem
    char pacote[UART_BUF_SIZE];
    snprintf(pacote, sizeof(pacote), "%s\n", tx_buffer);
    uart_puts(UART_ID, pacote);

    // Aguarda o eco do loopback lendo byte a byte até '\n' ou timeout de 200 ms
    char rx_buffer[UART_BUF_SIZE];
    int i = 0;
    absolute_time_t limite = make_timeout_time_ms(200);
    while (i < (int)(sizeof(rx_buffer) - 1)) {
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            rx_buffer[i++] = c;
            if (c == '\n') break;
        } else if (absolute_time_diff_us(get_absolute_time(), limite) <= 0) {
            break;
        }
    }
    rx_buffer[i] = '\0';

    // Imprime TX e RX juntos para evitar intercalação com outros logs
    char log_final[UART_BUF_SIZE * 2 + 32];
    if (i > 0)
        snprintf(log_final, sizeof(log_final), "[TX] %s\r\n[RX] %s\r\n", tx_buffer, rx_buffer);
    else
        snprintf(log_final, sizeof(log_final), "[TX] %s\r\n[RX] (nada recebido)\r\n", tx_buffer);
    uart_puts(UART_ID, log_final);

    // Aguarda e descarta o eco do próprio log_final para não poluir o próximo ciclo
    sleep_ms(30);
    while (uart_is_readable(UART_ID))
        (void)uart_getc(UART_ID);
}

/* Mantida para compatibilidade com main.c. A leitura do loopback já ocorre
 * dentro de uart_enviar_telemetria(), evitando leitura concorrente do buffer. */
void uart_ler_loopback_e_imprimir(void) {}
