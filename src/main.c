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

int main(void)
{
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock to 64/72 MHz */
    SystemClock_Config();

    /* Khởi tạo GPIO trước để cấp hình toàn bộ chân của Port A, B, C một lần duy nhất */
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_ADC1_Init();
    MX_TIM2_Init();

    /* Khởi tạo hàng đợi inter-task */
    xSensorQueue = xQueueCreate(5, sizeof(IndustrialSystemData_t));

    if (xSensorQueue != NULL)
    {
        /* Create FreeRTOS Tasks */
        xTaskCreate(vTaskSensorSimulation, "SensorSim", 128, NULL, 1, NULL);
        xTaskCreate(vTaskControlLogic, "CtrlLogic", 128, NULL, 2, NULL);
        xTaskCreate(vTaskModbusCommunication, "ModbusComm", 256, NULL, 2, NULL);

        /* Start the FreeRTOS scheduler */
        vTaskStartScheduler(); 
    }

    /* Infinite loop fallback - Chỉ chạm đến nếu thiếu bộ nhớ Heap cấp cho OS */
    while (1)
    {
        HAL_Delay(1000);
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
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2; /* HSI 8MHz / 2 = 4MHz */
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;             /* 4MHz * 16 = 64MHz */
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

    /* Kích hoạt đồng loạt Clock ngoại vi hệ thống */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* Thiết lập mức logic mặc định cho các chân Output */
    HAL_GPIO_WritePin(LED_WARNING_PORT, LED_WARNING_PIN, GPIO_PIN_SET); // Active LOW: SET = Tắt
    HAL_GPIO_WritePin(LED_HEARTBEAT_PORT, LED_HEARTBEAT_PIN, GPIO_PIN_RESET);

    /* 1. CẤU HÌNH TOÀN BỘ CHÂN TRÊN PORT A (Tránh lỗi ghi đè thanh ghi CRL/CRH) */
    // Chân PA0 làm nhiệm vụ Input Capture cho cảm biến bụi
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Chân PA1 & PA2 cấu hình chế độ Analog Input cho ADC1
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Chân PA9 làm nhiệm vụ TX cho UART1 (Alternate Function Output Push-Pull)
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Chân PA10 làm nhiệm vụ RX cho UART1 (Input Floating)
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 2. CẤU HÌNH CHÂN TRÊN PORT B (Warning LED Pin - PB0) */
    GPIO_InitStruct.Pin = LED_WARNING_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_WARNING_PORT, &GPIO_InitStruct);

    /* 3. CẤU HÌNH CHÂN TRÊN PORT C (Heartbeat LED Pin - PC13) */
    GPIO_InitStruct.Pin = LED_HEARTBEAT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_HEARTBEAT_PORT, &GPIO_InitStruct);
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
    htim2.Init.Prescaler = 71;         /* Chia clock 72MHz / 72 = 1MHz */
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