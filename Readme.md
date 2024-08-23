# FocusriteGlobalMute
> ⚠️ **This project is currently in a discontinued state due to time reasons and therefore archived for the moment** ⚠️

Adds a global mute function to a predefined macro button and visualizes the mute state on a focusrite audio interface.

## Features
- Mute the audio input device globally for the Windows system by using a predefined macro key (defaults to **F15**)
- Visualize the mute state by changing the audio input level halos to a static color on mute (defaults to **red**)
- Play a sound on mute/unmute (defaults to TeamSpeak audio snippets)

## Working devices
Currently only tested on the **Focusrite Scarlett 2i2 3rd Gen**.

If you want to add support for your own device, change the necessary item ids in [the definition file](Defs.h). 
You can retrieve the ids by intercepting and listening to your loopback traffic using [Wireshark](https://wireshark.org). 
