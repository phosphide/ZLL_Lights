# Serial configuration guide
Use 38400 baud, 8 data bits, no parity, 1 stop bit. All commands should be terminated with CR and LF. All responses will be terminated by CR and LF. In this document, CR will be represented by "\r" and LF will be represented by "\n".
## Commands

### Get version
Command format: ```v```

Command response: ```<version string>```

Example:
```
v\r\n
MultiLight Standard built on May 30 2018 21:39:19\r\n
```

This gets the firmware variant and build date. The first two words will be "MultiLight Mini" for the mini variant, and "MultiLight Standard" for the standard variant.

### Get temperature
Command format: ```t```

Command response: ```<temperature>```

Example:
```
t\r\n
24\r\n
```
This gets the board temperature, in degrees Celsius.

### Get raw channel names
Command format: ```n```

Command response: ```Comma-separated list of <channel>=<name>```

Example:
```
n\r\n
7=White 1,6=White 2,5=White 3,4=Red 1,3=Green 1,2=Blue 1,1=Red 2,0=Green 2,11=Blue 2,10=Red 3,9=Green 3,8=Blue 3\r\n
```
The firmware internally uses raw channel numbers, which are integers starting from 0. This command allows you to query the name of each raw channel. In the example, channel 0 is called "Green 2" and channel 1 is called "Red 2". The returned list is in no particular order.

### Set brightness
Command format: ```b <channel mask> <brightness>```

Command response: ```Comma-separated list of <channel>:brightness=<value>```

Example:
```
b 224 990\r\n
5:brightness=990,6:brightness=990,7:brightness=990\r\n
```
LEDs vary in brightness, even if they're supplied with the same current. This command allows you to change the relative brightness of channels so that all channels have equal perceived brightness.
The channel mask is an integer bitmask, where having a bit set means the command will affect that channel. In the example, the channel mask is 224, which has bits 5, 6 and 7 set. This means that the command will change the brightness for raw channels 5, 6 and 7 simultaneously.
The brightness is given as an integer, where 0 is equivalent to 0 relative brightness and 1024 is equivalent to 1.0 relative brightness. In the example, the brightness value is set to 990, equivalent to a relative brightness of about 0.9668 (990 divided by 1024). This means that when all LEDs are set to 100% in the Hue app, channels 5, 6 and 7 will actually only be at 96.68%.

The default setting for brightness is 1024 (relative brightness 1.0).

**This setting may be lost on reset. Use the 's' command to save it to non-volatile memory.**

### Set gamma
Command format: ```g <channel mask> <gamma>```

Command response: ```Comma-separated list of <channel>:gamma=<value>```

Example:
```
g 2052 2253\r\n
2:gamma=2253,11:gamma=2253\r\n
```
Human eyes have a non-linear perception of brightness. An LED at 50% PWM will not look half as bright as an LED at 100% PWM. See https://learn.adafruit.com/led-tricks-gamma-correction/the-issue for more information on this issue.
The channel mask in this command is a bitmask, just like with the "Set brightness" command - see the "Set brightness" documentation for more details. In the example, the channel mask is 2052, which will result in raw channels 2 and 11 having their gamma values changed simultaneously.
The gamma is given as an integer, where 0 is equivalent to 0 gamma and 1024 is equivalent to 1.0 gamma. In the example, the gamma is set to 2253, equivalent to a gamma of about 2.2 (2253 divided by 1024).

The default setting for gamma is 2867 (gamma = 2.8).

**This setting may be lost on reset. Use the 's' command to save it to non-volatile memory.**

### Set computed white mode
Command format: ```w <computed white mode>```

Command response: ```ComputedWhiteMode=<computed white mode>```

Example:
```
w 1\r\n
ComputedWhiteMode=1\r\n
```
Red+green+blue makes white, but in reality, combining RGB lights to make white results in ugly light because RGB lights have a poor Color Rendering Index (CRI). In computed white mode, a white channel is bonded to RGB channels. Possible values for computed white mode:
- 0: No computed white mode (the default).
- 1: Computed white mode that favors better color. In this mode, the white channel replaces white from the RGB channels. For properly-calibrated lights, this will result in accurate light colors.
- 2: Computed white mode that favors better brightness. In this mode, the white channel augments white from the RGB channels. This results in more overall brightness, but at the cost of desaturated colors.

When computed white mode is active, the white channels will become a part of their corresponding RGB bulb, and will not be independently controllable (as a Dimmable bulb) from the Hue app. When switching into and out of computed white mode, you should probably delete and re-add the lights in the Hue app.

**This setting requires a board reset to take effect. Use the 's' command to save it to non-volatile memory, then use the 'r' command to reset the board.**

### Save settings
Command format: ```s```

Command response: ```saving```

Example:
```
s\r\n
saving\r\n
```
This will save gamma, brightness and computed white settings to non-volatile memory, ensuring that they do not get wiped during a reset or power-outage.

### Reset
Command format: ```r```

Command response: ```[no response]```

Example:
```
r\r\n
```
This will soft-reset the JN5168 immediately. A reset is necessary for the computed white mode to take effect. Because the reset happens immediately, there is no response to this command. Unsaved changes will be lost on reset; see the "Save settings" command.

### Get all information
Command format: ```i```

Command response: ```Comma-separated list of <property>=<value>```

Example:
```
i\r\n
0:gamma=2700,0:brightness=1024,1:gamma=2700,1:brightness=1024,2:gamma=2253,2:brightness=1024,3:gamma=2867,3:brightness=1024,4:gamma=2867,4:brightness=1024,5:gamma=2867,5:brightness=990,6:gamma=2867,6:brightness=990,7:gamma=2867,7:brightness=990,8:gamma=2867,8:brightness=1024,9:gamma=2867,9:brightness=1024,10:gamma=2867,10:brightness=1024,11:gamma=2253,11:brightness=1024,ComputedWhiteMode=0\r\n
```
This will obtain the current value of all settings. This command is useful for obtaining the current state of the board. In the response, property is either "<channel>:gamma", "<channel>:brightness" or "ComputedWhiteMode", where <channel> is the raw channel number. Use the "Get raw channel names" command to get a list of channel names for each raw channel number. The representation of values for gamma are described in the documentation for the "Set gamma" command. Likewise, see the documentation for the "Set brightness" and "Set computed white mode" commands for the representation of values for brightness and computed white mode respectively.
