/*
 * DriverBulb_TimerPWM.c
 *
 *  Created on: 29/05/2018
 *      Author: Chris
 *
 * Bulb driver that uses the JN5168's internal timers to output PWM.
 */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <jendefs.h>
#include <AppHardwareApi.h>
#include <stdio.h>
#include <string.h>
#include "App_MultiLight.h"
#include "DriverBulb.h"
#include "app_light_calibration.h"
#include "app_temp_sensor.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/* Timer number of timer which starts the other timers at specific
 * times. */
#define PHASE_CONTROLLER_TIMER		1
/* Number of active PWM channels */
#define NUM_PWM_CHANNELS			((NUM_MONO_LIGHTS) + ((NUM_RGB_LIGHTS) * 3))

#define FAST_DIV_BY_255(x)			((((x) << 8) + (x) + 255) >> 16)

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

PRIVATE void UpdatePWMValue(uint8 u8Timer, uint16 u16Value);

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
PRIVATE bool_t  bIsOn[NUM_BULBS];
PRIVATE uint8   u8CurrLevel[NUM_BULBS];
PRIVATE uint8   u8CurrRed[NUM_BULBS];
PRIVATE uint8   u8CurrGreen[NUM_BULBS];
PRIVATE uint8   u8CurrBlue[NUM_BULBS];

/* List of PWM channels. These values are not timer numbers, they are values
 * that can be passed to the integrated peripheral library. */
PRIVATE volatile uint8  au8PWMChannels[NUM_PWM_CHANNELS];
/* List of PWM values (u16Hi parameter for vAHI_TimerStartRepeat) which will
 * be written into the corresponding PWM channel specified by au8PWMChannels. */
PRIVATE volatile uint16 au16PWMValues[NUM_PWM_CHANNELS];
/* Entries in this list will be set to TRUE if the corresponding entry in
 * au16PWMValues has been updated. */
PRIVATE volatile bool_t abPWMUpdated[NUM_PWM_CHANNELS];
/* This is an index into au8PWMChannels/au16PWMValues. This specifies the
 * channel which the phase controller will update next. */
PRIVATE volatile uint8  u8CurrentPWMChannel;

/* Array that is used to convert timer number into a value that can be passed
 * to the integrated peripheral library. */
PRIVATE uint8 au8Timers[5] = {E_AHI_TIMER_0, E_AHI_TIMER_1,
		E_AHI_TIMER_2, E_AHI_TIMER_3, E_AHI_TIMER_4};

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME:       		DriverBulb_vInit
 *
 * DESCRIPTION:		Initializes the JN516X's I2C system and relevant timers
 *
 * PARAMETERS:      Name     RW  Usage
 *
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void DriverBulb_vInit(void)
{
	static bool_t bInit = FALSE;
	uint8 i;
	teColour j;
	uint8 u8PWMIndex = 0;
	uint8 u8Timer;

	/* Not already initialized ? */
	if (bInit == FALSE)
	{
		/* Set all DIOs to be inputs, except for DIO11, which is connected to
		 * a green indicator LED */
		vAHI_DioSetDirection(0xfffff7ff, 0x00000800);

		/* Set DIO11 low i.e. switch LED on */
		vAHI_DioSetOutput(0x00000000, 0xffffffff);

		/* All timers have their prescaler set to 2x, so that the PWM frequency
		 * is about 2 kHz, similar to what the PCA9685 would produce. */

		/* Set up white lights. i is bulb number; white bulbs start at index 0 */
		for (i = 0; i < NUM_MONO_LIGHTS; i++)
		{
			u8Timer = au8Timers[u8LC_GetChannel(i, BULB_WHITE)];
			vAHI_TimerEnable(u8Timer, 1, FALSE, FALSE, TRUE);
			/* PWM invert is enabled, so that output will be high for the PWM
			 * value, instead of being low for the PWM value. */
			vAHI_TimerConfigureOutputs(u8Timer, TRUE, TRUE);
			abPWMUpdated[u8PWMIndex] = TRUE;
			au8PWMChannels[u8PWMIndex] = u8Timer;
			u8PWMIndex++;
		}
		/* Set up RGB lights. i is bulb number; RGB bulbs start at index NUM_MONO_LIGHTS */
		for (i = NUM_MONO_LIGHTS; i < NUM_BULBS; i++)
		{
			for (j = BULB_RED; j <= BULB_BLUE; j++)
			{
				u8Timer = au8Timers[u8LC_GetChannel(i, j)];
				vAHI_TimerEnable(u8Timer, 1, FALSE, FALSE, TRUE);
				vAHI_TimerConfigureOutputs(u8Timer, TRUE, TRUE);
				abPWMUpdated[u8PWMIndex] = TRUE;
				au8PWMChannels[u8PWMIndex] = u8Timer;
				u8PWMIndex++;
			}
		}

		/* Don't let phase controller timer output to DIO, as that's not
		 * necessary. The phase controller timer only needs to trigger
		 * interrupts. */
		vAHI_TimerDIOControl(au8Timers[PHASE_CONTROLLER_TIMER], FALSE);
		vAHI_TimerEnable(au8Timers[PHASE_CONTROLLER_TIMER], 1, FALSE, TRUE, FALSE);
		/* Start phase control timer to trigger an interrupt for each channel
		 * once in each PWM cycle. This is done so that all the timers don't
		 * start at the same time. This reduces stress on the power supply as
		 * all lights don't have to turn on at the same time. */
		vAHI_TimerStartRepeat(au8Timers[PHASE_CONTROLLER_TIMER], 0, 4096 / NUM_PWM_CHANNELS);

		/* Now initialized */
		bInit = TRUE;
	}
}

/****************************************************************************
 *
 * NAME:            DriverBulb_vOn
 *
 * DESCRIPTION:     Turns a bulb on
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number of the bulb to turn on
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void DriverBulb_vOn(uint8 u8Bulb)
{
	/* Lamp is not on ? */
	if (bIsOn[u8Bulb] != TRUE)
	{
		/* Note light is on */
		bIsOn[u8Bulb] = TRUE;
		/* Set outputs */
		DriverBulb_vOutput(u8Bulb);
	}
}

/****************************************************************************
 *
 * NAME:            DriverBulb_vOff
 *
 * DESCRIPTION:     Turns a bulb off
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number of the bulb to turn off
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void DriverBulb_vOff(uint8 u8Bulb)
{
	/* Lamp is on ? */
	if (bIsOn[u8Bulb] == TRUE)
	{
		/* Note light is off */
		bIsOn[u8Bulb] = FALSE;
		/* Set outputs */
		DriverBulb_vOutput(u8Bulb);
	}
}

/****************************************************************************
 *
 * NAME:            DriverBulb_vSetOnOff
 *
 * DESCRIPTION:     Turns a bulb off
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number of the bulb to turn on or off
 *                  bOn      R   Whether to turn bulb on (TRUE) or off (FALSE)
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void DriverBulb_vSetOnOff(uint8 u8Bulb, bool_t bOn)
{
	(bOn) ? DriverBulb_vOn(u8Bulb) : DriverBulb_vOff(u8Bulb);
}

/****************************************************************************
 *
 * NAMES:           DriverBulb_bOn
 *
 * DESCRIPTION:		Access functions for Monitored Lamp Parameters
 *
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number of the bulb to query
 *
 * RETURNS:
 * Lamp state
 *
 ****************************************************************************/
PUBLIC bool_t DriverBulb_bOn(uint8 u8Bulb)
{
	return (bIsOn[u8Bulb]);
}

/****************************************************************************
 *
 * NAME:       		DriverBulb_vSetLevel
 *
 * DESCRIPTION:		Sets brightness level of a bulb
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number to set level of
 *         	        u32Level R   Light level 0-LAMP_LEVEL_MAX
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void DriverBulb_vSetLevel(uint8 u8Bulb, uint32 u32Level)
{
	/* Different value ? */
	if (u8CurrLevel[u8Bulb] != (uint8) u32Level)
	{
		/* Note the new level */
		if (u32Level > CLD_LEVELCONTROL_MAX_LEVEL)
		{
			u8CurrLevel[u8Bulb] = CLD_LEVELCONTROL_MAX_LEVEL;
		}
		else
		{
			u8CurrLevel[u8Bulb] = (uint8) MAX(1, u32Level);
		}
		/* Is the lamp on ? */
		if (bIsOn[u8Bulb])
		{
			/* Set outputs */
			DriverBulb_vOutput(u8Bulb);
		}
	}
}

/****************************************************************************
 *
 * NAME:       		DriverBulb_vSetColour
 *
 * DESCRIPTION:		Updates colour of a bulb
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb number to set colour of
 *         	        u32Red   R   Relative red amount
 *         	        u32Green R   Relative green amount
 *         	        u32Blue  R   Relative blue amount
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/

PUBLIC void DriverBulb_vSetColour(uint8 u8Bulb, uint32 u32Red, uint32 u32Green, uint32 u32Blue)
{
	/* Different value ? */
	if ((u8CurrRed[u8Bulb] != (uint8) u32Red)
	 || (u8CurrGreen[u8Bulb] != (uint8) u32Green)
	 || (u8CurrBlue[u8Bulb] != (uint8) u32Blue))
	{
		/* Note the new values */
		u8CurrRed[u8Bulb]   = (uint8) MIN(u32Red, 255);
		u8CurrGreen[u8Bulb] = (uint8) MIN(u32Green, 255);
		u8CurrBlue[u8Bulb]  = (uint8) MIN(u32Blue, 255);
		/* Is the lamp on ? */
		if (bIsOn[u8Bulb])
		{
			/* Set outputs */
			DriverBulb_vOutput(u8Bulb);
		}
	}
}

/****************************************************************************
 *
 * NAME:			DriverBulb_vOutput
 *
 * DESCRIPTION:     Tell PCA9685 to update PWM channels for a bulb
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Bulb   R   Bulb to update
 *
 *
 * RETURNS:         void
 *
 ****************************************************************************/
PUBLIC void DriverBulb_vOutput(uint8 u8Bulb)
{
	uint32  v;
	uint16  u16PWM;
	uint8   u8Brightness[3];
	int8    i;
	uint8   u8Channel[3];
	uint8   u8NumChannels;
	bool_t  bIsRGB;

	bIsRGB = u8Bulb >= NUM_MONO_LIGHTS;
	u8NumChannels = bIsRGB ? 3 : 1;

	/* Is bulb on ? */
	if (bIsOn[u8Bulb] && !bOverheat)
	{
		if (bIsRGB)
		{
			/* Scale colour for brightness level */
			v = (uint32)u8CurrRed[u8Bulb] * (uint32)u8CurrLevel[u8Bulb];
			u8Brightness[0] = (uint8)FAST_DIV_BY_255(v);
			u8Channel[0] = u8LC_GetChannel(u8Bulb, BULB_RED);
			v = (uint32)u8CurrGreen[u8Bulb] * (uint32)u8CurrLevel[u8Bulb];
			u8Brightness[1] = (uint8)FAST_DIV_BY_255(v);
			u8Channel[1] = u8LC_GetChannel(u8Bulb, BULB_GREEN);
			v = (uint32)u8CurrBlue[u8Bulb] * (uint32)u8CurrLevel[u8Bulb];
			u8Brightness[2] = (uint8)FAST_DIV_BY_255(v);
			u8Channel[2] = u8LC_GetChannel(u8Bulb, BULB_BLUE);
		}
		else
		{
			u8Brightness[0] = u8CurrLevel[u8Bulb];
			u8Channel[0] = u8LC_GetChannel(u8Bulb, BULB_WHITE);
		}

		for (i = 0; i < u8NumChannels; i++)
		{
			/* Don't allow fully off */
			if (u8Brightness[i] == 0) u8Brightness[i] = 1;
			/* Set PWM duty cycle */
			u16PWM = (uint16)u32LC_AdjustIntensity(u8Brightness[i], u8Channel[i]);
			if (u16PWM >= 4095)
			{
				/* Full on */
				u16PWM = 4096;
			}
			UpdatePWMValue(au8Timers[u8Channel[i]], u16PWM);
		}
	}
	else /* Turn off */
	{
		if (bIsRGB)
		{
			u8Channel[0] = u8LC_GetChannel(u8Bulb, BULB_RED);
			u8Channel[1] = u8LC_GetChannel(u8Bulb, BULB_GREEN);
			u8Channel[2] = u8LC_GetChannel(u8Bulb, BULB_BLUE);
		}
		else
		{
			u8Channel[0] = u8LC_GetChannel(u8Bulb, BULB_WHITE);
		}
		for (i = 0; i < u8NumChannels; i++)
		{
			/* Full off */
			UpdatePWMValue(au8Timers[u8Channel[i]], 0);
		}
	}

}

/****************************************************************************
 * NAME: APP_isrTimer1
 *
 * DESCRIPTION:
 * ISR for Timer1
 ****************************************************************************/
OS_ISR(APP_isrTimer1)
{
	/* Clear interrupt source */
	u8AHI_TimerFired(au8Timers[PHASE_CONTROLLER_TIMER]);

	if (abPWMUpdated[u8CurrentPWMChannel])
	{
		/* Update next PWM value */
		vAHI_TimerStartRepeat(au8PWMChannels[u8CurrentPWMChannel], au16PWMValues[u8CurrentPWMChannel], 4096);
		abPWMUpdated[u8CurrentPWMChannel] = FALSE;
	}

	/* Move to next PWM channel */
	u8CurrentPWMChannel++;
	if (u8CurrentPWMChannel >= NUM_PWM_CHANNELS)
	{
		u8CurrentPWMChannel = 0;
	}
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME:			UpdatePWMValue
 *
 * DESCRIPTION:     Update PWM value for a timer.
 *                  To avoid phase glitches, this will not update the PWM value
 *                  immediately. Instead, the value will be updated in memory
 *                  and a separate timer (PHASE_CONTROLLER_TIMER) will do the
 *                  actual update at a more appropriate time.
 *
 * PARAMETERS:      Name     RW  Usage
 *                  u8Timer  R   Timer to update (integrated peripherals
 *                               library value)
 *                  u16Value R   PWM value, 0 = fully off, 4095 = fully on
 *
 *
 * RETURNS:         void
 *
 ****************************************************************************/
PRIVATE void UpdatePWMValue(uint8 u8Timer, uint16 u16Value)
{
	uint8 i;

	/* Find entry in au8PWMChannels, and update the corresponding entry in
	 * au16PWMValues. */
	for (i = 0; i < NUM_PWM_CHANNELS; i++)
	{
		if (au8PWMChannels[i] == u8Timer)
		{
			au16PWMValues[i] = u16Value;
			abPWMUpdated[i] = TRUE;
			break;
		}
	}
}
