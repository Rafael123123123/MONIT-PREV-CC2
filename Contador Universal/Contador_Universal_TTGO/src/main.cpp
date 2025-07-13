#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <ArduinoJson.h>

#include <esp_task_wdt.h>

#include <RTClib.h>
RTC_DS3231 rtc;

#include <LoRaWan.h>

// Prototipando funções
void configurarWiFi();
bool salvarConfiguracoes();
void carregarConfiguracoes();
void conectarWiFi();
void iniciarSD();
void adicionarAoMonitor(const String &mensagem);
void initializeWiFiAndTime();
void verificarHora();
String getDateTimeString();
void fusoHorario(DateTime now);
void enviarMensagemEmBlocos(const char *mensagemCompleta);
bool receberImagem(uint32_t tamanho);
uint32_t lerTamanhoImagem();
bool esperarInicioImagem();
bool salvarImagemNoSD(uint32_t imageSize);

int contagemSensor = 0;

//_____________________________Configurações de Wi-Fi__________________
String ssid_sta = "labs";
String password_sta = "1nv3nt@r2023_IPLEIRIA";

const char *ssid_ap = "Modulo Universal";
const char *password_ap = "123456789";

// Credenciais de autenticação
const char *authUsername = "admin";
const char *authPassword = "12344321";

//______________________________________________________________________

String timeServerList[4] = {"time.nist.gov", "0.pt.pool.ntp.org", "192.168.0.254", "10.20.229.254"};
int numServers = 4;

// Ajuste de fuso horário (0 para UTC)
const long gmtOffset_sec = 0;        
const int daylightOffset_sec = 3600; 

const char *jsonFileName = "/config.json";

bool existeHora = 0;

//_______________Sensor DE agua__________________
#define pinSensor 13

boolean primeiraVez = 1;
int pulseCounter = 0;
int pulseCounterAmosrtragem = 0;
unsigned long lastInterruptTime = 0;
unsigned long tempoRespostaLoRa = 0;
bool estadoAnterior = 1;
char registosSensor[30000];
int minutosAnt = 90;
int horaAnt = 0;

//_______________LORA ______________________________

char dataLoRa[2000] = "";
char dataLoRaHora[30] = "";
char dataLoRaHora2[30] = "";

#define MAX_BACKUPS 20
#define TAM_BACKUP 200
char dataLoRaMensagem[MAX_BACKUPS][TAM_BACKUP]; 

int storageUsado = 0;

int valoresLora = 0;
int somaValoresLora = 0;

byte comfirmacao = 0;
bool mensagemEnviadaLora = 0;
bool mensagemRecebidaLora = 0;

// LORA
int nAmostras = 1;
int dispositioID = 243;
int frequenciaLora = 868;
int potenciaLora = 20;

//_______________pagina debug__________________
bool debugSerie = 1;
bool debugMonitorGeral = 1;
bool debugMonitorLoRa = 1;
bool tirarFoto = 0;

//_______________Variáveis de escrita __________________
bool guardarValor = false;

#define MAX_LOGS 200
// Buffer para mensagens
char logsErrosInf[MAX_LOGS][70];
char logsOutros[MAX_LOGS][30];   

int logIndexErrosInf = 0;
int logIndexOutros = 0;

byte contMem = 0;

// Variáveis globais
WebServer server(80);

String html;

int intervaloTempo = 1;
int intervaloAmostragem = 1;
bool amostragem = 0;

unsigned long tempoDecorrido = 0;
unsigned long tempoDecorridoAmostragem = 0;
unsigned long tempoDecorridoServidor = 0;
unsigned long tempoDecorridoContagem = 0;

//_______________Verificação de tempos ______________________________
unsigned long tempoDecorrido2 = 0;
unsigned long tempoDecorrido3 = 0;
int counter1 = 0;
int counter2 = 0;
int counter3 = 0;
int counter4 = 0;
long valor1;
long valor2;

bool SD_ocupado = 0;
bool verificacao = 1;

#define SD_CS_PIN 17
#define LORA_CS_PIN 18

//_______________Variáveis para receber imagem __________________
#define IMG_START_MARKER "IMG_START"
#define MAX_IMAGE_SIZE 15000 

uint8_t imageBuffer[MAX_IMAGE_SIZE];

File imgFile;
bool novaImagem = false;
#define pinInterrup 4

//_____________________________________________________________________________________________________________
void iniciarSD()
{
  digitalWrite(SD_CS_PIN, LOW); 
  if (!SD.begin(SD_CS_PIN))
  {
    Serial.println("Erro ao inicializar o SD.");
    verificacao = 0;
    return;
  }  
  Serial.println("SD inicializado!");
}

void desligarSD()
{
  digitalWrite(SD_CS_PIN, HIGH);
}

// Função para salvar as configurações em um arquivo JSON
bool salvarConfiguracoes()
{
  if (!SD_ocupado)
  {
    iniciarSD();
    SD_ocupado = 1;
    // Abre o arquivo para escrita
    File file = SD.open(jsonFileName, FILE_WRITE); 
    if (!file)
    {
      Serial.println("Falha ao abrir o arquivo para escrita!");
      adicionarAoMonitor("Erro - Falha ao abrir o arquivo para escrita!");
      SD_ocupado = 0;
      desligarSD();
      return false;
    }

    StaticJsonDocument<2048> doc;
    doc["ssid_sta"] = ssid_sta;
    doc["password_sta"] = password_sta;

    JsonArray timeServers = doc.createNestedArray("timeServerList");
    for (int i = 0; i < numServers; i++)
    {
      timeServers.add(timeServerList[i]);
    }

    doc["numServers"] = numServers;
    doc["intervaloTempo"] = intervaloTempo;
    doc["intervaloAmostragem"] = intervaloAmostragem;
    doc["amostragem"] = amostragem;
    doc["meioComunicacao"] = tirarFoto;
    doc["nAmostras"] = nAmostras;
    doc["dispositivoID"] = dispositioID;
    doc["frequenciaLora"] = frequenciaLora;
    doc["potenciaLora"] = potenciaLora;
    doc["debugSerie"] = debugSerie;
    doc["debugMonitorGeral"] = debugMonitorGeral;
    doc["debugMonitorLoRa"] = debugMonitorLoRa;

    if (serializeJson(doc, file) == 0)
    {
      Serial.println("Falha ao escrever JSON no arquivo!");
      adicionarAoMonitor("Erro - Falha ao escrever JSON no arquivo!");
    }
    else
    {
      Serial.println("Configurações salvas com sucesso!");
      if (debugMonitorGeral)
        adicionarAoMonitor("Inf - Configurações salvas com sucesso!");
    }

    file.close();
    SD_ocupado = 0;
    desligarSD();
    return true;
  }
  else
  {
    Serial.println("SD ocupado, não foi possível guardar configurações");
    adicionarAoMonitor("Erro - SD ocupado, não foi possível guardar configurações");
    return false;
  }
}

// Função para ler as configurações de um arquivo JSON
void carregarConfiguracoes()
{
  iniciarSD();
  File file = SD.open(jsonFileName, FILE_READ);
  if (!file)
  {
    Serial.println("Arquivo de configuração não encontrado!");
    adicionarAoMonitor("Erro - JSON não encontrado");
    desligarSD();
    return;
  }

   // Aumente o tamanho do ficheiro
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.print("Erro ao carregar JSON: ");
    Serial.println(error.f_str());
    file.close();
    adicionarAoMonitor("Erro - Falha ao carregar JSON");
    desligarSD();
    return;
  }

  ssid_sta = doc["ssid_sta"] | ssid_sta;
  password_sta = doc["password_sta"] | password_sta;

  JsonArray timeServers = doc["timeServerList"];
  numServers = 0;
  for (JsonVariant v : timeServers)
  {
    if (numServers < 4) 
    {
      timeServerList[numServers++] = v.as<String>();
    }
    else
    {
      Serial.println("Erro: Número de servidores NTP excede o limite.");
      adicionarAoMonitor("Erro - Número de servidores NTP excede o limite");
      break;
    }
  }

  intervaloTempo = doc["intervaloTempo"] | intervaloTempo;
  intervaloAmostragem = doc["intervaloAmostragem"] | intervaloAmostragem;
  amostragem = doc["amostragem"] | amostragem;
  tirarFoto = doc["meioComunicacao"] | tirarFoto;
  dispositioID = doc["dispositivoiID"] | dispositioID;
  nAmostras = doc["nAmostras"] | nAmostras;
  frequenciaLora = doc["frequenciaLora"] | frequenciaLora;
  potenciaLora = doc["potenciaLora"] | potenciaLora;
  debugSerie = doc["debugSerie"] | debugSerie;
  debugMonitorGeral = doc["debugMonitorGeral"] | debugMonitorGeral;
  debugMonitorLoRa = doc["debugMonitorLoRa"] | debugMonitorLoRa;

  Serial.println("Configurações carregadas com sucesso!");

  file.close();
  desligarSD();
}

void configurarWiFi()
{
  html = "<html><body><h2>Configurar Wi-Fi</h2>";
  html += "<form method='POST' action='/salvarWiFi' style='margin-bottom: 20px;'>";
  html += "<label>SSID: </label><input type='text' name='ssid' value='" + ssid_sta + "'><br>";
  html += "<label>Senha: </label><input type='password' name='senha' value='" + password_sta + "'><br>";
  html += "<input type='submit' value='Salvar Wi-Fi' style='margin-right: 10px;'>";
  html += "<button type='button' onclick=\"window.location.href='/'\">Voltar</button>";
  html += "</form>";
  html += "<div id='mensagem' style='display: none; padding: 10px; border-radius: 5px;'></div>";
  html += "<script>";
  html += "document.querySelector('form').addEventListener('submit', function(event) {";
  html += "event.preventDefault();";
  html += "fetch('/salvarWiFi', { method: 'POST', body: new FormData(this) }).then(response => {";
  html += "if (response.ok) {";
  html += "document.getElementById('mensagem').innerHTML = 'Configuracao salva com sucesso!';";
  html += "document.getElementById('mensagem').style.display = 'block';";
  html += "document.getElementById('mensagem').style.backgroundColor = '#d4edda';";
  html += "document.getElementById('mensagem').style.color = '#155724';";
  html += "document.getElementById('mensagem').style.border = '1px solid #c3e6cb';";
  html += "setTimeout(() => { document.getElementById('mensagem').style.display = 'none'; }, 5000);";
  html += "} else {";
  html += "document.getElementById('mensagem').innerHTML = 'Erro ao salvar configuração!';";
  html += "document.getElementById('mensagem').style.display = 'block';";
  html += "document.getElementById('mensagem').style.backgroundColor = '#f8d7da';";
  html += "document.getElementById('mensagem').style.color = '#721c24';";
  html += "document.getElementById('mensagem').style.border = '1px solid #f5c6cb';";
  html += "}";
  html += "});";
  html += "});";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void salvarWiFi()
{
  if (server.hasArg("ssid") && server.hasArg("senha"))
  {
    ssid_sta = server.arg("ssid");
    password_sta = server.arg("senha");

    Serial.println("ssid: " + ssid_sta);
    Serial.println("senha: " + password_sta);

    conectarWiFi();

    server.send(200, "text/plain", "Configuracao salva com sucesso!");
  }
  else
  {
    server.send(400, "text/plain", "Faltam dados para configurar o Wi-Fi.");
  }
}

void configurarLoRa()
{
  html = "<html><body><h2>Configurar LoRa</h2>";
  html += "<p>Configuracao dos parametros do LoRa.</p>";
  html += "<form method='POST' action='/salvarLoRa' style='margin-bottom: 20px;'>";
  html += "<label>Numero de amostras: </label><input type='number' name='amostras' value='" + String(nAmostras) + "'><br>";
  html += "<label>ID do dispositivo: </label><input type='text' name='id' value='" + String(dispositioID) + "'><br>";
  html += "<label>Frequencia LoRa: </label><input type='number' name='frequencia' value='" + String(frequenciaLora) + "'><br>";
  html += "<label>Potencia LoRa: </label><input type='number' name='potencia' value='" + String(potenciaLora) + "'><br>";
  html += "<input type='submit' value='Salvar LoRa' style='margin-right: 10px;'>";
  html += "<button type='button' onclick=\"window.location.href='/'\">Voltar</button>";
  html += "</form>";
  html += "<div id='mensagem' style='display: none; padding: 10px; border-radius: 5px;'></div>";
  html += "<script>";
  html += "document.querySelector('form').addEventListener('submit', function(event) {";
  html += "event.preventDefault();";
  html += "fetch('/salvarLoRa', { method: 'POST', body: new FormData(this) }).then(response => {";
  html += "if (response.ok) {";
  html += "document.getElementById('mensagem').innerHTML = 'Configuracao salva com sucesso!';";
  html += "document.getElementById('mensagem').style.display = 'block';";
  html += "document.getElementById('mensagem').style.backgroundColor = '#d4edda';";
  html += "document.getElementById('mensagem').style.color = '#155724';";
  html += "document.getElementById('mensagem').style.border = '1px solid #c3e6cb';";
  html += "setTimeout(() => { document.getElementById('mensagem').style.display = 'none'; }, 5000);";
  html += "} else {";
  html += "document.getElementById('mensagem').innerHTML = 'Erro ao salvar configuração!';";
  html += "document.getElementById('mensagem').style.display = 'block';";
  html += "document.getElementById('mensagem').style.backgroundColor = '#f8d7da';";
  html += "document.getElementById('mensagem').style.color = '#721c24';";
  html += "document.getElementById('mensagem').style.border = '1px solid #f5c6cb';";
  html += "}";
  html += "});";
  html += "});";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void salvarLoRa()
{
  if (server.hasArg("amostras") && server.hasArg("id") && server.hasArg("frequencia") && server.hasArg("potencia"))
  {
    String amostras = server.arg("amostras");
    String id = server.arg("id");
    String frequencia = server.arg("frequencia");
    String potencia = server.arg("potencia");

    nAmostras = amostras.toInt();
    dispositioID = id.toInt();
    frequenciaLora = frequencia.toInt();
    potenciaLora = potencia.toInt();

    Serial.println("Número de amostras LoRa: " + amostras);
    Serial.println("ID do dispositivo LoRa: " + id);
    Serial.println("Frequência LoRa: " + frequencia);
    Serial.println("Potência LoRa: " + potencia);

    if (setupLora(dispositioID, frequenciaLora, potenciaLora) && debugMonitorLoRa)
    {
      adicionarAoMonitor("Inf - Lora iniciado com sucesso");
      adicionarAoMonitor("Inf - ID: " + String(dispositioID) + " | Freq: " + String(frequenciaLora) + " | Pot: " + String(potenciaLora));
    }

    html = "<html><body><h2>Configurar LoRa</h2>";
    html += "<p>Configuracao dos parametros do LoRa.</p>";
    html += "<form method='POST' action='/salvarLoRa' style='margin-bottom: 20px;'>";
    html += "<label>Numero de amostras: </label><input type='number' name='amostras' value='" + String(nAmostras) + "'><br>";
    html += "<label>ID do dispositivo: </label><input type='text' name='id' value='" + String(dispositioID) + "'><br>";
    html += "<label>Frequencia LoRa: </label><input type='number' name='frequencia' value='" + String(frequenciaLora) + "'><br>";
    html += "<label>Potencia LoRa: </label><input type='number' name='potencia' value='" + String(potenciaLora) + "'><br>";
    html += "<input type='submit' value='Salvar LoRa' style='margin-right: 10px;'>";
    html += "<button type='button' onclick=\"window.location.href='/'\">Voltar</button>";
    html += "</form>";
    html += "<div style='margin-top: 10px; padding: 10px; background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; border-radius: 5px;'>";
    html += "Configuracao salva com sucesso!";
    html += "</div>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  }
  else
  {
    html = "<html><body><h2>Configurar LoRa</h2>";
    html += "<p>Configuracao dos parametros do LoRa.</p>";
    html += "<form method='POST' action='/salvarLoRa' style='margin-bottom: 20px;'>";
    html += "<label>Numero de amostras: </label><input type='number' name='amostras' value='" + String(nAmostras) + "'><br>";
    html += "<label>ID do dispositivo: </label><input type='text' name='id' value='" + String(dispositioID) + "'><br>";
    html += "<label>Frequencia LoRa: </label><input type='number' name='frequencia' value='" + String(frequenciaLora) + "'><br>";
    html += "<label>Potencia LoRa: </label><input type='number' name='potencia' value='" + String(potenciaLora) + "'><br>";
    html += "<input type='submit' value='Salvar LoRa' style='margin-right: 10px;'>";
    html += "<button type='button' onclick=\"window.location.href='/'\">Voltar</button>";
    html += "</form>";
    html += "<div style='margin-top: 10px; padding: 10px; background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; border-radius: 5px;'>";
    html += "Faltam dados para configurar o LoRa.";
    html += "</div>";
    html += "</body></html>";
    server.send(400, "text/html", html);
  }
}

// Conectar-se ao Wi-Fi
void conectarWiFi()
{
  // Configura o modo Station (STA)
  WiFi.begin(ssid_sta.c_str(), password_sta.c_str());
  Serial.print("Conectando à rede Wi-Fi...");
  if (WiFi.waitForConnectResult() == WL_CONNECTED)
  {
    Serial.println("Conectado!");
    Serial.print("IP STA: ");
    Serial.println(WiFi.localIP());
    if (debugMonitorGeral)
      adicionarAoMonitor("Inf - Conectado à rede " + String(ssid_sta) + ": " + WiFi.localIP().toString());
    initializeWiFiAndTime();
  }
  else
  {
    Serial.println("Falha ao conectar ao Wi-Fi STA");
    adicionarAoMonitor("Erro - Falha ao conectar ao Wi-Fi STA");
    WiFi.disconnect();
  }
}

void conectarHotspot()
{
  // Configura o modo Access Point (AP)
  if (WiFi.softAP(ssid_ap, password_ap))
  {
    Serial.println("Hotspot criado com sucesso!");
    Serial.print("Hotspot criado com IP: ");
    Serial.println(WiFi.softAPIP());
  }
  else
  {
    Serial.println("Erro ao criar o Hotspot!");
    if (debugMonitorGeral)
      adicionarAoMonitor("Erro - Falha ao criar o Hotspot!");
    return; 
  }

  // Inicia o servidor web no modo AP
  server.begin();
  Serial.println("Servidor web iniciado no Hotspot!");
}

void guardarNoSDSensor()
{
  if (!SD_ocupado)
  {
    iniciarSD();
    SD_ocupado = 1;

    // Verifica se o arquivo existe
    if (!SD.exists("/ContagensSensor.txt"))
    {
      // Cria o arquivo vazio
      File contagensFile = SD.open("/ContagensSensor.txt", FILE_WRITE);
      if (contagensFile)
      {
        contagensFile.close(); 
      }
      else
      {
        Serial.println("Erro ao criar 'ContagensSensor.txt'.");
        adicionarAoMonitor("Erro - Não foi possível criar o arquivo ContagensSensor");
        SD_ocupado = 0;
        desligarSD();
        strncat(registosSensor, "\n", sizeof(registosSensor) - strnlen(registosSensor, sizeof(registosSensor)) - 1);
        return;
      }
    }

    File contagensFile = SD.open("/ContagensSensor.txt", FILE_APPEND); 
    if (contagensFile)
    {
      contagensFile.write((const uint8_t *)registosSensor, strnlen(registosSensor, sizeof(registosSensor)));
      contagensFile.write((const uint8_t *)"\n", 1);
      contagensFile.flush();
      contagensFile.close();

      if (!amostragem)
      {
        // Percorrer registosSensor e enviar cada linha separada por '\n' ao monitor
        const char *start = registosSensor;
        const char *end;

        while ((end = strchr(start, '\n')) != NULL)
        {
          // Calcula o comprimento da substring até '\n'
          size_t length = end - start;

          // Converte a substring para String e envia ao monitor
          String line = String(start).substring(0, length);
          adicionarAoMonitor(line);

          // Move o ponteiro inicial para a próxima linha
          start = end + 1;
        }

        // Enviar o restante caso não termine com '\n'
        if (*start != '\0')
        {
          adicionarAoMonitor(String(start));
        }
      }
    }
    else
    {
      Serial.println("Erro ao abrir 'ContagensSensor.txt' para escrita.");
      strncat(registosSensor, "\n", sizeof(registosSensor) - strnlen(registosSensor, sizeof(registosSensor)) - 1);

      SD_ocupado = 0;
      desligarSD();
      adicionarAoMonitor("Erro - Erro ao abrir ContagensSensor");
      return;
    }

    SD_ocupado = 0;
    desligarSD();
    // Limpa o buffer
    memset(registosSensor, 0, sizeof(registosSensor)); 
  }
  else
  {
    Serial.println("SD está ocupado");
    adicionarAoMonitor("Erro - SD está ocupado (guardarNoSDSensor)");

    strncat(registosSensor, "\n", sizeof(registosSensor) - strnlen(registosSensor, sizeof(registosSensor)) - 1);
  }
}

// Obtém a hora do NTP e atualiza o RTC externo
void initializeWiFiAndTime()
{
  struct tm timeinfo;
  bool timeSynced = false;

  if (!rtc.begin())
  {
    Serial.println("Erro ao inicializar o RTC externo.");
    adicionarAoMonitor("Erro - Falha ao inicializar o RTC externo");
    verificacao = 0;
    return;
  }

  for (int i = 0; i < numServers; i++)
  {
    const char *ntpServer = timeServerList[i].c_str();
    Serial.print("Tentando sincronizar com o servidor NTP: ");
    Serial.println(ntpServer);

    // Configura o NTP com o servidor atual
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Tenta obter a hora
    if (getLocalTime(&timeinfo))
    {
      Serial.println("Hora sincronizada com o servidor NTP");
      if (debugMonitorGeral)
        adicionarAoMonitor("Inf - Hora sincronizada com o servidor NTP");

      // Atualiza o RTC externo
      rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      Serial.println("RTC externo atualizado com sucesso");
      if (debugMonitorGeral)
        adicionarAoMonitor("Inf - RTC externo atualizado com sucesso");

      timeSynced = true;
      break;
    }
    else
    {
      Serial.println("Falha ao obter a hora do servidor NTP: " + String(ntpServer));
    }
  }

  if (!timeSynced)
  {
    Serial.println("Falha ao sincronizar a hora com todos os servidores NTP.");
    adicionarAoMonitor("Erro - Falha ao sincronizar a hora com todos os servidores NTP");
  }
}

// Função para obter a data e hora atual do RTC como string
String getDateTimeString()
{
  if (!rtc.begin())
  {
    Serial.println("Erro ao inicializar o RTC.");
    return "";
  }
  else
  {
    if (existeHora == 0)
    {
      existeHora = 1;
      minutosAnt = rtc.now().minute();
    }
  }

  DateTime now = rtc.now();
  char timeString[25];
  snprintf(timeString, sizeof(timeString), "%04d/%02d/%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  return String(timeString);
}

// Página inicial
void paginaInicial()
{
  html = R"rawliteral(
    <html>
      <head>
        <title>Menu Principal</title>
        <style>
          body {
            display: flex;
            flex-direction: column;
            align-items: center;
            font-family: Arial, sans-serif;
            background-color: #f4f4f9;
            margin: 0;
            padding: 20px;
          }
          .menu {
            width: 100%;
            max-width: 600px;
            background-color: #fff;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
            margin-bottom: 20px;
          }
          .menu h2, .menu h3, .menu h4 {
            color: #333;
          }
          .menu label {
            display: block;
            margin-bottom: 10px;
          }
          .menu input[type="button"] {
            width: 100%;
            padding: 10px;
            margin: 5px 0;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 16px;
          }
          .menu input[type="button"]:hover {
            background-color: #007bff;
            color: #fff;
          }
          .monitor-container {
            width: 100%;
            max-width: 1200px;
            display: flex;
            flex-direction: column;
            align-items: center;
          }
          .monitor {
            width: 100%;
            max-width: 600px;
            border: 1px solid #000;
            padding: 10px;
            overflow-y: scroll;
            height: 300px;
            background-color: #f0f0f0;
            border-radius: 10px;
            margin-bottom: 20px;
          }
          .blue-button {
            background-color: #add8e6;
          }
        </style>
        <script>
          function salvarConfiguracoes() {
            fetch('/salvarConfiguracoes').then(response => {
              if (response.ok) {
                alert('Configuracao salva com sucesso!');
              } else {
                alert('Erro ao salvar configurações.');
              }
            });
          }

          function atualizarMonitor() {
            fetch('/obterMonitor?tipo=erros').then(response => response.text()).then(data => {
              document.getElementById('monitor-erros').innerHTML = data;
            });
            fetch('/obterMonitor?tipo=outros').then(response => response.text()).then(data => {
              document.getElementById('monitor-outros').innerHTML = data;
            });
          }

          setInterval(atualizarMonitor, 1000);
        </script>
      </head>
      <body>
        <div class="menu">
          <h2>Menu Principal</h2>

          <!-- Configurar meio -->
          <form>
            <label><input type="button" value="Configurar Debug" onclick="window.location.href='/debug'"></label>
          </form>
          
          <!-- Intervalo de tempo para captura -->
          <form>
            <label><input type="button" value="Intervalo de captura" onclick="window.location.href='/configurarIntervalo'"></label><br>
          </form>
                  
          <!-- Configurar comunicacao -->
          <h3>Configurar comunicacao:</h3>
          <form>
            <label><input type="button" value="Configurar Wi-Fi" onclick="window.location.href='/configurarWiFi'"></label><br>
            <label><input type="button" value="Configurar LoRa" onclick="window.location.href='/configurarLoRa'"></label><br><br>
          </form>
          
          <!-- Cartao de memoria -->
          <form>
            <label><input type="button" class="blue-button" value="Cartao SD" onclick="window.location.href='/cartaoSD'"></label><br>
          </form>

          <!-- Salvar Configuracoes -->
          <form>
            <label><input type="button" value="Salvar Configuracoes" onclick="salvarConfiguracoes()"></label>
          </form>
        </div>

        <!-- Monitores separados -->
        <div class="monitor-container">
          <div class="monitor" id="monitor-erros">
            <h4>Erros e Informacoes</h4>
          </div>
          <div class="monitor" id="monitor-outros">
            <h4>Contagens Gerais</h4>
          </div>
        </div>
      </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// Manipula a requisição de logs (separados)
void handleObterMonitor()
{
  char tipo[10];
  server.arg("tipo").toCharArray(tipo, sizeof(tipo));
  html = "";

  if (strcmp(tipo, "erros") == 0)
  {
    for (int i = 0; i < MAX_LOGS; i++)
    {
      int index = (logIndexErrosInf + i) % MAX_LOGS;
      if (index >= 0 && index < MAX_LOGS && strlen(logsErrosInf[index]) > 0)
      {
        html += String(logsErrosInf[index]) + "<br>";
      }
    }
  }
  else
  { 
    for (int i = 0; i < MAX_LOGS; i++)
    {
      int index = (logIndexOutros + i) % MAX_LOGS;
      if (index >= 0 && index < MAX_LOGS && strlen(logsOutros[index]) > 0)
      {
        html += String(logsOutros[index]) + "<br>";
      }
    }
  }

  server.send(200, "text/plain", html);
}

// Adiciona mensagens no monitor correto
void adicionarAoMonitor(const String &mensagem)
{
  if (mensagem.startsWith("Erro") || mensagem.startsWith("Inf"))
  {
    if (logIndexErrosInf >= 0 && logIndexErrosInf < MAX_LOGS)
    {
      strncpy(logsErrosInf[logIndexErrosInf], mensagem.c_str(), sizeof(logsErrosInf[logIndexErrosInf]) - 1);
      logsErrosInf[logIndexErrosInf][sizeof(logsErrosInf[logIndexErrosInf]) - 1] = '\0';
      logIndexErrosInf = (logIndexErrosInf + 1) % MAX_LOGS;
    }
    else
    {
      Serial.println("Erro - Índice de log de erros fora dos limites");
      adicionarAoMonitor("Erro - Índice de log de erros fora dos limites");
    }
  }
  else
  {
    if (logIndexOutros >= 0 && logIndexOutros < MAX_LOGS)
    {
      strncpy(logsOutros[logIndexOutros], mensagem.c_str(), sizeof(logsOutros[logIndexOutros]) - 1);
      logsOutros[logIndexOutros][sizeof(logsOutros[logIndexOutros]) - 1] = '\0';
      logIndexOutros = (logIndexOutros + 1) % MAX_LOGS;
    }
    else
    {
      Serial.println("Erro - Índice de log de outros fora dos limites");
      adicionarAoMonitor("Erro - Índice de log de outros fora dos limites");
    }
  }
}


// Endpoint para salvar configurações
void handleSalvarConfiguracoes()
{
  if (salvarConfiguracoes())
    server.send(200, "text/plain", "Configurações salvas com sucesso.");
}

// Página de configuração de Sensor
void paginaDebug()
{
  html = "<html><body><h2>Modo Debug</h2>";
  html += "<form action='/salvarDebug' method='POST'>";
  html += "<label><input type='checkbox' name='debugSerie' value='1' " + String(debugSerie ? "checked" : "") + "> Debug Serie</label><br>";
  html += "<label><input type='checkbox' name='debugMonitorGeral' value='1' " + String(debugMonitorGeral ? "checked" : "") + "> Debug Monitor Geral</label><br>";
  html += "<label><input type='checkbox' name='debugMonitorLoRa' value='1' " + String(debugMonitorLoRa ? "checked" : "") + "> Debug Monitor LoRa</label><br>";
  html += "<label><input type='checkbox' name='TirarFotos' value='1' " + String(tirarFoto ? "checked" : "") + "> Tirar Fotos periodicas</label><br>";
  html += "<button type='submit'>Salvar Configuracao</button>";
  html += "<button type='button' style='margin-left: 10px;' onclick=\"window.location.href='/'\">Voltar</button>";
  html += "</form><br>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void salvarDebug()
{
  if (server.hasArg("debugSerie"))
    debugSerie = server.arg("debugSerie").toInt();
  else
    debugSerie = 0;

  if (server.hasArg("debugMonitorGeral"))
    debugMonitorGeral = server.arg("debugMonitorGeral").toInt();
  else
    debugMonitorGeral = 0;

  if (server.hasArg("debugMonitorLoRa"))
    debugMonitorLoRa = server.arg("debugMonitorLoRa").toInt();
  else
    debugMonitorLoRa = 0;

  if (server.hasArg("TirarFotos"))
  {
    tirarFoto = server.arg("TirarFotos").toInt();
    digitalWrite(pinInterrup, LOW);
    novaImagem = true;
    Serial.println("Tirou foto");
  }
  else
    tirarFoto = 0;

  if (debugSerie)
    Serial.begin(115200);
  else
    Serial.end();

  Serial.println("Debug Serie: " + String(debugSerie));
  Serial.println("Debug Monitor Geral: " + String(debugMonitorGeral));
  Serial.println("Debug Monitor LoRa: " + String(debugMonitorLoRa));
  Serial.println("Tirar fotos: " + String(tirarFoto));

  html = "<html><body><h2>Modo Debug</h2>";
  html += "<form action='/salvarDebug' method='POST'>";
  html += "<label><input type='checkbox' name='debugSerie' value='1' " + String(debugSerie ? "checked" : "") + "> Debug Serie</label><br>";
  html += "<label><input type='checkbox' name='debugMonitorGeral' value='1' " + String(debugMonitorGeral ? "checked" : "") + "> Debug Monitor Geral</label><br>";
  html += "<label><input type='checkbox' name='debugMonitorLoRa' value='1' " + String(debugMonitorLoRa ? "checked" : "") + "> Debug Monitor LoRa</label><br>";
  html += "<label><input type='checkbox' name='TirarFotos' value='1' " + String(tirarFoto ? "checked" : "") + "> Tirar Fotos periodicas</label><br>";
  html += "<button type='submit'>Salvar Configuracao</button>";
  html += "<button type='button' style='margin-left: 10px;' onclick=\"window.location.href='/'\">Voltar</button>";
  html += "</form><br>";
  html += "<div id='mensagem' style='margin-top: 10px; padding: 10px; background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; border-radius: 5px;'>";
  html += "Configuracao salva com sucesso!";
  html += "</div>";
  html += "<script>";
  html += "setTimeout(function() { document.getElementById('mensagem').style.display = 'none'; }, 5000);";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Função para exibir a página de configuração de intervalo e amostragem
void configurarIntervalo()
{
  // Página HTML gerada com a interface interativa para configurar o intervalo e amostragem
  html = R"rawliteral(
  <html>
    <head>
      <title>Configurar Intervalo e Amostragem</title>
      <script>
        // Função para exibir mensagens de feedback (sucesso ou erro)
        function mostrarMensagem(mensagem, sucesso = true) {
          const msgDiv = document.getElementById('mensagem');
          msgDiv.innerHTML = mensagem;
          msgDiv.style.display = 'block';
          msgDiv.style.backgroundColor = sucesso ? '#d4edda' : '#f8d7da';
          msgDiv.style.color = sucesso ? '#155724' : '#721c24';
          msgDiv.style.border = sucesso ? '1px solid #c3e6cb' : '1px solid #f5c6cb';
          setTimeout(() => { msgDiv.style.display = 'none'; }, 3000); 
        }

        // Função para salvar o intervalo via formulário
        async function salvarIntervalo(event) {
          event.preventDefault(); 
          const formData = new FormData(document.getElementById('intervaloForm'));
          const response = await fetch('/salvarIntervalo', {
            method: 'POST',
            body: formData
          });
          if (response.ok) {
            mostrarMensagem('Intervalo salvo com sucesso!');
          } else {
            mostrarMensagem('Erro ao salvar o intervalo!', false);
          }
        }
      </script>
      <style>
        body { font-family: Arial, sans-serif; }
        #mensagem { 
          display: none; 
          margin-top: 10px; 
          padding: 10px; 
          border-radius: 5px; 
        }
      </style>
    </head>
    <body>
      <h2>Configurar Intervalo de Tempo e Amostragem</h2>
      <form id="intervaloForm" onsubmit="salvarIntervalo(event)">
        <label>Intervalo em Minutos (1 a 60): </label>
        <input type="number" name="intervalo" min="1" max="60" value=")rawliteral" +
         String(intervaloTempo) + R"rawliteral(" required>
        <br><br>
        <label>Intervalo de Amostragem em Segundos (1 a 10000): </label>
        <input type="number" name="intervaloAmostragem" min="1" max="10000" value=")rawliteral" +
         String(intervaloAmostragem) + R"rawliteral(" required>
        <br><br>
        <label>Amostragem Ativada: </label>
        <input type="checkbox" name="amostragem" )rawliteral" +
         (amostragem ? "checked" : "") + R"rawliteral(>
        <br><br>
        <input type="submit" value="Salvar Intervalo">
        <button type="button" onclick="window.location.href='/'">Voltar</button>
      </form>
      <div id="mensagem"></div>
    </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html); 
}

// Função para salvar o novo intervalo de tempo e amostragem
void salvarIntervalo()
{
  // Verifica se o formulário possui os argumentos "intervalo" e "intervaloAmostragem"
  if (server.hasArg("intervalo") && server.hasArg("intervaloAmostragem"))
  {
    int novoIntervalo = server.arg("intervalo").toInt();                     
    int novoIntervaloAmostragem = server.arg("intervaloAmostragem").toInt(); 
    intervaloTempo = novoIntervalo;                                          
    intervaloAmostragem = novoIntervaloAmostragem;                           
    amostragem = server.hasArg("amostragem");                                
    Serial.println("Intervalo de tempo salvo: " + String(intervaloTempo) + " minutos");
    Serial.println("Intervalo de amostragem salvo: " + String(intervaloAmostragem) + " segundos");
    Serial.println("Amostragem ativada: " + String(amostragem ? "Sim" : "Não"));
    server.send(200, "text/plain", "OK"); 
  }
  else
  {
    server.send(400, "text/plain", "Erro: Valores de intervalo ou amostragem não fornecidos.");
  }
}

int num = 0;

void controlo()
{
  if (primeiraVez)
  {
    strncpy(registosSensor, getDateTimeString().c_str(), sizeof(registosSensor) - 1);
    registosSensor[sizeof(registosSensor) - 1] = '\0'; 
    snprintf(dataLoRaHora, sizeof(dataLoRaHora), "%s", getDateTimeString().c_str());

    pulseCounter = 0;
    primeiraVez = 0;
  }
  else
  {
    // Garantir espaço antes de concatenar
    int spaceLeft = sizeof(registosSensor) - strnlen(registosSensor, sizeof(registosSensor)) - 1;
    if (spaceLeft > 10)
    {
      snprintf(registosSensor + strnlen(registosSensor, sizeof(registosSensor)), spaceLeft, " %d", pulseCounter);
    }
    else
    {
      adicionarAoMonitor("Erro - Espaço ins. para na var registosSensor 1");
    }

    guardarNoSDSensor();
    num++;
    // Formatar mensagem de monitor de forma eficiente

    spaceLeft = sizeof(registosSensor) - strnlen(registosSensor, sizeof(registosSensor)) - 1;
    if (spaceLeft > 30)
    {
      strncat(registosSensor, getDateTimeString().c_str(), sizeof(registosSensor) - strnlen(registosSensor, sizeof(registosSensor)) - 1);
      registosSensor[sizeof(registosSensor) - 1] = '\0';
    }
    else
    {
      adicionarAoMonitor("Erro - Espaço ins. para na var registosSensor 2");
    }

    valoresLora++;
    somaValoresLora += pulseCounter;
    strncat(dataLoRa, (" " + String(pulseCounter)).c_str(), sizeof(dataLoRa) - strlen(dataLoRa) - 1);

    if (valoresLora >= nAmostras)
    {
      if (storageUsado <= MAX_BACKUPS)
      {
        if (snprintf(dataLoRaMensagem[storageUsado], TAM_BACKUP, "%s %d %d%s %d", dataLoRaHora, valoresLora, intervaloTempo, dataLoRa, somaValoresLora) > TAM_BACKUP - 10)
        {
          valoresLora = 0;
          somaValoresLora = 0;
          memset(dataLoRa, 0, sizeof(dataLoRa));
          strcpy(dataLoRaHora, dataLoRaHora2);
          storageUsado++;

          if (debugMonitorLoRa)
          {
            adicionarAoMonitor("Inf - Backup " + String(storageUsado) + " a ser usado");
          }
        }
      }
      else
      {
        if (debugMonitorLoRa)
          adicionarAoMonitor("Inf - Backups esgotados");
      }

      enviarMensagem(dataLoRaMensagem[0]);

      if (debugMonitorLoRa)
        adicionarAoMonitor("Inf - " + String(dataLoRaMensagem[0]));

      snprintf(dataLoRaHora2, sizeof(dataLoRaHora2), "%s", getDateTimeString().c_str());

      mensagemEnviadaLora = 1;
      tempoRespostaLoRa = millis();
    }

    pulseCounter = 0;
  }
}

// Função para listar diretórios do cartão SD
String listarDiretorios(const char *path)
{
  String html = "";
  // Abre o diretório atual
  File root = SD.open(path); 
  if (!root || !root.isDirectory())
  {
    adicionarAoMonitor("Erro - Não foi possível abrir os diretórios do SD");
    return "<p>Erro ao abrir o diretório</p>";
  }

  File file = root.openNextFile();
  while (file)
  {
    // Concatena o caminho base com o nome do arquivo/diretório
    String fullPath = String(path);
    if (!fullPath.endsWith("/"))
      fullPath += "/";       
    fullPath += file.name(); 

    if (file.isDirectory())
    {
      html += "<li><a href=\"/cartaoSD?path=" + fullPath + "\">📁 " + String(file.name()) + "</a></li>";
    }
    else
    {
      html += "<li>📄 " + String(file.name()) +
              " (" + String(file.size()) + " bytes) " +
              "<a href=\"/abrirArquivo?path=" + fullPath + "\" target=\"_blank\">[Abrir]</a> " +
              "<a href=\"/baixarArquivo?path=" + fullPath + "\">[Download]</a></li>";
    }
    file = root.openNextFile();
  }
  return html;
}

// Função para renderizar a página do cartão SD
void cartaoSD()
{
  iniciarSD();
  SD_ocupado = 1;

  // Obtém o diretório especificado no parâmetro "path"
  String path = "/";
  if (server.hasArg("path"))
  {
    path = server.arg("path");
  }

  // Monta o HTML da página
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Cartão SD</title>
      <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f4f4f9; }
        h1 { color: #333; }
        a { text-decoration: none; color: #007bff; }
        ul { list-style-type: none; padding: 0; }
        li { margin: 5px 0; }
        .back { margin-bottom: 20px; }
        button { padding: 10px 20px; background-color: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; }
        button:hover { background-color: #0056b3; }
      </style>
      <script>
        // Função para desligar o SD e voltar ao menu principal
        function voltarAoMenu() {
          fetch('/desligarSD').then(() => {
            window.location.href = '/';
          });
        }
      </script>
    </head>
    <body>
      <h1>Explorador do Cartão SD</h1>
      <div class="back">
        <button onclick="voltarAoMenu()">Voltar ao Menu Principal</button>
        <a href="/cartaoSD?path=/"><button>Início</button></a>
      </div>
      <ul>
  )rawliteral";

  // Adiciona a lista de arquivos e diretórios ao HTML
  html += listarDiretorios(path.c_str());

  // Fecha o HTML
  html += R"rawliteral(
      </ul>
    </body>
    </html>
  )rawliteral";

  // Envia a resposta HTML para o cliente
  server.send(200, "text/html", html);
}

// Adiciona um endpoint para desligar o SD
void handleDesligarSD()
{
  SD_ocupado = 0;
  desligarSD();
  server.send(200, "text/plain", "Cartão SD desligado");
}

// Função para abrir um arquivo no navegador
void abrirArquivo()
{
  if (!server.hasArg("path"))
  {
    server.send(400, "text/plain", "Caminho do arquivo nao especificado");
    adicionarAoMonitor("Erro - Caminho do arquivo para ver não encontrado");
    return;
  }

  String path = server.arg("path");
  File file = SD.open(path);
  if (!file)
  {
    server.send(404, "text/plain", "Arquivo nao encontrado");
    adicionarAoMonitor("Erro - Arquivo para abrir não encontrado");
    return;
  }

  // Determina o tipo de conteúdo do arquivo (apenas exemplos comuns)
  String contentType = "text/plain";
  if (path.endsWith(".htm") || path.endsWith(".html"))
    contentType = "text/html";
  else if (path.endsWith(".jpg"))
    contentType = "image/jpeg";
  else if (path.endsWith(".png"))
    contentType = "image/png";
  else if (path.endsWith(".gif"))
    contentType = "image/gif";
  else if (path.endsWith(".css"))
    contentType = "text/css";
  else if (path.endsWith(".js"))
    contentType = "application/javascript";

  server.streamFile(file, contentType);
  file.close();
}

// Função para baixar um arquivo
void baixarArquivo()
{
  if (!server.hasArg("path"))
  {
    server.send(400, "text/plain", "Caminho do arquivo nao especificado");
    adicionarAoMonitor("Erro - Caminho do arquivo para baixar não encontrado");
    return;
  }

  String path = server.arg("path");
  File file = SD.open(path);
  if (!file)
  {
    server.send(404, "text/plain", "Arquivo nao encontrado");
    adicionarAoMonitor("Erro - Arquivo para baixar não encontrado");
    return;
  }

  server.sendHeader("Content-Disposition", "attachment; filename=" + String(file.name()));
  server.streamFile(file, "application/octet-stream");
  file.close();
}

// Função para verificar autenticação
bool verificarAutenticacao()
{
  if (!server.authenticate(authUsername, authPassword))
  {
    server.requestAuthentication(); 
    return false;
  }
  return true;
}

// Configuração inicial do servidor
void configurarServidor()
{
  server.on("/", []()
            {
    if (verificarAutenticacao()) {
      paginaInicial();
    } });

  server.on("/debug", []()
            {
    if (verificarAutenticacao()) {
      paginaDebug();
    } });

  server.on("/salvarDebug", []()
            {
    if (verificarAutenticacao()) {
        salvarDebug();
    } });

  server.on("/configurarIntervalo", []()
            {
    if (verificarAutenticacao()) {
      configurarIntervalo();
    } });

  server.on("/salvarIntervalo", HTTP_POST, []()
            {
    if (verificarAutenticacao()) {
      salvarIntervalo();
    } });

  server.on("/configurarWiFi", []()
            {
    if (verificarAutenticacao()) {
      configurarWiFi();
    } });

  server.on("/configurarLoRa", []()
            {
    if (verificarAutenticacao()) {
      configurarLoRa();
    } });

  server.on("/salvarWiFi", HTTP_POST, []()
            {
    if (verificarAutenticacao()) {
      salvarWiFi();
    } });

  server.on("/salvarLoRa", HTTP_POST, []()
            {
    if (verificarAutenticacao()) {
      salvarLoRa();
    } });

  server.on("/cartaoSD", HTTP_GET, []()
            {
    if (verificarAutenticacao()) { 
      cartaoSD(); 
    } else {
      server.send(401, "text/plain", "Não autorizado");
    } });

  server.on("/abrirArquivo", HTTP_GET, []()
            {
    if (verificarAutenticacao()) {
      abrirArquivo();
    } });

  server.on("/baixarArquivo", HTTP_GET, []()
            {
    if (verificarAutenticacao()) {
      baixarArquivo();
    } });

  server.on("/salvarConfiguracoes", HTTP_GET, []()
            {
    if (verificarAutenticacao()) {
      handleSalvarConfiguracoes();
    } });

  server.on("/desligarSD", HTTP_GET, []()
            {
    if (verificarAutenticacao()) {
      handleDesligarSD();
    } });

  server.on("/obterMonitor", HTTP_GET, []()
            {
    if (verificarAutenticacao()) {
      handleObterMonitor();
    } });

  server.begin();
}

void verificarHora()
{
  existeHora = 0;
  if (rtc.begin())
  {
    if (rtc.lostPower())
    {
      Serial.println("RTC perdeu a energia, configurando a hora...");
      iniciarSD();
      File file = SD.open("/ContagensSensor.txt", FILE_READ);
      if (!file)
      {
        Serial.println("Erro ao abrir 'ContagensSensor.txt'");
        adicionarAoMonitor("Erro - Falha ao abrir ContagensSensor.txt");
        desligarSD();
        return;
      }

      String lastLine;
      while (file.available())
      {
        lastLine = file.readStringUntil('\n');
      }
      file.close();

      if (lastLine.length() > 0)
      {
        int year, month, day, hour, minute, second;
        if (sscanf(lastLine.c_str(), "%d/%d/%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6)
        {
          rtc.adjust(DateTime(year, month, day, hour, minute, second));
          Serial.println("Hora ajustada a partir do arquivo: " + lastLine);
          if (debugMonitorGeral)
            adicionarAoMonitor("Inf - Hora ajustada a partir do arquivo: " + lastLine);
          existeHora = 1;
          minutosAnt = rtc.now().minute();
        }
        else
        {
          Serial.println("Erro ao analisar a linha: " + lastLine);
          adicionarAoMonitor("Erro - Falha ao analisar a linha: " + lastLine);
        }
      }
      else
      {
        Serial.println("Arquivo 'ContagensSensor.txt' está vazio");
        adicionarAoMonitor("Erro - Arquivo 'ContagensSensor.txt' está vazio");
      }
      desligarSD();
    }
    else
    {
      existeHora = 1;
      minutosAnt = rtc.now().minute();
    }
  }
  else
  {
    Serial.println("Erro ao inicializar o RTC (verificarHora)");
    adicionarAoMonitor("Erro - Falha ao inicializar o RTC (verificarHora)");
    tempoDecorrido = millis();
  }
  Serial.println("Existe hora: " + String(existeHora));
}

void controlomemoria()
{
  // Coleta informações de memória
  size_t ramLivre = ESP.getFreeHeap();
  size_t maiorBloco = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  // Monta a string
  char infoMemoria[100]; 
  snprintf(infoMemoria, sizeof(infoMemoria), "Inf - Ram: %u | Bl. seg.: %u", ramLivre, maiorBloco);

  if (debugMonitorGeral)
    adicionarAoMonitor(infoMemoria);
}

void fusoHorario(DateTime now)
{
  // 0 = Domingo, 1 = Segunda, ..., 6 = Sábado
  int diaSemana = now.dayOfTheWeek(); 
  int diaMes = now.day();
  static int mesAnterior;

  if (now.month() == 10 && mesAnterior != 10)
  {
    if (diaMes >= 25 && diaSemana == 0)
    {
      if (now.hour() == 2)
      {
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), 1, now.minute(), now.second()));
        // Serial.println("horario de inverno");
        mesAnterior = 10;
      }
    }
  }

  // Horário de verão começa no último domingo de março
  if (now.month() == 3 && mesAnterior != 3)
  {
    if (diaMes >= 25 && diaSemana == 0)
    {
      if (now.hour() == 1)
      {
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), 2, now.minute(), now.second()));
        // Serial.println("horario de verão");
        mesAnterior = 3;
      }
    }
  }
}

bool esperarInicioImagem()
{
  if (Serial.available())
  {
    while (Serial.available())
    {
      uint8_t c = Serial.read();
      if (c == 0xA5)
      {
        Serial.println("Marcador de início detectado.");
        return true;
      }
    }
  }
  else
  {
    return false;
  }
}

uint32_t lerTamanhoImagem()
{
  uint8_t sizeBytes[4];
  int bytesLidos = 0;

  while (bytesLidos < 4)
  {
    if (Serial.available())
    {
      sizeBytes[bytesLidos] = Serial.read();
      Serial.printf("Byte %d do tamanho: 0x%02X\n", bytesLidos, sizeBytes[bytesLidos]);
      bytesLidos++;
    }
  }

  uint32_t tamanho =
      ((uint32_t)sizeBytes[0]) |
      ((uint32_t)sizeBytes[1] << 8) |
      ((uint32_t)sizeBytes[2] << 16) |
      ((uint32_t)sizeBytes[3] << 24);

  Serial.printf("Tamanho da imagem: %lu bytes\n", tamanho);
  return tamanho;
}

bool receberImagem(uint32_t tamanho)
{
  uint32_t recebidos = 0;

  while (recebidos < tamanho)
  {
    if (Serial.available())
    {
      imageBuffer[recebidos++] = Serial.read();
    }
  }

  Serial.println("Imagem recebida.");
  return true;
}

bool salvarImagemNoSD(uint32_t imageSize)
{
  // Obtém a data/hora formatada corretamente
  String data_original = getDateTimeString(); 
  data_original.replace("/", "");
  data_original.replace(" ", "_");
  data_original.replace(":", "");
  String data_formatada = data_original; 

  // Garante que a pasta "imagens" exista
  if (!SD.exists("/imagens"))
  {
    if (!SD.mkdir("/imagens"))
    {
      Serial.println("Falha ao criar pasta /imagens.");
      return false;
    }
  }

  // Define o nome do arquivo dentro da pasta
  String fileName = "/imagens/img_" + data_formatada + ".jpg";

  // Abre o arquivo para escrita
  File imgFile = SD.open(fileName.c_str(), FILE_WRITE);
  if (!imgFile)
  {
    Serial.println("Falha ao abrir arquivo para escrita.");
    return false;
  }

  // Escreve a imagem no arquivo
  for (uint32_t i = 0; i < imageSize; i++)
  {
    imgFile.write(imageBuffer[i]);
  }

  imgFile.close(); 
  return true;
}

void setup()
{
  // Reserva 10000 bytes para evitar realocações
  html.reserve(10000); 
  // Inicializando o SPI
  SPI.begin();
  // Configurando os pinos CS como saída
  pinMode(SD_CS_PIN, OUTPUT);
  pinMode(LORA_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(LORA_CS_PIN, HIGH);
  // Tentativa de iniciar o SD
  iniciarSD();
  desligarSD();
  // Carregar configurações
  carregarConfiguracoes();
  if (debugSerie)
    Serial.begin(115200);
  else
    Serial.end();
  // Conectar ao WiFi
  conectarWiFi();
  // Configurar servidor web
  configurarServidor();
  // Configurar pino do sensor
  pinMode(pinSensor, INPUT); 
  pinMode(pinInterrup, OUTPUT);
  digitalWrite(pinInterrup, HIGH);
  Serial.println("Configuração do lora");
  if (setupLora(dispositioID, frequenciaLora, potenciaLora) && debugMonitorLoRa)
  {
    adicionarAoMonitor("Inf - Lora iniciado com sucesso");
    adicionarAoMonitor("Inf - ID: " + String(dispositioID) + " | Freq: " + String(frequenciaLora) + " | Pot: " + String(potenciaLora));
  }
  pinMode(21, INPUT);
  delay(100);
  Wire.begin(23, 22); 
  delay(100);

  // Verificar RTC ou horário
  verificarHora(); 

  // Configura o Watchdog Timer (WDT)
  esp_task_wdt_init(30, true); 
  esp_task_wdt_add(NULL);     

  // Desliga todas as interfaces e periféricos desnecessários
  WiFi.disconnect(true); 
  WiFi.mode(WIFI_OFF);   
  btStop();             

  conectarHotspot();

  adicionarAoMonitor("Inf - Sistema iniciado com sucesso");
  adicionarAoMonitor("Inf - Hora: " + getDateTimeString());
}

// Loop principal
void loop()
{
  unsigned long tempoAtual = millis();

  if (WiFi.softAPgetStationNum() > 0 || tempoAtual - tempoDecorridoServidor > 5000) 
  {
     esp_task_wdt_reset(); 
    server.handleClient();
    tempoDecorridoServidor = tempoAtual;
  }

  // Reseta o watchdog timer
  esp_task_wdt_reset(); 

  // Verifica se não há clientes conectados ao WiFi e se o SD está ocupado
  if (WiFi.softAPgetStationNum() == 0 && SD_ocupado == 1)
  {
    SD_ocupado = 0;
    Serial.println("Nenhum cliente conectado. SD_ocupado definido para 0.");
  }

  if (tempoAtual - tempoDecorridoContagem >= 1000)
  {
    if (!existeHora)
    {
      if (tempoAtual - tempoDecorrido >= intervaloTempo * 60000)
      {
        if (contMem > 50)
        {
          controlomemoria();
          contMem = 0;
          delay(100);
        }
        contMem++;
        controlo();

        tempoDecorrido = tempoAtual;
      }
    }
    else
    {
      if (rtc.begin() && !rtc.lostPower())
      {
        DateTime now = rtc.now();
        fusoHorario(now);
        if ((now.minute() % intervaloTempo == 0) && minutosAnt != now.minute())
        {
          if (contMem > 50)
          {
            controlomemoria();
            contMem = 0;
          }
          contMem++;
          controlo();
          minutosAnt = now.minute();
        }

        if (tirarFoto)
        {
          if ((now.hour() == 3 || now.hour() == 11) && horaAnt != now.hour())
          {
            digitalWrite(pinInterrup, LOW); 
            novaImagem = true;              
          }
          horaAnt = now.hour();
        }
      }
      else
      {
        Serial.println("Erro ao inicializar o RTC");
        adicionarAoMonitor("Erro - Falha ao inicializar o RTC");
        verificarHora();
      }
    }

    tempoDecorridoContagem = tempoAtual;
  }

  if (amostragem)
  {
    if (tempoAtual - tempoDecorridoAmostragem >= intervaloAmostragem * 1000)
    {
      adicionarAoMonitor("Consumo: " + String(pulseCounterAmosrtragem));
      pulseCounterAmosrtragem = 0;
      tempoDecorridoAmostragem = tempoAtual;
    }
  }

  if (digitalRead(pinSensor) == 0 && estadoAnterior && (tempoAtual - lastInterruptTime) > 10)
  {
    pulseCounter++;
    if (amostragem)
    {
      pulseCounterAmosrtragem++;
    }
    estadoAnterior = 0;
    lastInterruptTime = tempoAtual;
  }

  if (digitalRead(pinSensor) == 1)
  {
    estadoAnterior = 1;
  }

  if (mensagemEnviadaLora)
  {
    if ((millis() - tempoRespostaLoRa) < 20000)
    {
      if (haPacotes())
      {
        uint16_t confirmacao = recebermensagem();
        Serial.println("ACK recebido: " + String(confirmacao));
        if (debugMonitorLoRa)
          adicionarAoMonitor("Inf - ACK recebido: " + String(confirmacao));
        mensagemEnviadaLora = 0;

        if (validarACK(confirmacao))
        {
          Serial.println("Mensagem LoRa enviada sem distorção");
          if (storageUsado > 0)
          {
            for (int i = 0; i < storageUsado - 1; i++)
            {
              strcpy(dataLoRaMensagem[i], dataLoRaMensagem[i + 1]);
            }

            storageUsado--;
          }
          else
          {
            valoresLora = 0;
            somaValoresLora = 0;
            memset(dataLoRa, 0, sizeof(dataLoRa));
            strcpy(dataLoRaHora, dataLoRaHora2);
          }
          if (debugMonitorLoRa)
            adicionarAoMonitor("Inf - Mensagem LoRa enviada sem distorção");
        }
        else
        {
          Serial.println("Mensagem LoRa rejeitada");
          if (debugMonitorLoRa)
          {
            adicionarAoMonitor("Erro - Mensagem LoRa rejeitada");
          }
        }
      }
    }
    else
    {
      mensagemEnviadaLora = 0;
      Serial.println("Tempo de espera pelo ACK acabou");
      if (debugMonitorLoRa)
      {
        adicionarAoMonitor("Erro - Tempo de espera pelo ACK acabou");
      }
    }
  }

  if (novaImagem)
  {
    if (esperarInicioImagem())
    {
      esp_task_wdt_reset();
      digitalWrite(pinInterrup, HIGH); 
      uint32_t imageSize = lerTamanhoImagem();
      if (imageSize > 0 && imageSize < MAX_IMAGE_SIZE)
      {
        if (receberImagem(imageSize))
        {
          Serial.printf("Imagem recebida com sucesso! (%lu bytes)\n", imageSize);
          novaImagem = false;

          while (Serial.available() > 0)
          {
            Serial.read(); 
          }

          if (SD_ocupado == 1)
          {
            Serial.println("SD ocupado, não salvando imagem.");
          }
          else
          {
            SD_ocupado = 1;
            if (salvarImagemNoSD(imageSize))
            {
              Serial.println("Imagem salva com sucesso no SD.");
            }
            else
            {
              Serial.println("Falha ao salvar a imagem no SD.");
            }
            SD_ocupado = 0;
          }
        }
        else
        {
          Serial.println("Erro ao receber a imagem.");
        }
      }
      else
      {
        Serial.println("Tamanho inválido.");
      }
    }
  }

  delay(1);
}
