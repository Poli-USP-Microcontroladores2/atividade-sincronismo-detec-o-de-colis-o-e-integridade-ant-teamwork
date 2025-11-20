/*
 * TRANSCEPTOR FINAL (Versão Limpa)
 *
 * CONFIGURAÇÃO:
 * - SOU_MESTRE 1: Placa Mestre.
 * - SOU_MESTRE 0: Placa Escrava.
 *
 * ALTERAÇÕES:
 * - Contadores numéricos removidos.
 * - Mensagens de texto fixas ("Mestre a falar", "Escravo a responder").
 * - Feedback visual de estado (RX HABILITADO/DESABILITADO) mantido.
 */

#define SOU_MESTRE 1  // <--- 1 = MESTRE, 0 = ESCRAVO

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdbool.h>

// --- Configurações ---
#define TEMPO_CICLO_MESTRE_SEG 5  
#define TIMEOUT_CONEXAO_MS 15000  
#define RX_BUF_SIZE 64

// --- Hardware ---
#define UART_DEVICE_NODE DT_NODELABEL(uart0)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

#define GPIO_NODE DT_NODELABEL(gpiob)
#define PINO_SYNC 1 // PTB1

static const struct device *gpio_dev = DEVICE_DT_GET(GPIO_NODE);
static struct gpio_callback escravo_cb_data;

// --- Variáveis Globais ---
static char rx_buf[RX_BUF_SIZE];
static volatile int rx_buf_pos = 0;

// Bandeiras de Controle
static volatile bool rx_ativo = false; 
static volatile bool tx_permitido = false; 

// Controle de Conexão do Escravo
static volatile bool mestre_presente = false;
static volatile uint64_t last_sync_time = 0;

// ============================================================================
// FUNÇÕES
// ============================================================================

static void definir_papel(bool modo_ouvinte)
{
    unsigned int key = irq_lock();
    if (modo_ouvinte) {
        rx_ativo = true;
        tx_permitido = false; 
        rx_buf_pos = 0;      
    } else {
        rx_ativo = false;     
        tx_permitido = true;  
    }
    irq_unlock(key);
}

static void uart_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);
    while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
        uint8_t c;
        while (uart_fifo_read(dev, &c, 1) == 1) {
            if (rx_ativo) {
                if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
                    rx_buf[rx_buf_pos] = '\0';
                    // Imprime mensagem recebida sem contador
                    printk("RX MSG: %s\n", rx_buf);
                    rx_buf_pos = 0;
                } else if (rx_buf_pos < (RX_BUF_SIZE - 1)) {
                    rx_buf[rx_buf_pos++] = c;
                }
            }
        }
    }
}

static void uart_enviar(const char *buf)
{
    int msg_len = strlen(buf);
    for (int i = 0; i < msg_len; i++) {
        uart_poll_out(uart_dev, buf[i]);
    }
}

// ============================================================================
// CALLBACK ESCRAVO
// ============================================================================
#if !SOU_MESTRE
void escravo_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    mestre_presente = true;
    last_sync_time = k_uptime_get();

    int pino_val = gpio_pin_get(dev, PINO_SYNC);

    if (pino_val == 1) {
        definir_papel(true); // Mestre TX -> Escravo RX
    } else {
        definir_papel(false); // Mestre RX -> Escravo TX
    }
}
#endif

// ============================================================================
// MAIN
// ============================================================================
void main(void)
{
    if (!device_is_ready(uart_dev) || !device_is_ready(gpio_dev)) return;

    uart_irq_callback_user_data_set(uart_dev, uart_isr, NULL);
    uart_irq_rx_enable(uart_dev); 

    // -----------------------------------------------------------------------
    // MESTRE
    // -----------------------------------------------------------------------
    #if SOU_MESTRE
    printk("--- MESTRE (PTB1 OUTPUT) ---\n");
    gpio_pin_configure(gpio_dev, PINO_SYNC, GPIO_OUTPUT_LOW);
    // Removido: int contador = 0;

    while (1) {
        // FASE TX
        definir_papel(false); 
        gpio_pin_set(gpio_dev, PINO_SYNC, 1); 
        printk("\n[MESTRE] TX ATIVO (Sinalizando 1)\n");

        for(int i=0; i<3; i++) {
            // Mensagem fixa sem contador
            uart_enviar("Mestre: A transmitir dados...\r\n");
            k_sleep(K_MSEC(500));
        }
        k_sleep(K_MSEC(2000));

        // FASE RX
        definir_papel(true); 
        gpio_pin_set(gpio_dev, PINO_SYNC, 0);
        printk("\n[MESTRE] RX HABILITADO (Sinalizando 0)\n");

        k_sleep(K_MSEC(TEMPO_CICLO_MESTRE_SEG * 1000));
    }

    // -----------------------------------------------------------------------
    // ESCRAVO
    // -----------------------------------------------------------------------
    #else
    printk("--- ESCRAVO (PTB1 INPUT) ---\n");
    
    gpio_pin_configure(gpio_dev, PINO_SYNC, GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&escravo_cb_data, escravo_cb, BIT(PINO_SYNC));
    gpio_add_callback(gpio_dev, &escravo_cb_data);
    gpio_pin_interrupt_configure(gpio_dev, PINO_SYNC, GPIO_INT_EDGE_BOTH);

    // Estado inicial
    int estado_inicial = gpio_pin_get(gpio_dev, PINO_SYNC);
    if(estado_inicial == 1) definir_papel(true);  
    else                    definir_papel(false); 
    
    if(estado_inicial == 0) {
        mestre_presente = true;
        last_sync_time = k_uptime_get();
    }

    // Removido: int contador_slv = 0;
    uint64_t next_auto_toggle = k_uptime_get();
    bool ultimo_estado_rx_impresso = !rx_ativo; 

    while (1) {
        uint64_t now = k_uptime_get();

        // Timeout e Polling
        if (mestre_presente && (now - last_sync_time > TIMEOUT_CONEXAO_MS)) {
            mestre_presente = false;
            printk("\n!!! MESTRE PERDIDO !!!\n");
        }
        if (gpio_pin_get(gpio_dev, PINO_SYNC) == 0) {
            if (!mestre_presente) printk("!!! MESTRE ENCONTRADO !!!\n");
            mestre_presente = true;
            last_sync_time = now; 
        }

        if (mestre_presente) {
            // --- MODO SINCRONIZADO ---
            int val_real = gpio_pin_get(gpio_dev, PINO_SYNC);
            if (val_real == 1 && !rx_ativo) definir_papel(true);
            if (val_real == 0 && rx_ativo)  definir_papel(false);

            if (rx_ativo != ultimo_estado_rx_impresso) {
                ultimo_estado_rx_impresso = rx_ativo;
                if (rx_ativo) {
                    printk("\n[SYNC] RX HABILITADO (Ouvindo Mestre)\n");
                } else {
                    printk("\n[SYNC] RX DESABILITADO / TX ATIVO (Minha vez)\n");
                }
            }

            if (tx_permitido) {
                // Mensagem fixa sem contador
                uart_enviar("Escravo: A responder...\r\n");
                k_sleep(K_MSEC(1000));
            } else {
                k_sleep(K_MSEC(100));
            }

        } else {
            // --- MODO AUTONOMO ---
            if (now >= next_auto_toggle) {
                bool novo = !rx_ativo;
                definir_papel(novo); 
                ultimo_estado_rx_impresso = novo; 
                
                if (novo) printk("[AUTO] RX Habilitado\n");
                else      printk("[AUTO] TX Ativo\n");

                next_auto_toggle = now + K_MSEC(5000);
            }
            k_sleep(K_MSEC(100));
        }
    }
    #endif
}
