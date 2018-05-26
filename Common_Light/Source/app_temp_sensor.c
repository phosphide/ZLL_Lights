/*****************************************************************************
 *
 * MODULE:             JN-AN-1171
 *
 * COMPONENT:          app_temp_sensor.c
 *
 * DESCRIPTION:        Measures board temperature
 *
 *
 ****************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <jendefs.h>
#include <AppHardwareApi.h>
#include "os.h"
#include "PeripheralRegs_JN5168.h"
#include "app_temp_sensor.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

/****************************************************************************/
/*          Exported Variables                                              */
/****************************************************************************/

/* This will be set to TRUE iff the board is overheating */
volatile bool_t bOverheat;

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

#include "temperature_table.h"
/* Most recent accumulated ADC reading */
volatile uint16 u16AccumulatedADC;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 * NAME: vTS_InitTempSensor
 *
 * DESCRIPTION:
 * Initialises and configures peripherals for interacting with temperature
 * sensor.
 ****************************************************************************/
PUBLIC void vTS_InitTempSensor(void)
{
	/* Enable analogue peripheral regulator, use interrupts, set sampling
	 * to 8 clock periods (since thermistor is higher-impedance), use the
	 * recommended clock of 500 kHz, and use the internal 1.235 V bandgap
	 * reference. */
	vAHI_ApConfigure(E_AHI_AP_REGULATOR_ENABLE, E_AHI_AP_INT_ENABLE,
			E_AHI_AP_SAMPLE_8, E_AHI_AP_CLOCKDIV_500KHZ, E_AHI_AP_INTREF);
	/* Wait until analogue peripheral regulator has started up */
	while (bAHI_APRegulatorEnabled() == FALSE)
	{
	}
	/* Set range of ADC to 0 to 2 * reference voltage i.e. 0 V to 2.47 V.
	 * Use ADC1 as source. */
	vAHI_AdcEnable(E_AHI_ADC_CONTINUOUS, E_AHI_AP_INPUT_RANGE_2,
			E_AHI_ADC_SRC_ADC_1);
	/* Start ADC in accumulation mode */
	vAHI_AdcStartAccumulateSamples(E_AHI_ADC_ACC_SAMPLE_16);
}

/****************************************************************************
 * NAME: APP_isrAdc
 *
 * DESCRIPTION:
 * ISR for ADC
 ****************************************************************************/
OS_ISR(APP_isrAdc)
{
	uint32 u32Temp;

	/* Get reading */
	u16AccumulatedADC = u16AHI_AdcRead();
	/* Acknowledge interrupt. See JenOS user guide, Appendix B, section
	 * "Analogue Peripherals". */
	u32Temp = u32REG_AnaRead(REG_ANPER_IS);
	vREG_AnaWrite(REG_ANPER_IS, u32Temp);
	/* Start next set of samples */
	vAHI_AdcStartAccumulateSamples(E_AHI_ADC_ACC_SAMPLE_16);
}

/****************************************************************************
 * NAME: i16TS_GetTemperature
 *
 * DESCRIPTION:
 * Get board temperature, in degrees Celsius
 ****************************************************************************/
PUBLIC int16 i16TS_GetTemperature(void)
{
	uint32 left_index;
	uint32 right_index;
	uint32 diff_left;
	uint32 diff_right;
	uint32 adc;

	adc = u16AccumulatedADC;
	if (adc > temperature_lookup[0])
		return 0;
	if (adc < temperature_lookup[TEMPERATURE_LOOKUP_LENGTH - 1])
		return TEMPERATURE_LOOKUP_LENGTH - 1;
	/* Binary search through temperature_lookup */
	left_index = 0;
	right_index = TEMPERATURE_LOOKUP_LENGTH - 1;
	while ((left_index + 1) != right_index)
	{
		uint32 i = (right_index + left_index) >> 1;
		if (temperature_lookup[i] < adc)
			right_index = i;
		else
			left_index = i;
	}
	diff_left = temperature_lookup[left_index] - adc;
	diff_right = adc - temperature_lookup[right_index];
	if (diff_left < diff_right)
		return (int16)left_index;
	else
		return (int16)right_index;
}

/****************************************************************************/
/***        Local    Functions                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

