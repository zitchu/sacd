#ifndef __NEOPIXEL_INC
#define __NEOPIXEL_INC

#include <stdlib.h>
#include "ws2818b.pio.h"

// Definição de pixel GRB
struct pixel_t {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.


// Declaração do buffer de pixels que formam a matriz.
static npLED_t *leds;
static uint led_count;

// Variáveis para uso da máquina PIO.
static PIO np_pio;
static uint np_sm;

/**
 * Inicializa a máquina PIO para controle da matriz de LEDs.
 */
void npInit(uint pin, uint amount) {

  led_count = amount;
  leds = (npLED_t *)calloc(led_count, sizeof(npLED_t));

  // Cria programa PIO.
  uint offset = pio_add_program(pio0, &ws2818b_program);
  np_pio = pio0;

  // Toma posse de uma máquina PIO.
  np_sm = pio_claim_unused_sm(np_pio, false);
  if (np_sm < 0) {
    np_pio = pio1;
    np_sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
  }

  // Inicia programa na máquina PIO obtida.
  ws2818b_program_init(np_pio, np_sm, offset, pin, 800000.f);

  // Limpa buffer de pixels.
  for (uint i = 0; i < led_count; ++i) {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}

/**
 * Atribui uma cor RGB a um LED.
 */
void npSetLED(const uint index, uint8_t r, uint8_t g, uint8_t b) {
  
  r = ((r & 0xF0) >> 4) | ((r & 0x0F) << 4); // Troca os 4 MSB com os 4 LSB
  r = ((r & 0xCC) >> 2) | ((r & 0x33) << 2); // Troca pares de bits
  r = ((r & 0xAA) >> 1) | ((r & 0x55) << 1); // Troca bits individuais

  g = ((g & 0xF0) >> 4) | ((g & 0x0F) << 4); // Troca os 4 MSB com os 4 LSB
  g = ((g & 0xCC) >> 2) | ((g & 0x33) << 2); // Troca pares de bits
  g = ((g & 0xAA) >> 1) | ((g & 0x55) << 1); // Troca bits individuais

  b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4); // Troca os 4 MSB com os 4 LSB
  b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2); // Troca pares de bits
  b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1); // Troca bits individuais

  
  leds[index].R = r;
  leds[index].G = g;
  leds[index].B = b;
}

/**
 * Limpa o buffer de pixels.
 */
void npClear() {
  for (uint i = 0; i < led_count; ++i)
    npSetLED(i, 0, 0, 0);
}

/**
 * Escreve os dados do buffer nos LEDs.
 */
void npWrite() {
  // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
  for (uint i = 0; i < led_count; ++i) {
    pio_sm_put_blocking(np_pio, np_sm, leds[i].G);
    pio_sm_put_blocking(np_pio, np_sm, leds[i].R);
    pio_sm_put_blocking(np_pio, np_sm, leds[i].B);
  }
  sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

#endif