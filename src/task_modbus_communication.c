#include "task_modbus_communication.h"

void vTaskModbusCommunication(void *pvParameters)
{
    /* Task initialization: Setup UART for Modbus RTU */

    for(;;)
    {
        /* TODO: Listen for Modbus Master requests via UART */
        /* TODO: Serialize and transmit the IndustrialSystemData_t payload */
        
        vTaskDelay(pdMS_TO_TICKS(10)); /* Polling interval */
    }
}