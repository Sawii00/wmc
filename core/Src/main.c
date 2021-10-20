/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported variables --------------------------------------------------------*/
/* Buffer to store microphone samples */
uint16_t PCMBuffer[PCM_BUFFER_SIZE];

extern uint16_t WMCBuffer[];

osSemaphoreId_t AUDIOLOGSem_id;
osSemaphoreId_t WMCSem_id;

/* Private variables ---------------------------------------------------------*/
uint8_t ChangeApplicationMode = 0;
uint8_t AudioLogEnabled = 1;
uint8_t WMCEnabled = 0;

static volatile uint32_t ArduinoTriggerCounter = 0;

/* Semaphore to enable wmc or audio logging */
osSemaphoreId_t enableSem_id;

/* Timer to detect a double click */
uint8_t DoubleClick = 0;
osTimerId_t doubleClickTimer_id;

/* Main thread definition */
osThreadId_t mainThread_id;
const osThreadAttr_t mainThread_attr = {
  .name = "mainThread",
  .stack_size = 128 * 9,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Initialize microphone parameters */
BSP_AUDIO_Init_t MicParams;

/* Private function prototypes -----------------------------------------------*/
static void MainThread(void *argument);
static void ArduinoTrigger(void);
static void ArduinoTriggerInit(void);
static void DoubleClickTimerCallback(void *argument);
static void SystemClock_Config(void);

/* Init/deinit microphones */
void InitMic(void);
void DeInitMic(void);

/* Start/top audio recording audio logging or wmc */
void StartRecording(void);
void StopRecording(void);

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

  /* Initialize LEDs */
  BSP_LED_Init(LED_BLUE);
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);

  /* Initialize user button */
  BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);

  /* Initalize SD card and microphone */
  AUDIOLOG_SDInit();
  InitMic();

  /* Initialize WMC alogrithm */
  WMC_Init();

  /* Initialize trigger for the Arduino Uno */
  ArduinoTriggerInit();

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

  /* Show everything is ready: default is audio logging */
  BSP_LED_On(LED_GREEN);

  for (;;) {
    /* Wait until user buton releases semaphore */
    osSemaphoreAcquire(enableSem_id, osWaitForever);
    osDelay(600);

    /****Change application mode when double click on user button****/
    if(ChangeApplicationMode == 1) {
      if(WMCEnabled == 1) {
        AudioLogEnabled = 1;
		WMCEnabled = 0;

		/* Green LED shows that audio logging mode is on */
		BSP_LED_Off(LED_BLUE);
		BSP_LED_On(LED_GREEN);
		}
		else {
		WMCEnabled = 1;
		  AudioLogEnabled = 0;

	/* BLUE LED shows that classification mode is on */
	BSP_LED_Off(LED_GREEN);
	BSP_LED_On(LED_BLUE);
      }

      ChangeApplicationMode = 0;
    }
    /****Run application****/
    else {
      if(AudioLogEnabled == 1) {
		osDelay(2000);
		/* Blue LED on while recording */
        BSP_LED_On(LED_BLUE);

		AUDIOLOG_Enable();
		StartRecording();

		/* TODO: Change to own function? AUDIOLOG_Acquisition?
		 * On the other hand this way things are simpler */
		/* Acquisition and saving of samples */
		/* Saving to SD card happens currently every 48*64/2=1536 samples
		 * with 48kHz sampling thus every 32ms */
		uint32_t nb_saves = FRAME_SIZE / 1536 * NB_FRAMES;
        for(int i=0; i<nb_saves; i++) {
          osSemaphoreAcquire(AUDIOLOGSem_id, osWaitForever);
          AUDIOLOG_Save2SD();
        }
		StopRecording();
		AUDIOLOG_Disable();

		BSP_LED_Off(LED_BLUE);
      }
      else {
        /* Run wood moisture classification algorithm */
        osDelay(2000);
		BSP_LED_On(LED_GREEN);
		AUDIOLOG_Enable();
		StartRecording();

		/* TODO: Change to own function? WMC_Acquisition? */
		/* Acquisition and saving of samples */
		for(int i=0; i<NB_FFTS; i++) {
		  osSemaphoreAcquire(WMCSem_id, osWaitForever);

		  /* Check for last if last FFT window */
		  uint8_t at_end = 0;
		  if (i==31) {
			  at_end = 1;
		  }

		  AUDIOLOG_ClassificationSave2SD(at_end);
		  WMC_Process();
		}

		StopRecording();
		AUDIOLOG_Disable();

		ai_float cnn_out[AI_WMC_OUT_1_SIZE] = {0.0, 0.0, 0.0};
		WMC_Run(cnn_out);

		BSP_LED_Off(LED_GREEN);
		BSP_LED_Off(LED_BLUE);

		WMC_ClassificationResult(cnn_out);
		BSP_LED_On(LED_GREEN);
      }
    }
  }
}

/** @brief Initialize microphone
  * @param  None
  * @retval None
  */
void InitMic(void)
{
  MicParams.BitsPerSample = 16;
  MicParams.ChannelsNbr = 1;
  MicParams.Device = AMIC_ONBOARD;
  MicParams.SampleRate = AUDIO_SAMPLING_FREQUENCY;
  MicParams.Volume = 32;

  if(BSP_AUDIO_IN_Init(BSP_AUDIO_IN_INSTANCE, &MicParams) != BSP_ERROR_NONE) {
    ErrorHandler(ERROR_AUDIO);
  }
}

/** @brief DeInitialize microphone
  * @param  None
  * @retval None
  */
void DeInitMic(void)
{
  if(BSP_AUDIO_IN_DeInit(BSP_AUDIO_IN_INSTANCE) != BSP_ERROR_NONE) {
    ErrorHandler(ERROR_AUDIO);
  }
}

/**
  * @brief  Start audio recording i.e fill the PCM buffer
  * @param  None
  * @retval None
  */
void StartRecording(void)
{
  /* Start filling the intermediate buffer */
  if(BSP_AUDIO_IN_Record(BSP_AUDIO_IN_INSTANCE, (uint8_t *)PCMBuffer, PCM_BUFFER_SIZE*2) != BSP_ERROR_NONE) {
    ErrorHandler(ERROR_AUDIO);
  }
}

/**
  * @brief  Stop audio recording
  * @param  None
  * @retval None
  */
void StopRecording(void)
{
  /* Stop filling the intermediate buffer */
  if(BSP_AUDIO_IN_Stop(BSP_AUDIO_IN_INSTANCE) != BSP_ERROR_NONE) {
    ErrorHandler(ERROR_AUDIO);
  }
}

/**
  * @brief  Half Transfer user callback, called by BSP functions.
  * @param  uint32_t Instance Not used
  * @retval None
  */
void BSP_AUDIO_IN_HalfTransfer_CallBack(uint32_t Instance) {
	if(AudioLogEnabled == 1) {
    AUDIOLOG_RecordingProcess(PCMBuffer);
  }
  else {
    WMC_RecordingProcess(PCMBuffer);
  }
}

/**
  * @brief  Transfer Complete user callback, called by BSP functions.
  * @param  uint32_t Instance Not used
  * @retval None
  */
void BSP_AUDIO_IN_TransferComplete_CallBack(uint32_t Instance)
{
  ArduinoTrigger();

  if(AudioLogEnabled == 1) {
    AUDIOLOG_RecordingProcess(PCMBuffer);
  }
  else {
    WMC_RecordingProcess(PCMBuffer);
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
  * @brief Trigger arduino to do the frequency swipe.
  * @param None
  * @retval None
  */
static void ArduinoTrigger(void)
{
  /* Arduino trigger in every spectrogram */
  ArduinoTriggerCounter += PCM_BUFFER_SIZE*2;

  /* Trigger arduino after 1056 samples */
  if (ArduinoTriggerCounter == (PCM_BUFFER_SIZE*2*11)) {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13, GPIO_PIN_SET);
  }
  /* Stop Arduino after 4224 samples */
  else if (ArduinoTriggerCounter == (PCM_BUFFER_SIZE*2*11*4)) {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13, GPIO_PIN_RESET);
  }
  else if ((ArduinoTriggerCounter == FRAME_SIZE) && (AudioLogEnabled == 1)) {
	  ArduinoTriggerCounter = 0;
  }
}

/**
  * @brief Initialize SWDIO pin to trigger the arduino.
  * @param None
  * @retval None
  */
static void ArduinoTriggerInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /*Configure GPIO pin output level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
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

