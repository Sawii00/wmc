//! @file wmc_processing.c

/* Includes ------------------------------------------------------------------*/
#include "wmc_processing.h"

/* Imported variables --------------------------------------------------------*/
/* Buffer to store microphone samples, defined in main.c */
extern uint16_t PCMBuffer[];

extern osSemaphoreId_t WMCSem_id;

/* Private defines   ---------------------------------------------------------*/
#define TOP_DB			 80.0f

/* Private variables ---------------------------------------------------------*/
/* Process buffers for WMC algorithm */
int16_t WMCBuffer[FRAME_SIZE*2];
static float32_t WMCBuffer_f[FRAME_SIZE];
static float32_t WMCBuffer_f2[NB_BINS];
uint32_t WMCBufferWriteIndex;
volatile uint32_t WMCCutOffSamples = 0;


/* Spectrogram which results from FFT */
static float32_t Spectrogram[NB_BINS*NB_FRAMES];
static float32_t SpectrogramColBuffer[NB_BINS];
static uint32_t SpectrogramColIndex;
float32_t WorkingBuffer[FRAME_SIZE];

/* Variables for Spectrogram */
static arm_rfft_fast_instance_f32 S_Rfft;
uint32_t FilterStartIndices[30];
uint32_t FilterStopIndices[30];

/* Private function prototypes -----------------------------------------------*/
static void PreprocessingInit(void);
static void FilterBank(float32_t *pSpectrCol, float32_t *pFilterCol);
static void FilterBankInit(uint32_t *FilterStartIndices, uint32_t *FilterStopIndices);
static void SpectrogramColumn(float32_t *pInSignal, float32_t *pOutCol);
static void StandardizeFeatures(float32_t *pSpectrogram);
static void PowerTodB(float32_t *pSpectrogram);

/* Exported functions --------------------------------------------------------*/
/**
  * @brief  Create and init WMC convolutional neural network
  * @param  None
  * @retval None
  */
void WMC_Init(void)
{
  /* Configure audio preprocessing */
  PreprocessingInit();

 /* Enabling CRC clock for using AI libraries (to check if STM32
  microprocessor is used) */
  __HAL_RCC_CRC_CLK_ENABLE();

  ai_init();
}

/**
  * @brief  DeInit WMC Convolutional Neural Network
  * @param  None
  * @retval None
  */
void WMC_DeInit(void)
{
  /* Disable CRC Clock */
  __HAL_RCC_CRC_CLK_DISABLE();

  ai_deinit();
}

/**
  * @brief  Function that is called when data has been appended to WMC buffer
  * @param  pPCMBuffer  Pointer to PCM buffer, data from microphone
  * @retval None
  */
void WMC_RecordingProcess(uint16_t *pPCMBuffer)
{
  /* Copy PCM_Buffer from microphones onto WMC_Buffer */
  memcpy(WMCBuffer + WMCBufferWriteIndex, pPCMBuffer, sizeof(int16_t) * PCM_BUFFER_SIZE);
  WMCBufferWriteIndex += PCM_BUFFER_SIZE;

  float32_t sample;

  /* Create 1024 (FRAME_SIZE) samples window every 512 samples */
  if (WMCBufferWriteIndex >= FRAME_SIZE) {
    /* The FRAME_SIZE is not in sync with the audio frequency, samples
     which are to much get chopped off and added later */
    WMCCutOffSamples = WMCBufferWriteIndex - FRAME_SIZE;

	/* Copy Fill Buffer in Proc Buffer */
    for (uint32_t i = 0; i < FRAME_SIZE; i++) {
      sample = ((float32_t) WMCBuffer[i]);
      /* Invert the scale of the data */
      sample /= (float32_t) ((1 << (8 * sizeof(int16_t) - 1)));
      WMCBuffer_f[i] = sample;
    }

    /* Left shift WMC_Buffer by 512 samples and add the cut off samples */
    memmove(WMCBuffer, WMCBuffer + (FRAME_SIZE / 2), sizeof(int16_t) * ((FRAME_SIZE / 2) + WMCCutOffSamples));
    WMCBufferWriteIndex = FRAME_SIZE / 2 + WMCCutOffSamples;

    osSemaphoreRelease(WMCSem_id);
  }
}

/**
  * @brief  Process Wood Moisture Classification (WMC) algorithm
  * @note   This function needs to be executed multiple times to extract audio features
  * @retval None
  */
void WMC_Process(void)
{
  /* Create a Mel-scaled spectrogram column */
  SpectrogramColumn(WMCBuffer_f, WMCBuffer_f2);

	FilterBank(WMCBuffer_f2, SpectrogramColBuffer);

  /* Reshape and copy into output spectrogram column */
  for (uint32_t i=0; i<NB_BINS; i++) {
    Spectrogram[i*NB_FRAMES+SpectrogramColIndex] = SpectrogramColBuffer[i];
  }
  SpectrogramColIndex++;

  if (SpectrogramColIndex == NB_FRAMES) {
    /* Set index to 0 for next run of algorithm */
    WMCBufferWriteIndex = 0;
    SpectrogramColIndex = 0;

    PowerTodB(Spectrogram);
    StandardizeFeatures(Spectrogram);
  }
}

/**
  * @brief  WMC convolutional neural net inference
  * @param  pCNNOut outputs of CNN
  * @retval None
  */
void WMC_Run(float32_t *pCNNOut)
{
  ai_run(Spectrogram, pCNNOut);
}

/**
  * @brief  Blink LED in function of classification result
  * @param  pCNNOut outputs of CNN
  * @retval None
  */
void WMC_ClassificationResult(float32_t *pCNNOut)
{
  /* Turn off LEDs */
  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_BLUE);

  float32_t max_out;
  uint32_t classification_result;

/* ArgMax to associate NN output with the most likely classification label */
  max_out = pCNNOut[0];
  classification_result = 0;

  for (uint32_t i = 1; i<3; i++) {
    if (pCNNOut[i] > max_out) {
      max_out = pCNNOut[i];
      classification_result = i;
    }
  }

    if(classification_result == 0) {
      for(uint32_t i = 0; i<5; i++) {
        BSP_LED_On(LED_GREEN);
		HAL_Delay(500);
        BSP_LED_Off(LED_GREEN);
        HAL_Delay(250);
      }
    }
    if(classification_result == 1) {
      for(uint32_t i = 0; i<5; i++) {
        BSP_LED_On(LED_BLUE);
		HAL_Delay(500);
        BSP_LED_Off(LED_BLUE);
        HAL_Delay(250);
      }
    }
    if(classification_result == 2) {
      for(uint32_t i = 0; i<5; i++) {
        BSP_LED_On(LED_RED);
		HAL_Delay(500);
        BSP_LED_Off(LED_RED);
        HAL_Delay(250);
      }
    }
}

/**
  * @brief  Standardize input features
  * @param  pSpectrogram Feature inputs for CNN
  * @retval None
  */
static void StandardizeFeatures(float32_t *pSpectrogram)
{
  /* Zero mean and variance and on input feature */
  for (uint32_t i=0; i<NB_BINS*NB_FRAMES; i++) {
    pSpectrogram[i] = (pSpectrogram[i] - featureScalerMean[i]) / featureScalerStd[i];
  }
}

/**
  * @brief  FFT to get a spectrogram column
  * @param  pInSignal
  * @param	pOutCol
  * @retval None
  */
static void SpectrogramColumn(float32_t *pInSignal, float32_t *pOutCol)
{
  uint32_t frame_len = FRAME_SIZE;
  float32_t *scratch_buffer = WorkingBuffer;

  float32_t first_energy;
  float32_t last_energy;

  /* In-place window application (on signal length, not entire n_fft) */
  /* @note: OK to typecast because hannWin content is not modified */
  arm_mult_f32(pInSignal, (float32_t*) hannWin_1024, pInSignal, frame_len);

  /* Zero pad if signal frame length is shorter than n_fft */
  memset(&pInSignal[frame_len], 0, FRAME_SIZE - frame_len);

  /* FFT */
  arm_rfft_fast_f32(&S_Rfft, pInSignal, scratch_buffer, 0);

  /* Power spectrum */
  first_energy = scratch_buffer[0] * scratch_buffer[0];
  last_energy = scratch_buffer[1] * scratch_buffer[1];
  pOutCol[0] = first_energy;
  arm_cmplx_mag_squared_f32(&scratch_buffer[2], &pOutCol[1], (FRAME_SIZE / 2) - 1);
  pOutCol[FRAME_SIZE / 2] = last_energy;
}

/**
  * @brief	Filter FFT values into bins
  * @param   *pSpectrCol points to the input spectrogram
  * @retval	None
  */
static void FilterBank(float32_t *pSpectrCol, float32_t *pFilterCol)
{
  uint16_t start_idx;
  uint16_t stop_idx;
  float32_t sum;

  for (uint16_t i = 0; i < NB_BINS; i++) {
	  start_idx = FilterStartIndices[i];
	  stop_idx = FilterStopIndices[i];

    sum = 0.0f;
    for (uint16_t j = start_idx; j < stop_idx; j++) {
      sum += pSpectrCol[j];
    }

    pFilterCol[i] = sum;
  }
}

/**
  * @brief	Store indices for filter bins in arrays
  * @param   *pFilterStartIndices
  * @param   *pFilterStopIndices
  * @retval	None
  */
static void FilterBankInit(uint32_t *pFilterStartIndices, uint32_t *pFilterStopIndices)
{
	uint16_t start_idx = 107; /* 48000 / 1024 * i > 5000 */

	float32_t fft_freq;
	float32_t freq_bin;

	for (uint16_t i=0; i<NB_BINS; i++) {
	  FilterStartIndices[i] = start_idx;

	  for (uint16_t j=start_idx; j<FRAME_SIZE; j++) {
		  fft_freq = (float)AUDIO_SAMPLING_FREQUENCY / (float)FRAME_SIZE * (float)j;
		  freq_bin = 5300.0 + i * 600.0;

		  if (fabs(freq_bin-fft_freq) > 300.0) {
			start_idx = j;
			FilterStopIndices[i] = j;
			break;
		}
    }
	}
}

/**
  * @brief Initialize STFT preprocessing
  * @param None
  * @retval None
  */
static void PreprocessingInit(void)
{
  /* Init RFFT */
  arm_rfft_fast_init_f32(&S_Rfft, FRAME_SIZE);

  /* Precalculate indices for filterbank */
  FilterBankInit(FilterStartIndices, FilterStopIndices);
}

/**
  * @brief      Convert powerspectrogram to decibel
  * @param      pSpectrogram Power spectrogram
  * @retval     None
  */
static void PowerTodB(float32_t *pSpectrogram)
{
  float32_t max_energy = 0.0f;
  uint32_t i;

  /* Find energy Scaling factor */
  for (i=0; i<NB_BINS*NB_FRAMES; i++) {
    max_energy = (max_energy > pSpectrogram[i]) ? max_energy : pSpectrogram[i];
  }

  /* Scale energies */
  for (i=0; i<NB_BINS*NB_FRAMES; i++) {
    pSpectrogram[i] /= max_energy;
  }

  /* Convert power spectrogram to decibel */
  for (i=0; i<NB_BINS*NB_FRAMES; i++) {
    pSpectrogram[i] = 10.0f * log10f(pSpectrogram[i]);
  }

  /* Threshold output to -80.0 dB (TOP_DB) */
  for (i = 0; i<NB_BINS*NB_FRAMES; i++) {
    pSpectrogram[i] = (pSpectrogram[i] < -TOP_DB) ? (-TOP_DB) : (pSpectrogram[i]);
  }
}

/****END OF FILE****/

