/*****************************************************************************
 *
 * MODULE:             JN-AN-1171
 *
 * COMPONENT:          App_Light_ColorLight.c
 *
 * DESCRIPTION:        ZLL Demo: Colored Light - Implementation
 *
 ****************************************************************************
 *
 * This software is owned by NXP B.V. and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved. We grant You,
 * and any third parties, a license to use this software solely and
 * exclusively on NXP products [NXP Microcontrollers such as JN5168, JN5164,
 * JN5161, JN5148, JN5142, JN5139].
 * You, and any third parties must reproduce the copyright and warranty notice
 * and any other legend of ownership on each copy or partial copy of the
 * software.
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
 * Copyright NXP B.V. 2012. All rights reserved
 *
 ***************************************************************************/

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <jendefs.h>
#include "AppHardwareApi.h"
#include "zps_gen.h"
#include "App_MultiLight.h"
#include "app_common.h"
#include "app_zcl_light_task.h"
#include "dbg.h"
#include <string.h>

#include "app_light_interpolation.h"
#include "app_light_calibration.h"
#include "app_temp_sensor.h"
#include "DriverBulb.h"


/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#ifdef DEBUG_LIGHT_TASK
#define TRACE_LIGHT_TASK  TRUE
#else
#define TRACE_LIGHT_TASK FALSE
#endif

#ifdef DEBUG_PATH
#define TRACE_PATH  TRUE
#else
#define TRACE_PATH  FALSE
#endif

/* This macro will be used in computed white mode. It calculates the
 * white (mono) bulb number associated with an RGB bulb number. That white
 * bulb will be used as the computed white channel for the color light. */
#define RGB_BULB_TO_MONO(x)			((x) - (NUM_MONO_LIGHTS))

#define FAST_DIV_BY_255(x)			((((x) << 8) + (x) + 255) >> 16)

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

tsZLL_DimmableLightDevice sLightMono[NUM_MONO_LIGHTS];
tsZLL_ColourLightDevice sLightRGB[NUM_RGB_LIGHTS];

tsIdentifyWhite sIdEffectMono[NUM_MONO_LIGHTS];
tsIdentifyColour sIdEffectRGB[NUM_RGB_LIGHTS];

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

#ifdef VARIANT_MINI
PRIVATE tsCLD_ZllDeviceTable sDeviceTable =
	{NUM_MONO_LIGHTS + NUM_RGB_LIGHTS,
		{
			{0,
			 ZLL_PROFILE_ID,
			 DIMMABLE_LIGHT_DEVICE_ID,
			 MULTILIGHT_LIGHT_MONO_1_ENDPOINT,
			 2,
			 3,
			 0},

			{0,
			 ZLL_PROFILE_ID,
			 COLOUR_LIGHT_DEVICE_ID,
			 MULTILIGHT_LIGHT_RGB_1_ENDPOINT,
			 2,
			 3,
			 3}
		}
	};
#else
PRIVATE tsCLD_ZllDeviceTable sDeviceTable =
	{NUM_MONO_LIGHTS + NUM_RGB_LIGHTS,
		{
			{0,
			 ZLL_PROFILE_ID,
			 DIMMABLE_LIGHT_DEVICE_ID,
			 MULTILIGHT_LIGHT_MONO_1_ENDPOINT,
			 2,
			 3,
			 0},

			{0,
			 ZLL_PROFILE_ID,
			 DIMMABLE_LIGHT_DEVICE_ID,
			 MULTILIGHT_LIGHT_MONO_2_ENDPOINT,
			 2,
			 3,
			 1},

			{0,
			 ZLL_PROFILE_ID,
			 DIMMABLE_LIGHT_DEVICE_ID,
			 MULTILIGHT_LIGHT_MONO_3_ENDPOINT,
			 2,
			 3,
			 2},

			{0,
			 ZLL_PROFILE_ID,
			 COLOUR_LIGHT_DEVICE_ID,
			 MULTILIGHT_LIGHT_RGB_1_ENDPOINT,
			 2,
			 3,
			 3},

			{0,
			 ZLL_PROFILE_ID,
			 COLOUR_LIGHT_DEVICE_ID,
			 MULTILIGHT_LIGHT_RGB_2_ENDPOINT,
			 2,
			 3,
			 4},

			{0,
			 ZLL_PROFILE_ID,
			 COLOUR_LIGHT_DEVICE_ID,
			 MULTILIGHT_LIGHT_RGB_3_ENDPOINT,
			 2,
			 3,
			 5}
		}
	};
#endif

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

PRIVATE void vOverideProfileId(uint16* pu16Profile, uint8 u8Ep);

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 **
 ** NAME: u8App_GetNumberOfDevices
 **
 ** DESCRIPTION:
 ** Get number of ZLL devices that are registered (or will be registered) by
 ** the firmware.
 **
 ** PARAMETER: void
 **
 ** RETURNS:
 ** Number of devices
 *
 ****************************************************************************/
PUBLIC uint8 u8App_GetNumberOfDevices(void)
{
	if (u32ComputedWhiteMode == COMPUTED_WHITE_NONE)
	{
		return NUM_MONO_LIGHTS + NUM_RGB_LIGHTS;
	}
	else
	{
		return NUM_RGB_LIGHTS;
	}
}

/****************************************************************************
 **
 ** NAME: psApp_GetDeviceRecord
 **
 ** DESCRIPTION:
 ** Get ZLL device record
 **
 ** PARAMETER
 ** Type                                Name                    Descirption
 ** uint8                               u8RecordNum             0 = first record, 1 = second record etc.
 **
 **
 ** RETURNS:
 ** Pointer to tsCLD_ZllDeviceRecord structure
 *
 ****************************************************************************/
PUBLIC tsCLD_ZllDeviceRecord *psApp_GetDeviceRecord(uint8 u8RecordNum)
{
	if (u32ComputedWhiteMode == COMPUTED_WHITE_NONE)
	{
		return &(sDeviceTable.asDeviceRecords[u8RecordNum]);
	}
	else
	{
		return &(sDeviceTable.asDeviceRecords[u8RecordNum + NUM_MONO_LIGHTS]);
	}
}

/****************************************************************************
 **
 ** NAME: eApp_ZLL_RegisterEndpoint
 **
 ** DESCRIPTION:
 ** Register ZLL endpoints
 **
 ** PARAMETER
 ** Type                                Name                    Descirption
 ** tfpZCL_ZCLCallBackFunction          fptr                    Pointer to ZCL Callback function
 ** tsZLL_CommissionEndpoint            psCommissionEndpoint    Pointer to Commission Endpoint
 **
 **
 ** RETURNS:
 ** teZCL_Status
 *
 ****************************************************************************/
PUBLIC teZCL_Status eApp_ZLL_RegisterEndpoint(tfpZCL_ZCLCallBackFunction fptr,
                                       tsZLL_CommissionEndpoint* psCommissionEndpoint)
{
	teZCL_Status r;
	unsigned int i;

	ZPS_vAplZdoRegisterProfileCallback(vOverideProfileId);
	zps_vSetIgnoreProfileCheck();

	eZLL_RegisterCommissionEndPoint(MULTILIGHT_COMMISSION_ENDPOINT,
                                    fptr,
                                    psCommissionEndpoint);

	r = E_ZCL_SUCCESS;

	if (u32ComputedWhiteMode == COMPUTED_WHITE_NONE)
	{
		for (i = 0; i < NUM_MONO_LIGHTS; i++)
		{
			if (r == E_ZCL_SUCCESS)
			{
				r = eZLL_RegisterDimmableLightEndPoint(MULTILIGHT_LIGHT_MONO_1_ENDPOINT + i,
													   fptr,
													   &(sLightMono[i]));
			}
		}
	}
	for (i = 0; i < NUM_RGB_LIGHTS; i++)
	{
		if (r == E_ZCL_SUCCESS)
		{
			r = eZLL_RegisterColourLightEndPoint(MULTILIGHT_LIGHT_RGB_1_ENDPOINT + i,
												 fptr,
												 &(sLightRGB[i]));
		}
	}
    return r;
}


/****************************************************************************
*
* NAME: vOverideProfileId
*
* DESCRIPTION: Allows the application to over ride the profile in the
* simple descriptor (0xc05e) with the ZHA profile id (0x0104)
* required for on air packets
*
*
* PARAMETER: pointer to the profile  to be used, the end point sending the data
*
* RETURNS: void
*
****************************************************************************/
PRIVATE void vOverideProfileId(uint16* pu16Profile, uint8 u8Ep)
{
    if (((u8Ep >= MULTILIGHT_LIGHT_MONO_1_ENDPOINT)
      && (u8Ep < (MULTILIGHT_LIGHT_MONO_1_ENDPOINT + NUM_MONO_LIGHTS)))
     || ((u8Ep >= MULTILIGHT_LIGHT_RGB_1_ENDPOINT)
	  && (u8Ep < (MULTILIGHT_LIGHT_RGB_1_ENDPOINT + NUM_RGB_LIGHTS))))
    {
        *pu16Profile = 0x0104;
    }
}

/****************************************************************************
*
* NAME: bEndPointToNum
*
* DESCRIPTION: Convert endpoint number into light index
*
*
* PARAMETER: u8Endpoint is the endpoint number, bIsRGB will be set to TRUE
* if the endpoint is detected to be an RGB endpoint, otherwise it will be
* set to FALSE. u8Num will be set to the light index e.g. 0 = first mono or
* RGB light, 1 = second light etc.
*
* RETURNS: TRUE iff u8Endpoint is a valid light endpoint
*
****************************************************************************/
PUBLIC bool_t bEndPointToNum(uint8 u8Endpoint, bool_t* bIsRGB, uint8* u8Num)
{
	if ((u8Endpoint >= MULTILIGHT_LIGHT_MONO_1_ENDPOINT)
	 && (u8Endpoint < (MULTILIGHT_LIGHT_MONO_1_ENDPOINT + NUM_MONO_LIGHTS)))
	{
		*bIsRGB = FALSE;
		*u8Num = u8Endpoint - MULTILIGHT_LIGHT_MONO_1_ENDPOINT;
		return TRUE;
	}
	else if ((u8Endpoint >= MULTILIGHT_LIGHT_RGB_1_ENDPOINT)
		  && (u8Endpoint < (MULTILIGHT_LIGHT_RGB_1_ENDPOINT + NUM_RGB_LIGHTS)))
	{
		*bIsRGB = TRUE;
		*u8Num = u8Endpoint - MULTILIGHT_LIGHT_RGB_1_ENDPOINT;
		return TRUE;
	}
	else
	{
		DBG_vPrintf(TRACE_LIGHT_TASK, "Unknown endpoint in bEndPointToNum %d\n", (int)u8Endpoint);
		return FALSE;
	}
}

/****************************************************************************
 *
 * NAME: vApp_eCLD_ColourControl_GetRGB
 *
 * DESCRIPTION:
 * To get RGB value
 *
 * PARAMETER
 * Type                   Name                    Description
 * uint8                  u8Endpoint              Endpoint to query
 * uint8 *                pu8Red                  Pointer to Red in RGB value
 * uint8 *                pu8Green                Pointer to Green in RGB value
 * uint8 *                pu8Blue                 Pointer to Blue in RGB value
 *
 * RETURNS:
 * teZCL_Status
 *
 ****************************************************************************/
PUBLIC void vApp_eCLD_ColourControl_GetRGB(uint8 u8Endpoint,uint8 *pu8Red,uint8 *pu8Green,uint8 *pu8Blue)
{
    eCLD_ColourControl_GetRGB(u8Endpoint,
                              pu8Red,
                              pu8Green,
                              pu8Blue);
}

/****************************************************************************
 *
 * NAME: vAPP_ZCL_DeviceSpecific_Init
 *
 * DESCRIPTION:
 * ZLL Device Specific initialization
 *
 * PARAMETER: void
 *
 * RETURNS: void
 *
 ****************************************************************************/
PUBLIC void vAPP_ZCL_DeviceSpecific_Init(void)
{
	unsigned int i;

	/* Initialise serial control interface */
	vLC_InitSerialInterface();
	/* Start measuring board temperature */
	vTS_InitTempSensor();


    /* Initialize the strings in Basic */
	if (u32ComputedWhiteMode == COMPUTED_WHITE_NONE)
	{
		for (i = 0; i < NUM_MONO_LIGHTS; i++)
		{
			memcpy(sLightMono[i].sBasicServerCluster.au8ManufacturerName, "NXP", CLD_BAS_MANUF_NAME_SIZE);
			memcpy(sLightMono[i].sBasicServerCluster.au8ModelIdentifier, "ZLL-MonoLight   ", CLD_BAS_MODEL_ID_SIZE);
			memcpy(sLightMono[i].sBasicServerCluster.au8DateCode, "20150212", CLD_BAS_DATE_SIZE);
			memcpy(sLightMono[i].sBasicServerCluster.au8SWBuildID, "1000-0004", CLD_BAS_SW_BUILD_SIZE);
			sIdEffectMono[i].u8Effect = E_CLD_IDENTIFY_EFFECT_STOP_EFFECT;
			sIdEffectMono[i].u8Tick = 0;
		}
	}
	for (i = 0; i < NUM_RGB_LIGHTS; i++)
	{
		memcpy(sLightRGB[i].sBasicServerCluster.au8ManufacturerName, "NXP", CLD_BAS_MANUF_NAME_SIZE);
		memcpy(sLightRGB[i].sBasicServerCluster.au8ModelIdentifier, "ZLL-ColorLight  ", CLD_BAS_MODEL_ID_SIZE);
		memcpy(sLightRGB[i].sBasicServerCluster.au8DateCode, "20150212", CLD_BAS_DATE_SIZE);
		memcpy(sLightRGB[i].sBasicServerCluster.au8SWBuildID, "1000-0004", CLD_BAS_SW_BUILD_SIZE);
		sIdEffectRGB[i].u8Effect = E_CLD_IDENTIFY_EFFECT_STOP_EFFECT;
		sIdEffectRGB[i].u8Tick = 0;
	}

	/* Load device-specific calibration values from NVM */
	vLC_LoadCalibrationFromNVM();
	/* Update bulb levels based on calibrations values */
	for (i = 0; i < NUM_BULBS; i++)
	{
		DriverBulb_vOutput((uint8)i);
	}
}

/****************************************************************************
 *
 * NAME: APP_vHandleIdentify
 *
 * DESCRIPTION:
 * ZLL Device Specific identify
 *
 * PARAMETERS:
 * u8Endpoint: Endpoint to identify
 *
 * RETURNS: void
 *
 ****************************************************************************/
PUBLIC void APP_vHandleIdentify(uint8 u8Endpoint) {

    uint8 u8Red, u8Green, u8Blue;
    uint8 u8Effect = E_CLD_IDENTIFY_EFFECT_STOP_EFFECT;
    uint8 u8Index;
    uint16 u16Time = 0;
    bool_t bIsRGB;

    if (!bEndPointToNum(u8Endpoint, &bIsRGB, &u8Index))
    {
    	return;
    }

    if (bIsRGB)
    {
    	u8Effect = sIdEffectRGB[u8Index].u8Effect;
    	u16Time = sLightRGB[u8Index].sIdentifyServerCluster.u16IdentifyTime;
    }
    else if (u32ComputedWhiteMode == COMPUTED_WHITE_NONE)
    {
    	u8Effect = sIdEffectMono[u8Index].u8Effect;
    	u16Time = sLightMono[u8Index].sIdentifyServerCluster.u16IdentifyTime;
    }

    DBG_vPrintf(TRACE_LIGHT_TASK, "JP Time %d\n", u16Time);

    if (u8Effect < E_CLD_IDENTIFY_EFFECT_STOP_EFFECT) {
        /* do nothing */
        //DBG_vPrintf(TRACE_LIGHT_TASK, "Effect do nothing\n");
    }
    else if (u16Time == 0)
    {
		/*
		 * Restore to off/off/colour state
		 */
        DBG_vPrintf(TRACE_PATH, "\nPath 3");

        if (bIsRGB)
        {
			vApp_eCLD_ColourControl_GetRGB(MULTILIGHT_LIGHT_RGB_1_ENDPOINT + u8Index, &u8Red, &u8Green, &u8Blue);

			DBG_vPrintf(TRACE_LIGHT_TASK, "R %d G %d B %d L %d Hue %d Sat %d\n", u8Red, u8Green, u8Blue,
			                    sLightRGB[u8Index].sLevelControlServerCluster.u8CurrentLevel,
			                    sLightRGB[u8Index].sColourControlServerCluster.u8CurrentHue,
			                    sLightRGB[u8Index].sColourControlServerCluster.u8CurrentSaturation);

			DBG_vPrintf(TRACE_LIGHT_TASK, "\nidentify stop");

			vRGBLight_SetLevels(BULB_NUM_RGB(u8Index),
					            sLightRGB[u8Index].sOnOffServerCluster.bOnOff,
					            sLightRGB[u8Index].sLevelControlServerCluster.u8CurrentLevel,
			                    u8Red,
			                    u8Green,
			                    u8Blue);
        }
        else if (u32ComputedWhiteMode == COMPUTED_WHITE_NONE)
        {
        	vSetBulbState(BULB_NUM_MONO(u8Index),
						  sLightMono[u8Index].sOnOffServerCluster.bOnOff,
						  sLightMono[u8Index].sLevelControlServerCluster.u8CurrentLevel);
        }
    }
    else
	{
		/* Set the Identify levels */
		DBG_vPrintf(TRACE_PATH, "\nPath 4");
		if (bIsRGB)
		{
			vRGBLight_SetLevels(BULB_NUM_RGB(u8Index), TRUE, 159, 250, 0, 0);
		}
		else if (u32ComputedWhiteMode == COMPUTED_WHITE_NONE)
		{
			sIdEffectMono[u8Index].u8Level = 250;
			sIdEffectMono[u8Index].u8Count = 5;
			vSetBulbState(BULB_NUM_MONO(u8Index), TRUE, CLD_LEVELCONTROL_MAX_LEVEL);
		}
	}
}

/****************************************************************************
 *
 * NAME: APP_vHandleIdentifyAll
 *
 * DESCRIPTION:
 * Identify all endpoints on device
 *
 * PARAMETERS:
 * void
 *
 * RETURNS: void
 *
 ****************************************************************************/
PUBLIC void APP_vHandleIdentifyAll(void)
{
	uint8 u8Endpoint;

	if (u32ComputedWhiteMode == COMPUTED_WHITE_NONE)
	{
		for (u8Endpoint = MULTILIGHT_LIGHT_MONO_1_ENDPOINT;
				u8Endpoint < (MULTILIGHT_LIGHT_MONO_1_ENDPOINT + NUM_MONO_LIGHTS);
				u8Endpoint++)
		{
			APP_vHandleIdentify(u8Endpoint);
		}
	}
	for (u8Endpoint = MULTILIGHT_LIGHT_RGB_1_ENDPOINT;
			u8Endpoint < (MULTILIGHT_LIGHT_RGB_1_ENDPOINT + NUM_RGB_LIGHTS);
			u8Endpoint++)
	{
		APP_vHandleIdentify(u8Endpoint);
	}
}

/****************************************************************************
 *
 * NAME: vIdEffectTick
 *
 * DESCRIPTION:
 * ZLL Device Specific identify tick
 *
 * PARAMETER: void
 *
 * RETURNS: void
 *
 ****************************************************************************/
PUBLIC void vIdEffectTick(void) {
	uint8 i;

    for (i = 0; i < NUM_RGB_LIGHTS; i++)
    {
    	/* Colour lights */

		if (sIdEffectRGB[i].u8Effect < E_CLD_IDENTIFY_EFFECT_STOP_EFFECT)
		{
			if (sIdEffectRGB[i].u8Tick > 0)
			{
				DBG_vPrintf(TRACE_PATH, "\nPath 5");

				sIdEffectRGB[i].u8Tick--;
				/* Set the light parameters */
				vRGBLight_SetLevels(BULB_NUM_RGB(i),
						            TRUE,
						            sIdEffectRGB[i].u8Level,
						            sIdEffectRGB[i].u8Red,
						            sIdEffectRGB[i].u8Green,
						            sIdEffectRGB[i].u8Blue);
				/* Now adjust parameters ready for for next round */
				switch (sIdEffectRGB[i].u8Effect) {
					case E_CLD_IDENTIFY_EFFECT_BLINK:
						break;

					case E_CLD_IDENTIFY_EFFECT_BREATHE:
						if (sIdEffectRGB[i].bDirection) {
							if (sIdEffectRGB[i].u8Level >= 250) {
								sIdEffectRGB[i].u8Level -= 50;
								sIdEffectRGB[i].bDirection = 0;
							} else {
								sIdEffectRGB[i].u8Level += 50;
							}
						} else {
							if (sIdEffectRGB[i].u8Level == 0) {
								// go back up, check for stop
								sIdEffectRGB[i].u8Count--;
								if ((sIdEffectRGB[i].u8Count) && ( !sIdEffectRGB[i].bFinish)) {
									sIdEffectRGB[i].u8Level += 50;
									sIdEffectRGB[i].bDirection = 1;
								} else {
									//DBG_vPrintf(TRACE_LIGHT_TASK, "\n>>set tick 0<<");
									/* lpsw2773 - stop the effect on the next tick */
									sIdEffectRGB[i].u8Tick = 0;
								}
							} else {
								sIdEffectRGB[i].u8Level -= 50;
							}
						}
						break;
					default:
						if ( sIdEffectRGB[i].bFinish ) {
							sIdEffectRGB[i].u8Tick = 0;
						}
					}
			} else {
				/*
				 * Effect finished, restore the light
				 */
				DBG_vPrintf(TRACE_PATH, "\nPath 6");
				sIdEffectRGB[i].u8Effect = E_CLD_IDENTIFY_EFFECT_STOP_EFFECT;
				sIdEffectRGB[i].bDirection = FALSE;
				APP_ZCL_vSetIdentifyTime(FALSE, MULTILIGHT_LIGHT_RGB_1_ENDPOINT + i, 0);
				uint8 u8Red, u8Green, u8Blue;
				vApp_eCLD_ColourControl_GetRGB(MULTILIGHT_LIGHT_RGB_1_ENDPOINT + i, &u8Red, &u8Green, &u8Blue);
				DBG_vPrintf(TRACE_LIGHT_TASK, "EF - R %d G %d B %d L %d Hue %d Sat %d\n",
				                    u8Red,
				                    u8Green,
				                    u8Blue,
				                    sLightRGB[i].sLevelControlServerCluster.u8CurrentLevel,
				                    sLightRGB[i].sColourControlServerCluster.u8CurrentHue,
				                    sLightRGB[i].sColourControlServerCluster.u8CurrentSaturation);

				vRGBLight_SetLevels(BULB_NUM_RGB(i),
						            sLightRGB[i].sOnOffServerCluster.bOnOff,
						            sLightRGB[i].sLevelControlServerCluster.u8CurrentLevel,
				                    u8Red,
				                    u8Green,
				                    u8Blue);
			}
		}
    }

    if (u32ComputedWhiteMode == COMPUTED_WHITE_NONE)
    {
		for (i = 0; i < NUM_MONO_LIGHTS; i++)
		{
			/* Mono lights */

			if (sIdEffectMono[i].u8Effect < E_CLD_IDENTIFY_EFFECT_STOP_EFFECT)
			{
				if (sIdEffectMono[i].u8Tick > 0)
				{
					//DBG_vPrintf(TRACE_PATH, "\nPath 5");

					sIdEffectMono[i].u8Tick--;
					/* Set the light parameters */

					vSetBulbState(BULB_NUM_MONO(i), TRUE, sIdEffectMono[i].u8Level);

					/* Now adjust parameters ready for for next round */
					switch (sIdEffectMono[i].u8Effect) {
						case E_CLD_IDENTIFY_EFFECT_BLINK:
							break;

						case E_CLD_IDENTIFY_EFFECT_BREATHE:
							if (sIdEffectMono[i].bDirection) {
								if (sIdEffectMono[i].u8Level >= 250) {
									sIdEffectMono[i].u8Level -= 50;
									sIdEffectMono[i].bDirection = 0;
								} else {
									sIdEffectMono[i].u8Level += 50;
								}
							} else {
								if (sIdEffectMono[i].u8Level == 0) {
									// go back up, check for stop
									sIdEffectMono[i].u8Count--;
									if ((sIdEffectMono[i].u8Count) && ( !sIdEffectMono[i].bFinish)) {
										sIdEffectMono[i].u8Level += 50;
										sIdEffectMono[i].bDirection = 1;
									} else {
										//DBG_vPrintf(TRACE_LIGHT_TASK, "\n>>set tick 0<<");
										/* lpsw2773 - stop the effect on the next tick */
										sIdEffectMono[i].u8Tick = 0;
									}
								} else {
									sIdEffectMono[i].u8Level -= 50;
								}
							}
							break;
						case E_CLD_IDENTIFY_EFFECT_OKAY:
							if ((sIdEffectMono[i].u8Tick == 10) || (sIdEffectMono[i].u8Tick == 5)) {
								sIdEffectMono[i].u8Level ^= 254;
								if (sIdEffectMono[i].bFinish) {
									sIdEffectMono[i].u8Tick = 0;
								}
							}
							break;
						case E_CLD_IDENTIFY_EFFECT_CHANNEL_CHANGE:
							if ( sIdEffectMono[i].u8Tick == 74) {
								sIdEffectMono[i].u8Level = 1;
								if (sIdEffectMono[i].bFinish) {
									sIdEffectMono[i].u8Tick = 0;
								}
							}
							break;
						default:
							if ( sIdEffectMono[i].bFinish ) {
								sIdEffectMono[i].u8Tick = 0;
							}
						}
				} else {
					/*
					 * Effect finished, restore the light
					 */
					DBG_vPrintf(TRACE_PATH, "\nEffect End");
					sIdEffectMono[i].u8Effect = E_CLD_IDENTIFY_EFFECT_STOP_EFFECT;
					sIdEffectMono[i].bDirection = FALSE;
					APP_ZCL_vSetIdentifyTime(FALSE, MULTILIGHT_LIGHT_MONO_1_ENDPOINT + i, 0);
					vSetBulbState(BULB_NUM_MONO(i),
								  sLightMono[i].sOnOffServerCluster.bOnOff,
								  sLightMono[i].sLevelControlServerCluster.u8CurrentLevel);
				}
			} else {
				if (sLightMono[i].sIdentifyServerCluster.u16IdentifyTime) {
					sIdEffectMono[i].u8Count--;
					if (0 == sIdEffectMono[i].u8Count) {
						sIdEffectMono[i].u8Count = 5;
						if (sIdEffectMono[i].u8Level) {
							sIdEffectMono[i].u8Level = 0;
							vSetBulbState(BULB_NUM_MONO(i), FALSE, 0);
						}
						else
						{
							sIdEffectMono[i].u8Level = 250;
							vSetBulbState(BULB_NUM_MONO(i), TRUE, CLD_LEVELCONTROL_MAX_LEVEL);
						}
					}
				}
			}
		}
    }
}

/****************************************************************************
 *
 * NAME: vStartEffect
 *
 * DESCRIPTION:
 * ZLL Device Specific identify effect set up
 *
 * PARAMETER: endpoint to start effect on, and effect number
 *
 * RETURNS: void
 *
 ****************************************************************************/
PUBLIC void vStartEffect(uint8 u8Endpoint, uint8 u8Effect) {

	uint8 u8Index;
	bool_t bIsRGB;

	if (!bEndPointToNum(u8Endpoint, &bIsRGB, &u8Index))
	{
		return;
	}

	if (bIsRGB)
	{
		/* Colour light */

		switch (u8Effect) {
			case E_CLD_IDENTIFY_EFFECT_BLINK:
				sIdEffectRGB[u8Index].u8Effect = E_CLD_IDENTIFY_EFFECT_BLINK;
				sIdEffectRGB[u8Index].u8Level = 250;
				sIdEffectRGB[u8Index].u8Red = 255;
				sIdEffectRGB[u8Index].u8Green = 0;
				sIdEffectRGB[u8Index].u8Blue = 0;
				sIdEffectRGB[u8Index].bFinish = FALSE;
				APP_ZCL_vSetIdentifyTime(FALSE, u8Endpoint, 2);
				sIdEffectRGB[u8Index].u8Tick = 10;
				break;
			case E_CLD_IDENTIFY_EFFECT_BREATHE:
				sIdEffectRGB[u8Index].u8Effect = E_CLD_IDENTIFY_EFFECT_BREATHE;
				sIdEffectRGB[u8Index].bDirection = 1;
				sIdEffectRGB[u8Index].bFinish = FALSE;
				sIdEffectRGB[u8Index].u8Level = 0;
				sIdEffectRGB[u8Index].u8Count = 15;
				eCLD_ColourControl_GetRGB(u8Endpoint,
						                  &sIdEffectRGB[u8Index].u8Red,
						                  &sIdEffectRGB[u8Index].u8Green,
						                  &sIdEffectRGB[u8Index].u8Blue);
				APP_ZCL_vSetIdentifyTime(FALSE, u8Endpoint, 17);
				sIdEffectRGB[u8Index].u8Tick = 200;
				break;
			case E_CLD_IDENTIFY_EFFECT_OKAY:
				sIdEffectRGB[u8Index].u8Effect = E_CLD_IDENTIFY_EFFECT_OKAY;
				sIdEffectRGB[u8Index].bFinish = FALSE;
				sIdEffectRGB[u8Index].u8Level = 250;
				sIdEffectRGB[u8Index].u8Red = 0;
				sIdEffectRGB[u8Index].u8Green = 255;
				sIdEffectRGB[u8Index].u8Blue = 0;
				APP_ZCL_vSetIdentifyTime(FALSE, u8Endpoint, 2);
				sIdEffectRGB[u8Index].u8Tick = 10;
				break;
			case E_CLD_IDENTIFY_EFFECT_CHANNEL_CHANGE:
				sIdEffectRGB[u8Index].u8Effect = E_CLD_IDENTIFY_EFFECT_CHANNEL_CHANGE;
				sIdEffectRGB[u8Index].u8Level = 250;
				sIdEffectRGB[u8Index].u8Red = 255;
				sIdEffectRGB[u8Index].u8Green = 127;
				sIdEffectRGB[u8Index].u8Blue = 4;
				sIdEffectRGB[u8Index].bFinish = FALSE;
				APP_ZCL_vSetIdentifyTime(FALSE, u8Endpoint, 9);
				sIdEffectRGB[u8Index].u8Tick = 80;
				break;

			case E_CLD_IDENTIFY_EFFECT_FINISH_EFFECT:
				if (sIdEffectRGB[u8Index].u8Effect < E_CLD_IDENTIFY_EFFECT_STOP_EFFECT)
				{
					DBG_vPrintf(TRACE_LIGHT_TASK, "\n<FINISH>");
					sIdEffectRGB[u8Index].bFinish = TRUE;
				}
				break;
			case E_CLD_IDENTIFY_EFFECT_STOP_EFFECT:
				sIdEffectRGB[u8Index].u8Effect = E_CLD_IDENTIFY_EFFECT_STOP_EFFECT;
				APP_ZCL_vSetIdentifyTime(FALSE, u8Endpoint, 1);
				break;
		}
	}
	else if (u32ComputedWhiteMode == COMPUTED_WHITE_NONE)
	{
		/* Mono light */

		switch (u8Effect) {
		case E_CLD_IDENTIFY_EFFECT_BLINK:
			sIdEffectMono[u8Index].u8Effect = E_CLD_IDENTIFY_EFFECT_BLINK;
			if (sLightMono[u8Index].sOnOffServerCluster.bOnOff) {
				sIdEffectMono[u8Index].u8Level = 0;
			} else {
				sIdEffectMono[u8Index].u8Level = 250;
			}
			sIdEffectMono[u8Index].bFinish = FALSE;
			APP_ZCL_vSetIdentifyTime(FALSE, u8Endpoint, 2);
			sIdEffectMono[u8Index].u8Tick = 10;
			break;
		case E_CLD_IDENTIFY_EFFECT_BREATHE:
			sIdEffectMono[u8Index].u8Effect = E_CLD_IDENTIFY_EFFECT_BREATHE;
			sIdEffectMono[u8Index].bDirection = 1;
			sIdEffectMono[u8Index].bFinish = FALSE;
			sIdEffectMono[u8Index].u8Level = 0;
			sIdEffectMono[u8Index].u8Count = 15;
			APP_ZCL_vSetIdentifyTime(FALSE, u8Endpoint, 17);
			sIdEffectMono[u8Index].u8Tick = 200;
			break;
		case E_CLD_IDENTIFY_EFFECT_OKAY:
			sIdEffectMono[u8Index].u8Effect = E_CLD_IDENTIFY_EFFECT_OKAY;
			sIdEffectMono[u8Index].bFinish = FALSE;
			if (sLightMono[u8Index].sOnOffServerCluster.bOnOff) {
				sIdEffectMono[u8Index].u8Level = 0;
			} else {
				sIdEffectMono[u8Index].u8Level = 254;
			}
			APP_ZCL_vSetIdentifyTime(FALSE, u8Endpoint, 3);
			sIdEffectMono[u8Index].u8Tick = 15;
			break;
		case E_CLD_IDENTIFY_EFFECT_CHANNEL_CHANGE:
			sIdEffectMono[u8Index].u8Effect = E_CLD_IDENTIFY_EFFECT_CHANNEL_CHANGE;
			sIdEffectMono[u8Index].u8Level = 254;
			sIdEffectMono[u8Index].bFinish = FALSE;
			APP_ZCL_vSetIdentifyTime(FALSE, u8Endpoint, 9);
			sIdEffectMono[u8Index].u8Tick = 80;
			break;

		case E_CLD_IDENTIFY_EFFECT_FINISH_EFFECT:
			if (sIdEffectMono[u8Index].u8Effect < E_CLD_IDENTIFY_EFFECT_STOP_EFFECT)
			{
				DBG_vPrintf(TRACE_LIGHT_TASK, "\n<FINISH>");
				sIdEffectMono[u8Index].bFinish = TRUE;
			}
			break;
		case E_CLD_IDENTIFY_EFFECT_STOP_EFFECT:
			sIdEffectMono[u8Index].u8Effect = E_CLD_IDENTIFY_EFFECT_STOP_EFFECT;
			APP_ZCL_vSetIdentifyTime(FALSE, u8Endpoint, 1);
			break;
		}
	}
}


/****************************************************************************
 *
 * NAME: vRGBLight_SetLevels
 *
 * DESCRIPTION:
 * Set the RGB and levels
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/

PUBLIC void vRGBLight_SetLevels(uint8 u8Bulb, bool_t bOn, uint8 u8Level, uint8 u8Red, uint8 u8Green, uint8 u8Blue)
{
	uint32 v;
	uint8 u8ComputedWhite;

    if (bOn == TRUE)
    {
    	if (u32ComputedWhiteMode == COMPUTED_WHITE_NONE)
    	{
    		vLI_Start(u8Bulb, u8Level, u8Red, u8Green, u8Blue, 0);
    	}
    	else if ((u32ComputedWhiteMode == COMPUTED_WHITE_BETTER_COLOR)
    			|| (u32ComputedWhiteMode == COMPUTED_WHITE_BETTER_BRIGHTNESS))
    	{
    		/* Better color:
    		 * white = MIN(R, G, B)
    		 * R = R - white
    		 * G = G - white
    		 * B = B - white
    		 *
    		 * Better brightness:
    		 * white = MIN(R, G, B) */

    		u8ComputedWhite = MIN(u8Red, u8Green);
    		u8ComputedWhite = MIN(u8ComputedWhite, u8Blue);
    		if (u32ComputedWhiteMode == COMPUTED_WHITE_BETTER_COLOR)
    		{
    			u8Red -= u8ComputedWhite;
    			u8Green -= u8ComputedWhite;
    			u8Blue -= u8ComputedWhite;
    		}
    		vLI_Start(u8Bulb, u8Level, u8Red, u8Green, u8Blue, 0);
    		/* Scale u8ComputedWhite by desired level */
    		v = (uint32)u8ComputedWhite * (uint32)u8Level;
    		vLI_Start(RGB_BULB_TO_MONO(u8Bulb), (uint8)(FAST_DIV_BY_255(v)), 0, 0, 0, 0);
    	}
    }
    else
    {
        vLI_Stop(u8Bulb);
        if (u32ComputedWhiteMode != COMPUTED_WHITE_NONE)
        {
        	/* Also stop associated computed white bulb */
        	vLI_Stop(RGB_BULB_TO_MONO(u8Bulb));
        }
    }
    DriverBulb_vSetOnOff(u8Bulb, bOn);
    if (u32ComputedWhiteMode != COMPUTED_WHITE_NONE)
	{
    	/* Turn on/off associated computed white bulb */
    	DriverBulb_vSetOnOff(RGB_BULB_TO_MONO(u8Bulb), bOn);
	}
}

/****************************************************************************
 *
 * NAME: vSetBulbState
 *
 * DESCRIPTION:
 * Set level of dimmable bulb
 *
 * RETURNS: void
 *
 ****************************************************************************/
PUBLIC void vSetBulbState(uint8 u8Bulb, bool bOn, uint8 u8Level)
{
	if (bOn)
	{
		vLI_Start(u8Bulb, u8Level, 0, 0, 0, 0);
	}
	else
    {
        vLI_Stop(u8Bulb);
    }
	DriverBulb_vSetOnOff(u8Bulb, bOn);
}

/****************************************************************/
/* OS Stub functions to allow single osconfig diagram (ZLL/ZHA) */
/* to be used for all driver variants (just clear interrupt)    */
/****************************************************************/

#ifndef DR1192

OS_ISR(vISR_Timer3)
{
	(void) u8AHI_TimerFired(E_AHI_TIMER_3);
}

OS_ISR(vISR_Timer4)
{
	(void) u8AHI_TimerFired(E_AHI_TIMER_4);
}

#endif
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
