# stepper-ninja

Uma interface open-source, gratuita e de alto desempenho de step generator, contador de encoder em quadratura, I/O digital e PWM para LinuxCNC.

O módulo de encoder também permite movimentos sincronizados com o spindle, por exemplo roscamento e outros movimentos síncronos ao eixo-árvore.

Você não precisa da breakout board oficial para usar o stepper-ninja. Uma breakout board simples de porta paralela já pode ser suficiente, e outras configurações também são possíveis.

![official breakout board](docs/20250812_165926.jpg)

## Documentação

- [Guia de Instalação](docs/INSTALL.pt-BR.md)
- [Guia de Configuração](docs/CONFIG.pt-BR.md)
- [Guia de Configuração de IP](docs/IPCONFIG.pt-BR.md)
- [Monte Sua Própria Breakout Board](docs/MAKE-YOUR-OWN-BREAKOUTBOARD.pt-BR.md)

## Idiomas

- [English](README.md)
- [Deutsch](README.de.md)
- [हिन्दी](README.hi.md)
- [Magyar](README.hu.md)
- [Português (Brasil)](README.pt-BR.md)

## Recursos

- **Configurações suportadas**:

  - W5100S-evb-pico UDP Ethernet
  - W5500-evb-pico
  - W5100S-evb-pico2
  - W5500-evb-pico2
  - pico + módulo W5500
  - pico2 + módulo W5500
  - pico + Raspberry Pi4 via SPI
  - pico2 + Raspberry Pi4 via SPI
  - pico + PI ZERO2W via SPI
  - pico2 + PI ZERO2W via SPI
  - breakout board oficial do Stepper-Ninja

- **Breakout-board v1.0 - versão digital**: 16 entradas opto-isoladas, 8 saídas opto-isoladas, 4 step generators, 2 entradas de encoder de alta velocidade, 2 saídas DAC unipolares de 12 bits.

- **step-generator**: máximo de 8 com Pico 1, máximo de 12 com Pico2. Até 1 MHz por canal. A largura do pulso é definida por um pino HAL.

- **quadrature-encoder**: máximo de 8 com Pico1, máximo de 12 com Pico2. Alta velocidade, tratamento de índice, estimativa de velocidade e suporte a movimentos sincronizados com o spindle.

- **digital I/O**: os pinos livres do Pico podem ser configurados como entradas e saídas.

- **PWM**: até 16 GPIOs podem ser configurados para saída PWM.

- **Software**: driver HAL do LinuxCNC com múltiplas instâncias e funções de segurança.

- **Open-Source**: código e documentação sob licença MIT.

- **projeto KiCad de breakout board de exemplo**: um projeto KiCad simples e de baixo custo para breakout board DIY está disponível em `configurations/RaspberryPi`. Ele conecta um Pico/Pico2 ao Raspberry Pi via SPI e requer apenas um Pico ou Pico2, um conector GPIO padrão de 40 pinos e bornes de parafuso PCB de 2,54 mm.

## Configuração PIO e Limitações de Recursos

O Raspberry Pi Pico e Pico2 utilizam **blocos PIO (Programmable I/O)** para implementar step generation e contagem de encoder diretamente em hardware. Como a memória de instruções PIO é muito limitada, **step generators e encoders não podem ser usados ao mesmo tempo** — habilitar um desabilita o outro.

### RP2040 (Pico)

- 2 blocos PIO (PIO0 e PIO1), cada um com **4 state machines** e **32 slots de instrução**
- Programa PIO do step generator: ~20 instruções por state machine
- Programa PIO do encoder: ~15–18 instruções por state machine
- **4 step generators** preenchem completamente um bloco PIO — sem espaço restante para encoders no mesmo bloco
- **4 encoders** preenchem completamente um bloco PIO — sem espaço para step generators
- Máximo: **8 step generators** OU **8 encoders** (usando ambos os blocos PIO), nunca misturados

### RP2350 (Pico2)

- 2 blocos PIO, cada um com **4 state machines** e **32 slots de instrução** (mesma estrutura do RP2040)
- O RP2350 possui adicionalmente um **PIO2** com mais 4 state machines
- Máximo: **12 step generators** OU **12 encoders** (usando todos os três blocos PIO)
- Step generators e encoders ainda não podem ser misturados — o programa PIO de cada função preenche o bloco completamente

### Alternando Entre Modos

Edite `firmware/inc/config.h` para escolher o modo:

```c
#define stepgens 4   // número de step generators (0 = modo somente encoder)
#define encoders 0   // número de encoders (0 = modo somente step generator)
```

Recompile o firmware e faça o flash no Pico após alterar essa configuração.

### Variantes de PIO para Encoder

Duas implementações de encoder estão disponíveis:

- **`ENCODER_PIO_SUBSTEP`** (padrão): interpolação sub-step para estimativa de velocidade suave (~18 instruções)
- **`ENCODER_PIO_LEGACY`**: contagem em quadratura simples, um pouco menor (~15 instruções)

Compilar com encoder legado:
```bash
CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY' cmake ..
```

---

## Licença

- O programa PIO do encoder em quadratura usa licença BSD-3 da Raspberry Pi (Trading) Ltd.
- O `ioLibrary_Driver` é licenciado sob a MIT License da Wiznet.