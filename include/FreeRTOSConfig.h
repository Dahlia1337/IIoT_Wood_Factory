#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Sử dụng thư viện CMSIS của ARM và cấu hình clock cho STM32 */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
    #include <stdint.h>
    extern uint32_t SystemCoreClock;
#endif

/* Cấu hình cơ bản */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      ( 64000000 ) /* Xung nhịp STM32F103: 64MHz */
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 ) /* 1 tick = 1ms */
#define configMAX_PRIORITIES                    ( 5 )
#define configMINIMAL_STACK_SIZE                ( ( uint16_t ) 128 )
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW          0

/* Quản lý RAM (Dành 15KB RAM cho FreeRTOS) */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 5 * 1024 ) )

/* API Inclusion */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_xTaskGetSchedulerState          1

/* Cấu hình mức độ ưu tiên ngắt của Cortex-M3 (Rất quan trọng) */
#define configPRIO_BITS                         4 /* STM32F103 dùng 4 bit cho mức độ ưu tiên */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY              ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY         ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* * ÁNH XẠ NGẮT (Giải quyết xung đột hệ thống)
 * Map các ngắt mặc định của STM32 sang các ngắt xử lý lõi của FreeRTOS 
 */
#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler
// #define xPortSysTickHandler SysTick_Handler



#endif /* FREERTOS_CONFIG_H */