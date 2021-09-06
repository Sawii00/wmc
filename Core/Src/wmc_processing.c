/* Includes ------------------------------------------------------------------*/
#include "wmc_processing.h"
#include "ai.h"
#include "ai_platform.h"

#include "wmc_featurescaler.h"

/* Private variables ---------------------------------------------------------*/
float32_t Spectrogram[SPECTROGRAM_ROWS*SPECTROGRAM_COLS];
static float32_t SpectrogramColBuffer[SPECTROGRAM_ROWS];
static uint32_t SpectrogramColIndex;
float32_t WorkingBuffer[FFT_SIZE];

static arm_rfft_fast_instance_f32 S_Rfft;
static MelFilterTypeDef           S_MelFilter;
uint32_t pMelFilterStartIndices[30];
uint32_t pMelFilterStopIndices[30];
float32_t pMelFilterCoefs[968];
static SpectrogramTypeDef         S_Spectr;
static MelSpectrogramTypeDef      S_MelSpectr;

static void Preprocessing_Init(void);
static void StandardizeFeatures(float32_t *pSpectrogram);
static void PowerTodB(float32_t *pSpectrogram);

/* Exported functions --------------------------------------------------------*/
/**
  * @brief  Create and Init WMC Convolutional Neural Network
  * @param  None
  * @retval WMC status
  */
WMC_Status_t WMC_Init(void)
{
  if (AI_WMC_IN_1_SIZE != (SPECTROGRAM_ROWS*SPECTROGRAM_COLS)) {
    return WMC_ERROR;
  }

  /* Configure audio preprocessing */
  Preprocessing_Init();

 /* Enabling CRC clock for using AI libraries (to checking if STM32
  microprocessor is used) */
  __HAL_RCC_CRC_CLK_ENABLE();

  ai_init();

  return WMC_OK;
}

/**
  * @brief  DeInit WMC Convolutional Neural Network
  * @param  None
  * @retval WMC status
  */
WMC_Status_t WMC_DeInit(void)
{

  /* Disable CRC Clock */
  __HAL_RCC_CRC_CLK_DISABLE();

  ai_deinit();

  return WMC_OK;
}

/**
 * @brief  Process Wood Moisture Classification (WMC) algorithm
 * @note   This function needs to be executed multiple times to extract audio features
 * @param None
 * @retval WMC status
 */
WMC_Status_t WMC_Process(float32_t *pBuffer)
{
  /* Create a Mel-scaled spectrogram column */
  MelSpectrogramColumn(&S_MelSpectr, pBuffer, SpectrogramColBuffer);

  /* Reshape and copy into output spectrogram column */
  for (uint32_t i=0; i<MEL_SIZE; i++) {
    Spectrogram[i*SPECTROGRAM_COLS+SpectrogramColIndex] = SpectrogramColBuffer[i];
  }
  SpectrogramColIndex++;

  if (SpectrogramColIndex == SPECTROGRAM_COLS) {
    SpectrogramColIndex = 0;

    PowerTodB(Spectrogram);
    StandardizeFeatures(Spectrogram);

    return WMC_OK;
  }
  else {
    return WMC_ERROR;
  }
}

/**
 * @brief  WMC Convolutional Neural Net inference
 * @param  pSpectrogram Feature inputs for CNN
 * @param  pNetworkOut Outputs of CNN
 * @retval WMC classification result
 */
WMC_Status_t WMC_Run(float32_t *pSpectrogram, float32_t *pNetworkOut)
{
  ai_i8 WMCInput[AI_WMC_IN_1_SIZE];
  ai_i8 WMCOutput[AI_WMC_OUT_1_SIZE];
  StandardizeFeatures(pSpectrogram);

  ai_run(pSpectrogram, pNetworkOut);

  return WMC_OK;
}

WMC_Status_t WMC_ClassificationResult(float32_t *pNNOut)
{
  float32_t max_out;
  uint32_t classification_result;

/* ArgMax to associate NN output with the most likely classification label */
  max_out = pNNOut[0];
  classification_result = 0;

  for (uint32_t i = 1; i<3; i++) {
    if (pNNOut[i] > max_out) {
      max_out = pNNOut[i];
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
  return WMC_OK;
}

/**
 * @brief  Standardize input features
 * @param  pSpectrogram Feature inputs for CNN
 * @retval WMC classification result
 */
static void StandardizeFeatures(float32_t *pSpectrogram)
{
  /* Zero mean and variance and on input feature */
  for (uint32_t i=0; i<SPECTROGRAM_ROWS*SPECTROGRAM_COLS; i++) {
    pSpectrogram[i] = (pSpectrogram[i] - featureScalerMean[i]) / featureScalerStd[i];
  }
}

/**
 * @brief Initialize LogMel preprocessing
 * @param none
 * @retval none
 */
static void Preprocessing_Init(void)
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
 * @brief      LogMel Spectrum Calculation when all columns are populated
 * @param      pSpectrogram  Mel-scaled power spectrogram
 * @retval     None
 */
static void PowerTodB(float32_t *pSpectrogram)
{
  float32_t max_mel_energy = 0.0f;
  uint32_t i;

  /* Find MelEnergy Scaling factor */
  for (i = 0; i < MEL_SIZE * SPECTROGRAM_COLS; i++) {
    max_mel_energy = (max_mel_energy > pSpectrogram[i]) ? max_mel_energy : pSpectrogram[i];
  }

  /* Scale Mel Energies */
  for (i = 0; i < MEL_SIZE * SPECTROGRAM_COLS; i++) {
    pSpectrogram[i] /= max_mel_energy;
  }

  /* Convert power spectrogram to decibel */
  for (i = 0; i < MEL_SIZE * SPECTROGRAM_COLS; i++) {
    pSpectrogram[i] = 10.0f * log10f(pSpectrogram[i]);
  }

  /* Threshold output to -80.0 dB */
  for (i = 0; i < MEL_SIZE * SPECTROGRAM_COLS; i++) {
    pSpectrogram[i] = (pSpectrogram[i] < -80.0f) ? (-80.0f) : (pSpectrogram[i]);
  }
}

/****END OF FILE****/

