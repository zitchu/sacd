// Includes .h
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Includes hardware/
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"

// Includes variados
#include "inc/ssd1306.h"
#include "ws2818b.pio.h"

// Includes pico
#include "pico/binary_info.h"
#include "pico/stdlib.h"

// Definição do Microfone no ADC.
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL)

// Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7

// Definição joystick e botões
#define JOYSTICK_VRY 26
#define JOYSTICK_VRX 27
#define JOYSTICK_SW 22
#define BUTTON_A 5
#define BUTTON_B 6

// Definição do display ssd oled
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;
// Parâmetros e macros do ADC.

#define ADC_CLOCK_DIV 96.f
#define SAMPLES 200 // Número de amostras que serão feitas do ADC.
#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f) // Ajuste do valor do ADC para Volts.
#define ADC_MAX 3.3f
#define ADC_STEP (3.3f/5.f) // Intervalos de volume do microfone.

#define abs(x) ((x < 0) ? (-x) : (x))


// Definição de pixel GRB
struct pixel_t {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

// Canal e configurações do DMA
uint dma_channel;
dma_channel_config dma_cfg;

// Buffer de amostras do ADC.
uint16_t adc_buffer[SAMPLES];

// Variáveis para o timer
uint8_t minutes = 0;
uint8_t seconds = 0;

// Declaração das matrizes de sprites
// Ponto vermelho
const int matriz1r[5][5][3] = {
  {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, 
  {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, 
  {{0, 0, 0}, {0, 0, 0}, {255, 0, 0}, {0, 0, 0}, {0, 0, 0}}, 
  {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, 
  {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
};

// Quadrado pequeno vermelho
const int matriz2r[5][5][3] = {
  {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, 
  {{0, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {0, 0, 0}}, 
  {{0, 0, 0}, {255, 0, 0}, {0, 0, 0}, {255, 0, 0}, {0, 0, 0}}, 
  {{0, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {0, 0, 0}}, 
  {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
};

// Quadrado grande vermelho
const int matriz3r[5][5][3] = {
    {{255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}}, 
    {{255, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 0, 0}}, 
    {{255, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 0, 0}}, 
    {{255, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 0, 0}}, 
    {{255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}}
};

// Check mark verde
const int matrizVv[5][5][3] = {
  {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, 
  {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}}, 
  {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}}, 
  {{0, 255, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}}, 
  {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
};

// Check mark amarela
const int matrizVa[5][5][3] = {
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, 
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 255, 0}}, 
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 255, 0}, {0, 0, 0}}, 
    {{255, 255, 0}, {0, 0, 0}, {255, 255, 0}, {0, 0, 0}, {0, 0, 0}}, 
    {{0, 0, 0}, {255, 255, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
};

// Função para desenhar um sprite
void draw_sprite(const int sprite[5][5][3]) {
    for (int linha = 0; linha < 5; linha++) {
        for (int coluna = 0; coluna < 5; coluna++) {
            int posicao = getIndex(linha, coluna);
            npSetLED(posicao, sprite[coluna][linha][0], sprite[coluna][linha][1], sprite[coluna][linha][2]);
        }
    }
    npWrite();
}

void sample_mic();
float mic_power();
uint8_t get_intensity(float v);
void update_timer(uint8_t intensity) {
  seconds += intensity;
  if (seconds >= 60) {
      seconds = 0;
      minutes++;
  }
}

/**
 * Inicializa a máquina PIO para controle da matriz de LEDs.
 */
void npInit(uint pin) {
    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;

    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }

    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

    // Limpa buffer de pixels.
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

/**
 * Atribui uma cor RGB a um LED.
 */
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

/**
 * Limpa o buffer de pixels.
 */
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

/**
 * Escreve os dados do buffer nos LEDs.
 */
void npWrite() {
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

int getIndex(int x, int y) {
    // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
    // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
    if (y % 2 == 0) {
        return 24 - (y * 5 + x); // Linha par (esquerda para direita).
    } else {
        return 24 - (y * 5 + (4 - x)); // Linha ímpar (direita para esquerda).
    }
}

int main() {
    // Inicializa entradas e saídas.
    stdio_init_all();

    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Processo de inicialização completo do OLED SSD1306
    ssd1306_init();
    // Inicializa matriz de LEDs NeoPixel.
    npInit(LED_PIN);
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);
    // Zera o display
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    restart:
    char *texto[] = {
        " Sistema de",
        " Auxílio",
        " em Cozinha",
        " Doméstica",
    };

    int y = 0;
    for (uint i = 0; i < count_of(texto); i++) {
        ssd1306_draw_string(ssd, 5, y, texto[i]);
        y += 8;
    }
    render_on_display(ssd, &frame_area);

    npClear();
    npWrite(); // Escreve os dados nos LEDs.

    while (true) {
        npClear();
        sleep_ms(50);

        // Desenha cada sprite usando a função draw_sprite
        draw_sprite(matriz1r);
        sleep_ms(100);
        npClear();
        sleep_ms(100);

        draw_sprite(matriz2r);
        sleep_ms(100);
        npClear();
        sleep_ms(100);

        draw_sprite(matriz3r);
        sleep_ms(100);
        npClear();
        sleep_ms(100);

        draw_sprite(matriz2r);
        sleep_ms(100);
        npClear();
        sleep_ms(100);

        draw_sprite(matriz1r);
        sleep_ms(100);
        npClear();
        sleep_ms(100);

        draw_sprite(matrizVv);
        sleep_ms(100);
        npClear();
        sleep_ms(100);

        draw_sprite(matrizVa);
        sleep_ms(100);
        npClear();
        sleep_ms(100);
    }
}