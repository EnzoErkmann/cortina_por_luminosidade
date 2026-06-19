/* Módulo: UART Protocolo (1 MÓDULO MAX3232 + JUMPER, LOG TX/RX SEM ECO EM CASCATA, LEITURA POR DELIMITADOR) */
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 8
#define UART_RX_PIN 9
#define UART_BUF_SIZE 128

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

    // Limpa qualquer resíduo pendente no buffer ANTES de mandar
    while (uart_is_readable(UART_ID)) {
        (void)uart_getc(UART_ID);
    }

    // 1) Manda o dado puro, terminado em \n (delimitador de fim de mensagem)
    char pacote[UART_BUF_SIZE];
    snprintf(pacote, sizeof(pacote), "%s\n", tx_buffer);
    uart_puts(UART_ID, pacote);

    // 2) Lê o eco ATIVAMENTE até achar o \n ou estourar um timeout de segurança
    char rx_buffer[UART_BUF_SIZE];
    int i = 0;
    absolute_time_t limite = make_timeout_time_ms(200); // timeout de segurança

    while (i < (int)(sizeof(rx_buffer) - 1)) {
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            rx_buffer[i++] = c;
            if (c == '\n') break;  // achou o fim da mensagem, para de ler
        } else {
            if (absolute_time_diff_us(get_absolute_time(), limite) <= 0) {
                break; // timeout, evita travar para sempre se nada voltar
            }
        }
    }
    rx_buffer[i] = '\0';

    // 3) Escreve o log final UMA ÚNICA VEZ, com os dois rótulos juntos
    char log_final[UART_BUF_SIZE * 2 + 32];
    if (i > 0) {
        snprintf(log_final, sizeof(log_final), "[TX] %s\r\n[RX] %s\r\n", tx_buffer, rx_buffer);
    } else {
        snprintf(log_final, sizeof(log_final), "[TX] %s\r\n[RX] (nada recebido)\r\n", tx_buffer);
    }
    uart_puts(UART_ID, log_final);

    // 4) Drena qualquer eco do PRÓPRIO log_final que tenha voltado, sem reagir a ele
    sleep_ms(30);
    while (uart_is_readable(UART_ID)) {
        (void)uart_getc(UART_ID);
    }
}

void uart_ler_loopback_e_imprimir(void) {
    // Função mantida por compatibilidade com main.c, mas agora não faz nada:
    // toda a leitura/log já acontece dentro de uart_enviar_telemetria().
    // Isso evita uma segunda leitura concorrente do mesmo buffer.
}
