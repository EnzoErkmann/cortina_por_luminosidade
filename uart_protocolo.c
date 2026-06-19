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
// UART0 (GPIO 0/1) continua livre para o stdio interno (printf padrão).
// ALTERADO: agora os logs "Enviado ->" e "Recebido ->" são escritos
// diretamente na própria UART1 (uart_puts), para aparecerem no MESMO
// terminal do adaptador onde os dados de telemetria são vistos.
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
    char log_buffer[UART_BUF_SIZE + 32];

    // Monta a string em texto na memória com as novas informações
    snprintf(tx_buffer, sizeof(tx_buffer), "TEMP:%.1f,LUM:%d,CORTINA:%s,MODO:%s,VENT:%s",
             temp, 
             lum_pct, 
             cortina_aberta ? "ABERTA" : "FECHADA",
             modo_auto ? "AUTO" : "MANUAL",
             fan_ligado ? "ON" : "OFF");

    // Monta a linha de log explícita: "[TX] " na frente dos dados
    snprintf(log_buffer, sizeof(log_buffer), "[TX] %s\r\n", tx_buffer);

    // Envia a linha JÁ com o prefixo, pela própria UART1, pro mesmo terminal do adaptador
    uart_puts(UART_ID, log_buffer);

    // Mantém também o espelho no stdio (UART0/USB) para debug interno, se precisar
    printf("Enviado -> %s\r\n", tx_buffer);
    fflush(stdout);
}

void uart_ler_loopback_e_imprimir(void) {
    char rx_buffer[UART_BUF_SIZE];
    char log_buffer[UART_BUF_SIZE + 32];
    int i = 0;

    // Escuta o pino GPIO9 (RX) para ver se chegou algo do módulo externo
    while (uart_is_readable(UART_ID) && i < (int)(sizeof(rx_buffer) - 1)) {
        rx_buffer[i++] = uart_getc(UART_ID);
    }
    rx_buffer[i] = '\0';

    // Se ouviu algo, escreve de volta na própria UART1 com prefixo "[RX] "
    if (i > 0) {
        snprintf(log_buffer, sizeof(log_buffer), "[RX] %s\r\n", rx_buffer);
        uart_puts(UART_ID, log_buffer);

        // Espelha no stdio também
        printf("Loopback Recebido -> %s\r\n\n", rx_buffer);
        fflush(stdout);
    }
}
