#include <SPI.h>
#include <LoRa.h>
#include <LoRaWan.h>

bool setupLora(int ID, int frequencia, int potencia)
{
// Pinos SPI e LoRa no TTGO LoRa32 v1.3
#define SCK 5   // GPIO 5
#define MISO 19 // GPIO 19
#define MOSI 27 // GPIO 27
#define SS 18   // GPIO 18
#define RST 14  // GPIO 14
#define DIO0 26 // GPIO 26

  Serial.println("Iniciando LoRa...");

  // Configura manualmente os pinos do SPI
  SPI.begin(SCK, MISO, MOSI, SS);

  LoRa.setPins(SS, RST, DIO0);
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

void enviarMensagem(const char *mensagem)
{
  digitalWrite(SS, LOW);
  Serial.print("Enviando mensagem: ");
  Serial.println(mensagem);

  // Envia a mensagem pelo LoRa
  LoRa.beginPacket();
  LoRa.print(mensagem);
  if (LoRa.endPacket() == 0)
  {
    Serial.println("Erro: Não foi possível enviar a mensagem.");
  }
  else
  {
    Serial.println("Mensagem enviada com sucesso!");
  }
  digitalWrite(SS, HIGH);
}

bool haPacotes()
{
  return LoRa.parsePacket() > 0;
}

uint16_t recebermensagem()
{
  uint16_t ack = 0;

  // Verifica se há pelo menos 2 bytes disponíveis
  if (LoRa.available() >= 2)
  {
    ack = LoRa.read() << 8;
    ack |= LoRa.read();

    Serial.print("ACK recebido: 0x");
    Serial.println(ack, HEX);
  }
  else
  {
    Serial.println("Nenhum ACK recebido ou dados insuficientes.");
  }

  return ack;
}

bool validarACK(uint16_t ack)
{
  int contadorBits1 = 0;

  for (int i = 0; i < 16; i++)
  {
    if (bitRead(ack, i) == 1)
    {
      contadorBits1++;
    }
  }

  // Considera válido se a maioria dos bits (>= 9) for 1
  return (contadorBits1 >= 9);
}
