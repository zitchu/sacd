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
#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f // Ajuste do valor do ADC para Volts.
#define ADC_MAX 3.3f
#define ADC_STEP (3.3f / 5.f) // Intervalos de volume do microfone.

#define abs(x) ((x < 0) ? (-x) : (x))

uint8_t menu_option = 0; // 0: Opção 1 e 1: Opção 2
bool option_selected = false;

// Variáveis para o timer
uint8_t minutes = 0;
uint8_t seconds = 0;

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

// Declaração das matrizes de sprites
// Ponto vermelho
const int matriz1r[5][5][3] = {
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {255, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};

// Quadrado pequeno vermelho
const int matriz2r[5][5][3] = {
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {255, 0, 0}, {0, 0, 0}, {255, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};

// Quadrado grande vermelho
const int matriz3r[5][5][3] = {
    {{255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}},
    {{255, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 0, 0}},
    {{255, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 0, 0}},
    {{255, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 0, 0}},
    {{255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}, {255, 0, 0}}};

// Check mark verde
const int matrizVv[5][5][3] = {
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
    {{0, 255, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};

// Check mark amarela
const int matrizVa[5][5][3] = {
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 255, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 255, 0}, {0, 0, 0}},
    {{255, 255, 0}, {0, 0, 0}, {255, 255, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {255, 255, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};

// Protótipos das funções
void draw_sprite(const int sprite[5][5][3]);
void sample_mic();
float mic_power();
uint8_t get_intensity(float v);
void update_timer(uint8_t intensity);
void npInit(uint pin);
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b);
void npClear();
void npWrite();
int getIndex(int x, int y);
void opcao_1_led();
void opcao_2_timer();
void draw_menu(uint8_t option);
void handle_joystick();
void handle_buttons();

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

void sample_mic() {
    // Implementação da função
}

float mic_power() {
    // Implementação da função
    return 0.0f;
}

uint8_t get_intensity(float v) {
    // Implementação da função
    return 0;
}

void update_timer(uint8_t intensity) {
    seconds += intensity;
    if (seconds >= 60) {
        seconds = 0;
        minutes++;
    }
}

void npInit(uint pin) {
    // Implementação da função
}

void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100);
}

int getIndex(int x, int y) {
    if (y % 2 == 0) {
        return 24 - (y * 5 + x);
    } else {
        return 24 - (y * 5 + (4 - x));
    }
}

void opcao_1_led() {
    while (true) {
        npClear();
        sleep_ms(50);

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

        if (!gpio_get(BUTTON_B)) {
            break;
        }
    }
}

void opcao_2_timer() {
    adc_init();
    adc_gpio_init(MIC_PIN);
    adc_select_input(MIC_CHANNEL);
    struct render_area frame_area = {
      start_column : 0,
      end_column : ssd1306_width - 1,
      start_page : 0,
      end_page : ssd1306_n_pages - 1
  };
    while (true) {
        sample_mic();
        float power = mic_power();
        uint8_t intensity = get_intensity(power);

        update_timer(intensity);

        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        char timer_str[16];
        sprintf(timer_str, "Timer: %02d:%02d", minutes, seconds);
        ssd1306_draw_string(ssd, 5, 0, timer_str);
        render_on_display(ssd, &frame_area);

        if (!gpio_get(BUTTON_B)) {
            break;
        }
    }
}

void handle_joystick() {
    adc_select_input(0); // Seleciona o canal do eixo Y do joystick
    uint16_t vry = adc_read();
    adc_select_input(1); // Seleciona o canal do eixo X do joystick
    uint16_t vrx = adc_read();

    if (vrx < 1000) {
        menu_option = 0; // Move para a opção 1
    } else if (vrx > 3000) {
        menu_option = 1; // Move para a opção 2
    }
}

void handle_buttons() {
    if (!gpio_get(BUTTON_A)) {
        option_selected = true; // Confirma a seleção da opção
    }
    if (!gpio_get(BUTTON_B)) {
        option_selected = false; // Retorna ao menu
    }
}

void draw_menu(uint8_t option) {
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length); // Preenche o buffer com zeros

    // Desenha o título do menu
    ssd1306_draw_string(ssd, 5, 0, "Menu Principal");

    // Desenha as opções do menu
    if (option == 0) {
        ssd1306_draw_string(ssd, 5, 16, "> 1. Acionar Painel LED");
        ssd1306_draw_string(ssd, 5, 32, "  2. Captura Microfone e Timer");
    } else {
        ssd1306_draw_string(ssd, 5, 16, "  1. Acionar Painel LED");
        ssd1306_draw_string(ssd, 5, 32, "> 2. Captura Microfone e Timer");
    }

    // Renderiza o conteúdo no display OLED
    render_on_display(ssd, &frame_area);
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

    // Inicializa os botões e joystick
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    gpio_init(JOYSTICK_SW);
    gpio_set_dir(JOYSTICK_SW, GPIO_IN);
    gpio_pull_up(JOYSTICK_SW);

    // Inicializa o ADC para o joystick
    adc_init();
    adc_gpio_init(JOYSTICK_VRX);
    adc_gpio_init(JOYSTICK_VRY);

    while (true) {
        draw_menu(menu_option);
        handle_joystick();
        handle_buttons();

        if (option_selected) {
            if (menu_option == 0) {
                opcao_1_led();
            } else {
                opcao_2_timer();
            }
            option_selected = false;
        }
    }
}