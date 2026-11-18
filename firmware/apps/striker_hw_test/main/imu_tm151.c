/*
 * Prueba de la IMU TM151 de Striker 1.
 *
 * La TM151 responde a solicitudes: no basta con escuchar. Igual que robot-mov,
 * este codigo usa la biblioteca EasyProfile del fabricante
 * (components/sensors/tm151/v1_striker1) para pedir los angulos (EP_CMD_RPY_)
 * y para decodificar la respuesta. Solo se compilan los archivos del protocolo,
 * no EasyRetrieve.c, que depende de la HAL vieja.
 */
#include "imu_tm151.h"

#include <stdio.h>

#include "EasyProfile.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TM151_UART UART_NUM_1
#define TM151_BAUD 115200 // los firmwares viejos piden 115600; la diferencia (0,35 %) no afecta

int tm151_test(int tx_gpio, int rx_gpio, int secs, bool (*stop_requested)(void))
{
    static bool protocol_ready = false;
    if (!protocol_ready) {
        EasyProfile_C_Interface_Init(); // reserva memoria: solo una vez
        protocol_ready = true;
    }

    uart_config_t ucfg = {
        .baud_rate = TM151_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    if (uart_driver_install(TM151_UART, 2048, 0, 0, NULL, 0) != ESP_OK) {
        printf("ERROR: no se pudo abrir la UART de la IMU.\n");
        return 1;
    }
    if (uart_param_config(TM151_UART, &ucfg) != ESP_OK ||
        uart_set_pin(TM151_UART, tx_gpio, rx_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        printf("ERROR: no se pudo configurar la UART de la IMU.\n");
        uart_driver_delete(TM151_UART);
        return 1;
    }

    printf("Pidiendo los angulos a la TM151 10 veces por segundo durante %d s\n", secs);
    printf("(ESP32 TX GPIO%d -> IMU, IMU -> ESP32 RX GPIO%d). Gira el robot con la mano.\n", tx_gpio, rx_gpio);
    printf("Presiona Enter para terminar.\n");

    long bytes = 0;
    int answers = 0;
    uint8_t buf[128];
    for (int k = 0; k < secs * 10; k++) {
        char *tx = NULL;
        int ntx = 0;
        if (EasyProfile_C_Interface_TX_Request(EP_ID_BROADCAST_, EP_CMD_RPY_, &tx, &ntx) == EP_SUCC_) {
            uart_write_bytes(TM151_UART, tx, ntx);
        }
        int n = uart_read_bytes(TM151_UART, buf, sizeof(buf), pdMS_TO_TICKS(100));
        if (n > 0) {
            bytes += n;
            Ep_Header header;
            if (EasyProfile_C_Interface_RX((char *)buf, n, &header) == EP_SUCC_ && header.cmd == EP_CMD_RPY_) {
                answers++;
                if (answers % 2 == 1) { // unas 5 lineas por segundo
                    printf("alabeo %7.1f  cabeceo %7.1f  rumbo %7.1f\n",
                           (double)ep_RPY.roll, (double)ep_RPY.pitch, (double)ep_RPY.yaw);
                }
            }
        }
        if (stop_requested()) {
            break;
        }
    }
    uart_driver_delete(TM151_UART);

    printf("\nBytes recibidos: %ld. Respuestas de angulos validas: %d.\n", bytes, answers);
    if (answers > 0) {
        printf("La TM151 responde y entrega angulos: funciona.\n");
    } else if (bytes > 0) {
        printf("Llegan datos pero no respuestas validas: revisa la velocidad (baudios) configurada en la IMU.\n");
    } else {
        printf("La IMU no respondio. Revisa su alimentacion y que TX/RX no esten cruzados al reves.\n");
    }
    return 0;
}
