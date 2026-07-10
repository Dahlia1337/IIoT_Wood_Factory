#include "task_modbus_communication.h"

/* External variables declared in global.h / main.c */
extern UART_HandleTypeDef huart1;
SemaphoreHandle_t xModbusSemaphore = NULL;
uint16_t g_ModbusRegisters[MODBUS_HOLDING_REG_COUNT] = {0};

/* Shared global receive buffer where ISR stores incoming 8-byte frames */
uint8_t g_modbus_rx_frame[MODBUS_RX_BUFFER_SIZE] = {0};

/**
 * @brief Computes the standard Modbus RTU CRC-16 (Polynomial: 0xA001).
 */
uint16_t Modbus_CalculateCRC(uint8_t *buffer, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= buffer[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief validates FC03 request and transmits the response back to Master.
 */
void Modbus_ProcessRequest(uint8_t *rx_buf, uint16_t length)
{
    if (length != MODBUS_RX_BUFFER_SIZE)
    {
        return; /* Invalid frame size for basic FC03 request */
    }

    /* 1. Check Slave ID */
    if (rx_buf[0] != MODBUS_SLAVE_ID)
    {
        return; /* Ignore message if it's not targeted to this slave */
    }

    /* 2. Check Function Code (Only FC03 is supported in this layer) */
    if (rx_buf[1] != MODBUS_FC_READ_HOLDING_REGS)
    {
        /* Modbus Exception: Illegal Function (Optional implementation) */
        return;
    }

    /* 3. Validate CRC Checksum */
    uint16_t received_crc = ((uint16_t)rx_buf[7] << 8) | rx_buf[6]; /* Modbus CRC is Low-Byte First */
    uint16_t calculated_crc = Modbus_CalculateCRC(rx_buf, length - 2);

    if (received_crc != calculated_crc)
    {
        return; /* Checksum failure, discard packet to prevent corruption */
    }

    /* 4. Parse Register Address and Register Quantity */
    uint16_t start_address = ((uint16_t)rx_buf[2] << 8) | rx_buf[3];
    uint16_t reg_quantity  = ((uint16_t)rx_buf[4] << 8) | rx_buf[5];

    /* Boundary Check: Ensure the Master isn't reading out of bounds */
    if ((start_address + reg_quantity) > MODBUS_HOLDING_REG_COUNT)
    {
        /* Out of bounds exception handling can be placed here */
        return;
    }

    /* 5. Construct Response Packet */
    uint8_t tx_buf[MODBUS_TX_BUFFER_SIZE];
    uint8_t byte_count = (uint8_t)(reg_quantity * 2);
    uint16_t tx_index = 0;

    tx_buf[tx_index++] = MODBUS_SLAVE_ID;
    tx_buf[tx_index++] = MODBUS_FC_READ_HOLDING_REGS;
    tx_buf[tx_index++] = byte_count;

    /* Load Register Data into packet (Big Endian standard for Modbus Data field) */
    for (uint16_t i = 0; i < reg_quantity; i++)
    {
        uint16_t current_reg_val = g_ModbusRegisters[start_address + i];
        tx_buf[tx_index++] = (uint8_t)((current_reg_val >> 8) & 0xFF); /* High Byte */
        tx_buf[tx_index++] = (uint8_t)(current_reg_val & 0xFF);        /* Low Byte */
    }

    /* Calculate and append CRC for the outgoing response */
    uint16_t response_crc = Modbus_CalculateCRC(tx_buf, tx_index);
    tx_buf[tx_index++] = (uint8_t)(response_crc & 0xFF);        /* CRC Low Byte */
    tx_buf[tx_index++] = (uint8_t)((response_crc >> 8) & 0xFF); /* CRC High Byte */

    /* 6. Non-blocking/Blocking UART Transmission back to Modbus Master */
    HAL_UART_Transmit(&huart1, tx_buf, tx_index, 100);
}

/**
 * @brief Modbus Communication Task loop.
 */
void vTaskModbusCommunication(void *pvParameters)
{
    /* Prevent unused parameter warning */
    (void)pvParameters;

    for (;;)
    {
        /* Block indefinitely waiting for the UART RX ISR to signal data arrival */
        if (xSemaphoreTake(xModbusSemaphore, portMAX_DELAY) == pdTRUE)
        {
            /* Process the static buffer filled by the ISR */
            Modbus_ProcessRequest(g_modbus_rx_frame, MODBUS_RX_BUFFER_SIZE);
        }
    }
}