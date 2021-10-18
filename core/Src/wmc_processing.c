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
int16_t WMCBuffer[FFT_SIZE*2];
static float32_t WMCBuffer_f[FFT_SIZE];
static uint32_t WMCBufferIndex;

/* Spectrogram which results from FFT */
static float32_t Spectrogram[NB_BINS*NB_FFTS];
static float32_t SpectrogramColBuffer[NB_BINS];
static uint32_t SpectrogramColIndex;
float32_t WorkingBuffer[FFT_SIZE];

/* Variables for LogMel spectrogram */
static arm_rfft_fast_instance_f32 S_Rfft;
static MelFilterTypeDef           S_MelFilter;
uint32_t pMelFilterStartIndices[30];
uint32_t pMelFilterStopIndices[30];
float32_t pMelFilterCoefs[968];
static SpectrogramTypeDef         S_Spectr;
static MelSpectrogramTypeDef      S_MelSpectr;

/* Private function prototypes -----------------------------------------------*/
static void PreprocessingInit(void);
static void NormalizeFeatures(float32_t *pSpectrogram);
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
  static volatile uint32_t cut_off_samples = 0;

  /* Copy PCM_Buffer from microphones onto WMC_Buffer */
  memcpy(WMCBuffer + WMCBufferIndex, pPCMBuffer, sizeof(int16_t) * PCM_BUFFER_SIZE);
  WMCBufferIndex += PCM_BUFFER_SIZE;

  float32_t sample;

  /* Create 1024 (FFT_SIZE) samples window every 512 (HOP_LENGTH) samples */
  if (WMCBufferIndex >= FFT_SIZE) {
    /* The FFT_SIZE is not in sync with the audio frequency, samples
     which are to much get chopped off and added later */
    cut_off_samples = WMCBufferIndex - FFT_SIZE;
	/* Copy Fill Buffer in Proc Buffer */

    for (uint32_t i = 0; i < FFT_SIZE; i++) {
      sample = ((float32_t) WMCBuffer[i]);
      /* Invert the scale of the data */
      sample /= (float32_t) ((1 << (8 * sizeof(int16_t) - 1)));
      WMCBuffer_f[i] = sample;
    }

    /* Left shift WMC_Buffer by 512 samples and add the cut off samples */
    memmove(WMCBuffer, WMCBuffer + (FFT_SIZE / 2), sizeof(int16_t) * ((FFT_SIZE / 2) + cut_off_samples));
    WMCBufferIndex = FFT_SIZE / 2 + cut_off_samples;

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
  MelSpectrogramColumn(&S_MelSpectr, WMCBuffer_f, SpectrogramColBuffer);

  /* Reshape and copy into output spectrogram column */
  for (uint32_t i=0; i<NB_BINS; i++) {
    Spectrogram[i*NB_FFTS+SpectrogramColIndex] = SpectrogramColBuffer[i];
  }
  SpectrogramColIndex++;

  if (SpectrogramColIndex == NB_FFTS) {
    /* Set index to 0 for next run of algorithm */
    WMCBufferIndex = 0;
    SpectrogramColIndex = 0;

    PowerTodB(Spectrogram);
    NormalizeFeatures(Spectrogram);
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
  * @brief  Normalize input features
  * @param  pSpectrogram Feature inputs for CNN
  * @retval None
  */
static void NormalizeFeatures(float32_t *pSpectrogram)
{
  /* Zero mean and variance and on input feature */
  for (uint32_t i=0; i<NB_BINS*NB_FFTS; i++) {
    pSpectrogram[i] = (pSpectrogram[i] - featureScalerMean[i]) / featureScalerStd[i];
  }
}

/**
  * @brief Initialize LogMel preprocessing
  * @param None
  * @retval None
  */
static void PreprocessingInit(void)
{
  /* Init RFFT */
  arm_rfft_fast_init_f32(&S_Rfft, 1024);

  /* Init Spectrogram */
  S_Spectr.pRfft    = &S_Rfft;
  S_Spectr.Type     = SPECTRUM_TYPE_POWER;
  S_Spectr.pWindow  = (float32_t *) hannWin_1024;
  S_Spectr.SampRate = AUDIO_SAMPLING_FREQUENCY;
  S_Spectr.FrameLen = 1024;
  S_Spectr.FFTLen   = 1024;
  S_Spectr.pScratch = WorkingBuffer;

  /* Init Mel filter */
  S_MelFilter.pStartIndices = pMelFilterStartIndices;
  S_MelFilter.pStopIndices  = pMelFilterStopIndices;
  S_MelFilter.pCoefficients = pMelFilterCoefs;
  S_MelFilter.NumMels       = 30;
  S_MelFilter.FFTLen    	= 1024;
  S_MelFilter.SampRate  	= AUDIO_SAMPLING_FREQUENCY;
  S_MelFilter.FMin      	= 4000;
  S_MelFilter.FMax      	= AUDIO_SAMPLING_FREQUENCY / 2;
  S_MelFilter.Formula   	= MEL_SLANEY;
  S_MelFilter.Normalize 	= 1;
  S_MelFilter.Mel2F     	= 1;

  /* Init MelSpectrogram */
  MelFilterbank_Init(&S_MelFilter);
  S_MelSpectr.SpectrogramConf = &S_Spectr;
  S_MelSpectr.MelFilter       = &S_MelFilter;
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
  for (i=0; i < NB_BINS*NB_FFTS; i++) {
    max_energy = (max_energy > pSpectrogram[i]) ? max_energy : pSpectrogram[i];
  }

  /* Scale energies */
  for (i=0; i < NB_BINS*NB_FFTS; i++) {
    pSpectrogram[i] /= max_energy;
  }

  /* Convert power spectrogram to decibel */
  for (i=0; i < NB_BINS*NB_FFTS; i++) {
    pSpectrogram[i] = 10.0f * log10f(pSpectrogram[i]);
  }

  /* Threshold output to -80.0 dB (TOP_DB) */
  for (i = 0; i < NB_BINS*NB_FFTS; i++) {
    pSpectrogram[i] = (pSpectrogram[i] < -TOP_DB) ? (-TOP_DB) : (pSpectrogram[i]);
  }
}

/****END OF FILE****/

