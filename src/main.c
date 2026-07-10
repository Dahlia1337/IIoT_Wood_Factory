#include "global.h"
#include "task_sensor.h"
#include "task_control_logic.h"
#include "task_modbus_communication.h"
#include "task.h"

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void); 
extern void xPortSysTickHandler(void);

QueueHandle_t xSensorQueue = NULL;
ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim2;

static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);

/* Modbus ISR global variables */
extern uint8_t g_modbus_rx_frame[MODBUS_RX_BUFFER_SIZE];
uint8_t g_rx_byte = 0;
uint8_t g_rx_index = 0;

int main(void)
{
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();
    
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_ADC1_Init();
    MX_TIM2_Init();

    /* 1. Initialize FreeRTOS Binary Semaphore for Modbus synchronization */
    xModbusSemaphore = xSemaphoreCreateBinary();

    /* Initialize inter-task queue */
    xSensorQueue = xQueueCreate(5, sizeof(IndustrialSystemData_t));

    if ((xSensorQueue != NULL) && (xModbusSemaphore != NULL))
    {
        HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1);

        xTaskCreate(vTaskSensorSimulation, "SensorSim", 128, NULL, 1, NULL);
        xTaskCreate(vTaskControlLogic, "CtrlLogic", 128, NULL, 2, NULL);
        xTaskCreate(vTaskModbusCommunication, "ModbusComm", 256, NULL, 4, NULL);

        /* Start the FreeRTOS scheduler */
        vTaskStartScheduler(); 
    }

    /* Infinite loop fallback - Only reached if heap memory is insufficient */
    while (1)
    {
        HAL_Delay(1000);
    }
}

/**
  * @brief  Rx Transfer completed callbacks.
  * @param  huart pointer to a UART_HandleTypeDef structure that contains
  * the configuration information for the specified UART module.
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* Store incoming byte into the Modbus RX buffer */
        if (g_rx_index < MODBUS_RX_BUFFER_SIZE)
        {
            g_modbus_rx_frame[g_rx_index++] = g_rx_byte;
        }

        /* Check if a full Modbus RTU standard frame (8 bytes) has been received */
        if (g_rx_index >= MODBUS_RX_BUFFER_SIZE)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;

            /* Unblock the Modbus communication task immediately from ISR */
            xSemaphoreGiveFromISR(xModbusSemaphore, &xHigherPriorityTaskWoken);

            /* Reset the index buffer for the next incoming request frame */
            g_rx_index = 0;

            /* Perform a context switch if the Modbus task has a higher priority */
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }

        /* Re-enable UART receive interrupt for the next byte */
        HAL_UART_Receive_IT(&huart1, &g_rx_byte, 1);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2; 
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;             
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        while(1);
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        while(1);
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable peripheral clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* Set default output states */
    HAL_GPIO_WritePin(LED_WARNING_PORT, LED_WARNING_PIN, GPIO_PIN_SET); 
    HAL_GPIO_WritePin(LED_HEARTBEAT_PORT, LED_HEARTBEAT_PIN, GPIO_PIN_RESET);

    /* 1. PORT A CONFIGURATION (Strictly preserves old pins & sets UART1) */
    // PA0: TIM2_CH1 Input Capture
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA1 & PA2: ADC1_CH1 & ADC1_CH2 Analog Inputs
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA9: USART1_TX (Alternate Function Output Push-Pull)
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA10: USART1_RX (Input Floating)
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 2. PORT B CONFIGURATION (Warning LED Pin - PB0, Active LOW) */
    GPIO_InitStruct.Pin = LED_WARNING_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_WARNING_PORT, &GPIO_InitStruct);

    /* 3. PORT C CONFIGURATION (Heartbeat LED Pin - PC13, Active HIGH) */
    GPIO_InitStruct.Pin = LED_HEARTBEAT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_HEARTBEAT_PORT, &GPIO_InitStruct);

    /* Enable USART1 Global Interrupt inside NVIC */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0); /* Priority must be lower than configMAX_SYSCALL_INTERRUPT_PRIORITY */
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void MX_ADC1_Init(void)
{
    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        while(1);
    }
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        while(1);
    }
}

static void MX_TIM2_Init(void)
{
    TIM_IC_InitTypeDef sConfigIC = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 71;         
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFF;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
    {
        while(1);
    }

    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 4;
    if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
    {
        while(1);
    }

    HAL_TIM_IC_Start(&htim2, TIM_CHANNEL_1);
}

void SysTick_Handler(void)
{
    HAL_IncTick();
    #if (configUSE_TIMERS == 1) || (configUSE_PREEMPTION == 1)
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
    #endif
}