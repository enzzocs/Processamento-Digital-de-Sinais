#include <stm32f4xx.h>
#include <arm_math.h>
#include <stm32f4_discovery.h>
#include <stm32f4_discovery_accelerometer.h>
#include <wolfson_pi_audio.h>
#include <diag/Trace.h>
#include <tests.h>
#include <dwt.h>
#include "filter.h"

//#include "math_helper.h"
#define NOTCH

#ifdef NOTCH
	#define NUM_STAGES 1
#else
	#define NUM_STAGES 2
#endif

#define BLOCK_SIZE (WOLFSON_PI_AUDIO_TXRX_BUFFER_SIZE)/4

#undef CYCLE_COUNTER

int16_t TxBuffer[WOLFSON_PI_AUDIO_TXRX_BUFFER_SIZE];
int16_t RxBuffer[WOLFSON_PI_AUDIO_TXRX_BUFFER_SIZE];

__IO BUFFER_StateTypeDef buffer_offset = BUFFER_OFFSET_NONE;

__IO uint8_t Volume =100;

uint32_t AcceleroTicks;
int16_t AcceleroAxis[3];

/* -------------------------------------------------------------------
 * Declare State buffer of size (4*nNUM_STAGES)
 * ------------------------------------------------------------------- */
static float32_t iirStateF32[4*NUM_STAGES];


/* ----------------------------------------------------------------------
** IIR Coefficients
** The coefficients are stored in the array pCoeffs in the following order:
    {b10, b11, b12, a11, a12, b20, b21, b22, a21, a22, ...}
    http://www.keil.com/pack/doc/CMSIS/DSP/html/group__BiquadCascadeDF1.html

** Esses coeficientes s�o de um filtro notch,
** Artigo: Narrowband notch filter using feedback structure, S.-C. Pei et al., IEEE Sig. Proc. Mag. May 2016
% filtro C_n(z)
w=0.4*pi;
p=0.9;
b=[1 -2*cos(w) 1];
a=[1 -2*p*cos(w) p^2];
freqz(b,a)
Para implementar no CMSIS-DSP, os coef. a1 e a2 foram multiplicados por -1
** ------------------------------------------------------------------- */
#ifdef NOTCH
	const float32_t iirCoeffsF32[5*NUM_STAGES] = {1.0, -1.95989827, 1.0,  1.9819196,  -0.97888889};
#else
	const float32_t iirCoeffsF32[5*NUM_STAGES] = {   0.98672551, -1.9556117,   0.98672551,       1.97963936,  -0.99769786,
			  1.,         -1.98192069,  1.,               1.97962374,  -0.9977285 ,
			  1.,         -1.98192069,  1.,               1.97972126,  -0.99773423,
			  1.,         -1.98192069,  1.,               1.9796755 ,  -0.9978255 ,
			  1.,         -1.98192069,  1.,               1.9798664 ,  -0.99783626,
			  1.,         -1.98192069,  1.,               1.97979376,  -0.99798624,
			  1.,         -1.98192069,  1.,               1.98006988,  -0.9980007 ,
			  1.,         -1.98192069,  1.,               1.97997575,  -0.99820625,
			  1.,         -1.98192069,  1.,               1.98032526,  -0.99822263,
			  1.,         -1.98192069,  1.,               1.98021719,  -0.99847956,
			  1.,         -1.98192069,  1.,               1.98062485,  -0.99849586,
			  1.,         -1.98192069,  1.,               1.98051297,  -0.99879942,
			  1.,         -1.98192069,  1.,               1.98096047,  -0.99881366,
			  1.,         -1.98192069,  1.,               1.9808593 ,  -0.99916015,
			  1.,         -1.98192069,  1.,               1.98132506,  -0.99917061,
			  1.,         -1.98192069,  1.,               1.98126203,  -0.99956467,
			  1.,         -1.98192069,  1.,               1.98171907,  -0.99957005,};
#endif
/* ------------------------------------------------------------------
 * Global variables for IIR LPF Example
 * ------------------------------------------------------------------- */
FilterTypeDef filterType=IIR_DF1_FLOAT32;

int main(int argc, char* argv[])
{
	UNUSED(argc);
	UNUSED(argv);

	uint32_t cycleCount;
	uint32_t i, k;

	float32_t inputF32Buffer[BLOCK_SIZE];
	float32_t outputF32Buffer[BLOCK_SIZE];
/*
#ifdef OS_USE_SEMIHOSTING
	//Semihosting example
	FILE *CoefficientsFile;
	FILE *CycleFile;
	float Coefficients[NUM_TAPS];

	//Semihosting example
	CoefficientsFile = fopen("coefficients.txt", "r");
	if (!CoefficientsFile) {
		trace_printf("Error trying to open CoefficientsFile. Check the name/location.\n");
		while(1);
	}

	for(i=0; i<5; i++)
		fscanf(CoefficientsFile, "%f", &Coefficients[i]);

	for(i=0; i<5; i++)
		trace_printf("Coefficient %d: %f\n", i, Coefficients[i]);

	fclose(CoefficientsFile);
#endif
*/

	// Initialise the HAL Library; it must be the first
	// instruction to be executed in the main program.
	HAL_Init();

	DWT_Enable();

#ifdef CYCLE_COUNTER
	CycleFile = fopen("cyclecounter.txt", "w");
	if (!CycleFile) {
		trace_printf("Error trying to open cycle counter file\n.");
		while(1);
	}
#endif

	WOLFSON_PI_AUDIO_Init((INPUT_DEVICE_LINE_IN << 8) | OUTPUT_DEVICE_BOTH, 80, AUDIO_FREQUENCY_48K);

	WOLFSON_PI_AUDIO_SetInputMode(INPUT_DEVICE_LINE_IN);

	WOLFSON_PI_AUDIO_SetMute(AUDIO_MUTE_ON);

	WOLFSON_PI_AUDIO_Play(TxBuffer, RxBuffer, WOLFSON_PI_AUDIO_TXRX_BUFFER_SIZE);

	WOLFSON_PI_AUDIO_SetVolume(Volume);

	BSP_ACCELERO_Init();

	TEST_Init();

	arm_biquad_casd_df1_inst_f32 S;
	float32_t  *inputF32, *outputF32;

	/* Initialize input and output buffer pointers */
	inputF32 = &inputF32Buffer[0];
	outputF32 = &outputF32Buffer[0];

	/* Call IIR init function to initialize the instance structure. */
	arm_biquad_cascade_df1_init_f32(&S, NUM_STAGES, (float32_t*)iirCoeffsF32, iirStateF32);

	trace_printf("End of filter initialization.\n filterType is %d\n", filterType);

	while (1) {
		// Add your code here.
		if(buffer_offset == BUFFER_OFFSET_HALF)
		{
			DWT_Reset();

			cycleCount = DWT_GetValue();

			if (filterType==IIR_DF1_FLOAT32) {
				for(i=0, k=0; i<(WOLFSON_PI_AUDIO_TXRX_BUFFER_SIZE/2); i++) {
					if(i%2) {
						inputF32Buffer[k] = (float32_t)(RxBuffer[i]/32768.0);//convert to float LEFT
						k++;
					}
					else {
						TxBuffer[i] = RxBuffer[i];//   RIGHT (canal de baixo no OcenAudio)
					}

				}
				//inputF32Buffer[0]=0.5;inputF32Buffer[1]=0.7; //for testing purposes
				arm_biquad_cascade_df1_f32(&S, inputF32, outputF32, BLOCK_SIZE);
				for(i=0, k=0; i<(WOLFSON_PI_AUDIO_TXRX_BUFFER_SIZE/2); i++) {
					if(i%2)	{
						TxBuffer[i] = (int16_t)(outputF32Buffer[k]*32768);//back to 1.15
						k++;
					}
				}
			}

#ifdef CYCLE_COUNTER
			fprintf(CycleFile, "\nHALF: %lu", (DWT_GetValue()- cycleCount));
#endif

			buffer_offset = BUFFER_OFFSET_NONE;
		}

		if(buffer_offset == BUFFER_OFFSET_FULL)
		{
			DWT_Reset();

			cycleCount = DWT_GetValue();

			if (filterType==IIR_DF1_FLOAT32) {
				for(i=(WOLFSON_PI_AUDIO_TXRX_BUFFER_SIZE/2), k=0; i<WOLFSON_PI_AUDIO_TXRX_BUFFER_SIZE; i++) {
					if(i%2) {
						inputF32Buffer[k] = (float32_t)(RxBuffer[i]/32768.0);//convert to float
						k++;
					}
					else {
						TxBuffer[i] = RxBuffer[i];//pass-through(int16_t)0.3*32768;//
					}
				}
				//inputF32Buffer[0]=0.5;inputF32Buffer[1]=0.7; //for testing purposes
				arm_biquad_cascade_df1_f32(&S, inputF32, outputF32, BLOCK_SIZE);
				for(i=(WOLFSON_PI_AUDIO_TXRX_BUFFER_SIZE/2), k=0; i<WOLFSON_PI_AUDIO_TXRX_BUFFER_SIZE; i++) {
					if(i%2)	{
						TxBuffer[i] = (int16_t)(outputF32Buffer[k]*32768.0);//back to 1.15
						k++;
					}
				}
			}


#ifdef CYCLE_COUNTER
			fprintf(CycleFile, "\nFULL: %lu", (DWT_GetValue()- cycleCount));
#endif

			buffer_offset = BUFFER_OFFSET_NONE;
		}

		TEST_Main();
	}
#ifdef CYCLE_COUNTER
	fclose(CycleFile);
#endif
	return 0;
}

/*--------------------------------
Callbacks implementation:
--------------------------------------------------------*/

/**
  * @brief  Manages the DMA full Transfer complete event.
  */
void WOLFSON_PI_AUDIO_TransferComplete_CallBack(void)
{
	buffer_offset = BUFFER_OFFSET_FULL;
}

/**
  * @brief  Manages the DMA Half Transfer complete event.
  */
void WOLFSON_PI_AUDIO_HalfTransfer_CallBack(void)
{
	  buffer_offset = BUFFER_OFFSET_HALF;
}

/**
  * @brief  Manages the DMA FIFO error interrupt.
  * @param  None
  * @retval None
  */
void WOLFSON_PI_AUDIO_OUT_Error_CallBack(void)
{
  /* Stop the program with an infinite loop */
  while (1);
}
