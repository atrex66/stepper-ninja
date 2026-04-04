# IPCONFIG - Configuração do Terminal Serial do stepper-ninja

## Visão Geral

A funcionalidade `ipconfig` em `serial_terminal.c` fornece uma interface de terminal serial para configurar o dispositivo stepper-ninja. Ela permite que os usuários definam parâmetros de rede (IP, MAC, porta etc.), gerenciem timeouts, salvem configurações na memória flash e reiniciem o dispositivo. Os comandos são digitados via conexão serial, processados em tempo real e validados quanto à formatação correta.

## Pré-requisitos

- Um emulador de terminal serial (ex.: **minicom** no Linux) configurado para se conectar à porta serial do dispositivo (ex.: `/dev/ttyACM0`) na taxa de baud apropriada (tipicamente 115200 8N1).
- O usuário deve fazer parte do grupo `dialout` para acessar portas seriais no Linux:

  ```bash
  sudo usermod -a -G dialout $USER
  ```

- O dispositivo deve estar ligado e conectado via cabo USB utilizado para programação do dispositivo.

## Configuração do Terminal Serial (Linux)

1. Instale o minicom:

   ```bash
   sudo apt update
   sudo apt install minicom
   ```

2. Inicie o minicom para a porta serial adequada (ex.: `/dev/ttyACM0`):

   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

3. Certifique-se de que o controle de fluxo por hardware/software esteja desabilitado no minicom (`Ctrl+A`, depois `O`, selecione **Serial port setup**, defina **F** e **G** como `No`).

## Comandos Suportados

O terminal serial aceita comandos digitados no terminal, finalizados por um retorno de carro (`\r`, tecla Enter). Os comandos são sensíveis a maiúsculas e minúsculas e devem seguir a sintaxe exata descrita abaixo. Comandos inválidos ou formatos incorretos retornam uma mensagem de erro.

| Comando | Descrição | Exemplo | Saída/Efeito |
|---------|-----------|---------|--------------|
| `help` | Exibe uma lista de comandos disponíveis e suas descrições. | `help` | Imprime o menu de ajuda. |
| `check` | Mostra a configuração atual, incluindo MAC, IP, sub-rede, gateway, DNS, status DHCP, porta, status PHY (duplex/velocidade) e timeout. | `check` | Imprime detalhes de configuração, ex.: `IP: 192.168.1.100`. |
| `ip <x.x.x.x>` | Define o endereço IP do dispositivo para o valor especificado. | `ip 192.168.1.100` | Atualiza o IP. Imprime `IP changed to 192.168.1.100`. |
| `ip` | Exibe o endereço IP atual. | `ip` | Imprime `IP: 192.168.1.100`. |
| `port <porta>` | Define o número de porta do dispositivo. | `port 5000` | Atualiza a porta. Imprime `Port changed to 5000`. |
| `port` | Exibe o número de porta atual. | `port` | Imprime `Port: 5000`. |
| `mac <xx:xx:xx:xx:xx:xx>` | Define o endereço MAC do dispositivo (hexadecimal, separado por dois pontos). | `mac 00:1A:2B:3C:4D:5E` | Atualiza o MAC. Imprime `MAC changed to 00:1A:2B:3C:4D:5E`. |
| `mac` | Exibe o endereço MAC atual. | `mac` | Imprime `MAC: 00:1A:2B:3C:4D:5E`. |
| `timeout <valor>` | Define o valor de timeout em microssegundos. | `timeout 1000000` | Atualiza o timeout. Imprime `Timeout changed to 1000000`. |
| `timeout` | Exibe o valor de timeout atual. | `timeout` | Imprime `Timeout: 1000000`. |
| `tim <valor>` | Define o valor da constante de tempo. | `tim 500` | Atualiza a constante de tempo. Imprime `Time const changed to 500`. |
| `defaults` | Restaura a configuração padrão. | `defaults` | Redefine a configuração para os padrões de fábrica. |
| `reset` | Reinicia o dispositivo usando o watchdog timer. | `reset` | Inicia um reset do dispositivo. |
| `reboot` | Igual a `reset`, reinicia o dispositivo. | `reboot` | Inicia um reset do dispositivo. |
| `save` | Salva a configuração atual na memória flash. | `save` | Persiste as alterações de configuração na flash. |

### Observações

- Comandos com parâmetros (ex.: `ip <x.x.x.x>`) exigem formatação exata. Por exemplo, endereços IP devem ser quatro números decimais separados por pontos (0-255), e endereços MAC devem ter seis bytes hexadecimais separados por dois pontos.
- Formatos inválidos geram uma mensagem de erro, ex.: `Invalid IP format` ou `Invalid MAC format`.
- O comando `save` deve ser emitido para persistir as alterações na flash após modificar configurações (ex.: IP, porta, MAC). Caso contrário, as alterações serão perdidas ao reiniciar.
- O terminal pode bloquear/desbloquear com base no estado `timeout_error`:
  - Se `timeout_error == 0`, o terminal bloqueia (`Terminal locked.`).
  - Se `timeout_error == 1`, o terminal desbloqueia (`Terminal unlocked.`).

## Exemplo de Uso

1. Conecte-se ao dispositivo usando minicom:

   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

2. Verifique a configuração atual:

   ```terminal
   check
   ```

   Saída:

   ```terminal
   Current configuration:
   MAC: 00:1A:2B:3C:4D:5E
   IP: 192.168.1.100
   Subnet: 255.255.255.0
   Gateway: 192.168.1.1
   DNS: 8.8.8.8
   DHCP: 1 (1-Static, 2-Dynamic)
   PORT: 5000
   *******************PHY status**************
   PHY Duplex: Full
   PHY Speed: 100Mbps
   *******************************************
   Timeout: 1000000
   Ready.
   ```

3. Defina um novo endereço IP:

   ```terminal
   ip 192.168.1.200
   ```

   Saída: `IP changed to 192.168.1.200`

4. Salve a configuração:

   ```terminal
   save
   ```

5. Reinicie o dispositivo:

   ```terminal
   reset
   ```

## Detalhes Técnicos

- **Tratamento de Entrada**: A função `handle_serial_input` lê caracteres de forma não bloqueante via `getchar_timeout_us`. Ela ignora caracteres ASCII não imprimíveis (exceto `\r`) e armazena entrada válida em um buffer de 64 bytes. Quando `\r` é recebido ou o buffer está cheio, o comando é processado e o buffer é limpo.
- **Processamento de Comandos**: A função `process_command` analisa comandos usando `strcmp` e `strncmp`. Para comandos com parâmetros, `sscanf` valida os formatos de entrada (ex.: IP, MAC, porta). As alterações são aplicadas a variáveis globais (`net_info`, `port`, `TIMEOUT_US`, `time_constant`) e salvas via `save_configuration`.
- **Armazenamento de Configuração**: A função `save_configuration` copia as configurações para uma estrutura `flash_config`, que é persistida na flash usando `save_config_to_flash`. A função `load_configuration` inicializa as configurações a partir da flash na inicialização.
- **Mecanismo de Bloqueio**: O terminal bloqueia quando `timeout_error == 0` e desbloqueia quando `timeout_error == 1`, controlando o acesso à interface serial.

## Solução de Problemas

- **Sem resposta do dispositivo**:
  - Verifique a porta serial correta (ex.: `ls /dev/tty*` para listar portas).
  - Certifique-se de que a taxa de baud esteja correta (padrão: 115200).
  - Verifique as conexões do cabo serial e a alimentação do dispositivo.
- **Permissão negada na porta serial**:
  - Adicione o usuário ao grupo `dialout` (veja Pré-requisitos).
- **Saída corrompida**:
  - Verifique a taxa de baud, paridade (8N1) e configurações de controle de fluxo no minicom.
- **Erros de formato inválido**:
  - Certifique-se da sintaxe exata do comando (ex.: `ip 192.168.1.100`, não `ip 192.168.001.100`).
- **Alterações não persistidas**:
  - Emita o comando `save` após modificar as configurações.

## Limitações

- O buffer do terminal é limitado a 63 caracteres (mais o terminador nulo). Comandos mais longos serão truncados.
- A interface serial é não bloqueante, portanto entradas rápidas podem ser ignoradas se o dispositivo estiver ocupado.
- O mecanismo de bloqueio (baseado em `timeout_error`) pode restringir o acesso inesperadamente se as condições de rede causarem timeouts.

## Arquivos Relacionados

- `serial_terminal.c`: Implementa a interface do terminal serial e o processamento de comandos.
- `config.h`: Define a estrutura `configuration_t` e funções como `save_config_to_flash`, `load_config_from_flash` e `restore_default_config`.
- `wizchip_conf.h`: Define a estrutura `wiz_NetInfo` e funções como `getPHYSR` para configuração de rede.
