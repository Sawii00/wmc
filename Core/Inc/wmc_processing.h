/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _WMC_H_
#define _WMC_H_

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "wmc.h"
#include "audiolog.h"
#include "feature_extraction.h"

/* Exported defines ----------------------------------------------------------*/
#define FFT_SIZE         1024
#define MEL_SIZE         30

#define SPECTROGRAM_ROWS MEL_SIZE
#define SPECTROGRAM_COLS 32

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  WMC_OK      = 0x00,
  WMC_ERROR   = 0x01,
} WMC_Status_t;

/* Exported functions  -------------------------------------------------------*/
WMC_Status_t WMC_Init(void);
WMC_Status_t WMC_DeInit(void);

WMC_Status_t WMC_Process(float32_t *pBuffer);
WMC_Status_t WMC_Run(float32_t *pSpectrogram, float32_t *pNetworkOut);
WMC_Status_t WMC_ClassificationResult(float32_t *pNNOut);

#ifdef __cplusplus
}
#endif

#endif /* _WMC_H_ */

/****END OF FILE****/

