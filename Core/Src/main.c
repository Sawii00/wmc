/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "arm_math.h"
#include "wmc_processing.h"

/* Private variables ---------------------------------------------------------*/
uint8_t ChangeApplicationMode = 0;
uint8_t DoubleClick = 0;

uint8_t AudioLogEnabled = 0;
uint8_t WMCEnabled = 1;

osSemaphoreId_t enableSem_id;
osSemaphoreId_t AUDIOLOGSem_id;
osSemaphoreId_t WMCSem_id;

osTimerId_t doubleClickTimer_id;

osThreadId_t mainThread_id;
const osThreadAttr_t mainThread_attr = {
  .name = "mainThread",
  .stack_size = 128 * 9,
  .priority = (osPriority_t) osPriorityNormal,
};

/* TODO: Change copying from SENSING1 */
#define FILL_BUFFER_SIZE 1024
#define PCM_AUDIO_IN_SAMPLES     (AUDIO_SAMPLING_FREQUENCY / 1000)
float32_t Proc_Buffer_f[FILL_BUFFER_SIZE];
uint16_t PCM_Buffer[PCM_AUDIO_IN_SAMPLES];
int16_t Fill_Buffer[2048];
static volatile uint32_t runs = 0;
static uint32_t index_buff_fill = 0;
static volatile uint8_t add_samples = 0;
extern uint16_t IntermediateBuffer[INTERMEDIATE_BUFFER_SIZE];
extern float32_t Spectrogram[16896];

/* Private function prototypes -----------------------------------------------*/
static void MainThread(void *argument);
static void ArduinoTriggerInit(void);
static void InitSensorTilebox(void);
static void DoubleClickTimerCallback(void *argument);
static void SystemClock_Config(void);
static void WMC_RecordingProcess(void);
void WMC_StartRecording(void);
void WMC_StopRecording(void);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /*Initialize trigger for the Arduino Uno */
  //ArduinoTriggerInit();

  /* Initialize compontents of the SensorTile.box */
  InitSensorTilebox();

  /* Init scheduler */
  osKernelInitialize();

  /* Create the thread */
  mainThread_id = osThreadNew(MainThread, NULL, &mainThread_attr);

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  while (1)
  {
  }
}

static void MainThread(void *argument)
{
  enableSem_id = osSemaphoreNew(1U, 0U, NULL);
  AUDIOLOGSem_id = osSemaphoreNew(1U, 0U, NULL);
  WMCSem_id = osSemaphoreNew(1U, 0U, NULL);

  doubleClickTimer_id = osTimerNew(DoubleClickTimerCallback, osTimerOnce, (void *)0, NULL);

  //AUDIOLOG_SDInit();
 // WMC_Init();

  /* Show everything is ready: default mode is classification */
  BSP_LED_On(LED_GREEN);

  for (;;) {
    /* Wait until user buton releases semaphore */
    osSemaphoreAcquire(enableSem_id, osWaitForever);
    osDelay(600);

    /* ****Change application mode when double click on user button**** */
    if(ChangeApplicationMode == 1) {
      if(WMCEnabled == 1) {
        AudioLogEnabled = 1;
	WMCEnabled = 0;

	/* Blue LED shows that audio logging mode is on */
	BSP_LED_Off(LED_GREEN);
	BSP_LED_On(LED_BLUE);
      }
      else {
        WMCEnabled = 1;
	AudioLogEnabled = 0;

	/* Green LED shows that classsification mode is on */
	BSP_LED_Off(LED_BLUE);
	BSP_LED_On(LED_GREEN);
      }

      ChangeApplicationMode = 0;
    }
    /* ****Run application**** */
    else {
      if(AudioLogEnabled == 1) {
        AUDIOLOG_Enable();
	AUDIOLOG_StartRecording();

      	/* TODO: How many times */
        for(int i=0; i<1000; i++) {
          osSemaphoreAcquire(AUDIOLOGSem_id, osWaitForever);
          AUDIOLOG_Save2SD();
        }
	AUDIOLOG_Disable();
	AUDIOLOG_StopRecording();
      }
      else {
        /* Run wood moisture classification algorithm */
        osDelay(1000);
	add_samples = 0;
	index_buff_fill = 0;
	runs = 0;
	WMC_Init();
	BSP_LED_On(LED_BLUE);
	WMC_StartRecording();
	//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13, GPIO_PIN_SET);

	for(int i=0; i<32; i++) {
          osSemaphoreAcquire(WMCSem_id, osWaitForever);
          WMC_Process(Proc_Buffer_f);
        }
	WMC_StopRecording();
	//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13, GPIO_PIN_RESET);
	ai_float dense_2_out[AI_WMC_OUT_1_SIZE] = {0.0, 0.0, 0.0};
	WMC_Run(Spectrogram, dense_2_out);

	BSP_LED_Off(LED_BLUE);

	WMC_ClassificationResult(dense_2_out);
	BSP_LED_On(LED_GREEN);
     }
    }
  }
}

static void ArduinoTriggerInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
* @brief  Function that is called when data has been appended to Fill Buffer
* @param  none
* @retval None
*/
static void WMC_RecordingProcess(void)
{
  float32_t sample;

  /* Create a 64ms (1024 samples) window every 32ms (512 samples)
   Audio Feature Extraction is ran every 32ms on a 64ms window (50% overlap) */
  if (index_buff_fill >= 1024) {
    add_samples = index_buff_fill - 1024;

    /* Copy Fill Buffer in Proc Buffer */
    for (uint32_t i = 0; i < FILL_BUFFER_SIZE; i++) {
      sample = ((float32_t) Fill_Buffer[i]);
      /* Invert the scale of the data */
      sample /= (float32_t) ((1 << (8 * sizeof(int16_t) - 1)));
      Proc_Buffer_f[i] = sample;
    }

    runs += 1;

    /* Left shift Fill Buffer by 512 samples */
    memmove(Fill_Buffer, Fill_Buffer + (FILL_BUFFER_SIZE / 2), sizeof(int16_t) * ((FILL_BUFFER_SIZE / 2) + add_samples));
    index_buff_fill = (FILL_BUFFER_SIZE / 2 + add_samples);

    osSemaphoreRelease(WMCSem_id);
  }
}

/**
 * @brief Initialize SensorTile.box components
 * @param None
 * @retval None
 */
static void InitSensorTilebox(void)
{
  /* Initialize LEDs */
  BSP_LED_Init(LED_BLUE);
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);

  /* Initialize user button */
  BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);

  /* Initalize SD card and microphone */
  //AUDIOLOG_SDInit();
  AUDIOLOG_InitMic();
}

/**
 * @brief Double click on user button callback function
 * @param None
 * @retval None
 */
static void DoubleClickTimerCallback (void *argument)
{
  DoubleClick = 0;
}

/**
 * @brief  EXTI line detection callback.
 * @param  uint16_t GPIO_Pin Specifies the pin connected EXTI line
 * @retval None
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == KEY_BUTTON_PIN) {
//    if(DoubleClick == 1) {
//      osTimerDelete(doubleClickTimer_id);
//      DoubleClick = 0;
//      ChangeApplicationMode = 1;
//    }
//    else {
//      osTimerStart(doubleClickTimer_id, 500U);
      osSemaphoreRelease(enableSem_id);
//      DoubleClick = 1;
//    }
  }
}

/**
* @brief  Half Transfer user callback, called by BSP functions.
* @param  uint32_t Instance Not used
* @retval None
*/
void BSP_AUDIO_IN_HalfTransfer_CallBack(uint32_t Instance)
{
  if(AudioLogEnabled == 1) {
    AUDIOLOG_RecordingProcess(IntermediateBuffer, INTERMEDIATE_BUFFER_SIZE);
  }
  else {
    /* Copy PCM_Buffer from Microphones onto Fill_Buffer */
    memcpy(Fill_Buffer + index_buff_fill, PCM_Buffer, sizeof(int16_t) * PCM_AUDIO_IN_SAMPLES);
    index_buff_fill += PCM_AUDIO_IN_SAMPLES;

    WMC_RecordingProcess();
  }
}

/**
* @brief  Transfer Complete user callback, called by BSP functions.
* @param  uint32_t Instance Not used
* @retval None
*/
void BSP_AUDIO_IN_TransferComplete_CallBack(uint32_t Instance)
{
  if(AudioLogEnabled == 1) {
    AUDIOLOG_RecordingProcess(IntermediateBuffer, INTERMEDIATE_BUFFER_SIZE);
  }
  else {
    /* Copy PCM_Buffer from Microphones onto Fill_Buffer */
    memcpy(Fill_Buffer + index_buff_fill, PCM_Buffer, sizeof(int16_t) * PCM_AUDIO_IN_SAMPLES);
    index_buff_fill += PCM_AUDIO_IN_SAMPLES;

    WMC_RecordingProcess();
  }
}

/**
  * @brief  Manages the BSP audio in error event.
  * @param  Instance Audio in instance.
  * @retval None.
  */
void BSP_AUDIO_IN_Error_CallBack(uint32_t Instance)
{
  ErrorHandler(ERROR_AUDIO);
}

/**
  * @brief  Start audio recording i.e fill intermediate buffer
  * @param  None
  * @retval None
  */
void WMC_StartRecording(void)
{
  /* Start filling the intermediate buffer */
  if(BSP_AUDIO_IN_Record(BSP_AUDIO_IN_INSTANCE, (uint8_t *)PCM_Buffer, PCM_AUDIO_IN_SAMPLES*2) != BSP_ERROR_NONE) {
    ErrorHandler(ERROR_AUDIO);
  }
}

/**
  * @brief  Stop audio recording
  * @param  None
  * @retval None
  */
void WMC_StopRecording(void)
{
  /* Stop filling the intermediate buffer */
  if(BSP_AUDIO_IN_Stop(BSP_AUDIO_IN_INSTANCE) != BSP_ERROR_NONE) {
    ErrorHandler(ERROR_AUDIO);
  }
}

/**
* @brief  System Clock tree configuration
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /**Configure the main internal regulator output voltage
    */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK) {
    /* Initialization Error */
    while(1);
  }

   /**Configure LSE Drive Capability
    */
  HAL_PWR_EnableBkUpAccess();

  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

    /**Initializes the CPU, AHB and APB busses clocks
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|
                                     RCC_OSCILLATORTYPE_LSE  |
                                     RCC_OSCILLATORTYPE_HSE  |
                                     RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    /* Initialization Error */
    while(1);
  }

    /**Initializes the CPU, AHB and APB busses clocks
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK   |
                                RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1  |
                                RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
    /* Initialization Error */
    while(1);
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SAI1   |
                                      RCC_PERIPHCLK_DFSDM1 |
                                      RCC_PERIPHCLK_USB    |
                                      RCC_PERIPHCLK_RTC    |
                                      RCC_PERIPHCLK_SDMMC1 |
                                      RCC_PERIPHCLK_ADC;

  PeriphClkInit.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLLSAI1;
  PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
  PeriphClkInit.Dfsdm1ClockSelection = RCC_DFSDM1CLKSOURCE_PCLK2;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  PeriphClkInit.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_PLLP;
  PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_HSE;
  PeriphClkInit.PLLSAI1.PLLSAI1M = 5;
  PeriphClkInit.PLLSAI1.PLLSAI1N = 96;
  PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV25;
  PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV4;
  PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV4;
  PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_ADC1CLK|RCC_PLLSAI1_SAI1CLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    /* Initialization Error */
    while(1);
  }
}

/** @brief  Blink LED in function of the error code
  * @param  ErrorType_t ErrorType Error Code
  * @retval None
  */
void ErrorHandler(ErrorType_t ErrorType)
{
  ErrorType_t CountError;
  while(1) {
    for(CountError=ERROR_INIT; CountError<ErrorType; CountError++) {
       BSP_LED_On(LED_RED);
       HAL_Delay(200);
       BSP_LED_Off(LED_RED);
       HAL_Delay(1000);
    }
    HAL_Delay(5000);
  }
}

/****END OF FILE****/

