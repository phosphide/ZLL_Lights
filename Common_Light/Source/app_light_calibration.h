/*****************************************************************************
 *
 * MODULE:             JN-AN-1171
 *
 * COMPONENT:          app_light_calibration.c
 *
 * DESCRIPTION:        Interface for app_light_calibration.c
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

#ifndef APP_LC_H
#define APP_LC_H

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/* Computed white mode disabled */
#define COMPUTED_WHITE_NONE					0
/* Computed white mode which more faithfully reproduces color, at the expense
 * of overall brightness */
#define COMPUTED_WHITE_BETTER_COLOR			1
/* Computed white mode which results in brighter whites, at the expense
 * of color reproduction */
#define COMPUTED_WHITE_BETTER_BRIGHTNESS	2

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
/****************************************************************************/
typedef enum
{
	BULB_WHITE = 0,
	BULB_RED = 0,
	BULB_GREEN = 1,
	BULB_BLUE = 2
} teColour;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC void vLC_InitSerialInterface(void);
PUBLIC uint8 u8LC_GetChannel(uint8 u8Bulb, teColour eColour);
PUBLIC void vLC_LoadCalibrationFromNVM(void);
PUBLIC void vLC_SaveCalibrationToNVM(void);
PUBLIC uint32 u32LC_AdjustIntensity(uint8 u8Intensity, uint8 u8ChannelNum);

/****************************************************************************/
/***        External Variables                                            ***/
/****************************************************************************/

#endif /* APP_LC_H */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
