# Instruções de Instalação e Compilação do Stepper-Ninja

Este guia explica como compilar o **Stepper-Ninja**, um driver HAL do LinuxCNC para o Raspberry Pi Pico, usando **CMake** com o gerador Unix Makefiles (`make`). O projeto suporta W5100S-EVB-Pico e o Pico padrão com módulos Ethernet W5500, atingindo geração de passos a 255 kHz e contagem de encoder a 12,5 MHz.

Testado com Pico SDK 2.1.1, CMake 3.20.6 e GNU ARM Embedded Toolchain. Consulte [Solução de Problemas](#solução-de-problemas) para os erros mais comuns.

---

## Pré-requisitos

Antes de compilar, instale as seguintes dependências:

1. **CMake** (versão 3.15 ou superior):

   ```bash
   sudo apt install cmake  # Debian/Ubuntu
   ```

2. **GNU ARM Embedded Toolchain**:

   ```bash
   sudo apt update
   sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi unzip
   ```

3. **Pico SDK (2.1.1)**:

   ```bash
   git clone https://github.com/raspberrypi/pico-sdk
   cd pico-sdk
   git submodule update --init
   export PICO_SDK_PATH=/caminho/para/pico-sdk
   ```

   Adicione `PICO_SDK_PATH` ao seu perfil de shell (por exemplo, `~/.bashrc`).

4. **Ferramentas de Build**:

   - Linux: certifique-se de que o `make` está instalado (`sudo apt install build-essential`).

5. **Instalação do PICOTOOL (opcional)**:

   ```bash
   cd ~
   git clone https://raspberrypi/picotool
   cd picotool
   mkdir build && cd build
   cmake ..
   make
   sudo make install
   ```

---

## Clonando o Repositório

Clone o repositório do Stepper-Ninja:

```bash
git clone https://github.com/atrex66/stepper-ninja
cd stepper-ninja
```

---

## Compilando com CMake e Make

Siga estes passos para compilar o Stepper-Ninja usando CMake com Unix Makefiles.

### 1. Crie o Diretório de Build

```bash
cd firmware/
mkdir build && cd build
```

### 2. Configure o CMake

Execute o CMake para gerar os Makefiles, especificando o tipo de chip WIZnet (`W5100S` ou `W5500`). O padrão é `W5100S` se não for especificado.

- Para **W5100S-EVB-Pico** (padrão):

  ```bash
  cmake ..
  ```

- Para **W5100S-EVB-Pico2** (padrão):

  ```bash
  cmake -DBOARD=pico2 ..
  ```

- Para **W5500 + pico**:

  ```bash
  cmake -DWIZCHIP_TYPE=W5500 ..
  ```

- Para **W5500 + pico2**:

  ```bash
  cmake -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 ..
  ```

### 3. Compile o Projeto

Compile usando `make`:

```bash
make
```

Isso gera `stepper-ninja-picoX-W5XXX.uf2` (para gravar no Pico; o X depende das definições do cmake).

### 4. Grave o Pico

- Conecte o Pico no modo BOOTSEL (segure BOOTSEL e conecte o USB).
- Se o Pico já tiver outro firmware, use o flash_nuke para apagar os dados anteriores da memória flash.
- Copie `stepper-ninja-picoX-W5XXX.uf2` para o dispositivo de armazenamento em massa do Pico.
- O Pico reinicia e executa o firmware.

---

## Suporte ao W5500

Para um Pico padrão com módulo W5500, certifique-se de que:

- O W5500 está corretamente cabeado (pinos SPI, alimentação 3,3V).
- Use `-DWIZCHIP_TYPE=W5500` na etapa do CMake.

## Suporte ao Pico2

Para um Pico2 com módulo W5500, certifique-se de que:

- O W5500 está corretamente cabeado (pinos SPI, alimentação 3,3V).
- Use `-DBOARD=pico2 -DWIZCHIP_TYPE=W5500` na etapa do CMake.

---

## Instalando o Driver HAL do LinuxCNC

Para integrar o Stepper-Ninja ao LinuxCNC:

1. **Instale o driver HAL**:

   ```bash
   cd hal-driver
   ./install.sh
   ```

2. **Crie um arquivo HAL** (ex.: `stepper-ninja.hal`):

   ```hal
   loadrt stepgen-ninja ip_address="192.168.0.177:8888"

   addf stepgen-ninja.0.watchdog-process servo-thread
   addf stepgen-ninja.0.process-send servo-thread
   addf stepgen-ninja.0.process-recv servo-thread

   net x-pos-cmd joint.0.motor-pos-cmd => stepgen-ninja.0.stepgen.0.command
   net x-pos-fb stepgen-ninja.0.stepgen.0.feedback => joint.0.motor-pos-fb
   net x-enable axis.0.amp-enable-out => stepgen-ninja.0.stepgen.0.enable
   ```

3. **Atualize o arquivo INI** (ex.: `your_config.ini`):

   ```ini
   [HAL]
   HALFILE = stepper-ninja.hal

   [EMC]
   SERVO_PERIOD = 1000000
   ```

4. **Execute o LinuxCNC**:

   ```bash
   linuxcnc your_config.ini
   ```

---

## Solução de Problemas

- **CMake Error: PICO_SDK_PATH not found**:
  Certifique-se de que `PICO_SDK_PATH` está definido e aponta para o diretório do Pico SDK.

  ```bash
  export PICO_SDK_PATH=/caminho/para/pico-sdk
  ```

- **pioasm/elf2uf2 ausentes**:
  Compile essas ferramentas no Pico SDK:

  ```bash
  cd pico-sdk/tools/pioasm
  mkdir build && cd build
  cmake .. && make
  ```

- **Erros de UTF-8 BOM** (ex.: `∩╗┐` na saída do linker):
  Use CMake 3.20.6 ou adicione `-DCMAKE_UTF8_BOM=OFF`:

  ```bash
  cmake -DCMAKE_UTF8_BOM=OFF ..
  ```

- **Erros no driver HAL**:
  Verifique problemas de conexão UDP com `dmesg`:

  ```bash
  dmesg | grep stepgen-ninja
  ```

Para mais ajuda, compartilhe os logs de erro na [página de Issues do GitHub](https://github.com/atrex66/stepper-ninja/issues) ou na [thread do Reddit](https://www.reddit.com/r/hobbycnc/comments/1koomzu/).

---

## Notas da Comunidade

Obrigado à comunidade r/hobbycnc (4,7 mil visualizações!) pelos testes, especialmente ao usuário que compilou com CMake e um módulo Pico+W5500! O Stepper-Ninja v1.0 agora está marcado como versão estável: <https://github.com/atrex66/stepper-ninja/releases/tag/v1.0>

Para builds Ninja ou outras configurações, consulte o [README](README.md).
