# Arduino-mediacontroller

This code works as an "always-on" controller unit for a raspberry pi media unit. It interprets button presses and rotations from 4 buttons and a rotary encoder, and sends different signals based on push duration and status of the Raspberry pi.

Power is switched by either sending shutdown signals by pulling a GPIO pin low, or by cycling the relay.

Commands for the media player are sent as text over serial at 9600 baud, for example: "media:play", "media:mute", "power:togglescreen"

The raspberry pi needs additional code to interpret these signals: the power code is quite simple, the serial and media control is a bit more involved, but available on my other repos.

This is intended for an Arduino Nano or Pro Mini with some extra components:
-Separated DC-DC stepdown
-A relay module to switch unregulated power to the Rpi
-2 LED to signal Arduino and Rpi status.
-A passive buzzer of the correct voltage
-A logic-level converter with 4 channels
-3 push buttons and a rotary encoder and switch with hardware debounce, I used an IQAudio Cosmic Controller.
