/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _WMC_H_
#define _WMC_H_

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "wmc.h"
#include "wmc_tables.h"
#include "audiolog.h"
#include "ai.h"
#include "ai_platform.h"

/* Exported defines ----------------------------------------------------------*/
#define FFT_SIZE         1024
#define NB_FFTS          32
#define NB_BINS			 30
#define FRAME_SIZE       16896

/* Exported functions --------------------------------------------------------*/
void WMC_Init(void);
void WMC_DeInit(void);

void WMC_RecordingProcess(uint16_t *pPCMBuffer);
void WMC_Process(void);
void WMC_Run(float32_t *pCNNOut);
void WMC_ClassificationResult(float32_t *pCNNOut);

#ifdef __cplusplus
}
#endif

#endif /* _WMC_H_ */

/****END OF FILE****/

