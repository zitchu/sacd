#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"

// Definição dos pinos do joystick e botões
const int JOYSTICK_VRY = 26;
const int JOYSTICK_VRX = 27;
const int ADC_CHANNEL_0 = 0; // Canal ADC para o eixo X
const int ADC_CHANNEL_1 = 1; // Canal ADC para o eixo Y
const int JOYSTICK_SW = 22;

#define BUTTON_A 5
#define BUTTON_B 6

// Definição do display SSD OLED
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Variáveis globais para controle do menu
int menu_selection = 0; // 0 para opção A, 1 para opção B
bool button_a_pressed = false;
bool button_b_pressed = false;
bool option_selected = false; // Indica se uma opção foi selecionada

// Variáveis para debounce
uint32_t last_button_a_time = 0;
uint32_t last_button_b_time = 0;
const uint32_t debounce_time = 200; // Tempo de debounce em milissegundos

// Função para ler os eixos X e Y do joystick
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value) {
    // Leitura do valor do eixo X do joystick
    adc_select_input(ADC_CHANNEL_0); // Seleciona o canal ADC para o eixo X
    sleep_us(2);                     // Pequeno delay para estabilidade
    *vrx_value = adc_read();         // Lê o valor do eixo X (0-4095)

    // Leitura do valor do eixo Y do joystick (invertido)
    adc_select_input(ADC_CHANNEL_1); // Seleciona o canal ADC para o eixo Y
    sleep_us(2);                     // Pequeno delay para estabilidade
    *vry_value = 4095 - adc_read();  // Lê o valor do eixo Y e inverte (0-4095)
}

// Função para desenhar o menu na tela OLED
void draw_menu(uint8_t *ssd, int selection, struct render_area *frame_area) {
    // Limpa o display
    memset(ssd, 0, ssd1306_buffer_length);

    // Desenha as opções do menu
    if (selection == 0) {
        ssd1306_draw_string(ssd, 5, 10, "Bem vindo SDCE");
        ssd1306_draw_string(ssd, 5, 21, " Escolha uma");
        ssd1306_draw_string(ssd, 5, 32, " opcao abaixo");
        ssd1306_draw_string(ssd, 5, 43, "X  Opcao A");
        ssd1306_draw_string(ssd, 5, 54, "   Opcao B");
    } else {
        ssd1306_draw_string(ssd, 5, 10, "Bem vindo SDCE");
        ssd1306_draw_string(ssd, 5, 21, " Escolha uma");
        ssd1306_draw_string(ssd, 5, 32, " opcao abaixo");
        ssd1306_draw_string(ssd, 5, 43, "   Opcao A");
        ssd1306_draw_string(ssd, 5, 54, "X  Opcao B");
    }

    // Renderiza o display
    render_on_display(ssd, frame_area);
}

// Função para ler o joystick e os botões com debounce
void read_inputs(uint8_t *ssd, struct render_area *frame_area) {
    uint16_t vrx_value, vry_value;

    // Lê os valores dos eixos X e Y do joystick
    joystick_read_axis(&vrx_value, &vry_value);

    // Navegação do menu com base no eixo X do joystick
    if (!option_selected) { // Só permite navegação se nenhuma opção foi selecionada
        if (vrx_value < 100) { // Joystick para a esquerda
            menu_selection = 1;
        } else if (vrx_value > 3000) { // Joystick para a direita
            menu_selection = 0;
        }
    }

    // Leitura dos botões com debounce
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (!gpio_get(BUTTON_A) && (current_time - last_button_a_time > debounce_time)) {
        button_a_pressed = true;
        last_button_a_time = current_time;
    } else {
        button_a_pressed = false;
    }

    if (!gpio_get(BUTTON_B) && (current_time - last_button_b_time > debounce_time)) {
        button_b_pressed = true;
        last_button_b_time = current_time;
    } else {
        button_b_pressed = false;
    }

    // Verifica se o botão A ou B foi pressionado
    if (button_a_pressed && !option_selected) {
        printf("Opcao A selecionada\n");
        option_selected = true; // Marca que uma opção foi selecionada

        // Limpa a tela antes de exibir a opção escolhida
        memset(ssd, 0, ssd1306_buffer_length);
        render_on_display(ssd, frame_area); // Renderiza a tela limpa

        // Exibe a opção escolhida
        ssd1306_draw_string(ssd, 5, 30, "Opcao A selecionada");
        render_on_display(ssd, frame_area); // Renderiza a alteração no display
    } else if (button_b_pressed && !option_selected) {
        printf("Opcao B selecionada\n");
        option_selected = true; // Marca que uma opção foi selecionada

        // Limpa a tela antes de exibir a opção escolhida
        memset(ssd, 0, ssd1306_buffer_length);
        render_on_display(ssd, frame_area); // Renderiza a tela limpa

        // Exibe a opção escolhida
        ssd1306_draw_string(ssd, 5, 30, "Opcao B selecionada");
        render_on_display(ssd, frame_area); // Renderiza a alteração no display
    }
}

int main() {
    // Inicializa entradas e saídas
    stdio_init_all();

    // Inicializa o I2C para o OLED
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa o OLED
    ssd1306_init();

    // Configura a área de renderização
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);

    // Buffer para o display
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    // Inicializa o ADC para o joystick
    adc_init();
    adc_gpio_init(JOYSTICK_VRX);
    adc_gpio_init(JOYSTICK_VRY);

    // Inicializa os pinos dos botões
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    // Loop principal
    while (true) {
        // Lê as entradas do joystick e dos botões
        read_inputs(ssd, &frame_area);

        // Desenha o menu na tela OLED apenas se nenhuma opção foi selecionada
        if (!option_selected) {
            draw_menu(ssd, menu_selection, &frame_area);
        }

        // Pequeno delay para evitar leituras muito rápidas
        sleep_ms(100);
    }

    return 0;
}