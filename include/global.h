#ifndef GLOBAL_H
#define GLOBAL_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "string.h"
#include "semphr.h"

/* Mode Configuration: 1 = Simulation, 0 = Real Hardware */
#define SIMULATION_MODE 0

/* =========================================================================
 * System Thresholds for Woodworking Environment
 * ========================================================================= */
#define TEMP_MAX_THRESHOLD      (80U)
#define DUST_MAX_THRESHOLD      (130U)

/* =========================================================================
 * GPIO Pin Definitions for Proteus Simulation
 * ========================================================================= */
#define LED_HEARTBEAT_PIN       GPIO_PIN_13
#define LED_HEARTBEAT_PORT      GPIOC

#define LED_WARNING_PIN         GPIO_PIN_0
#define LED_WARNING_PORT        GPIOB

/* ------------------------------------------------------------------------- */
/* MODBUS CONFIGURATIONS & GLOBAL VARIABLES                                  */
/* ------------------------------------------------------------------------- */

#define MODBUS_SLAVE_ID             1
#define MODBUS_HOLDING_REG_COUNT    4

/* Modbus Register Mapping Index */
#define REG_INDEX_TEMPERATURE       0
#define REG_INDEX_VIBRATION         1
#define REG_INDEX_DUST              2
#define REG_INDEX_SYS_STATUS        3

/**
 * @brief Global binary semaphore handle for Modbus RTU synchronization.
 * This semaphore is given by the UART RX ISR and taken by the Modbus Task.
 */
extern SemaphoreHandle_t xModbusSemaphore;

/**
 * @brief Modbus Holding Registers array mapping to system data.
 * g_ModbusRegisters[0] : Temperature
 * g_ModbusRegisters[1] : Vibration
 * g_ModbusRegisters[2] : Dust
 * g_ModbusRegisters[3] : System Status
 */
extern uint16_t g_ModbusRegisters[MODBUS_HOLDING_REG_COUNT];

/* =========================================================================
 * Global Data Structures
 * ========================================================================= */

 /* Shared System Data Structure */
typedef struct {
    uint16_t temperature;
    uint16_t vibration;
    uint16_t dust_density;
    uint8_t system_status; /* 0: Normal, 1: Warning / Exhaust Fan Active */
} IndustrialSystemData_t;

/* Global Inter-Task Interprocess Communication Queue */
extern QueueHandle_t xSensorQueue;

#endif /* GLOBAL_H */