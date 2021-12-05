# WMC (wood moisture classification)
WMC is an embedded machine learning project for the real-time classification of wood moisture content. The algorithm runs on the STMicroelectronics SensorTile.box evaluation board which is equipped with a 32-bit microcontroller. The project provides code to record the acoustic response of materials upon a frequency swipe, then train a CNN model using python and do real-time classification with an adapted 8-bit version of the model.

This code is a further development of a semester project at the Embedded system laboratory ([ESL](https://www.epfl.ch/labs/esl/)) at EPFL Lausanne and in collaboration with EMPA Thun. More details are given in [docs/report.pdf](docs/report.pdf).

## How does it work?
The acoustic response of wood depends on several factors such as density, temperature, thickness, and moisture content. To detect only changes in moisture content a machine learning algorithm can be used to relate these changes to the acoustic transmission in the material. To do so the material is excited by a frequency swipe and the response is recorded and saved to an SD card. The logged audio data is then used to train a CNN model. Currently, the classification differentiates between dry, semi-wet, and wet wood samples.

Note that this codebase could be used to analyze and classify other material properties upon the response of acoustic stimuli.

## Hardware and sofware
For the experimental setup see the report. The following components are used: 

* Steval MKSBOX1V1 (SensoreTile.box)
* Arduino Uno
* 8 Ohm Loudspeaker

The software is adapted from STMicroelectronics function packs:
- [FP-SNS-STBOX1](https://www.st.com/content/st_com/en/products/embedded-software/mcu-mpu-embedded-software/stm32-embedded-software/stm32-ode-function-pack-sw/fp-sns-stbox1.html)
- [FP-AI-SENSING1](https://www.st.com/content/st_com/en/products/embedded-software/mcu-mpu-embedded-software/stm32-embedded-software/stm32-ode-function-pack-sw/fp-ai-sensing1.html)

## File structure
The important components are grouped as follows:

**core**: STM32 application \
**model**: Pipeline for Keras model creation \
**arduino**: Arduino script for Arduino Uno \
**scripts**: Useful scripts for analyzing and flashing \
**docs**: Doxygen and report of semester project
  
## Workflow
Note that the frequency swipe settings for the recording and emission have to match. See the [frequency swipe settings](model/settings_frequency_swipe.txt).

1. Record audio data corresponding to dry, semi-wet, and wet wood samples
2. Copy and label WAV files in [model/dataset](model/dataset)
3. Run python pipeline and create Keras model

The next steps have to be done with the STM32CubeIDE and the [X-Cube-AI](https://www.st.com/en/embedded-software/x-cube-ai.html) extension pack: \
4. Create a new project for the SensorTile.box named "wmc" \
5. Load the _Keras_ model and transform it to an 8-bit integer version \
6. Generate the code \
7. Copy wmc_data.c and wmc_data.h and replace the corresponding files in the STM32 application \
8. In wmc_tables.c replace the arrays for the standardization from the ones saved in [model/standardize](model/standardize) \
9. Build and flash project on the evaluation board
10. Try real-time prediction using the classification mode on the STM

## STM32 program flow
After powering:
Green LED means audio-logging mode is on and ready \
Blue LED means classification mode is on and ready

1. Switch modes with a double click on the user button
2. Click on the user button to start logging or classification
3. Depending on mode
    1. Audio logging mode will trigger the loudspeaker. Multiple frequency swipes can be saved to the SD card.
    2. In classification mode one frequency swipe is triggered and the classification result is shown as a blinking LED.
4.  Run next classification or logging
  
## Flashing binaries
On Linux:
A script is provided in scripts to debug with OpenOCD and arm-none-eabi-gdb using the ST-Link V2. Flashing can be done via USB in DFU mode (hold boot button while connecting the board to PC) or via the ST-Link V2. When using OpenOCD the config file stm32l4plusx.cfg has to be copied to the OpenOCD directory. See this [tutorial](https://www.plguo.com/posts/stm32/STM32-Development-without-an-IDE) to set up the toolchain.

On Windows:
The makefile project can be imported in STM32CubeIde provided by STMicroelectronics. To set up a toolchain for OpenOCD follow this [tutorial](https://www.youtube.com/watch?v=PxQw5_7yI8Q). Use the [OpenOCD](https://github.com/STMicroelectronics/OpenOCD) version of STMicroelectronics instead and copy the config file as specified for Linux.

Note that debugging via the ST-Link is currently not possible because the SWDIO pin is reprogrammed to trigger the frequency swipe of the Arduino.

## Versions of libraries
* X-Cube-AI V7.0.0
* CMSIS RTOS V2
* FatFs, ST-Version V2.1.2

Python libraries:
* Tensor Flow V2.4.1
* Keras V2.4.3
* Librosa V0.8.1

## Contributors
Moritz Waldleben \
Giulio Masinelli
