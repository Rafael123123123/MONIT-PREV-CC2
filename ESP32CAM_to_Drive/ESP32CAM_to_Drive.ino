// Inclui bibliotecas necessárias para comunicação WiFi, cliente seguro, câmara, sistema de ficheiros e sincronização de tempo
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Base64.h"
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"  
#include "time.h"

// Define uma lista de servidores NTP para sincronização de tempo
String timeServerList[4] = {"time.nist.gov", "0.pt.pool.ntp.org", "192.168.0.254", "10.20.229.254"};
const int numServers = 4; // Número total de servidores NTP disponíveis

// Configurações de fuso horário (GMT e horário de verão)
const long gmtOffset_sec = 0;   
const int daylightOffset_sec = 0; 

// Definição dos pinos da câmara ESP32-CAM para conexão do sensor
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Pino para o LED de flash
#define FLASH_LED_PIN 4

// Credenciais da rede WiFi principal
const char* ssid = "Monit_CC2";
const char* password = "monitcc2rmjspmlt20425";

// Identificador de implementação e nome da pasta para o Google Drive
String myDeploymentID = "AKfycbxlws8gWSdxYaO50yECkVXiQwnLBEufla2bAnE0xf5CoP4Asx8Qpu06lpC9pfmg-30iBg";
String myMainFolderName = "ESP32-CAM_2";

// Variáveis para controlo de tempo, intervalos e contagem
unsigned long long previousMillis = 0; // Armazena o último tempo registado
long Interval = 1; // Intervalo inicial para captura de fotos (em minutos)
int minutosAnt = 90; // Minuto anterior para evitar repetições

unsigned long tempoDesdeInicio = 0; // Tempo decorrido desde o início
int counter = 0; // Contador de fotos enviadas
bool rtcyes = 0; // Indicador de sincronização de tempo bem-sucedida

bool LED_Flash_ON = true; // Estado do LED de flash

// Cria um cliente seguro para comunicação HTTPS
WiFiClientSecure client;
const char* host = "script.google.com"; // Endereço do host para Google Apps Script

// Função para testar a conexão com o servidor
void Test_Con() {
  while (1) {
    Serial.println("-----------");
    Serial.println("A realizar teste de conexão...");
    Serial.println("A tentar conectar a " + String(host));

    client.setInsecure(); // Configura o cliente para ignorar verificação de certificado (não recomendado para produção)

    if (client.connect(host, 443)) { // Tenta conectar ao host na porta HTTPS (443)
      Serial.println("Conexão estabelecida com sucesso.");
      Serial.println("-----------");
      client.stop(); // Fecha a conexão com o servidor
      break; // Sai do ciclo após conexão bem-sucedida
    } else {
      Serial.println("Falha ao conectar a " + String(host) + ".");
      Serial.println("A aguardar para tentar novamente.");
      Serial.println("-----------");
      client.stop(); // Fecha qualquer conexão pendente
    }

    delay(1000); // Aguarda 1 segundo antes de tentar novamente
  }
}

// Função para inicializar o WiFi e sincronizar a hora com servidores NTP
void initializeWiFiAndTime() {
  struct tm timeinfo; // Estrutura para armazenar informações de tempo
  bool timeSynced = false; // Indicador de sincronização de tempo

  for (int i = 0; i < numServers; i++) { // Passa pelos servidores NTP disponíveis
    const char* ntpServer = timeServerList[i].c_str(); // Obtém o endereço do servidor NTP
    Serial.print("A tentar sincronizar com o servidor NTP: ");
    Serial.println(ntpServer);

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // Configura o fuso horário e servidor NTP

    if (getLocalTime(&timeinfo)) { // Verifica se a hora foi obtida com sucesso
      Serial.println("Hora sincronizada com o servidor NTP.");
      piscaFlashLED(3, 500); // Pisca o LED 3 vezes para indicar sucesso
      timeSynced = true; // Marca a sincronização como bem-sucedida
      rtcyes = 1; // Ativa o indicador de sincronização
      break; // Sai do ciclo após sincronização
    } else {
      Serial.println("Falha ao obter a hora do servidor NTP: " + String(ntpServer));
    }
  }

  if (!timeSynced) { // Se nenhum servidor NTP respondeu
    Serial.println("Falha ao sincronizar a hora com todos os servidores NTP.");
  }
}

// Função para obter a data e hora formatada
String getDateTimeString() {
  struct tm timeinfo; // Estrutura para armazenar informações de tempo
  if (!getLocalTime(&timeinfo)) { // Verifica se a hora pode ser obtida
    Serial.println("Falha ao obter a hora do RTC.");
    return ""; // Retorna string vazia em caso de falha
  }
  char timeString[25]; // Buffer para a string de data/hora
  strftime(timeString, sizeof(timeString), "%Y_%m_%d_%H_%M_%S", &timeinfo); // Formata a data/hora
  return String(timeString); // Retorna a string formatada
}

// Função para capturar e enviar fotos
void SendCapturedPhotos() {
  Serial.println("A ligar o flash...");
  pinMode(FLASH_LED_PIN, OUTPUT); // Configura o pino do LED como saída
  digitalWrite(FLASH_LED_PIN, HIGH); // Liga o LED de flash
  delay(500); // Aguarda 500ms para estabilizar a iluminação

  // Captura várias imagens para estabilizar o sensor da câmara
  for (int i = 0; i <= 3; i++) {
    camera_fb_t * fb = NULL; // Ponteiro para o buffer da câmara
    fb = esp_camera_fb_get(); // Captura uma imagem
    if (!fb) { // Verifica se a captura falhou
      Serial.println("Falha na captura da câmara.");
      Serial.println("A reiniciar o ESP32 CAM.");
      delay(1000);
      ESP.restart(); // Reinicia o ESP32 em caso de erro
      return;
    }
    esp_camera_fb_return(fb); // Liberta o buffer da imagem capturada
    delay(200); // Aguarda 200ms entre capturas
  }

  camera_fb_t * fb = NULL; // Ponteiro para o buffer da imagem final
  fb = esp_camera_fb_get(); // Captura a imagem principal
  if (!fb) { // Verifica se a captura falhou
    Serial.println("Falha na captura da câmara.");
    Serial.println("A reiniciar o ESP32 CAM.");
    delay(1000);
    ESP.restart(); // Reinicia o ESP32 em caso de erro
    return;
  }

  delay(500); // Aguarda 500ms antes de desligar o flash
  digitalWrite(FLASH_LED_PIN, LOW); // Desliga o LED de flash
  Serial.println("Flash desligado.");

  Serial.println("Fotografia capturada com sucesso.");

  Serial.println();
  Serial.println("-----------");
  Serial.println("A conectar a " + String(host));

  client.setInsecure(); // Configura o cliente para ignorar verificação de certificado

  if (client.connect(host, 443)) { // Tenta conectar ao servidor na porta HTTPS
    Serial.println("Conexão estabelecida com sucesso.");

    Serial.println();
    Serial.println("A enviar imagem para o Google Drive.");
    Serial.println("Tamanho: " + String(fb->len) + "byte");

    String url = "/macros/s/" + myDeploymentID + "/exec?folder=" + myMainFolderName; // Constrói a URL para o Google Apps Script

    client.println("POST " + url + " HTTP/1.1"); // Inicia uma requisição HTTP POST
    client.println("Host: " + String(host)); // Define o host no cabeçalho HTTP
    client.println("Transfer-Encoding: chunked"); // Indica que os dados serão enviados em pedaços
    client.println(); // Linha em branco para separar cabeçalho e corpo

    int fbLen = fb->len; // Tamanho do buffer da imagem
    char *input = (char *)fb->buf; // Ponteiro para os dados da imagem
    int chunkSize = 3 * 1000; // Tamanho de cada pedaço de dados
    int chunkBase64Size = base64_enc_len(chunkSize); // Tamanho do buffer para codificação Base64
    char output[chunkBase64Size + 1]; // Buffer para dados codificados

    Serial.println();
    int chunk = 0; // Contador de pedaços enviados
    for (int i = 0; i < fbLen; i += chunkSize) { // Divide a imagem em pedaços
      int l = base64_encode(output, input, min(fbLen - i, chunkSize)); // Codifica o pedaço em Base64
      client.print(l, HEX); // Envia o tamanho do pedaço em hexadecimal
      client.print("\r\n"); // Nova linha após o tamanho
      client.print(output); // Envia os dados codificados
      client.print("\r\n"); // Nova linha após os dados
      delay(100); // Pequena pausa para evitar sobrecarga
      input += chunkSize; // Avança o ponteiro para o próximo pedaço
      Serial.print("."); // Indica progresso no envio
      chunk++;
      if (chunk % 50 == 0) { // Nova linha após 50 pedaços
        Serial.println();
      }
    }
    client.print("0\r\n"); // Indica o fim dos dados chunked
    client.print("\r\n"); // Nova linha final

    esp_camera_fb_return(fb); // Liberta o buffer da imagem capturada

    Serial.println("A aguardar resposta do servidor.");
    long int StartTime = millis(); // Regista o tempo inicial
    while (!client.available()) { // Aguarda resposta do servidor
      Serial.print("."); // Indica espera
      delay(100);
      if ((StartTime + 10 * 1000) < millis()) { // Timeout após 10 segundos
        Serial.println();
        Serial.println("Sem resposta do servidor.");
        break;
      }
    }
    Serial.println();
    while (client.available()) { // Lê a resposta do servidor
      Serial.print(char(client.read())); // Imprime cada caractere recebido
    }

    digitalWrite(FLASH_LED_PIN, HIGH); // Liga o LED para indicar sucesso
    delay(500);
    digitalWrite(FLASH_LED_PIN, LOW); // Desliga o LED
    delay(500);
  } else { // Se a conexão ao servidor falhar
    pinMode(FLASH_LED_PIN, INPUT); // Configura o pino do LED como entrada para evitar interferência
    if (!SD_MMC.begin()) { // Tenta inicializar o cartão SD
      Serial.println("Erro ao inicializar o cartão SD.");
    } else {
      Serial.println("Cartão SD inicializado novamente!");
    }

    String dateTime; // String para armazenar data/hora
    if (rtcyes) { // Se o tempo estiver sincronizado
      dateTime = getDateTimeString(); // Obtém data/hora formatada
      Serial.println("Data e Hora: " + dateTime);
    } else { // Caso contrário, usa o tempo desde o início
      dateTime = String(tempoDesdeInicio);
    }

    String path = "/photo_" + dateTime + ".jpg"; // Caminho do ficheiro no cartão SD
    File file = SD_MMC.open(path, FILE_WRITE); // Abre o ficheiro para escrita
    if (file) { // Verifica se o ficheiro foi aberto com sucesso
      file.write(fb->buf, fb->len); // Escreve a imagem no cartão SD
      Serial.println("Foto salva no cartão SD: " + path);
      file.close(); // Fecha o ficheiro
    } else {
      Serial.println("Erro ao salvar a foto no cartão SD.");
    }
    esp_camera_fb_return(fb); // Liberta o buffer da imagem capturada
    SD_MMC.end(); // Finaliza a comunicação com o cartão SD
    delay(200); // Pequena pausa para estabilizar
  }
  client.stop(); // Fecha a conexão com o servidor
}

// Função para ler configurações do servidor
void ReadConfig() {
  const char* host = "script.google.com"; // Define o host do Google Apps Script
  if (client.connect(host, 443)) { // Tenta conectar ao servidor
    String url = "/macros/s/" + String(myDeploymentID) + "/exec"; // Constrói a URL do script
    client.print("GET " + url + " HTTP/1.1\r\nHost: " + String(host) + "\r\n\r\n"); // Envia uma requisição GET

    String redirectedUrl; // String para armazenar URL de redirecionamento

    while (client.connected()) { // Enquanto a conexão estiver ativa
      String line = client.readStringUntil('\n'); // Lê uma linha do cabeçalho
      Serial.print("Conteúdo do cabeçalho da resposta: ");
      Serial.println(line);

      if (line.startsWith("Location: ")) { // Verifica se há um redirecionamento
        redirectedUrl = line.substring(10); // Extrai a URL de redirecionamento
        redirectedUrl.trim(); // Remove espaços em branco
        Serial.print("URL de redirecionamento encontrada: ");
        Serial.println(redirectedUrl);
        break;
      }
      if (line == "\r") break; // Sai se encontrar uma linha vazia
    }
    client.stop(); // Fecha a conexão inicial

    if (redirectedUrl.length() > 0) { // Se foi encontrado um redirecionamento
      if (client.connect("script.googleusercontent.com", 443)) { // Conecta ao servidor redirecionado
        client.print("GET " + redirectedUrl + " HTTP/1.1\r\nHost: script.googleusercontent.com\r\n\r\n"); // Envia nova requisição GET

        while (client.connected()) { // Enquanto a conexão estiver ativa
          String line = client.readStringUntil('\n'); // Lê uma linha do cabeçalho
          Serial.print("Conteúdo do cabeçalho da resposta: ");
          Serial.println(line);
          if (line == "\r") break; // Sai ao encontrar linha vazia
        }

        String responseBody = ""; // String para armazenar o corpo da resposta
        while (client.connected() || client.available()) { // Enquanto houver dados para ler
          String chunkSizeStr = client.readStringUntil('\r'); // Lê o tamanho do pedaço
          client.read(); // Descarta o caractere '\n'
          int chunkSize = (int) strtol(chunkSizeStr.c_str(), NULL, 16); // Converte o tamanho para inteiro

          if (chunkSize > 0) { // Se há dados no pedaço
            String chunk = client.readStringUntil('\n'); // Lê o pedaço
            responseBody += chunk; // Adiciona ao corpo da resposta
            client.read(); // Descarta o '\r' final
          } else {
            break; // Sai quando não há mais pedaços
          }
        }

        Serial.print("Conteúdo completo da resposta: ");
        Serial.println(responseBody);

        Interval = responseBody.toInt(); // Converte a resposta para o novo intervalo
        Serial.print("Intervalo definido para: ");
        Serial.print(Interval / 1000); // Converte para segundos
        Serial.println("s");
        client.stop(); // Fecha a conexão
      } else {
        Serial.println("Erro: Não foi possível conectar ao script.googleusercontent.com.");
      }
    } else {
      Serial.println("Erro: Redirecionamento não encontrado.");
    }
  } else {
    Serial.println("Erro: Não foi possível conectar ao host.");
  }
}

// Função para piscar o LED de flash
void piscaFlashLED(int vezes, int duracao) {
  for (int i = 0; i < vezes; i++) { // Repete o número de vezes especificado
    digitalWrite(FLASH_LED_PIN, HIGH); // Liga o LED
    delay(duracao); // Aguarda a duração especificada
    digitalWrite(FLASH_LED_PIN, LOW); // Desliga o LED
    delay(duracao); // Aguarda novamente
  }
}

// Função de configuração inicial
void setup() {
  int connecting_process_timed_out = 20; // Tempo limite para conexão WiFi

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Desativa a proteção contra brownout

  Serial.begin(115200); // Inicia a comunicação serial a 115200 baud
  Serial.println();
  delay(1000); // Aguarda 1 segundo para estabilizar

  pinMode(FLASH_LED_PIN, OUTPUT); // Configura o pino do LED como saída

  Serial.println("A configurar o ESP32 em modo estação.");
  WiFi.mode(WIFI_STA); // Define o modo WiFi como estação (cliente)

  Serial.println();

  WiFi.begin(ssidIn, passwordIn); // Inicia conexão com a rede inicial
  Serial.print("A conectar à rede inicial...");
  while (WiFi.status() != WL_CONNECTED && connecting_process_timed_out > 0) { // Tenta conectar
    Serial.print(".");
    delay(400); // Aguarda 400ms entre tentativas
    connecting_process_timed_out--;
  }
  delay(10);
  Serial.println("");

  if (WiFi.status() == WL_CONNECTED) { // Se conectado com sucesso
    Serial.println("Conectado à rede inicial.");
    initializeWiFiAndTime(); // Sincroniza o tempo
    WiFi.disconnect(); // Desconecta da rede inicial
    Serial.println("A desconectar da rede inicial.\n");
  } else {
    Serial.println("Falha ao conectar à rede inicial.\n");
  }
  delay(10);

  connecting_process_timed_out = 20; // Reinicia o tempo limite

  Serial.print("A conectar a: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password); // Inicia conexão com a rede principal
  delay(10);

  Serial.print("A tentar conectar à segunda rede...");
  while (WiFi.status() != WL_CONNECTED && connecting_process_timed_out > 0) { // Tenta conectar
    Serial.print(".");
    delay(500); // Aguarda 500ms entre tentativas
    connecting_process_timed_out--;
  }

  Serial.println("");

  if (WiFi.status() == WL_CONNECTED) { // Se conectado com sucesso
    Serial.println("Conectado à segunda rede.");
    Test_Con(); // Testa a conexão com o servidor
    initializeWiFiAndTime(); // Sincroniza o tempo
    delay(1000);
    piscaFlashLED(2, 700); // Pisca o LED para indicar sucesso
  } else {
    Serial.println("Falha ao conectar à segunda rede.");
    piscaFlashLED(4, 200); // Pisca o LED para indicar falha
    delay(1000);
  }

  Serial.println();
  Serial.println("A configurar a câmara ESP32 CAM...");

  camera_config_t config; // Estrutura para configuração da câmara
  config.ledc_channel = LEDC_CHANNEL_0; // Canal PWM para clock
  config.ledc_timer = LEDC_TIMER_0; // Temporizador PWM
  config.pin_d0 = Y2_GPIO_NUM; // Pinos de dados da câmara
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; // Pino de clock externo
  config.pin_pclk = PCLK_GPIO_NUM; // Pino de clock de pixel
  config.pin_vsync = VSYNC_GPIO_NUM; // Pino de sincronização vertical
  config.pin_href = HREF_GPIO_NUM; // Pino de referência horizontal
  config.pin_sscb_sda = SIOD_GPIO_NUM; // Pino de dados I2C
  config.pin_sscb_scl = SIOC_GPIO_NUM; // Pino de clock I2C
  config.pin_pwdn = PWDN_GPIO_NUM; // Pino de power-down
  config.pin_reset = RESET_GPIO_NUM; // Pino de reset
  config.xclk_freq_hz = 20000000; // Frequência do clock externo (20MHz)
  config.pixel_format = PIXFORMAT_JPEG; // Formato de imagem JPEG

  if (psramFound()) { // Verifica se há PSRAM disponível
    config.frame_size = FRAMESIZE_UXGA; // Define resolução alta
    config.jpeg_quality = 10; // Qualidade JPEG média
    config.fb_count = 2; // Usa dois buffers de frame
  } else {
    config.frame_size = FRAMESIZE_SVGA; // Define resolução mais baixa
    config.jpeg_quality = 8; // Qualidade JPEG mais alta
    config.fb_count = 1; // Usa um buffer de frame
  }

  esp_err_t err = esp_camera_init(&config); // Inicializa a câmara com a configuração
  if (err != ESP_OK) { // Verifica se houve erro
    Serial.printf("Falha na inicialização da câmara com erro 0x%x", err);
    Serial.println();
    Serial.println("A reiniciar o ESP32 CAM.");
    delay(1000);
    ESP.restart(); // Reinicia o ESP32 em caso de erro
  }

  sensor_t * s = esp_camera_sensor_get(); // Obtém o sensor da câmara

  s->set_framesize(s, FRAMESIZE_SXGA); // Define a resolução SXGA para o sensor

  Serial.println("Câmara configurada com sucesso.");
  Serial.println();

  delay(1000);
  piscaFlashLED(5, 500); // Pisca o LED para indicar configuração concluída
}

// Ciclo principal
void loop() {
  String currentTime = getDateTimeString(); // Obtém a data/hora atual
  if (currentTime != "") { // Se a hora estiver disponível
    int currentMinute = currentTime.substring(14, 16).toInt(); // Extrai o minuto
    int currentSecond = currentTime.substring(17, 19).toInt(); // Extrai o segundo

    if ((currentMinute % Interval == 0) && minutosAnt != currentMinute) { // Verifica se é hora de capturar
      if (currentSecond >= 0 && currentSecond <= 5) { // Captura nos primeiros 5 segundos
        minutosAnt = currentMinute; // Atualiza o minuto anterior
        SendCapturedPhotos(); // Captura e envia a foto
        counter++; // Incrementa o contador de fotos

        if (counter > 3) { // Após 3 fotos, atualiza o intervalo
          Interval = 30; // Define intervalo padrão de 30 minutos
          ReadConfig(); // Lê nova configuração do servidor
          counter = 0; // Reinicia o contador
        }
      }
    }
    delay(1000); // Aguarda 1 segundo antes da próxima verificação
  } else { // Se a hora não estiver disponível
    if (WiFi.status() == WL_CONNECTED) { // Verifica se está conectado ao WiFi
      client.setInsecure(); // Configura cliente para ignorar certificado
      initializeWiFiAndTime(); // Tenta sincronizar o tempo novamente
    }
  }
}
