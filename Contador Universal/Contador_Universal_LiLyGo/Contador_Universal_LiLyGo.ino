#include <WiFi.h>
#include <PubSubClient.h>

// === CONFIGURAÇÕES LORA e RTC ===
#include <SPI.h>
#include <LoRa.h>
#include <TimeLib.h>

// Pinos SPI e LoRa no TTGO LoRa32 v1.3
#define SCK 5  
#define MISO 19   
#define MOSI 27   
#define SS 18
#define RST 14    
#define DIO0 26   

// === CONFIGURAÇÕES DE DISPLAY OLED ===
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Definir dimensões do display OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Endereço I2C do display (padrão 0x3C)
#define OLED_ADDR 0x3C

// Inicializar o display OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);


// === CONFIGURAÇÕES DE REDE ===
const char* ssid = "labs";
const char* password = "1nv3nt@r2023_IPLEIRIA";

// === CONFIGURAÇÕES MQTT ===
const char* mqtt_server = "10.20.229.224";
const int mqtt_port = 1883;
const char* mqtt_topic = "sucesso/consumo";
WiFiClient espClient;
PubSubClient client(espClient);


// === CONFIGURAÇÕES DE DADOS ===
String dados;
String mensagemRecebida;
String partes[200];
boolean valorCorreto;
int nDadosEnviados = 0;
String mensagemsEnviadasMQTT1 [100];
String mensagemsEnviadasMQTT2 [100];

bool ack_mqtt = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;

  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  Serial.println(msg);

  if (msg == "Dados enviados com sucesso") {
    ack_mqtt = 1;
    Serial.println("ACK recebido, ack_mqtt = 1");
  } else {
    ack_mqtt = 0;
    Serial.println("Mensagem inesperada, ack_mqtt = 0");
  }
}


// Reconnecta se cair a conexão
void reconnect() {
  if (client.connect("ESP32Cliente")) {
    Serial.println("conectado!");
    client.subscribe(mqtt_topic);
    client.publish("esp32/start","oii");
  }
}

void setup() {
  Serial.begin(115200);

  // Inicializar o display
  if (!display.begin(SSD1306_PAGEADDR, OLED_ADDR)) {
    Serial.println(F("Falha ao inicializar o display OLED"));
    for (;;); // Trava o programa
  }
  display.clearDisplay();
  display.display();


  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" conectado!");

  // Mostrar o IP e o SSID conectado
  Serial.print("IP atribuído: ");
  Serial.println(WiFi.localIP());

  Serial.print("Conectado à rede: ");
  Serial.println(WiFi.SSID());


  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("Iniciando LoRa...");
  if (setupLora(243, 868, 20)) {
    Serial.println("Lora iniciado com sucesso");
  } else {
    Serial.println("Lora não iniciou");
  }
}

void loop() {
  // Verifica se há pacotes LoRa recebidos
  if (LoRa.parsePacket()) {
    Serial.print("Pacote recebido: ");
    while (LoRa.available()) {
      mensagemRecebida = LoRa.readString();
      Serial.print(mensagemRecebida);
    }
    Serial.print(" | RSSI: ");
    Serial.println(LoRa.packetRssi());
    escreverNoOLED("RSSI: " + String(LoRa.packetRssi()), 0);

    if (LoRa.packetRssi() > -150) {
      if (mensagemRecebida.length() >= 19 && isDigit(mensagemRecebida[0]) && isDigit(mensagemRecebida[1]) && isDigit(mensagemRecebida[2]) && isDigit(mensagemRecebida[3]) && mensagemRecebida[4] == '/' && isDigit(mensagemRecebida[5]) && isDigit(mensagemRecebida[6]) && mensagemRecebida[7] == '/' && isDigit(mensagemRecebida[8]) && isDigit(mensagemRecebida[9]) && mensagemRecebida[10] == ' ' && isDigit(mensagemRecebida[11]) && isDigit(mensagemRecebida[12]) && mensagemRecebida[13] == ':' && isDigit(mensagemRecebida[14]) && isDigit(mensagemRecebida[15]) && mensagemRecebida[16] == ':' && isDigit(mensagemRecebida[17]) && isDigit(mensagemRecebida[18])) {
        dividirMensagem(mensagemRecebida, 200);
        valorCorreto = teste(partes[2].toInt());

        if (!client.connected()) {
          reconnect();
        }
        // client.loop();

        if (valorCorreto) {
          int tempoEspera = 0;
          nDadosEnviados = 0;
          if (client.connected()) {
            for (int i = 4; i < 4 + partes[2].toInt(); i++) {
              Serial.println("i: " + String(i - 3) + "/" +  String(partes[2].toInt() - 1));
              String dataCompleta = partes[0] + " " + partes[1];
              dados = addMinutesToDate(dataCompleta, (i - 4) * partes[3].toInt());
              dados += " ";
              dados += partes[i];
              if (mensagemsEnviadasMQTT1[i] != dados) {
                nDadosEnviados++;
                client.publish("esp32/dados", dados.c_str());
                Serial.println(dados);
                delay(100);
                mensagemsEnviadasMQTT2[i] = dados;
                if(i == partes[2].toInt() + 3){
                  client.publish("esp32/ndados", String(nDadosEnviados).c_str());
                }
              }
            }
            if(nDadosEnviados = 0){
              Serial.println("Todos os dados recebidos já foram enviados com sucesso para a base de dados");
              enviarACK(true);
            }
            
            while (!ack_mqtt && tempoEspera < 30000) {
              client.loop();
              tempoEspera++;
              delay(1);
            }
            if (ack_mqtt) {
              enviarACK(true);
              ack_mqtt = 0;
              escreverNoOLED("Enviado - RSSI: " + String(LoRa.packetRssi()), 0);
              for (int i = 0; i < 100; i++) {
                mensagemsEnviadasMQTT1[i] = mensagemsEnviadasMQTT2[i];
              }
              Serial.println("Dados anteriores atualizados");
            }
            mensagemRecebida = "";

          } else {
            Serial.println("Não se conectou ao MQTT");
            enviarACK(false);
          }
        } else {
          Serial.println("Confirmação do valor total não bateu certo");
          enviarACK(false);
        }
      } else {
        Serial.println("Inicio do pacote errado");
        enviarACK(false);
      }
    } else {
      Serial.println("Pacote lora com ruido");
      enviarACK(false);
    }
  }
  client.loop();
}



bool setupLora(int ID, int frequencia, int potencia)
{
  Serial.println("Iniciando LoRa...");

  // Configura manualmente os pinos do SPI
  SPI.begin(SCK, MISO, MOSI, SS);

  // Configura os pinos do LoRa
  LoRa.setPins(SS, RST, DIO0);
  // Start LoRa communication
  if (!LoRa.begin(frequencia * 1E6))
  {
    Serial.println("Starting LoRa failed!");
    return 0;
  }

  // Configura o Sync Word para evitar interferências com outros dispositivos LoRa 0xF3
  LoRa.setSyncWord(ID);
  LoRa.setTxPower(potencia);
  Serial.println("LoRa Initializing OK!");
  return 1;
}

long teste(int nValores) {
  long soma = 0;
  for (int i = 4; i < 4 + nValores; i++)
    soma += partes[i].toInt();
  if (soma == partes[4 + nValores].toInt()) {
    return 1;
  } else {
    return 0;
  }
}

void dividirMensagem(String mensagemRecebida, int maxPartes) {
  int startIndex = 0;
  int partIndex = 0;

  while (startIndex < mensagemRecebida.length() && partIndex < maxPartes) {
    int separatorIndex = mensagemRecebida.indexOf(' ', startIndex);

    if (separatorIndex == -1) { // Última parte da string
      partes[partIndex] = mensagemRecebida.substring(startIndex);
      partIndex++;
      break;
    } else {
      partes[partIndex] = mensagemRecebida.substring(startIndex, separatorIndex);
      startIndex = separatorIndex + 1;
      partIndex++;
    }
  }

  // Exibir as partes divididas no Serial Monitor e no OLED
  String valores = ""; 
  for (int i = 0; i < partIndex; i++) {
    Serial.print("Parte ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(partes[i]);

    switch (i) {
      case 0:
        escreverNoOLED("Data: " + partes[i], 2);
        break;
      case 1:
        escreverNoOLED("Hora: " + partes[i], 3);
        break;
      case 2:
        escreverNoOLED("Valores: " + partes[i], 4);
        break;
      case 3:
        escreverNoOLED("Delta t: " + partes[i], 5);
        break;
      default:
        valores += partes[i] + " ";
    }
  }

  if (!valores.isEmpty()) {
    escreverNoOLED("Valores: " + valores, 6);
  }
}


// Função para escrever texto em várias linhas no OLED
void escreverNoOLED(String texto, int linha) {
  int alturaLinha = 8; 
  int posicaoY = linha * alturaLinha; 

  if (linha == 0) {
    // Limpa o display apenas na primeira linha para manter as anteriores
    display.clearDisplay(); 
  }

  display.setTextSize(1);               
  display.setTextColor(SSD1306_WHITE);  
  display.setCursor(0, posicaoY);       
  display.println(texto);               
  display.display();                   
}

void enviarACK(bool sucesso) {
  byte ack[2];
  ack[0] = ack[1] = sucesso ? 0b11111111 : 0b00000000; // Dois bytes, todos iguais

  LoRa.beginPacket();
  // Envia os dois bytes
  LoRa.write(ack, 2); 
  LoRa.endPacket();

  // Exibir os bytes enviados no monitor serial
  Serial.print("ACK enviado: ");
  for (int i = 0; i < 2; i++) {
    Serial.print(String(ack[i], BIN) + " ");
  }
  Serial.println();
}


String addMinutesToDate(const String& dateStr, int minutes) {
  int year, month, day, hour, minute, second;
  if (sscanf(dateStr.c_str(), "%4d/%2d/%2d %2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second) != 6) {
    return "Erro: Formato inválido";
  }

  // Converter para time_t
  tmElements_t tm;
  tm.Year = year - 1970;
  tm.Month = month;
  tm.Day = day;
  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = second;
  time_t t = makeTime(tm);

  // Somar os minutos
  t += minutes * 60;
  breakTime(t, tm);

  // Retornar a nova data formatada
  char buffer[20];
  sprintf(buffer, "%04d/%02d/%02d %02d:%02d:%02d", tm.Year + 1970, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
  return String(buffer);
}
