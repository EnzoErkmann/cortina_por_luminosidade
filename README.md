# AP3 Projeto 19 — Automação de Cortina por Luminosidade
### Raspberry Pi Pico 2 W (RP2350-RISC-V)

---

## 1. Descrição

Sistema embarcado de automação residencial que controla uma cortina (servo PWM) e um ventilador (PWM proporcional) a partir de sensores de luminosidade (LDR) e temperatura (LM35). Inclui sete entradas digitais para lógica avançada, interrupção externa de trava de manutenção e telemetria serial via UART1 com loopback (MAX3232).

**Funcionalidades principais:**

- **Cortina automática** — servo com rampa suave guiado por luminosidade e histerese configurável.
- **Ventilador inteligente** — velocidade proporcional à temperatura, condicionada à presença (PIR) e estado da janela.
- **Cascata de decisão** — lógica de prioridade: luminosidade → chuva/cinema → segurança da janela.
- **Trava de manutenção** — interrupção externa (GPIO 15) congela toda ação autônoma imediatamente.
- **Proteção mecânica** — fins de curso (GPIO 2/3) param o servo diretamente no timer de 100 ms.
- **Telemetria UART** — pacote enviado a cada ciclo com temperatura, luminosidade, estado da cortina, modo e ventilador; confirmado por loopback via MAX3232.

---

## 2. Mapa de Pinos

### Sensores e Atuadores

| GPIO | Componente | Tipo |
|------|------------|------|
| 26   | LDR (ADC0) | Entrada analógica |
| 27   | LM35 (ADC1) | Entrada analógica |
| 18   | Servo Motor | PWM saída — pulso 1 000–2 000 µs |
| 16   | Ventilador | PWM saída — duty 0–1 000 |

### Entradas Digitais (pull-up interno, lógica invertida)

| GPIO | Identificador   | Componente              | Ação |
|------|-----------------|-------------------------|------|
| 2    | PIN_FIM_ABERTO  | Fim de curso — aberto   | Para o servo ao atingir 100% aberto |
| 3    | PIN_FIM_FECHADO | Fim de curso — fechado  | Para o servo ao atingir 100% fechado |
| 4    | PIN_JANELA      | Sensor magnético        | Bloqueia fechamento se janela aberta |
| 5    | PIN_PIR         | Sensor de presença PIR  | Habilita ventilador automático |
| 6    | PIN_CHUVA       | Sensor de chuva         | Força fechamento da cortina |
| 7    | PIN_CINEMA      | Chave modo cinema       | Força fechamento da cortina |
| 14   | PIN_VENT_MAX    | Botão ventilação máxima | Crava PWM do ventilador em 100% |
| 15   | BOTAO_MANUAL    | Botão de manutenção     | IRQ borda de descida — alterna manual/auto |

### Comunicação Serial (UART1)

| GPIO | Função | Descrição |
|------|--------|-----------|
| 8    | TX     | Transmissão para módulo MAX3232 |
| 9    | RX     | Recepção do loopback MAX3232 |

> **Atenção:** GPIOs 8 e 9 são **exclusivos da UART1**. Não conectar outros periféricos nesses pinos. Parâmetros: 115 200 bps · 8N1.

---

## 3. Como Compilar e Gravar

**Pré-requisitos:** Pico SDK 2.2.0 · CMake ≥ 3.13 · Toolchain `RISCV_ZCB_RPI_2_2_0_3`

```bash
mkdir build && cd build
cmake .. -DPICO_BOARD=pico2_w -DPICO_PLATFORM=rp2350-riscv
make -j$(nproc)
```

Arquivo gerado: `build/cortina_por_luminosidade.uf2`

**Gravação — método 1 (arrastar e soltar):** segure BOOTSEL, conecte o Pico ao USB, copie o `.uf2` para a unidade `RPI-RP2` que aparecer. O Pico reinicia automaticamente.

**Gravação — método 2 (picotool):**
```bash
picotool load build/cortina_por_luminosidade.uf2 --force && picotool reboot
```

---

## 4. Como Operar

Após energizar, o sistema aguarda 2 s e exibe no terminal serial:
```
=== AP3 Projeto 19 - AUTOMAÇÃO AVANÇADA (7 ENTRADAS + INTERRUPÇÃO) ===
```

### Modo Automático (padrão)

A cortina segue a cascata de decisão abaixo. O ventilador liga proporcionalmente à temperatura (`PWM = 350 + (T−25)×65`, máx. 1 000) quando há presença, janela fechada e modo automático ativo.

| Prioridade | Condição | Ação na cortina |
|------------|----------|-----------------|
| 1ª | Chuva ou modo cinema ativos | Fecha forçado |
| 2ª | Janela aberta | Bloqueia fechamento |
| 3ª | Luminosidade > 70% | Abre |
| 3ª | Luminosidade < 30% | Fecha |
| —  | Entre 30–70% | Mantém estado (histerese) |

### Controles Manuais

- **GPIO 14 (Vent. máxima):** crava o ventilador em 100%, sobrescrevendo qualquer outra lógica.
- **GPIO 15 (Manutenção):** cada pressionamento alterna entre modo manual (cortina e ventilador parados) e automático. A telemetria continua reportando `MODO:MANUAL`.

### Telemetria Serial

A cada ~1 s um pacote é enviado e o eco confirmado pelo loopback:
```
[TX] TEMP:28.5,LUM:65,CORTINA:ABERTA,MODO:AUTO,VENT:ON
[RX] TEMP:28.5,LUM:65,CORTINA:ABERTA,MODO:AUTO,VENT:ON
```
Se o loopback falhar, aparece `[RX] (nada recebido)` — verificar cabeamento do MAX3232.

---

## 5. Parâmetros Padrão

| Parâmetro | Valor | Descrição |
|-----------|-------|-----------|
| `LIM_LUM_H` | 70% | Luminosidade acima → abre cortina |
| `LIM_LUM_L` | 30% | Luminosidade abaixo → fecha cortina |
| `LIM_TEMP`  | 35 °C | Limite de referência configurável |
| Debounce da cortina | 10 s | Espera antes de iniciar movimento |
| Timeout loopback | 200 ms | Tempo máximo aguardando eco UART |
