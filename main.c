/*
 * TRANSCEPTOR SINCRONISMO PURO — Verificação de STRING + TAMANHO
 *
 * LEDS:
 * - LED0 (Verde): RX correto
 * - LED1 (Azul):  TX enviando
 * - LED2 (Vermelho): Erro (string diferente OU tamanho diferente)
 */

#define SOU_MESTRE 0  // 1 = mestre, 0 = escravo

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include <stdbool.h>

#define MSG_SECRETA "paralelepiped"
#define TEMPO_CICLO_MS 4000
#define RX_BUF_SIZE 64

#define UART_DEVICE_NODE DT_NODELABEL(uart0)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

#define GPIO_SYNC_NODE DT_NODELABEL(gpiob)
#define PINO_SYNC 1
static const struct device *gpio_sync_dev = DEVICE_DT_GET(GPIO_SYNC_NODE);

#define LED_GREEN_NODE DT_ALIAS(led0)
#define LED_BLUE_NODE  DT_ALIAS(led1)
#define LED_RED_NODE   DT_ALIAS(led2)

static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios);
static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);

static char rx_buf[RX_BUF_SIZE];
static volatile int rx_buf_pos = 0;
static volatile bool rx_ativo = false;
static volatile bool nova_msg_recebida = false;

// ============================================================================
// LEDS
// ============================================================================
void leds_off_all(void) {
    gpio_pin_set_dt(&led_red, 0);
    gpio_pin_set_dt(&led_green, 0);
    gpio_pin_set_dt(&led_blue, 0);
}

void indicar_rx(void) {
    leds_off_all();
    gpio_pin_set_dt(&led_green, 1);
}

void indicar_tx(void) {
    leds_off_all();
    gpio_pin_set_dt(&led_blue, 1);
}

void indicar_erro(void) {
    leds_off_all();
    gpio_pin_set_dt(&led_red, 1);
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
        memset(rx_buf, 0, RX_BUF_SIZE);
    } else {
        rx_ativo = false;
    }
    irq_unlock(key);
}

static void processar_mensagem(void)
{
    // --- Critérios da opção C ---
    bool tamanho_ok = strlen(rx_buf) == strlen(MSG_SECRETA);
    bool string_ok  = strcmp(rx_buf, MSG_SECRETA) == 0;

    if (tamanho_ok && string_ok) {
        // Certo → Verde fixo pela janela inteira
        gpio_pin_set_dt(&led_green, 1);
        k_sleep(K_MSEC(TEMPO_CICLO_MS / 2));
    } else {
        // Erro → Vermelho fixo pela janela inteira
        indicar_erro();
        k_sleep(K_MSEC(TEMPO_CICLO_MS / 2));
    }

    // limpa para próxima janela
    rx_buf_pos = 0;
    rx_buf[0] = '\0';
    nova_msg_recebida = false;

    indicar_rx();
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
                }
                else if (rx_buf_pos < RX_BUF_SIZE - 1) {
                    if (c >= 32 && c <= 126)
                        rx_buf[rx_buf_pos++] = c;
                }
            }
        }
    }
}

static void uart_enviar_string(void)
{
    const char *msg = MSG_SECRETA "\n";

    for (int i = 0; i < strlen(msg); i++)
        uart_poll_out(uart_dev, msg[i]);
}

// ============================================================================
// MAIN
// ============================================================================
void main(void)
{
    if (!device_is_ready(uart_dev) || !device_is_ready(gpio_sync_dev)) return;

    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);

    uart_irq_callback_user_data_set(uart_dev, uart_isr, NULL);
    uart_irq_rx_enable(uart_dev);

// ============================================================================
// MESTRE
// ============================================================================
#if SOU_MESTRE

    gpio_pin_configure(gpio_sync_dev, PINO_SYNC, GPIO_OUTPUT_LOW);

    while (1) {
        // --- TX ---
        gpio_pin_set(gpio_sync_dev, PINO_SYNC, 1);
        definir_papel(false);
        indicar_tx();

        k_sleep(K_MSEC(100));
        uart_enviar_string();

        k_sleep(K_MSEC(TEMPO_CICLO_MS / 2));

        // --- RX ---
        gpio_pin_set(gpio_sync_dev, PINO_SYNC, 0);
        definir_papel(true);
        indicar_rx();

        uint64_t deadline = k_uptime_get() + (TEMPO_CICLO_MS / 2);

        while (k_uptime_get() < deadline) {
            if (nova_msg_recebida)
                processar_mensagem();

            k_sleep(K_MSEC(10));
        }
    }

// ============================================================================
// ESCRAVO
// ============================================================================
#else

    gpio_pin_configure(gpio_sync_dev, PINO_SYNC, GPIO_INPUT | GPIO_PULL_UP);

    bool ja_enviei = false;
    int ultimo = -1;

    while (1) {
        int estado = gpio_pin_get(gpio_sync_dev, PINO_SYNC);

        if (estado != ultimo) {
            ultimo = estado;
            ja_enviei = false;
        }

        if (estado == 1) {
            // Mestre TX → Eu RX
            if (!rx_ativo) {
                definir_papel(true);
                indicar_rx();
            }

            if (nova_msg_recebida)
                processar_mensagem();
        }
        else {
            // Mestre RX → Eu TX
            if (rx_ativo)
                definir_papel(false);

            if (!ja_enviei) {
                k_sleep(K_MSEC(100));
                indicar_tx();
                uart_enviar_string();
                ja_enviei = true;
            }
        }

        k_sleep(K_MSEC(10));
    }

#endif
}
