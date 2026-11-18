# Guia de Configuração do Stepper-Ninja

As novas configurações de placas e o protótipo de I/O I²C estão no [branch test](https://github.com/atrex66/stepper-ninja/tree/test). A validação em hardware físico ainda está pendente.

Este guia explica as principais opções `#define` em `config.h` e onde elas são usadas.

O Stepper-Ninja mantém cabeçalhos de configuração correspondentes em:

- `firmware/inc/config.h`
- `hal-driver/config.h`

No uso normal, `hal-driver/config.h` deve ser um symlink para `firmware/inc/config.h`, e não uma cópia mantida separadamente.

Se os symlinks estiverem faltando, recrie-os a partir do diretório `hal-driver` com:

```bash
./make_symlinks.sh
```

Esse script recria os symlinks compartilhados usados pelo driver HAL.

---

## Qual arquivo deve ser editado?

- Edite `firmware/inc/config.h` quando quiser mudar o comportamento do firmware do Pico.
- `hal-driver/config.h` deve apontar para esse arquivo como symlink.
- Se `hal-driver/config.h` for um arquivo normal em vez de symlink, execute `hal-driver/make_symlinks.sh`.

Observação: alguns valores são substituídos automaticamente em `footer.h` quando `breakout_board` não é `0`.

---

## Padrões de rede

Esses valores definem a configuração de rede inicial gravada na flash, até que seja alterada pelo terminal serial.

| Define | Significado |
|---|---|
| `DEFAULT_MAC` | Endereço MAC Ethernet padrão |
| `DEFAULT_IP` | Endereço IP estático padrão |
| `DEFAULT_PORT` | Porta UDP usada pelo driver HAL |
| `DEFAULT_GATEWAY` | Gateway padrão |
| `DEFAULT_SUBNET` | Máscara de sub-rede padrão |
| `DEFAULT_TIMEOUT` | Timeout de comunicação em microssegundos |

Se você usar os comandos `ipconfig` no terminal serial depois, os valores em tempo de execução poderão sobrescrever os padrões armazenados na flash.

---

## Seleção da placa

`breakout_board` seleciona o layout de hardware.

| Valor | Significado |
|---|---|
| `0` | Mapeamento de pinos customizado a partir de `config.h` |
| `1` | Breakout board Stepper-Ninja |
| `2` | Breakout board IO-Ninja |
| `3` | Breakout board Analog-Ninja |
| `100` | BreakoutBoard100 |

Quando `breakout_board` é maior que `0`, `footer.h` substitui várias definições de pinos customizados por valores específicos da placa.

### Outras configurações de placas

- [Expatria PicoBOB Deluxe A3](../configurations/PicoBOB-DLX/picoBOB-DLX.h): atribuições corrigidas de RESET/INT do W5500, LED e PWM.
- [Phil Barrett RP23CNC V1.0/V2](../configurations/RP23CNC/README.md) (inglês): cinco eixos, 14 entradas, 12 saídas, um PWM de spindle e WIZ850io/W5500 opcional. Use `-DBOARD=rp23cnc -DWIZCHIP_TYPE=W5500`; `BOARD=pico2` seleciona outra variante do MCU, com 30 GPIOs.

Esses modelos usam `breakout_board=0`, sem novos IDs numéricos. Copie o modelo escolhido para `firmware/inc/config.h` e recompile firmware e HAL com a mesma configuração. As atribuições de pinos e o uso opcional de `GP_NULL` para LED/reset são específicos da placa.

---

## Canais de movimento

Esses defines definem quantos canais existem e quais pinos eles usam.

| Define | Significado |
|---|---|
| `stepgens` | Número de canais de stepgen |
| `stepgen_steps` | Pinos de saída de step |
| `stepgen_dirs` | Pinos de direção |
| `step_invert` | Inversão do sinal de step por canal |
| `encoders` | Número de canais de encoder |
| `enc_pins` | Primeiro pino de cada par de encoder em quadratura |
| `enc_index_pins` | Pinos de índice dos encoders |
| `enc_index_active_level` | Nível ativo de cada entrada de índice |

Para cada encoder, `enc_pins` usa dois GPIOs: `A` no pino configurado e `B` no próximo pino.

---

## Modo do encoder

O firmware suporta duas implementações PIO para encoder:

| Define | Significado |
|---|---|
| `ENCODER_PIO_LEGACY` | PIO original de contador em quadratura |
| `ENCODER_PIO_SUBSTEP` | PIO mais novo com substep |
| `encoder_pio_version` | Seleciona qual PIO de encoder será compilado |

Configuração típica:

```c
#define encoder_pio_version ENCODER_PIO_SUBSTEP
```

Para voltar ao encoder PIO antigo:

```c
#define encoder_pio_version ENCODER_PIO_LEGACY
```

Comportamento atual:

- A estimativa de velocidade do encoder é feita no driver HAL.
- No modo legacy, o firmware envia o delta da contagem do encoder a cada ciclo.
- No modo substep, o firmware continua enviando a contagem bruta do encoder, mas a velocidade é produzida no lado HAL.

Você também pode sobrescrever o modo pela linha de comando do build:

```bash
CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY' cmake -S firmware -B build -DWIZCHIP_TYPE=W5500
```

---

## Entradas e saídas digitais

| Define | Significado |
|---|---|
| `in_pins` | Lista de GPIOs de entrada |
| `in_pullup` | Habilita pull-up por entrada |
| `out_pins` | Lista de GPIOs de saída |

Esses valores só são usados no modo customizado ou quando a breakout board selecionada não os sobrescreve.

---

## I/O digital I²C configurável (protótipo)

I/O digital opcional para configurações personalizadas do **LinuxCNC** (`breakout_board=0`). Tipos: `SN_IO_MCP23008`, `SN_IO_MCP23017`, `SN_IO_TCA9535`, `SN_IO_TCA9555`. As placas breakout fixas mantêm sua implementação e o significado de `io_expanders`. Este protótipo não oferece suporte ao Mach3.

| Define | Significado |
|---|---|
| `I2C_IO_ENABLE` | Ativa o módulo; padrão `0` |
| `I2C_IO_BUS_COUNT` | Quantidade de barramentos, 1–2 quando ativo; padrão `0` |
| `I2C_IO_BUSES` | Lista de descritores de barramento; padrão `{}` |
| `I2C_IO_DEVICE_COUNT` | Total de dispositivos, 1–2 quando ativo; padrão `0` |
| `I2C_IO_DEVICES` | Lista de descritores de dispositivo; padrão `{}` |

Insira as opções **antes** de `#include "footer.h"`. O footer fornece os padrões ausentes com `#ifndef`: configurações existentes não precisam de uma nova seção nem inicializam barramentos adicionais. Recompile firmware e HAL com opções idênticas.

Exemplo: um TCA9555, bits 0–7 como entradas, 8–15 como saídas, leitura a cada 10 ms:

```c
#define I2C_IO_ENABLE 1
#define I2C_IO_BUS_COUNT 1
#define I2C_IO_BUSES {{0, GP00, GP01, 400000, 1000}}
#define I2C_IO_DEVICE_COUNT 1
#define I2C_IO_DEVICES { \
    {SN_IO_TCA9555, 0, 0x20, 0x00ff, 0, 0, 0, 0, 10} \
}
```

Campos do barramento: controlador (0/1), GPIO SDA, GPIO SCL, frequência em Hz (até 400000), timeout por transferência do SDK em µs (1–10000). Campos do dispositivo: tipo, índice na lista de barramentos, endereço de 7 bits (0x20–0x27), máscara de direção (`1=entrada`), pull-up, inversão de entradas, inversão de saídas, níveis lógicos de saída na inicialização/perda de conexão, intervalo em ms (1–60000).

GPIO0/1 no exemplo devem estar livres, sem conflito com STEP/DIR, encoder/índice, I/O nativo, PWM, teclado, LED ou comunicação. Adapte o [exemplo completo de I/O](../configurations/I2C-IO/config.h) à placa.

- Até dois dispositivos e 32 saídas digitais nativas. Cada expansor reserva 16 bits, inclusive MCP23008. Os dois bancos de entradas GPIO nativas são preservados; o formato dos pacotes não muda.
- Máscaras de 8 bits para MCP23008 e 16 bits para os demais. O novo caminho rejeita os bits 7/15 como entradas no MCP23017 (GPA7/GPB7 são apenas saídas no datasheet atual); as placas existentes não mudam.
- Máscara de pull-up zero nos TCA. TCA9535 não tem pull-ups internos de I/O; os pull-ups fixos de entrada do TCA9555 não podem ser desativados por esse campo.
- Nomes HAL: `stepgen-ninja.0.expander.0.input.0`, `input.0-not`, `output.8`, `online`, `valid`. Apenas a direção configurada é exportada: entradas físicas/status HAL_OUT; comandos de saída HAL_IN.
- As transferências ficam fora das IRQs de step/encoder. Uma falha limpa `online`/`valid`; as entradas mantêm a última amostra bem-sucedida. Um barramento com falha não garante a aplicação das saídas de perda de conexão.

[Detalhes, limites e testes](I2C-IO.en.md) (inglês). Os testes físicos de I²C ainda estão pendentes.

---

## Opções de PWM

| Define | Significado |
|---|---|
| `use_pwm` | Habilita suporte a PWM |
| `pwm_count` | Número de saídas PWM |
| `pwm_pin` | Pinos de saída PWM |
| `pwm_invert` | Inverte o PWM por canal |
| `default_pwm_frequency` | Frequência PWM padrão |
| `default_pwm_maxscale` | Limite de escala do HAL |
| `default_pwm_min_limit` | Limite mínimo da saída PWM |

Defina `use_pwm` como `1` somente quando a contagem de canais e os pinos PWM forem válidos para sua placa.

---

## Modo SPI com Raspberry Pi

| Define | Significado |
|---|---|
| `raspberry_pi_spi` | Usa link SPI com Raspberry Pi em vez de Ethernet Wiznet |
| `RASPI_INT_OUT_GPIO` | Pino de interrupção/status para o Raspberry Pi |
| `RASPI_SPI_CS_GPIO` | GPIO chip-select SPI para o Raspberry Pi |
| `raspi_inputs` | Entradas visíveis ao Raspberry Pi |
| `raspi_input_pullups` | Configuração de pull-up dessas entradas |
| `raspi_outputs` | Saídas controladas pelo Raspberry Pi |

Quando `raspberry_pi_spi` é `0`, o firmware usa o caminho Ethernet da Wiznet.

---

## Padrões de temporização

| Define | Significado |
|---|---|
| `default_pulse_width` | Largura padrão do pulso de step em nanossegundos |
| `default_step_scale` | Steps por unidade padrão |
| `use_timer_interrupt` | Habilita o ring buffer de comandos de step e a saída de step guiada por timer |

`use_timer_interrupt` pode reduzir o jitter visível entre PC e Pico ao armazenar comandos de step em buffer.

---

## Defines controlados por `footer.h`

Alguns defines importantes ficam em `footer.h` porque dependem da placa ou da plataforma.

| Define | Significado |
|---|---|
| `use_stepcounter` | Usa step counter em vez do caminho de encoder em quadratura |
| `debug_mode` | Comportamento extra de debug para comunicação com Raspberry Pi |
| `max_statemachines` | Quantidade total derivada de state machines PIO |
| `pico_clock` | Clock de sistema do Pico usado pelo firmware |

Não altere esses valores sem entender o impacto sobre temporização e alocação de PIO.

---

## Opções de build fora de `config.h`

Algumas opções úteis são escolhidas no CMake, não em `config.h`.

| Opção | Significado |
|---|---|
| `-DWIZCHIP_TYPE=W5100S` | Build para W5100S |
| `-DWIZCHIP_TYPE=W5500` | Build para W5500 |
| `-DBOARD=pico` | Build para Pico |
| `-DBOARD=pico2` | Build para Pico2 |
| `-DBOARD=rp23cnc` | Compilar para RP23CNC com RP2350B de 48 GPIOs (modelo correspondente e W5500) |
| `-DSTEPPER_NINJA_PROFILE=linuxcnc` / `mach3-3axis` | Perfil do host: `linuxcnc` (padrão) ou `mach3-3axis` (pinagem fixa) |
| `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` | Copia o firmware para SRAM antes da execução |

Exemplo:

```bash
cmake -S firmware -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_RUN_FROM_RAM=ON
```

O protótipo Mach3 utiliza seu próprio perfil fixo de três eixos. Veja [Mach3](MACH3.en.md) e as [instruções de compilação](MACH3-BUILD.en.md) (inglês). O firmware Pico também pode ser compilado no Windows.

---

## Fluxo de trabalho recomendado

1. Selecione a placa com `breakout_board`.
2. Defina `stepgens`, `encoders` e as listas de pinos se estiver usando modo customizado.
3. Escolha `encoder_pio_version`.
4. Habilite PWM apenas se seu hardware realmente usar.
5. Mantenha os arquivos de configuração do firmware e do driver HAL alinhados.
6. Recompile o firmware e o driver HAL após cada mudança relevante de configuração.

Para configurações de rede em tempo de execução, veja também [IPCONFIG.pt-BR.md](IPCONFIG.pt-BR.md).
