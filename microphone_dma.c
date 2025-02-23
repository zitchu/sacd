#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "neopixel.c"
#include "ssd1306.h"

// Configurações do ADC e Microfone
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL)
#define ADC_CLOCK_DIV 96.f
#define SAMPLES 200
#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f)
#define ADC_MAX 3.3f
#define ADC_STEP (3.3f/5.f)


// Configurações LED e Display
#define LED_PIN 7
#define LED_COUNT 25
#define LED_R 11
#define I2C_SDA 14
#define I2C_SCL 15

// Configurações Joystick e Botões

#define BUTTON_B 6
#define JOYSTICK_VRX 27
#define JOYSTICK_VRY 26
#define JOYSTICK_SW 22
#define BUZZER_A 10
#define BUZZER_B 21

// Tempos para o timer do miojo
#define TIMER_3MIN (3 * 60)
#define TIMER_10MIN (10 * 60)

// Adicionar no início do arquivo, após os includes existentes
void sample_mic(void);
float mic_power(void);
void joystick_read_axis(uint16_t* vrx, uint16_t* vry);
void play_tone(uint gpio, uint freq, uint duration_ms);
uint8_t get_intensity(float v);

// Adicionar nas definições no início do arquivo
#define ADC_STEP (3.3f/5.f) // Intervalo de tensão para cada nível de volume

// Estados do sistema
enum SystemState {
    STATE_MENU,
    STATE_MIOJO_SELECT,
    STATE_MIOJO_TIMER,
    STATE_FEIJAO_MONITOR,
    STATE_FEIJAO_TIMER
};

// Variáveis globais
enum SystemState current_state = STATE_MENU;
uint dma_channel;
dma_channel_config dma_cfg;
uint16_t adc_buffer[SAMPLES];
uint8_t ssd[ssd1306_buffer_length];
struct render_area frame_area = {
    .start_column = 0,
    .end_column = ssd1306_width - 1,
    .start_page = 0,
    .end_page = ssd1306_n_pages - 1,
    .buffer_length = 0  // Será calculado pela função calculate_render_area_buffer_length
};
int menu_selection = 0;
bool timer_active = false;
absolute_time_t timer_start;
int remaining_time = 0;
int selected_timer = TIMER_3MIN;
bool feijao_timer_started = false;

// Funções auxiliares
void setup_hardware() {
    stdio_init_all();
    sleep_ms(3000);

    // Inicialização do I2C primeiro
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicialização do display OLED
    ssd1306_init();
    calculate_render_area_buffer_length(&frame_area);
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

   // Configuração do ADC
   printf("Preparando ADC...\n");
   adc_gpio_init(MIC_PIN);
   adc_init();
   adc_select_input(MIC_CHANNEL);
   adc_fifo_setup(
       true,    // Habilitar FIFO
       true,    // Habilitar request de dados do DMA
       1,       // Threshold para ativar request DMA
       false,   // Não usar bit de erro
       false    // Manter 12-bits
   );
   adc_set_clkdiv(ADC_CLOCK_DIV);

   // Configuração do DMA
   dma_channel = dma_claim_unused_channel(true);
   dma_cfg = dma_channel_get_default_config(dma_channel);
   channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);
   channel_config_set_read_increment(&dma_cfg, false);
   channel_config_set_write_increment(&dma_cfg, true);
   channel_config_set_dreq(&dma_cfg, DREQ_ADC);

    // Inicialização dos LEDs
    printf("Inicializando matriz de LEDs...\n");
    npInit(LED_PIN, LED_COUNT);
    npClear();
    npWrite();

    // Inicialização dos botões
    
    gpio_init(BUTTON_B);
    gpio_init(JOYSTICK_SW);
    
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_set_dir(JOYSTICK_SW, GPIO_IN);
    
    gpio_pull_up(BUTTON_B);
    gpio_pull_up(JOYSTICK_SW);
}

void draw_menu() {
    // Limpa o buffer completamente
    memset(ssd, 0, ssd1306_buffer_length);
    
    // Desenha o título
    ssd1306_draw_string(ssd, 5, 8, "Menu Principal");
    
    // Desenha as opções - separando a seta do texto
    ssd1306_draw_string(ssd, 5, 24, menu_selection == 0 ? "X" : " ");
    ssd1306_draw_string(ssd, 20, 24, "Modo Miojo");
    
    ssd1306_draw_string(ssd, 5, 40, menu_selection == 1 ? "X" : " ");
    ssd1306_draw_string(ssd, 20, 40, "Modo Feijao");
    
    // Renderiza
    calculate_render_area_buffer_length(&frame_area);
    render_on_display(ssd, &frame_area);
}


void draw_miojo_menu() {
    // Limpa o buffer completamente
    memset(ssd, 0, ssd1306_buffer_length);
    
    // Desenha o título
    ssd1306_draw_string(ssd, 5, 8, "Timer Miojo");
    
    // Desenha as opções - separando a seta do texto
    ssd1306_draw_string(ssd, 5, 24, selected_timer == TIMER_3MIN ? "X" : " ");
    ssd1306_draw_string(ssd, 20, 24, "3 minutos");
    
    ssd1306_draw_string(ssd, 5, 40, selected_timer == TIMER_10MIN ? "X" : " ");
    ssd1306_draw_string(ssd, 20, 40, "10 minutos");
    
    // Renderiza
    calculate_render_area_buffer_length(&frame_area);
    render_on_display(ssd, &frame_area);
}


void update_timer_display(int seconds, bool is_countdown) {
    // Limpa o buffer completamente
    memset(ssd, 0, ssd1306_buffer_length);
    
    // Prepara a string do timer
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%s: %02d:%02d", 
             is_countdown ? "Restam" : "Pressao",
             seconds / 60, seconds % 60);
    
    // Calcula a posição central
    int x = (ssd1306_width - strlen(time_str) * 8) / 2;
    
    // Desenha as strings
    ssd1306_draw_string(ssd, 5, 10, "Press B Voltar");
    ssd1306_draw_string(ssd, x, 24, time_str);
    
    // Calcula o tamanho do buffer e renderiza
    calculate_render_area_buffer_length(&frame_area);
    render_on_display(ssd, &frame_area);
}


float get_sound_level() {
    sample_mic();
    float avg = mic_power();
    float adjusted = 3.3 * abs(ADC_ADJUST(avg));
    
    printf("Media ADC ajustada: %8.4f\n", adjusted);
    return adjusted;
}

int get_matrix_index(int pos) {
    // A matriz é 5x5, pos vai de 0 a 24
    int x = pos % 5;
    int y = pos / 5;
    
    // Se a linha é ímpar, inverte a direção
    if (y % 2 == 1) {
        x = 4 - x;
    }
    
    return y * 5 + x;
}

void update_leds(float sound_level) {
    npClear();
    uint8_t intensity = get_intensity(sound_level);

    // Definir os anéis uma única vez no início da função
    const int anel1[] = {7, 11, 13, 17};
    const int anel2[] = {2, 6, 8, 10, 14, 16, 18, 22};
    const int anel3[] = {1, 3, 5, 9, 15, 19, 21, 23};

    // A depender da intensidade do som, acende LEDs específicos
    switch (intensity) {
        case 0:
            npSetLED(get_matrix_index(12), 0, 0, 1); // Acende apenas o centro 
            break; // Se o som for muito baixo, não acende nada
            
        case 1:
            npSetLED(get_matrix_index(12), 0, 0, 1); // Acende apenas o centro
            break;
            
        case 2:
            npSetLED(get_matrix_index(12), 0, 0, 2); // Centro mais brilhante
            // Primeiro anel
            for (int i = 0; i < 4; i++) {
                int pos = get_matrix_index(7 + i*2);
                npSetLED(pos, 0, 0, 1);
            }
            break;
            
        case 3:
            npSetLED(get_matrix_index(12), 1, 1, 0); // Centro amarelo
            // Primeiro anel azul
            for (int i = 0; i < 4; i++) {
                int pos = get_matrix_index(7 + i*2);
                npSetLED(pos, 0, 0, 2);
            }
            // Segundo anel
            for (int i = 0; i < 8; i++) {
                npSetLED(get_matrix_index(anel2[i]), 0, 0, 1);
            }
            break;
            
        default: // Intensidade 4 ou maior
            npSetLED(get_matrix_index(12), 1, 0, 0); // Centro vermelho
            
            // Primeiro anel amarelo
            for (int i = 0; i < 4; i++) {
                npSetLED(get_matrix_index(anel1[i]), 1, 1, 0);
            }
            // Segundo anel azul
            for (int i = 0; i < 8; i++) {
                npSetLED(get_matrix_index(anel2[i]), 0, 0, 2);
            }
            // Terceiro anel azul fraco
            for (int i = 0; i < 8; i++) {
                npSetLED(get_matrix_index(anel3[i]), 0, 0, 1);
            }
            break;
    }
    
    npWrite();
    printf("Intensidade: %d\n", intensity);
}

void joystick_read_axis(uint16_t* vrx, uint16_t* vry) {
    adc_select_input(0); // Canal do VRX
    *vrx = adc_read();
    
    adc_select_input(1); // Canal do VRY
    *vry = adc_read();
    
    // Retorna para o canal do microfone
    adc_select_input(MIC_CHANNEL);
}

void play_tone(uint gpio, uint freq, uint duration_ms) {
    // Configura PWM para o buzzer
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    uint chan = pwm_gpio_to_channel(gpio);
    
    // Configura frequência
    uint32_t clock = 125000000;
    uint32_t divider = clock / 1000000;
    pwm_set_clkdiv(slice_num, divider);
    pwm_set_wrap(slice_num, 1000000/freq);
    pwm_set_chan_level(slice_num, chan, 1000000/(2*freq));
    
    // Ativa o PWM
    pwm_set_enabled(slice_num, true);
    sleep_ms(duration_ms);
    pwm_set_enabled(slice_num, false);
}

int main() {
    setup_hardware();
    
    while (true) {
        uint16_t vrx, vry;
        joystick_read_axis(&vrx, &vry);
        
        switch (current_state) {
            case STATE_MENU:
                draw_menu();
                if (vrx > 3000) menu_selection = 0;
                else if (vrx < 1000) menu_selection = 1;
                
                if (!gpio_get(BUTTON_B)) {
                    if (menu_selection == 0) {
                        current_state = STATE_MIOJO_SELECT;
                    } else {
                        current_state = STATE_FEIJAO_MONITOR;
                    }
                    sleep_ms(200);
                }
                break;

            case STATE_MIOJO_SELECT:
                draw_miojo_menu();
                if (vrx > 3000) selected_timer = TIMER_3MIN;
                else if (vrx < 1000) selected_timer = TIMER_10MIN;
                
                if (!gpio_get(BUTTON_B)) {
                    timer_active = true;
                    remaining_time = selected_timer;
                    timer_start = get_absolute_time();
                    current_state = STATE_MIOJO_TIMER;
                    sleep_ms(200);
                }
                break;

            case STATE_MIOJO_TIMER:
                if (timer_active) {
                    int elapsed = to_ms_since_boot(get_absolute_time()) - 
                                to_ms_since_boot(timer_start);
                    remaining_time = selected_timer - (elapsed / 1000);
                    
                    if (remaining_time <= 0) {
                        // Timer acabou
                        play_tone(BUZZER_A, 1000, 1000);
                        timer_active = false;
                        current_state = STATE_MENU;
                    } else {
                        update_timer_display(remaining_time, true);
                    }
                }
                break;

                case STATE_FEIJAO_MONITOR:
                float sound_level;
                uint8_t intensity;
                
                // Continua monitorando o som e atualizando os LEDs
                sound_level = get_sound_level();
                intensity = get_intensity(sound_level);
                update_leds(sound_level);
                
                // Inicia o timer se detectar som alto
                if (intensity > 1 && !feijao_timer_started) { // Reduzido de 2 para 1
                    feijao_timer_started = true;
                    timer_start = get_absolute_time();
                    current_state = STATE_FEIJAO_TIMER;
                }
                
                memset(ssd, 0, ssd1306_buffer_length);
                ssd1306_draw_string(ssd, 5, 24, "Monitorando...");
                calculate_render_area_buffer_length(&frame_area);
                render_on_display(ssd, &frame_area);
                break;
            
            case STATE_FEIJAO_TIMER:
                // Continua monitorando e atualizando os LEDs mesmo no modo timer
                sound_level = get_sound_level();
                update_leds(sound_level);
                
                if (feijao_timer_started) {
                    int elapsed = to_ms_since_boot(get_absolute_time()) - 
                                to_ms_since_boot(timer_start);
                    update_timer_display(elapsed / 1000, false);
                }
                break;
        }
        
        if (!gpio_get(BUTTON_B) && (current_state != STATE_MENU)) {
            current_state = STATE_MENU;
            timer_active = false;
            feijao_timer_started = false;
            // Desliga a matriz de LEDs ao sair do modo feijão
            npClear();
            npWrite();
            sleep_ms(200);
        }
        
        sleep_ms(100);
    }
    
    return 0;
}

/**
 * Realiza as leituras do ADC e armazena os valores no buffer.
 */
void sample_mic() {
    adc_fifo_drain(); // Limpa o FIFO do ADC.
    adc_run(false); // Desliga o ADC (se estiver ligado) para configurar o DMA.

    dma_channel_configure(dma_channel, &dma_cfg,
        adc_buffer, // Escreve no buffer.
        &(adc_hw->fifo), // Lê do ADC.
        SAMPLES, // Faz SAMPLES amostras.
        true // Liga o DMA.
    );

    // Liga o ADC e espera acabar a leitura.
    adc_run(true);
    dma_channel_wait_for_finish_blocking(dma_channel);
    
    // Acabou a leitura, desliga o ADC de novo.
    adc_run(false);
}

/**
 * Calcula a potência média das leituras do ADC. (Valor RMS)
 */
float mic_power() {
    float avg = 0.f;

    for (uint i = 0; i < SAMPLES; ++i)
        avg += adc_buffer[i] * adc_buffer[i];
    
    avg /= SAMPLES;
    return sqrt(avg);
}

/**
 * Calcula a intensidade do volume registrado no microfone, de 0 a 4, usando a tensão.
 */
uint8_t get_intensity(float v) {
    uint count = 0;
    while ((v -= ADC_STEP/20) > 0.f)
        ++count;
    return count;
}

/**
 * Atualiza o display OLED com o tempo decorrido desde que o timer foi iniciado.
 */
void update_oled_timer() {
    absolute_time_t now = get_absolute_time();
    int64_t elapsed_us = absolute_time_diff_us(timer_start, now); // Corrigido: usar timer_start em vez de timer_start_time
    int elapsed_seconds = elapsed_us / 1000000;

    char timer_str[32]; // Declaração local da string do timer
    snprintf(timer_str, sizeof(timer_str), "Time: %02d:%02d", elapsed_seconds / 60, elapsed_seconds % 60);

    // Limpa o buffer do display
    memset(ssd, 0, ssd1306_buffer_length);

    // Desenha o tempo no display
    ssd1306_draw_string(ssd, 5, 10, "Press B Voltar");
    ssd1306_draw_string(ssd, 5, 20, timer_str);

    // Renderiza o buffer no display
    render_on_display(ssd, &frame_area);
}

/**
 * Configura o display OLED.
 */
void setup_oled() {
    printf("Inicializando o OLED...\n");

    // Inicialização do I2C
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicialização do display SSD1306
    ssd1306_init();

    // Configura a área de renderização
    calculate_render_area_buffer_length(&frame_area);

    // Limpa o display
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);
}