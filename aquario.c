#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "inc/ssd1306.h"
#include <string.h>
#include <stdio.h>

#define LED_WIFI_PIN 11   // LED verde para indicar Wi-Fi conectado
#define LED_WHITE_PIN 8  // LED azul
#define LED_BLUE_PIN 9   // LED branco
#define BUTTON_A_PIN 5     // Botão 1
#define BUTTON_B_PIN 6     // Botão 2

#define WIFI_SSID "Nome da Rede"
#define WIFI_PASS "senha"

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

bool led_white_state = false;
bool led_blue_state = false;

// Função para ler a temperatura do sensor 

float ler_temperatura() {
    // Pino conecção so Sensor
    adc_select_input(4); 
    uint16_t raw = adc_read();
    float voltage = raw * 3.3f / (1 << 12);
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temperature;
}

// Configuração do display OLED

void setup_display() {
    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();
}

// Atualiza as informações do display com temperatura e título

void atualizar_display(float temperature) {
    struct render_area frame_area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&frame_area);

    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    ssd1306_draw_string(ssd, 10, 10, "Aquario_TEC");

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Temp: %.1fC", temperature);
    ssd1306_draw_string(ssd, 10, 30, buffer);

    render_on_display(ssd, &frame_area);
}

// Página Web do servidor

char http_response[2048];

void update_http_response() {
    snprintf(http_response, sizeof(http_response),
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
            "<!DOCTYPE html>"
            "<html lang='pt-br'>"
            "<head>"
            "  <meta charset='UTF-8'>"
            "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
            "  <title>Aquário Tec</title>"
            "  <style>"

            "    @import url('https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;600&display=swap');"

            "    * { margin: 0; padding: 0; box-sizing: border-box; }"
            "    body { display: flex; flex-direction: column; background-color: #C5FBFB; align-items: center; justify-content: center; height: 70vh; min-height: 100vh; text-align: center; font-family: 'Poppins', sans-serif; }"
            "    .container { width: 100%; max-width: 400px; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); background: #3AD9D9; }"
            "    h1 { font-size: 2.5rem; color: #00796B; margin-bottom: 10px; }"
            "    .btn-on1 { background-color: white; color: black; padding: 10px; border-radius: 5px; }"
            "    .btn-on2 { background-color: blue; color: white; padding: 10px; border-radius: 5px; }"
            "    .btn-off { background-color: red; color: white; padding: 10px; border-radius: 5px; }"
            "    .button-container { display: flex; flex-direction: column; gap: 10px; margin-top: 15px; }"
            "    .temperature-display { font-size: 16px; background: #FFFE44; color: black; margin: 10px 0; border: 2px solid #007bff; padding: 10px; border-radius: 10px; }"

            "  </style>"
            "</head>"
            "<body>"
            "   <div> <h1>Aquário Tec</h1> </div>"

            "  <div class='container'>"
            "    <h2>Status do Aquário</h2>"
            "    <p>Luz branca: %s</p>"
            "    <p>Luz Azul: %s</p>"
            "    <div class='temperature-display'>Temperatura da Água: %.2f °C</div>"

            "    <div class='button-container'>"
            "      <button class='btn-on1' onclick=\"window.location.href='/led/white/on'\">Luz branca</button>"
            "      <button class='btn-off' onclick=\"window.location.href='/led/white/off'\">Desligar</button>"
            "      <button class='btn-on2' onclick=\"window.location.href='/led/blue/on'\">Luz Azul</button>"
            "      <button class='btn-off' onclick=\"window.location.href='/led/blue/off'\">Desligar</button>"
            "    </div>"

            "  </div>"
            "</body>"
            "</html>",
             led_white_state ? "Ligado" : "Desligado",
             led_blue_state ? "Ligado" : "Desligado",
             ler_temperatura());
}

// Callback para lidar com requisições HTTP

static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *request = (char *)p->payload;
    if (strstr(request, "GET /led/white/on")) {
        gpio_put(LED_WHITE_PIN, 1);
        led_white_state = true;
    } else if (strstr(request, "GET /led/white/off")) {
        gpio_put(LED_WHITE_PIN, 0);
        led_white_state = false;
    } else if (strstr(request, "GET /led/blue/on")) {
        gpio_put(LED_BLUE_PIN, 1);
        led_blue_state = true;
    } else if (strstr(request, "GET /led/blue/off")) {
        gpio_put(LED_BLUE_PIN, 0);
        led_blue_state = false;
    }

    update_http_response();
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);
    pbuf_free(p);
    return ERR_OK;
}

// Callback para novas conexões HTTP

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);
    return ERR_OK;
}

static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return;

    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) return;

    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
}

// Monitoramento dos botões físicos para alterar LEDs

void monitor_buttons() {
    static bool last_button1_state = false;
    static bool last_button2_state = false;

    bool button1_state = !gpio_get(BUTTON_A_PIN);
    bool button2_state = !gpio_get(BUTTON_B_PIN);

    if (button1_state && !last_button1_state) {
        led_white_state = !led_white_state;
        gpio_put(LED_WHITE_PIN, led_white_state);
        update_http_response();
    }

    if (button2_state && !last_button2_state) {
        led_blue_state = !led_blue_state;
        gpio_put(LED_BLUE_PIN, led_blue_state);
        update_http_response();
    }

    last_button1_state = button1_state;
    last_button2_state = button2_state;
    update_http_response();
}

// Função principal

int main() {

    adc_init();
    adc_set_temp_sensor_enabled(true);

    sleep_ms(10000);
    printf("Iniciando servidor HTTP\n");

    gpio_init(LED_WIFI_PIN);
    gpio_set_dir(LED_WIFI_PIN, GPIO_OUT);
    gpio_put(LED_WIFI_PIN, 0);

    // Inicializa o Wi-Fi

    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    }else {
        printf("WIFI Connectedo.\n");
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
        gpio_put(LED_WIFI_PIN, 1);
    }


    // Configuração de LEDs e botões

    gpio_init(LED_WHITE_PIN);
    gpio_set_dir(LED_WHITE_PIN, GPIO_OUT);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);

    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);

    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);

    printf("Botões configurados com pull-up nos pinos %d e %d\n", BUTTON_A_PIN, BUTTON_B_PIN);

    // Inicializa o display

    setup_display();

    start_http_server();

    while (true) {
        cyw43_arch_poll();
        float temperature = ler_temperatura();
        atualizar_display(temperature);
        monitor_buttons();
        sleep_ms(100);
    }

    return 0;
}   
