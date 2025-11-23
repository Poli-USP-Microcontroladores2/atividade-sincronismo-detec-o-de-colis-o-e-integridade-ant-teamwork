/*
 * TRANSCEPTOR SINCRONISMO PURO (Base Sólida)
 *
 * OBJETIVO:
 * - Garantir sincronia perfeita via cabo PTB1.
 * - Sem tratamento de colisão.
 * - Sem printk.
 *
 * LEDS (Mapeamento Solicitado):
 * - LED0 (Verde): RX (Ouvindo/Recebendo)
 * - LED1 (Azul):  TX (Enviando)
 * - LED2 (Vermelho): Erro de Conteúdo (Recebeu mensagem errada)
 *
 * LÓGICA:
 * - Mestre alterna o pino PTB1 a cada 2 segundos.
 * - Escravo lê o pino e obedece imediatamente.
 * - NÃO há verificação de canal livre. Confiança total no cabo.
 */

#define SOU_MESTRE 1  // <--- 1 = MESTRE, 0 = ESCRAVO

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include <stdbool.h>

// --- Configurações ---
#define MSG_SECRETA "paralelepipedo"
#define TEMPO_CICLO_MS 4000
#define RX_BUF_SIZE 64

// --- Hardware ---
#define UART_DEVICE_NODE DT_NODELABEL(uart0)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

#define GPIO_SYNC_NODE DT_NODELABEL(gpiob)
#define PINO_SYNC 1
static const struct device *gpio_sync_dev = DEVICE_DT_GET(GPIO_SYNC_NODE);
// Callback apenas para acordar threads se necessário (uso opcional aqui)
static struct gpio_callback escravo_cb_data;

// --- LEDs (Verde=0, Azul=1, Vermelho=2) ---
#define LED_GREEN_NODE DT_ALIAS(led0) 
#define LED_BLUE_NODE  DT_ALIAS(led1) 
#define LED_RED_NODE   DT_ALIAS(led2) 

static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios);
static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);

// --- Variáveis Globais ---
static char rx_buf[RX_BUF_SIZE];
static volatile int rx_buf_pos = 0;
static volatile bool rx_ativo = false;      
static volatile bool nova_msg_recebida = false;

// ============================================================================
// FUNÇÕES DE LED
// ============================================================================

void leds_off_all(void) {
    gpio_pin_set_dt(&led_red, 0);
    gpio_pin_set_dt(&led_green, 0);
    gpio_pin_set_dt(&led_blue, 0);
}

void indicar_rx(void) {
    leds_off_all();
    gpio_pin_set_dt(&led_green, 1); // Verde
}

void indicar_tx(void) {
    leds_off_all();
    gpio_pin_set_dt(&led_blue, 1); // Azul
}

void indicar_erro(void) {
    leds_off_all();
    gpio_pin_set_dt(&led_red, 1); // Vermelho (Erro de conteúdo)
}

// ============================================================================
// UART
// ============================================================================

static void definir_papel(bool modo_ouvinte)
{
    unsigned int key = irq_lock();
    if (modo_ouvinte) {
        rx_ativo = true;
        rx_buf_pos = 0;
        nova_msg_recebida = false;
        // Limpa buffer lógico
        memset(rx_buf, 0, RX_BUF_SIZE);
    } else {
        rx_ativo = false;
    }
    irq_unlock(key);
}

static void processar_mensagem(void) {
    // Procura a palavra chave na string recebida
    if (strstr(rx_buf, MSG_SECRETA) != NULL) {
        // Sucesso: Pisca verde rápido ou mantém verde
        // Como já estamos em RX (Verde), apenas mantemos.
        // (Opcional: Piscar para indicar que chegou NOVO pacote)
        gpio_pin_set_dt(&led_green, 0);
        k_sleep(K_MSEC(100));
        gpio_pin_set_dt(&led_green, 1);
    } else {
        indicar_erro(); // Vermelho se conteúdo errado
        k_sleep(K_MSEC(500)); // Segura o erro um pouco
        indicar_rx(); // Volta a ouvir
    }
    rx_buf_pos = 0;
    rx_buf[0] = '\0';
    nova_msg_recebida = false; 
}

static void uart_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);
    while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
        uint8_t c;
        while (uart_fifo_read(dev, &c, 1) == 1) {
            if (rx_ativo) {
                if (c == '\n' || c == '\r') {
                    if (rx_buf_pos > 0) {
                        rx_buf[rx_buf_pos] = '\0';
                        nova_msg_recebida = true;
                        rx_buf_pos = 0;
                    }
                } else if (rx_buf_pos < (RX_BUF_SIZE - 1)) {
                    // Filtra caracteres visíveis
                    if (c >= 32 && c <= 126) rx_buf[rx_buf_pos++] = c;
                }
            }
        }
    }
}

static void uart_enviar_string(void)
{
    const char *msg = MSG_SECRETA "\r\n";
    for (int i = 0; i < strlen(msg); i++) {
        uart_poll_out(uart_dev, msg[i]);
    }
}

// ============================================================================
// ESCRAVO CB (Opcional, usamos polling no main para simplicidade extrema)
// ============================================================================
#if !SOU_MESTRE
void escravo_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Callback vazio, lógica no main
}
#endif

// ============================================================================
// MAIN
// ============================================================================
void main(void)
{
    if (!device_is_ready(uart_dev) || !device_is_ready(gpio_sync_dev)) return;
    if (!gpio_is_ready_dt(&led_red) || !gpio_is_ready_dt(&led_green) || !gpio_is_ready_dt(&led_blue)) return;

    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);

    uart_irq_callback_user_data_set(uart_dev, uart_isr, NULL);
    uart_irq_rx_enable(uart_dev); 

    // -----------------------------------------------------------------------
    // MESTRE (Dita o Ritmo)
    // -----------------------------------------------------------------------
    #if SOU_MESTRE
    
    gpio_pin_configure(gpio_sync_dev, PINO_SYNC, GPIO_OUTPUT_LOW);

    while (1) {
        // --- FASE 1: TX (Azul) ---
        gpio_pin_set(gpio_sync_dev, PINO_SYNC, 1); 
        definir_papel(false); // Não ouve
        indicar_tx();         // LED Azul
        
        k_sleep(K_MSEC(100)); // Estabiliza pino
        uart_enviar_string(); // Envia mensagem
        
        // Aguarda metade do ciclo
        k_sleep(K_MSEC(TEMPO_CICLO_MS / 2));

        // --- FASE 2: RX (Verde) ---
        gpio_pin_set(gpio_sync_dev, PINO_SYNC, 0);
        definir_papel(true);  // Ouve
        indicar_rx();         // LED Verde
        
        // Aguarda e processa
        uint64_t fim_rx = k_uptime_get() + (TEMPO_CICLO_MS / 2);
        while (k_uptime_get() < fim_rx) {
            if (nova_msg_recebida) processar_mensagem();
            k_sleep(K_MSEC(10));
        }
    }

    // -----------------------------------------------------------------------
    // ESCRAVO (Obedece o Pino)
    // -----------------------------------------------------------------------
    #else
    
    gpio_pin_configure(gpio_sync_dev, PINO_SYNC, GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&escravo_cb_data, escravo_cb, BIT(PINO_SYNC));
    gpio_add_callback(gpio_sync_dev, &escravo_cb_data);
    gpio_pin_interrupt_configure(gpio_sync_dev, PINO_SYNC, GPIO_INT_EDGE_BOTH);

    // Controle de envio único por ciclo para não inundar
    bool ja_enviei = false;
    int ultimo_estado = -1;

    while (1) {
        int pino_val = gpio_pin_get(gpio_sync_dev, PINO_SYNC);

        // Detecta borda para resetar flag de envio
        if (pino_val != ultimo_estado) {
            ja_enviei = false; 
            ultimo_estado = pino_val;
        }

        if (pino_val == 1) {
            // --- MESTRE ESTÁ FALANDO (TX) ---
            // Eu devo OUVIR (RX)
            if (!rx_ativo) {
                definir_papel(true);
                indicar_rx(); // Verde
            }
            
            if (nova_msg_recebida) processar_mensagem();
        } 
        else {
            // --- MESTRE ESTÁ OUVINDO (RX) ---
            // Eu devo FALAR (TX)
            if (rx_ativo) definir_papel(false);

            if (!ja_enviei) {
                k_sleep(K_MSEC(100)); // Espera guarda
                indicar_tx();         // Azul
                uart_enviar_string(); // Envia
                ja_enviei = true;
                
                // Depois de enviar, pode voltar pra verde visualmente ou ficar azul
                // Vamos manter azul para indicar que foi minha vez
            }
        }
        k_sleep(K_MSEC(10));
    }
    #endif
}
