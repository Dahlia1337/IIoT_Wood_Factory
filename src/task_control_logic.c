#include "task_control_logic.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;

void vTaskControlLogic(void *pvParameters) {
    IndustrialSystemData_t received_data;
    // char debug_buffer[64];
    
    TickType_t xHeartbeatWakeTime;
    const TickType_t xHeartbeatFrequency = pdMS_TO_TICKS(1000);

    /* Initialize state */
    HAL_GPIO_WritePin(LED_WARNING_PORT, LED_WARNING_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_HEARTBEAT_PORT, LED_HEARTBEAT_PIN, GPIO_PIN_RESET);
    
    xHeartbeatWakeTime = xTaskGetTickCount();
    received_data.system_status = 0;

    for (;;) {
        /* Chuyển sang quét Queue có thời gian chờ ngắn thay vì treo vô hạn, 
           để luồng code bên dưới có cơ hội chạy kiểm tra nhấp nháy LED độc lập */
        if (xQueueReceive(xSensorQueue, &(received_data), pdMS_TO_TICKS(10)) == pdPASS) {
            
            /* 1. Update sensor data to Modbus Holding Registers */
            g_ModbusRegisters[REG_INDEX_TEMPERATURE] = received_data.temperature;
            g_ModbusRegisters[REG_INDEX_VIBRATION]   = received_data.vibration;
            g_ModbusRegisters[REG_INDEX_DUST]        = received_data.dust_density;
            g_ModbusRegisters[REG_INDEX_SYS_STATUS]  = (uint16_t)received_data.system_status;

            /* 2. Evaluate safety conditions against configured thresholds */
            if ((received_data.temperature > TEMP_MAX_THRESHOLD) || 
                (received_data.dust_density > DUST_MAX_THRESHOLD)) {
                
                received_data.system_status = 1;
                HAL_GPIO_WritePin(LED_WARNING_PORT, LED_WARNING_PIN, GPIO_PIN_RESET);
            } else {
                received_data.system_status = 0;
                HAL_GPIO_WritePin(LED_WARNING_PORT, LED_WARNING_PIN, GPIO_PIN_SET);
            }

            /* --- COMMENT OUT OR REMOVE DEBUG TRANSMISSION TO PREVENT MODBUS CORRUPTION --- */
            /*
            sprintf(debug_buffer, "Temp: %d C | Dust: %d | Status: %d\r\n", 
                    received_data.temperature, 
                    received_data.dust_density, 
                    received_data.system_status);
                    
            HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 50);
            */
        }

        /* Check and execute independent Heartbeat LED Toggle every 1000ms */
        if ((xTaskGetTickCount() - xHeartbeatWakeTime) >= xHeartbeatFrequency) {
            HAL_GPIO_TogglePin(LED_HEARTBEAT_PORT, LED_HEARTBEAT_PIN);
            xHeartbeatWakeTime += xHeartbeatFrequency;
        }
    }
}