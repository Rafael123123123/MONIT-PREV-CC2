// Inclui a biblioteca SPI para comunicação com dispositivos SPI
#include <SPI.h>
// Inclui a biblioteca SD para manipulação de cartão SD
#include <SD.h>
// Inclui a biblioteca Wire para comunicação I2C
#include <Wire.h>
// Inclui a biblioteca RTClib para o módulo RTC (Relógio de Tempo Real)
#include <RTClib.h>
// Inclui a biblioteca LowPower para gerenciamento de energia
#include <LowPower.h>  
// Cria uma instância do objeto RTC_DS3231 para o relógio
RTC_DS3231 rtc;

// Define o intervalo de tempo (em minutos) para escrita do consumo
#define intervaleTime 5  
// Variável para armazenar o tempo anterior
unsigned long segundosAnt = 0;
// Variável para armazenar o tempo atual
unsigned long segundosAtu;
// Flag para indicar se é a primeira execução
boolean primeiraVez = 1;
// Variável para armazenar o minuto anterior
int minutosAnt = 90;

// Contador de pulsos
int pulseCount = 0;  
// Define o pino de interrupção 
#define interruptPin 2

// Define o pino de seleção do cartão SD
const int chipSelect = 10; 
// Objeto para manipulação de arquivos no cartão SD
File dataFile;
// Define o nome do arquivo de log
#define logFile "C2.txt"
// Define o tamanho do buffer para armazenamento temporário
#define tamanhoBuffer 6

// Variável para armazenar a quantidade de água atual
int aguaAtual;
// Variável para armazenar a quantidade acumulada de água
double aguaAcumulada = 0;

// Flag para controlar quando salvar valores
bool guardarValor = false;
// Índice para o buffer de salvamento
int indiceSave = 0;
// Array para armazenar dados temporariamente antes de salvar no SD
char arraySave[tamanhoBuffer][30];  
// Buffer para formatação de strings
char buf[50];

// Declaração da função para salvar dados no buffer
void salvarNoBuffer(char data[]);
// Declaração da função para salvar dados no cartão SD
void salvarNoSD();
// Declaração da função para contar pulsos
void countPulse();
// Declaração da função de controle principal
void controlo();

// Função de inicialização
void setup() {
  // Inicia a comunicação serial com baud rate de 9600
  Serial.begin(9600);
  // Configura os pinos como saída e desligados
  pinMode(13, OUTPUT);  
  pinMode(14, OUTPUT);  
  digitalWrite(13, LOW);  
  digitalWrite(14, LOW);  

  // Configura o pino 2 como entrada com pull-up interno
  pinMode(2, INPUT_PULLUP);  
  // Configura uma interrupção no pino 2, chamando countPulse no evento FALLING
  attachInterrupt(digitalPinToInterrupt(2), countPulse, FALLING);  

  // Configura o pino 3 e 4 como entradas com pull-up interno
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);

  // Tenta inicializar o cartão SD
  if (!SD.begin(chipSelect)) {
    Serial.println("Erro ao inicializar o cartão SD.");
  } else {
    Serial.println("Cartão SD iniciado");
  }

  // Verifica se o RTC foi inicializado corretamente
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
  }

  // Abre (ou cria) o arquivo de log no cartão SD
  dataFile = SD.open(logFile, FILE_WRITE);
  // Verifica se o arquivo foi aberto com sucesso
  if (dataFile) {
    Serial.println("Arquivo aberto ou criado com sucesso.");
    // Move o cursor para o início do arquivo
    dataFile.seek(0);
    // Verifica se há dados disponíveis no arquivo
    if(dataFile.available()){
      // Variável para armazenar a última linha lida
      String linha = "";
      // Lê todas as linhas do arquivo
      while (dataFile.available()) {
        // Lê a linha atual até encontrar uma quebra de linha
        String linhaAtual = dataFile.readStringUntil('\n');
        // Armazena a linha atual
        linha = linhaAtual;
      }
      // Extrai o valor de água acumulada da última linha
      String valorAgua = linha.substring(linha.lastIndexOf(' ') + 1);
      // Converte o valor para float e armazena
      aguaAcumulada = valorAgua.toFloat();
    }
    // Fecha o arquivo
    dataFile.close();
  } else {
    Serial.print("Erro ao abrir/criar o arquivo ");
  }

  // Pisca o LED no pino 13 para indicar inicialização
  digitalWrite(13, 1);
  delay(500);
  digitalWrite(13, 0);
  delay(500);
  digitalWrite(13, 1);
  delay(500);
  digitalWrite(13, 0);
  Serial.println("Sucesso");
}

// Função principal que roda em loop
void loop() {
  // Obtém a data e hora atuais do RTC
  DateTime now = rtc.now(); 
  // Verifica se o pino 3 está em LOW (botão pressionado)
  if (digitalRead(3) == 0) {
    // Ajusta o RTC para uma data e hora específicas
    rtc.adjust(DateTime(2024, 10, 8, 22, 42, 0));
    Serial.println("3");
  }
  // Verifica se o minuto atual é múltiplo do intervalo e diferente do minuto anterior
  if ((now.minute() % intervaleTime == 0) && minutosAnt != now.minute()) {
    // Verifica se é o início de um novo minuto
    if (now.second() == 0) {
      // Atualiza o minuto anterior
      minutosAnt = now.minute();
      // Chama a função de controle
      controlo();
    }
  }
  // Aguarda 1 segundo antes da próxima iteração
  delay(1000);
  // Chama a função para ajustar o fuso horário
  fusoHorario(now);
}

// Função para ajustar o fuso horário com base em regras de horário de verão
bool fusoHorario(DateTime now) {
  // Obtém o dia da semana
  int diaSemana = now.dayOfTheWeek(); 
  // Obtém o dia do mês
  int diaMes = now.day();
  // Variável estática para armazenar o mês anterior
  static int mesAnterior;

  // Verifica se é outubro e o mês anterior não era outubro
  if (now.month() == 10 && mesAnterior != 10) {
    // Verifica se é o último domingo de outubro
    if (diaMes >= 25 && diaSemana == 0) {
      // Verifica se são 2 da manhã
      if (now.hour() == 2) {
        // Ajusta o RTC para 1 hora atrás (fim do horário de verão)
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), 1, now.minute(), now.second()));
        // Atualiza o mês anterior
        mesAnterior = 10;
      }
    }
  }

  // Verifica se é março e o mês anterior não era março
  if (now.month() == 3 && mesAnterior != 3) {
    // Verifica se é o último domingo de março
    if (diaMes >= 25 && diaSemana == 0) {
      // Verifica se é 1 da manhã
      if (now.hour() == 1) {
        // Ajusta o RTC para 1 hora à frente (início do horário de verão)
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), 2, now.minute(), now.second()));
        // Atualiza o mês anterior
        mesAnterior = 3;
      }
    }
  }
}

// Função para salvar dados no buffer temporário
void salvarNoBuffer(char data[]) {
  // Verifica se há espaço no buffer
  if (indiceSave < tamanhoBuffer) {
    // Copia os dados para o buffer
    strcpy(arraySave[indiceSave], data);
    // Incrementa o índice do buffer
    indiceSave++;
  } else {
    // Buffer cheio, não faz nada
  }
}

// Função para salvar dados no cartão SD
void salvarNoSD(char buf[]) {
  // Abre o arquivo de log no modo escrita
  dataFile = SD.open(logFile, FILE_WRITE);
  // Verifica se o arquivo foi aberto com sucesso
  if (dataFile) {
    // Verifica se há dados no buffer
    if (indiceSave > 0) {
      // Escreve todos os dados do buffer no arquivo
      for (int i = 0; i < indiceSave; i++) {
        dataFile.println(arraySave[i]);
      }
    }
    // Escreve o conteúdo do buffer atual no arquivo
    dataFile.println(buf);
    // Fecha o arquivo
    dataFile.close();
    // Reseta o índice do buffer
    indiceSave = 0;
  } else {
    Serial.println("Arquivo não foi aberto!");
    // Se não conseguiu abrir o arquivo, salva no buffer
    salvarNoBuffer(buf);
    // Tenta reinicializar o sistema
    setup();
  }
}

// Função para contar pulsos (interrupção)
void countPulse() {
  // Armazena o tempo do último pulso
  static unsigned long lastPulseTime = 0;
  // Obtém o tempo atual
  unsigned long currentTime = millis();

  // Verifica se passou tempo suficiente desde o último pulso (debouncing)
  if (currentTime - lastPulseTime > 300) { 
    // Incrementa o contador de pulsos
    pulseCount++;  
    // Atualiza o tempo do último pulso
    lastPulseTime = currentTime;
  }
}

// Função de controle principal
void controlo() {
  // Verifica se é a primeira execução
  if (primeiraVez) {
    // Reseta o contador de pulsos
    pulseCount = 0;
    // Obtém a data e hora atuais
    DateTime now = rtc.now(); 
    // Formata a data e hora no buffer
    sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    // Marca que não é mais a primeira vez
    primeiraVez = 0;
  } else {
    // Obtém a data e hora atuais
    DateTime now = rtc.now(); 

    // Armazena a quantidade atual de pulsos
    aguaAtual = pulseCount;
    // Calcula a água acumulada (em litros, assumindo 1000 pulsos = 1 litro)
    aguaAcumulada += aguaAtual/1000.0;
    // Reseta o contador de pulsos
    pulseCount = 0;
    // Buffer temporário para conversão de números
    char aguaStr[10];
    // Converte aguaAtual para string
    dtostrf(aguaAtual, 6, 0, aguaStr);
    // Adiciona um tab ao buffer
    strcat(buf, "\t");
    // Adiciona o valor de aguaAtual ao buffer
    strcat(buf, aguaStr);
    // Converte aguaAcumulada para string com 3 casas decimais
    dtostrf(aguaAcumulada, 6, 3, aguaStr);
    // Adiciona um tab ao buffer
    strcat(buf, "\t");
    // Adiciona o valor de aguaAcumulada ao buffer
    strcat(buf, aguaStr);

    // Salva os dados no cartão SD
    salvarNoSD(buf);
    // Formata a nova data e hora no buffer
    sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  }
}
