/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "cmsis_os.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* Task handles */
osThreadId_t defaultTaskHandle;
osThreadId_t Task1Handle;
osThreadId_t Task2Handle;
osThreadId_t Task3Handle;

const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
const osThreadAttr_t Task1_attributes = {
  .name = "Task1",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
const osThreadAttr_t Task2_attributes = {
  .name = "Task2",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
const osThreadAttr_t Task3_attributes = {
  .name = "Task3",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

osSemaphoreId_t myBinarySem1Handle;
const osSemaphoreAttr_t myBinarySem1_attributes = {
  .name = "myBinarySem1"
};

/* Function prototypes -------------------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
void StartDefaultTask(void *argument);
void Task_1(void *argument);
void Task_2(void *argument);
void Task_3(void *argument);

/**
  * @brief  The application entry point.
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();

  osKernelInitialize();


  myBinarySem1Handle = osSemaphoreNew(1, 1, &myBinarySem1_attributes);

  /* Create tasks */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  Task1Handle       = osThreadNew(Task_1, NULL, &Task1_attributes);
  Task2Handle       = osThreadNew(Task_2, NULL, &Task2_attributes);
  Task3Handle       = osThreadNew(Task_3, NULL, &Task3_attributes);

  osKernelStart();

  while (1) {}
}

/* ---------------------------------------------------------------------------
 * defaultTask — idle placeholder
 * --------------------------------------------------------------------------- */
void StartDefaultTask(void *argument)
{
  for(;;)
  {
    osDelay(1);
  }
}

/* ---------------------------------------------------------------------------
 * --------------------------------------------------------------------------- */
void Task_1(void *argument)
{
  char *acq_msg = "Low  Task : Semaphore acquired - holding for 3s...\r\n";
  char *rel_msg = "Low  Task : Semaphore released!\r\n";

  for(;;)
  {
    osSemaphoreAcquire(myBinarySem1Handle, osWaitForever);

    HAL_UART_Transmit(&huart1, (uint8_t*)acq_msg, strlen(acq_msg), 100);

    HAL_Delay(3000);


    HAL_UART_Transmit(&huart1, (uint8_t*)rel_msg, strlen(rel_msg), 100);

    osSemaphoreRelease(myBinarySem1Handle);

    osDelay(1000);
  }
}

/* ---------------------------------------------------------------------------
 * --------------------------------------------------------------------------- */
void Task_2(void *argument)
{
  char *start_msg = "Med  Task : Hogging CPU...\r\n";
  char *done_msg  = "Med  Task : Done hogging.\r\n";

  osDelay(500);

  for(;;)
  {
    HAL_UART_Transmit(&huart1, (uint8_t*)start_msg, strlen(start_msg), 100);


    for(volatile uint32_t i = 0; i < 30000000; i++) { __asm("NOP"); }

    HAL_UART_Transmit(&huart1, (uint8_t*)done_msg, strlen(done_msg), 100);

    osDelay(5000);
  }
}

/* ---------------------------------------------------------------------------
 * Task_3  [HIGH PRIORITY]
 *
 * Should run most often due to highest priority.
 * Instead it gets STARVED because:
 *   1. It blocks on semaphore held by Task1 (LOW)
 *   2. Task1 gets preempted by Task2 (MEDIUM) — no priority boost
 *   3. Task2 hogs CPU for ~3s before Task1 can release
 * Result on UART: Task3 prints far less than expected.
 *
 * FIX: Replace osSemaphoreNew() with osMutexNew() — FreeRTOS will then
 * temporarily boost Task1 to HIGH priority the moment Task3 blocks,
 * letting Task1 finish and release before Task2 ever runs.
 * --------------------------------------------------------------------------- */
void Task_3(void *argument)
{
  char *wait_msg    = "High Task : Waiting for semaphore...\r\n";
  char *success_msg = "High Task : Got semaphore - running!\r\n";

  osDelay(1000);  /* Let Task1 acquire the semaphore first */

  for(;;)
  {
    HAL_UART_Transmit(&huart1, (uint8_t*)wait_msg, strlen(wait_msg), 100);

    osSemaphoreAcquire(myBinarySem1Handle, osWaitForever);  /* BLOCKED — priority inversion happens here */

    HAL_UART_Transmit(&huart1, (uint8_t*)success_msg, strlen(success_msg), 100);

    osSemaphoreRelease(myBinarySem1Handle);

    osDelay(500);
  }
}

/* ---------------------------------------------------------------------------
 * Peripheral Initialization
 * --------------------------------------------------------------------------- */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType    = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState          = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue    = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState          = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState      = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource     = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL        = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

static void MX_USART1_UART_Init(void)
{
  huart1.Instance          = USART1;
  huart1.Init.BaudRate     = 115200;
  huart1.Init.WordLength   = UART_WORDLENGTH_8B;
  huart1.Init.StopBits     = UART_STOPBITS_1;
  huart1.Init.Parity       = UART_PARITY_NONE;
  huart1.Init.Mode         = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin   = GPIO_PIN_13;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1) HAL_IncTick();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
