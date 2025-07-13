#ifndef LORAWAN_H
#define LORWAN_H

#include <Arduino.h>

bool setupLora(int ID, int frequencia, int potencia);
void enviarMensagem(const char* mensagem);
uint16_t recebermensagem();
bool haPacotes();
bool validarACK(uint16_t ack);

#endif