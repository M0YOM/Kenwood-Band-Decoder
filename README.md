# ESP32 Kenwood Band Decoder

ESP32 based Band Decoder for Kenwood Radios

This was created primarily to allow a Kenwood TS-990 to interface with the 5B4AGN Band Pass filters using the BCD band data. It uses an ESP32-WROOM-32 (although other ESP32 Variants should work too with some tweaks) to communicate with the TS-990 using it's serial port and output the band data in Yaesu BCD format.

The firmware is designed to be somewhat fault tolerant with automatic recovery from disconnections and power down of the radio. The power on sequence is not important.

The firmware also tracks the active TX VFO and decodes the transmitting band regardless of which VFO is being used for transmit.

More information on this project will be available at https://www.m0yom.co.uk shortly.
