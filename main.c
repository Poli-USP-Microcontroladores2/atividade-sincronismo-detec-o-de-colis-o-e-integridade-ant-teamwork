/*
 * CHAT SINCRONIZADO VIA UART1 (FRDM-KL25Z)
 * Correções aplicadas:
 * - Fix do erro de IRQ_CONNECT (passagem de parâmetro incorreta).
 * - Fix do warning de void main.
 * - Simplificação do acesso ao device dentro da ISR.
 */

#define SOU_MESTRE 1  // <--- ALTERE AQUI: 1 = Mestre, 0 = Escravo

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include <stdbool.h>
#include <soc.h> // Necessário para acesso direto aos registradores (UART1->C2)

// ============================================================================
// CONFIGURAÇÕES
// ============================================================================
#define TEMPO_CICLO_MS 2000
#define BUF_SIZE 64
#define START_BYTE '#'      // Caractere de integridade

// --- UART1 (Comunicação entre placas) ---
// Define o nó da árvore de dispositivos
#define UART_COM_NODE DT_NODELABEL(uart1)
// Obtém a estrutura do dispositivo (ponteiro C válido)
static const struct device *const uart_com = DEVICE_DT_GET(UART_COM_NODE);

// --- UART0 (Console com o PC) ---
static const struct device *const uart_console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

// --- Sincronismo (GPIO) ---
#define GPIO_SYNC_NODE DT_NODELABEL(gpiob)
#define PINO_SYNC 1
static const struct device *gpio_sync_dev = DEVICE_DT_GET(GPIO_SYNC_NODE);

// --- LEDs ---
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

// --- Variáveis Globais ---
static char rx_buf[BUF_SIZE];
static volatile int rx_buf_pos = 0;
static volatile bool nova_msg_recebida = false;
static volatile bool rx_ativo = false;

static char user_tx_buf[BUF_SIZE];
static int user_tx_pos = 0;
static bool user_msg_ready = false;

// ============================================================================
// LEDS HELPER
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
// LÓGICA DE INPUT DO USUÁRIO
// ============================================================================
static void ler_entrada_usuario(void) {
    uint8_t c;
    // Lê do console (UART0) sem bloquear
    while (uart_poll_in(uart_console, &c) == 0) {
        // Eco visual no terminal
        uart_poll_out(uart_console, c);

        if (c == '\r' || c == '\n') {
            if (user_tx_pos > 0) {
                user_tx_buf[user_tx_pos] = '\0';
                user_msg_ready = true;
                uart_poll_out(uart_console, '\n');
                uart_poll_out(uart_console, '\r');
                printk("[Sistema] Mensagem pronta para envio...\n");
            }
        } else if (user_tx_pos < BUF_SIZE - 2) {
            if (!user_msg_ready) {
                user_tx_buf[user_tx_pos++] = c;
            }
        }
    }
}

// ============================================================================
// UART1 - INTERRUPÇÃO (CORRIGIDA)
// ============================================================================
static void uart1_isr_wrapper(const void *arg)
{
    ARG_UNUSED(arg);
    // Usamos a variável global uart_com diretamente para evitar erros de ponteiro
    const struct device *dev = uart_com;
    
    // Verifica se há dados no RX
    if (uart_irq_rx_ready(dev)) {
        uint8_t c;
        // Lê enquanto houver dados no buffer (UART1 KL25Z tem FIFO de 1 byte na prática)
        while(uart_fifo_read(dev, &c, 1) > 0) {
            
            if (rx_ativo) {
                // Sincronismo de Start Byte
                if (rx_buf_pos == 0 && c != START_BYTE) {
                    return; // Ignora lixo
                }

                if (c == '\n' || c == '\r') {
                    if (rx_buf_pos > 0) {
                        rx_buf[rx_buf_pos] = '\0';
                        nova_msg_recebida = true;
                        rx_buf_pos = 0;
                    }
                }
                else if (rx_buf_pos < BUF_SIZE - 1) {
                    if ((c >= 32 && c <= 126) || c == START_BYTE) {
                        rx_buf[rx_buf_pos++] = c;
                    }
                }
            }
        }
    }
}

static void definir_papel(bool modo_ouvinte)
{
    unsigned int key = irq_lock();
    if (modo_ouvinte) {
        rx_ativo = true;
        rx_buf_pos = 0;
        nova_msg_recebida = false;
        memset(rx_buf, 0, BUF_SIZE);
    } else {
        rx_ativo = false;
    }
    irq_unlock(key);
}

// Envia mensagem via UART1
static void uart_enviar_mensagem_usuario(void)
{
    if (user_msg_ready) {
        uart_poll_out(uart_com, START_BYTE);
        
        for (int i = 0; i < strlen(user_tx_buf); i++) {
            uart_poll_out(uart_com, user_tx_buf[i]);
        }
        
        uart_poll_out(uart_com, '\n');

        user_tx_pos = 0;
        user_msg_ready = false;
        printk("[Sistema] Enviado!\n");
    } 
}

static void processar_mensagem_recebida(void)
{
    if (rx_buf[0] == START_BYTE) {
        // Imprime o que veio da outra placa
        printk("\n>> REMOTE: %s\n", &rx_buf[1]);
        
        gpio_pin_set_dt(&led_green, 1);
        k_sleep(K_MSEC(100));
    } else {
        indicar_erro();
        printk("Erro de integridade.\n");
    }

    rx_buf_pos = 0;
    rx_buf[0] = '\0';
    nova_msg_recebida = false;
    indicar_rx();
}

// ============================================================================
// MAIN
// ============================================================================
int main(void)
{
    if (!device_is_ready(uart_com) || !device_is_ready(gpio_sync_dev) || !device_is_ready(uart_console)) {
        return 0;
    }

    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);

    printk("--- CHAT UART1 INICIADO ---\n");
    printk("Papel: %s\n", SOU_MESTRE ? "MESTRE" : "ESCRAVO");

    // ------------------------------------------------------------------------
    // SETUP DA INTERRUPÇÃO UART1 (Manual para KL25Z)
    // ------------------------------------------------------------------------
    
    // 1. Conecta a função ISR ao vetor de interrupção da UART1
    // Passamos NULL no último argumento pois a ISR usa a variável global 'uart_com'
    IRQ_CONNECT(UART1_IRQn, 0, uart1_isr_wrapper, NULL, 0);
    
    // 2. Habilita a interrupção no NVIC (Processador)
    irq_enable(UART1_IRQn);

    // 3. Habilita a interrupção de RX no periférico UART (Hardware)
    // UART_C2_RIE_MASK = Receiver Interrupt Enable
    UART1->C2 |= (UART_C2_RIE_MASK);
    // ------------------------------------------------------------------------

#if SOU_MESTRE
    gpio_pin_configure(gpio_sync_dev, PINO_SYNC, GPIO_OUTPUT_LOW);

    while (1) {
        // --- TX ---
        gpio_pin_set(gpio_sync_dev, PINO_SYNC, 1);
        definir_papel(false);
        indicar_tx();

        k_sleep(K_MSEC(50));
        uart_enviar_mensagem_usuario();

        uint64_t fim_tx = k_uptime_get() + (TEMPO_CICLO_MS / 2);
        while(k_uptime_get() < fim_tx) {
            ler_entrada_usuario();
            k_sleep(K_MSEC(10));
        }

        // --- RX ---
        gpio_pin_set(gpio_sync_dev, PINO_SYNC, 0);
        definir_papel(true);
        indicar_rx();

        uint64_t deadline = k_uptime_get() + (TEMPO_CICLO_MS / 2);
        while (k_uptime_get() < deadline) {
            ler_entrada_usuario();
            if (nova_msg_recebida) processar_mensagem_recebida();
            k_sleep(K_MSEC(10));
        }
    }
#else
    // ESCRAVO
    gpio_pin_configure(gpio_sync_dev, PINO_SYNC, GPIO_INPUT | GPIO_PULL_UP);
    bool ja_enviei = false;
    int ultimo = -1;

    while (1) {
        ler_entrada_usuario();
        int estado = gpio_pin_get(gpio_sync_dev, PINO_SYNC);

        if (estado != ultimo) {
            ultimo = estado;
            ja_enviei = false;
        }

        if (estado == 1) { // Mestre fala (TX), eu ouço (RX)
            if (!rx_ativo) {
                definir_papel(true);
                indicar_rx();
            }
            if (nova_msg_recebida) processar_mensagem_recebida();
        }
        else { // Mestre ouve (RX), eu falo (TX)
            if (rx_ativo) definir_papel(false);

            if (!ja_enviei) {
                k_sleep(K_MSEC(50));
                indicar_tx();
                uart_enviar_mensagem_usuario();
                ja_enviei = true;
                indicar_rx();
            }
        }
        k_sleep(K_MSEC(10));
    }
#endif
    return 0;
}
