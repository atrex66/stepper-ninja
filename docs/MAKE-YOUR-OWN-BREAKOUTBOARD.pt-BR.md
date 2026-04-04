# Faça Sua Própria Placa de Breakout para o stepper-ninja

Este guia explica como projetar e integrar uma placa de breakout personalizada com o stepper-ninja, desde o planejamento elétrico até a integração de firmware/HAL e a inicialização no LinuxCNC.

## 1. O Que Você Está Construindo

Uma placa personalizada neste projeto tem dois lados de software:

- **Lado do Firmware no Pico/Pico2**:
  - Padrão de arquivo: `firmware/modules/breakoutboard_<ID>.c`
  - Gerencia o hardware físico (GPIO, expansores I2C, DACs, estado de segurança).
  - Mapeia o estado bruto da placa para campos de pacote.

- **Lado do HAL do LinuxCNC**:
  - Padrão de arquivo: `hal-driver/modules/breakoutboard_hal_<ID>.c`
  - Exporta pinos HAL e mapeia campos de pacote para esses pinos.
  - Empacota as saídas HAL de volta nos campos de pacote enviados ao firmware.

Ambos os lados são selecionados usando `breakout_board` em `firmware/inc/config.h` e `hal-driver/config.h`.

## 2. Pré-requisitos

- Ambiente de build do stepper-ninja funcionando.
- Pico SDK instalado (ou já utilizável pelo `firmware/CMakeLists.txt`).
- Headers/ferramentas de desenvolvimento do LinuxCNC instalados para o build do módulo HAL.
- Uma placa existente funcionando para usar como referência:
  - `firmware/modules/breakoutboard_1.c`
  - `hal-driver/modules/breakoutboard_hal_1.c`

## 3. Checklist de Planejamento de Hardware

Antes de codificar, defina estes itens:

- **Trilhos de alimentação e níveis lógicos**:
  - Compatibilidade com lógica 3,3V nos GPIOs do Pico.
  - Conversão de nível/isolamento óptico onde necessário.

- **Comportamento de segurança**:
  - Defina exatamente o que as saídas devem fazer em timeout/desconexão.
  - Garanta que as linhas de habilitação falhem de forma segura (motor desligado, spindle desabilitado, saídas analógicas em zero).

- **Orçamento de I/O**:
  - Quantidade de entradas/saídas digitais.
  - Quantidade de saídas analógicas e tipo de DAC.
  - Canais de encoder e comportamento do índice.

- **Alocação de barramento/periférico**:
  - Barramento/pinos I2C para expansores/DACs.
  - Cabeamento SPI/WIZnet e linhas de reset/interrupção.

- **Integridade de sinal**:
  - Mantenha as trilhas de step/dir limpas e curtas.
  - Adicione pull-ups/pull-downs onde o estado no boot importa.

## 4. Escolha um Novo ID de Placa

Escolha um ID inteiro não utilizado, por exemplo `42`.

Você usará este mesmo ID em:

- `firmware/modules/breakoutboard_42.c`
- `hal-driver/modules/breakoutboard_hal_42.c`
- `#define breakout_board 42` nos cabeçalhos de configuração
- Ramificações de seleção nos caminhos de build do firmware e do HAL

## 5. Integração do Lado do Firmware

### 5.1 Crie o módulo de firmware

1. Copie o template:

```bash
cp firmware/modules/breakoutboard_user.c firmware/modules/breakoutboard_42.c
```

2. Em `firmware/modules/breakoutboard_42.c`, altere:

```c
#if breakout_board == 255
```

para:

```c
#if breakout_board == 42
```

3. Implemente os callbacks obrigatórios:

- `breakout_board_setup()`:
  - Inicialize os dispositivos GPIO/I2C/SPI da sua placa.
  - Detecte e configure expansores e DACs.

- `breakout_board_disconnected_update()`:
  - Force todas as saídas para o estado seguro.
  - Defina saídas do DAC em zero ou polarização segura.

- `breakout_board_connected_update()`:
  - Aplique saídas do pacote recebido ao hardware.
  - Atualize snapshots de entrada do hardware.

- `breakout_board_handle_data()`:
  - Preencha os campos do pacote de saída (`tx_buffer->inputs[]`, etc.).
  - Consuma campos de entrada (`rx_buffer->outputs[]`, cargas analógicas).

### 5.2 Adicione seu módulo ao build do firmware

Edite `firmware/CMakeLists.txt` e adicione seu novo arquivo fonte em `add_executable(...)`:

```cmake
modules/breakoutboard_42.c
```

### 5.3 Defina os macros da placa no rodapé de configuração do firmware

Edite `firmware/inc/footer.h` e adicione um novo bloco:

```c
#if breakout_board == 42
    // seu mapeamento de pinos e macros específicos da placa
    // exemplos:
    // #define I2C_SDA 26
    // #define I2C_SCK 27
    // #define I2C_PORT i2c1
    // #define MCP23017_ADDR 0x20
    // #define ANALOG_CH 2
    // #define MCP4725_BASE 0x60

    // substitua macros de stepgen/encoder/in/out/pwm conforme necessário
#endif
```

Use os blocos existentes para os IDs 1, 2, 3 e 100 como referência de estilo.

### 5.4 Selecione sua placa na configuração do firmware

Edite `firmware/inc/config.h`:

```c
#define breakout_board 42
```

## 6. Integração do Lado do Driver HAL

### 6.1 Crie o módulo de placa HAL

1. Copie o template:

```bash
cp hal-driver/modules/breakoutboard_hal_user.c hal-driver/modules/breakoutboard_hal_42.c
```

2. Em `hal-driver/modules/breakoutboard_hal_42.c`, altere:

```c
#if breakout_board == 255
```

para:

```c
#if breakout_board == 42
```

3. Implemente:

- `bb_hal_setup_pins(...)`:
  - Exporte todos os pinos necessários (`hal_pin_bit_newf`, `hal_pin_float_newf`, etc.).
  - Inicialize os valores padrão.

- `bb_hal_process_recv(...)`:
  - Desempacote `rx_buffer->inputs[...]` e outros campos nos pinos de saída HAL.

- `bb_hal_process_send(...)`:
  - Empacote os pinos de comando HAL em `tx_buffer->outputs[...]` e campos analógicos.

### 6.2 Registre o include da sua placa HAL

Edite `hal-driver/stepper-ninja.c` na seção de include de seleção de placa e adicione:

```c
#elif breakout_board == 42
#include "modules/breakoutboard_hal_42.c"
```

### 6.3 Selecione o ID da placa na configuração HAL

Edite `hal-driver/config.h`:

```c
#define breakout_board 42
```

Mantenha o ID `breakout_board` idêntico no firmware e no HAL.

## 7. Regras de Mapeamento de Pacotes (Importante)

Use um contrato estável entre ambos os lados.

- **Lado do Firmware** escreve dados machine-to-host:
  - `tx_buffer->inputs[0..3]`
  - Campos de feedback/jitter do encoder

- **Lado do HAL** lê esses dados nos pinos HAL em `bb_hal_process_recv()`.

- **Lado do HAL** escreve comandos host-to-firmware:
  - `tx_buffer->outputs[0..1]`
  - Campos analógicos e de controle opcionais

- **Lado do Firmware** lê esses comandos via `rx_buffer` e os aplica em `breakout_board_connected_update()`.

Qualquer incompatibilidade aqui causa pinos trocados ou I/O morto. Mantenha uma única tabela de mapeamento nos comentários durante o desenvolvimento.

## 8. Compilar e Gravar o Firmware

A partir da raiz do repositório:

```bash
cd firmware
cmake -S . -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500
cmake --build build -j"$(nproc)"
ls build/*.uf2
```

Para Pico v1, configure `-DBOARD=pico`.

Gravar:

1. Segure BOOTSEL ao conectar o Pico.
2. Copie o `.uf2` gerado para a unidade RPI-RP2 montada.
3. Reinicie e abra a saída serial para verificar os logs de inicialização.

## 9. Compilar e Instalar o Módulo HAL do LinuxCNC

A partir da raiz do repositório:

```bash
cd hal-driver
cmake -S . -B build-cmake
cmake --build build-cmake --target stepper-ninja
sudo cmake --install build-cmake --component stepper-ninja
```

Isso instala `stepper-ninja.so` no diretório de módulos do LinuxCNC.

## 10. Inicialização do LinuxCNC HAL (Mínima)

Esqueleto de exemplo no seu arquivo HAL:

```hal
loadrt stepper-ninja ip_address="192.168.0.177:8888"
addf stepper-ninja.0.watchdog-process servo-thread
addf stepper-ninja.0.process-send     servo-thread
addf stepper-ninja.0.process-recv     servo-thread

# Redes de I/O de exemplo (os nomes dependem da sua implementação de bb_hal_setup_pins)
# net estop-in      stepper-ninja.0.inputs.0     => some-signal
# net coolant-out   some-command                 => stepper-ninja.0.outputs.0
```

Em seguida, verifique com `halshow`/`halcmd show pin stepper-ninja.0.*`.

## 11. Procedimento de Comissionamento (Recomendado)

Execute exatamente nesta ordem:

1. **Teste somente de alimentação**:
   - Valide os trilhos, sem superaquecimento, sem corrente ociosa excessiva.

2. **Teste de descoberta de barramento**:
   - Confirme que os endereços I2C esperados são detectados nas impressões do firmware.

3. **Teste de segurança na desconexão**:
   - Pare a comunicação do LinuxCNC.
   - Confirme que as saídas e canais analógicos vão para o estado seguro.

4. **Teste de polaridade de entrada**:
   - Comute cada entrada física e confirme o estado correspondente no pino HAL.
   - Valide os complementos `-not` se exportados.

5. **Teste de saída**:
   - Comute cada pino de saída HAL e verifique a saída física.

6. **Teste de encoder**:
   - Gire lentamente e rapidamente, verifique a direção da contagem e o tratamento do índice.

7. **Teste de movimento com motor desconectado**:
   - Valide primeiro a polaridade de step/dir e o temporização dos pulsos.

8. **Teste de movimento com motor conectado**:
   - Comece com velocidade/aceleração conservadoras.

## 12. Solução de Problemas

- **Sem resposta de I/O**:
  - Verifique se o ID `breakout_board` coincide nas configurações de firmware e HAL.
  - Verifique se os novos arquivos estão incluídos nos caminhos de build.

- **Build bem-sucedido, mas callbacks da placa não ativados**:
  - Confirme que a guarda `#if breakout_board == 42` está correta.
  - Confirme que o ramo de footer/config existe e compila.

- **Entradas deslocadas/ordem de bits errada**:
  - Verifique novamente o empacotamento/desempacotamento de bits entre:
    - `firmware/modules/breakoutboard_42.c`
    - `hal-driver/modules/breakoutboard_hal_42.c`

- **Comportamento de desconexão aleatória**:
  - Verifique o código timeout-safe em `breakout_board_disconnected_update()`.
  - Verifique a estabilidade da rede e o cabeamento do watchdog de pacotes.

- **Clipping/overflow na saída analógica**:
  - Limite os valores HAL antes de empacotar as palavras do DAC.
  - Confirme o escalonamento em relação à resolução do DAC.

## 13. Recomendações de Controle de Versão

Para cada conjunto de alterações de placa personalizada, faça commit nesta ordem:

1. Módulo de firmware + atualizações de footer/config.
2. Módulo HAL + atualizações de include/config.
3. Atualizações de configuração HAL do LinuxCNC.
4. Documentação para fiação e mapa de pinos.

Mantenha um arquivo markdown específico da placa com:

- Link do esquemático
- Tabela de mapeamento de pinos
- Mapa de endereços I2C
- Política de estado seguro
- Contrato de nomenclatura dos pinos HAL

## 14. Checklist Rápido de Arquivos

Você deve modificar todos os seguintes arquivos para um novo ID de placa:

- `firmware/modules/breakoutboard_42.c`
- `firmware/CMakeLists.txt`
- `firmware/inc/footer.h`
- `firmware/inc/config.h`
- `hal-driver/modules/breakoutboard_hal_42.c`
- `hal-driver/stepper-ninja.c`
- `hal-driver/config.h`
- (opcional) `docs/nome-da-sua-placa.md`

Se estes estiverem completos e os IDs forem consistentes, a integração da sua placa de breakout personalizada geralmente é direta.
