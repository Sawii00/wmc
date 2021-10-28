//! @file audiolog.c

/* Includes ------------------------------------------------------------------*/
#include "audiolog.h"

/* Imported variables --------------------------------------------------------*/
/* Buffer to store microphone samples, defined in main.c */
extern uint16_t PCMBuffer[];

/* Buffer to store samples for wmc algorithm, defined in wmc_processing.c
 * Used to save to SD during classication mod */
extern uint16_t WMCBuffer[];

extern osSemaphoreId_t AUDIOLOGSem_id;

/* Private variables ---------------------------------------------------------*/
#define AUDIOLOG_BUFFER_SIZE (PCM_BUFFER_SIZE * 64)

/* Private defines   ---------------------------------------------------------*/
/* SD card */
static FATFS SDFatFs;  /* File system object for SD card logical drive */
static FIL AudioFile;  /* File object for audio file */
static char SDPath[4]; /* SD card logical drive path */

/* Process buffer for audio logging */
static uint16_t AUDIOLOGBuffer[AUDIOLOG_BUFFER_SIZE];
static uint32_t AUDIOLOGBufferWriteIndex = 0;
static volatile uint32_t AUDIOLOGBufferReadIndex = 0;

static uint8_t pAudioHeader[44];

/* Private function prototypes -----------------------------------------------*/
void WAV_HeaderInit(void);
void WAV_HeaderUpdate(uint32_t len);

/**
  * @brief  Initialize SD card
  * @param  None
  * @retval None
  */
void AUDIOLOG_SDInit(void)
{
  if(FATFS_LinkDriver(&SD_Driver, SDPath) == 0) {
    /* Register the file system object to the FatFs module */
    if(f_mount(&SDFatFs, (TCHAR const*)SDPath, 0) != FR_OK) {
      ErrorHandler(ERROR_FATFS);
    }
  }
  /* Explicit initialization to trap if SD-Card missing */
  if (SD_Driver.disk_initialize(0) != RES_OK) {
     ErrorHandler(ERROR_FATFS);
  }
}

/**
  * @brief  De-initalize SD card
  * @param  None
  * @retval None
  */
void AUDIOLOG_SDDeInit(void)
{
  FATFS_UnLinkDriver(SDPath);
  HAL_SD_DeInit(&hsd1);
}

/**
  * @brief  Enable SD audio logging
  * @param  None
  * @retval None
  */
void AUDIOLOG_Enable(void)
{
  /* Create filename */
  static uint16_t file_counter = 0;
  char file_name[30];
  sprintf(file_name, "Mic%03d.wav", file_counter);
  file_counter++;

  /* Open audio file */
  if(f_open(&AudioFile, (char const*)file_name, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
    file_counter--;
    ErrorHandler(ERROR_FATFS);
  }

  WAV_HeaderInit();

  /* Write audio header */
  uint32_t byteswritten; /* written byte count */
  if(f_write(&AudioFile, (uint8_t*)pAudioHeader, sizeof(pAudioHeader), (void *)&byteswritten) != FR_OK) {
	ErrorHandler(ERROR_FATFS);
  }

  /* Flush the cached information (pAudioHeader) */
  if(f_sync(&AudioFile) != FR_OK) {
	ErrorHandler(ERROR_FATFS);
  }
}

/**
  * @brief  Disable SD-Card logging
  * @param  None
  * @retval None
  */
void AUDIOLOG_Disable(void)
{
  uint32_t len = f_size(&AudioFile);
  WAV_HeaderUpdate(len);

  /* Update the data length in the header of the recorded WAV */
  f_lseek(&AudioFile, 0);

  uint32_t bytes_written; /* Written byte count */
  if(f_write(&AudioFile, (uint8_t*)pAudioHeader, sizeof(pAudioHeader), (void*)&bytes_written) != FR_OK) {
    ErrorHandler(ERROR_FATFS);
  }

  /* Close file */
  if(f_close(&AudioFile) != FR_OK) {
    ErrorHandler(ERROR_FATFS);
  }

  /* Set index back to 0 for next saving */
  AUDIOLOGBufferWriteIndex = 0;
}

/**
  * @brief  Management of the audio data logging
  * @param  pPCMBuffer  Pointer to PCM buffer, data from microphone
  * @retval None
  */
void AUDIOLOG_RecordingProcess(uint16_t *pPCMBuffer)
{
  /* Accumulate audiolog buffer into local ping-pong buffer before writing to SD card */
  memcpy(AUDIOLOGBuffer + AUDIOLOGBufferWriteIndex, pPCMBuffer, sizeof(uint16_t) * PCM_BUFFER_SIZE);
  AUDIOLOGBufferWriteIndex += PCM_BUFFER_SIZE;

  /* Save first half to SD WAV file */
  if(AUDIOLOGBufferWriteIndex == (AUDIOLOG_BUFFER_SIZE/2)) {
    AUDIOLOGBufferReadIndex = 0;
    osSemaphoreRelease(AUDIOLOGSem_id);
  }
  /* Save second half to SD WAV file */
  else if (AUDIOLOGBufferWriteIndex == AUDIOLOG_BUFFER_SIZE) {
    AUDIOLOGBufferReadIndex = AUDIOLOG_BUFFER_SIZE/2;

    /* Set index back to 0 to write again from begging of the audiolog buffer */
    AUDIOLOGBufferWriteIndex = 0;

    osSemaphoreRelease(AUDIOLOGSem_id);
  }
}

/**
  * @brief  Save processed audio buffer to SD WAV file
  * @param  None
  * @retval None
  */
void AUDIOLOG_Save2SD(void)
{
  uint32_t bytes_written; /* Written byte count */
  if( f_write(&AudioFile, ((uint8_t *)(AUDIOLOGBuffer+AUDIOLOGBufferReadIndex)),
              AUDIOLOG_BUFFER_SIZE, (void *)&bytes_written) != FR_OK) {
    ErrorHandler(ERROR_FATFS);
  }
}

/**
  * @brief  Save for the buffer used forclassification to SD WAV file
  * @param  at_end	Check if if at end of STFT algorithm
  * @retval None
  */
void AUDIOLOG_ClassificationSave2SD(uint8_t at_end)
{
  /* Check if end of STFT to store the remaining samples
   * aswell (which are not overlapping in the STFT algorithm) */
  uint16_t length = FFT_SIZE / 2;
  if(at_end == 1) {
	length = FFT_SIZE;
  }

  uint32_t bytes_written; /* Written byte count */
  if( f_write(&AudioFile, (uint8_t *)WMCBuffer, length*2, (void *)&bytes_written) != FR_OK) {
    ErrorHandler(ERROR_FATFS);
  }
}

/**
  * @brief  Initialize the header for the WAV file
  * @param  None
  * @retval None
  */
void WAV_HeaderInit(void)
{
  uint16_t   BitPerSample=16;
  uint32_t   ByteRate=AUDIO_SAMPLING_FREQUENCY*(BitPerSample/8);

  uint32_t   SampleRate=AUDIO_SAMPLING_FREQUENCY;
  uint16_t   BlockAlign= BitPerSample/8;

  /* Write chunkID, must be 'RIFF' */
  pAudioHeader[0] = 'R';
  pAudioHeader[1] = 'I';
  pAudioHeader[2] = 'F';
  pAudioHeader[3] = 'F';

  /* Write the file length */
  /* This value will be be written back at the end of the recording operation */
  pAudioHeader[4] = 0x00;
  pAudioHeader[5] = 0x4C;
  pAudioHeader[6] = 0x1D;
  pAudioHeader[7] = 0x00;

  /* Write the file format, must be 'WAVE' */
  pAudioHeader[8]  = 'W';
  pAudioHeader[9]  = 'A';
  pAudioHeader[10] = 'V';
  pAudioHeader[11] = 'E';

  /* Write the format chunk, must be 'fmt ' */
  pAudioHeader[12]  = 'f';
  pAudioHeader[13]  = 'm';
  pAudioHeader[14]  = 't';
  pAudioHeader[15]  = ' ';

  /* Write the length of the 'fmt' data, must be 0x10 */
  pAudioHeader[16]  = 0x10;
  pAudioHeader[17]  = 0x00;
  pAudioHeader[18]  = 0x00;
  pAudioHeader[19]  = 0x00;

  /* Write the audio format, must be 0x01 (PCM) */
  pAudioHeader[20]  = 0x01;
  pAudioHeader[21]  = 0x00;

  /* Write the number of channels, ie. 0x01 (Mono) */
  pAudioHeader[22]  = 1;
  pAudioHeader[23]  = 0x00;

  /* Write the sample rate in Hz */
  pAudioHeader[24]  = (uint8_t)((SampleRate & 0xFF));
  pAudioHeader[25]  = (uint8_t)((SampleRate >> 8) & 0xFF);
  pAudioHeader[26]  = (uint8_t)((SampleRate >> 16) & 0xFF);
  pAudioHeader[27]  = (uint8_t)((SampleRate >> 24) & 0xFF);

  /* Write the byte rate */
  pAudioHeader[28]  = (uint8_t)(( ByteRate & 0xFF));
  pAudioHeader[29]  = (uint8_t)(( ByteRate >> 8) & 0xFF);
  pAudioHeader[30]  = (uint8_t)(( ByteRate >> 16) & 0xFF);
  pAudioHeader[31]  = (uint8_t)(( ByteRate >> 24) & 0xFF);

  /* Write the block alignment */
  pAudioHeader[32]  = BlockAlign;
  pAudioHeader[33]  = 0x00;

  /* Write the number of bits per sample */
  pAudioHeader[34]  = BitPerSample;
  pAudioHeader[35]  = 0x00;

  /* Write the data chunk, must be 'data' */
  pAudioHeader[36]  = 'd';
  pAudioHeader[37]  = 'a';
  pAudioHeader[38]  = 't';
  pAudioHeader[39]  = 'a';

  /* Write the number of sample data */
  /* This variable will be written back at the end of the recording operation */
  pAudioHeader[40]  = 0x00;
  pAudioHeader[41]  = 0x4C;
  pAudioHeader[42]  = 0x1D;
  pAudioHeader[43]  = 0x00;
}

/**
  * @brief  Update the header file after the recording was stopped
  * @param  len		updated file length
  * @retval None
  */
void WAV_HeaderUpdate(uint32_t len)
{
  /* Write the updated file length */
  pAudioHeader[4] = (uint8_t)(len);
  pAudioHeader[5] = (uint8_t)(len >> 8);
  pAudioHeader[6] = (uint8_t)(len >> 16);
  pAudioHeader[7] = (uint8_t)(len >> 24);

  /* Write the updated number of sample data */
  len -=44; // TODO: Check if this is necessary
  pAudioHeader[40] = (uint8_t)(len);
  pAudioHeader[41] = (uint8_t)(len >> 8);
  pAudioHeader[42] = (uint8_t)(len >> 16);
  pAudioHeader[43] = (uint8_t)(len >> 24);
}

/****END OF FILE****/

