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

## Licença

- O programa PIO do encoder em quadratura usa licença BSD-3 da Raspberry Pi (Trading) Ltd.
- O `ioLibrary_Driver` é licenciado sob a MIT License da Wiznet.