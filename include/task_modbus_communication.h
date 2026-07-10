#ifndef __TASK_MODBUS_COMMUNICATION_H
#define __TASK_MODBUS_COMMUNICATION_H

#include "global.h"

#define MODBUS_RX_BUFFER_SIZE       8
#define MODBUS_TX_BUFFER_SIZE       13  /* 1 (ID) + 1 (FC) + 1 (ByteCount) + 8 (Data) + 2 (CRC) */
#define MODBUS_FC_READ_HOLDING_REGS 0x03

/**
 * @brief Computes the standard Modbus RTU CRC-16 checksum.
 * @param buffer Pointer to the data array.
 * @param length Length of the data array.
 * @return 16-bit CRC checksum value.
 */
uint16_t Modbus_CalculateCRC(uint8_t *buffer, uint16_t length);

/**
 * @brief Parses the received Modbus request frame and sends a response if valid.
 * @param rx_buf Pointer to the received 8-byte Modbus request buffer.
 * @param length Length of the received frame (should be 8).
 */
void Modbus_ProcessRequest(uint8_t *rx_buf, uint16_t length);

/**
 * @brief FreeRTOS task responsible for handling Modbus RTU Slave communication.
 * @param pvParameters Pointer to task parameters (unused).
 */
void vTaskModbusCommunication(void *pvParameters);

#endif /* __TASK_MODBUS_COMMUNICATION_H */