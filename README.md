# Arduino-mediacontroller

This code works as an "always-on" controller unit for a raspberry pi media unit. It interprets button presses and rotations from 4 buttons and a rotary encoder, and sends different signals based on push duration and status of the Raspberry pi.

Power is switched by either sending shutdown signals by pulling a GPIO pin low, or by cycling the relay.

Commands for the media player are sent as text over serial at 9600 baud, for example: "media:play", "media:mute", "power:togglescreen"

The raspberry pi needs additional code to interpret these signals: the power code is quite simple, the serial and media control is a bit more involved, but available on my other repos.

More info:

Photos: https://photos.google.com/share/AF1QipNOalSMYOtYQD1bHhmhJNEraALUsxx1hYXyfX8KLlF5mXhvgUfo9PTx3TV2ps-Vuw?key=SXJBNlMyUThsRFEtbjJHa1V4aWxrYUlvcS15NFlR

Partlist: https://docs.google.com/spreadsheets/d/13uf0IYDIL0jGhBBMnfD-xuNvdJcvdOVAqd9ZSzGYDHY/edit?usp=sharing

3D Model Files: https://www.thingiverse.com/thing:3299786
