/*****************************************************************************
 *
 * MODULE:             JN-AN-1171
 *
 * COMPONENT:          app_light_calibration.c
 *
 * DESCRIPTION:        Does calculations for light calibration
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
#include <jendefs.h>
#include "PDM.h"
#include "PDM_IDs.h"
#include "app_light_calibration.h"
#include "DriverBulb.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/* Default calibration value for gamma. 1024 = 1.0. 2867 corresponds to 2.8,
 * a reasonable value for most LEDs. */
#define DEFAULT_GAMMA			2867
/* Default calibration value for brightness. 1024 = 1.0. */
#define DEFAULT_BRIGHTNESS		1024

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct
{
	uint16 u16Gamma;
	uint16 u16Brightness;
}tsLC_ChannelCalibration;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

PRIVATE uint32 antilog(uint32 y);

/****************************************************************************/
/*          Exported Variables                                              */
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

#include "log_table.h"
static tsLC_ChannelCalibration atsLC_Calibration[NUM_CHANNELS];

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 * NAME: vLC_LoadCalibrationFromNVM
 *
 * DESCRIPTION:
 * Load calibration values from EEPROM
 ****************************************************************************/
PUBLIC void vLC_LoadCalibrationFromNVM(void)
{
	PDM_teStatus eStatus;
	uint16 u16ByteRead;

	eStatus = PDM_eReadDataFromRecord(PDM_ID_APP_LIGHT_CALIB,
				&atsLC_Calibration,
	            sizeof(atsLC_Calibration), &u16ByteRead);

	if ((eStatus != PDM_E_STATUS_OK) || (u16ByteRead != sizeof(atsLC_Calibration)))
	{
		/* Failed to load calibration data from PDM; load defaults. */
		unsigned int i;
		for (i = 0; i < NUM_CHANNELS; i++)
		{
			atsLC_Calibration[i].u16Gamma = DEFAULT_GAMMA;
			atsLC_Calibration[i].u16Brightness = DEFAULT_BRIGHTNESS;
		}
	}
}

/****************************************************************************
 * NAME: vLC_SaveCalibrationToNVM
 *
 * DESCRIPTION:
 * Save calibration values to EEPROM
 ****************************************************************************/
PUBLIC void vLC_SaveCalibrationToNVM(void)
{
	PDM_eSaveRecordData(PDM_ID_APP_LIGHT_CALIB, &atsLC_Calibration, sizeof(atsLC_Calibration));
}

/****************************************************************************
 * NAME: u32LC_AdjustIntensity
 *
 * DESCRIPTION:
 * Adjusts intensity based on current calibration values. u8Intensity is
 * expected to be between 0 and 255 (inclusive). This will return a value
 * between 0 and 4095 (inclusive).
 ****************************************************************************/
PUBLIC uint32 u32LC_AdjustIntensity(uint8 u8Intensity, uint8 u8ChannelNum)
{
	uint32 x;
	uint32 y;
	uint32 u32Gamma = atsLC_Calibration[u8ChannelNum].u16Gamma;
	uint32 u32Brightness = atsLC_Calibration[u8ChannelNum].u16Brightness;

	if (u8Intensity == 0)
		return 0;
	y = log_table_short[u8Intensity];
	y = (y * u32Gamma) >> 10;
	x = antilog(y);
	x = (x * u32Brightness) >> 10;
	return x;
}

/****************************************************************************/
/***        Local    Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 * NAME:	antilog
 *
 * DESCRIPTION:
 *			Inverse log i.e. exponentiate. This will return the index i such
 *			that log_table_long(i) is close to y.
 *			Output values will be in the range 0 to 4095, where 0 represents
 *			0 intensity and 4095 represents maximum intensity.
 ****************************************************************************/
PRIVATE uint32 antilog(uint32 y)
{
	uint32 left_index;
	uint32 right_index;
	uint32 diff_left;
	uint32 diff_right;

    if (y > log_table_long[1])
        return 0;
    /* Binary search through log_table_long */
    left_index = 1;
    right_index = 4095;
    while ((left_index + 1) != right_index)
    {
    	uint32 i = (right_index + left_index) >> 1;
        if (log_table_long[i] < y)
            right_index = i;
        else
            left_index = i;
    }
    diff_left = log_table_long[left_index] - y;
    diff_right = y - log_table_long[right_index];
    if (diff_left < diff_right)
        return left_index;
    else
        return right_index;
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
