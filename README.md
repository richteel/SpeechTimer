# SpeechTimer
Clock/Timer for Speeches

# Folder Structure #

There are several folders in this project. Below is a brief description of each one.

- **CircuitPython:** An early prototype with a web interface. It works but is unstable due to memory fragmentation. It is no longer being developed. It is kept here if anyone using Python wants to use it or improve upon it.
- **Documentation:** Some documentation used during development along with user guide and other miscellaneous materials.
	- **Speech Timer - Developer Guide.docx:** Not much here at the moment. *(Just a placeholder)*
	- **Speech Timer - User Guide.docx:** Very basic user guide showing how to use the remote.
- **PCB:** Eagle project with schematic and PCB design. This is for the development version, which incorporates two Raspberry Pi Pico boards. A regular Raspberry Pi Pico as a Picoprobe and a Raspberry Pi Pico W as the micro-controller running the show.
- **SD Card:** Contents for the micro SD card.
- **SpeechTimer:** The Arduino Sketch C++ project written using the Arduino IDE. Currently everything seems to be working, but the Web Interface has not been created yet.


# Parts List #

- Raspberry Pi Pico
	- Quantity: 1
	- Note: Used as Picoprobe for programming and debugging
- Raspberry Pi Pico W
	- Quantity: 1
- 5mm Neopixels (Through hole)
	- Quantity: 58
	- Source(s): [Adafruit](https://www.adafruit.com/product/1938) / [AliExpress](https://www.aliexpress.us/item/3256804752897372.html?spm=a2g0o.order_list.order_list_main.18.3df818029J6f3W&gatewayAdapt=glo2usa)
- SD Card Breakout
	- Quantity: 1
	- Source(s): [Adafruit](https://www.adafruit.com/product/4682)
- Monochrome 0.96" 128x54 OLED Display
	- Quantity: 1
	- Source(s): [Adafruit](https://www.adafruit.com/product/326) / [Amazon](https://www.amazon.com/gp/product/B08VNRH5HR/ref=ppx_yo_dt_b_search_asin_image?ie=UTF8&th=1)
- Remote
	- Quantity: 1
	- Source(s): [Amazon](https://www.amazon.com/gp/product/B0C7BPJYH1/ref=ppx_yo_dt_b_search_asin_image?ie=UTF8&psc=1)
- 2.54mm (0.1") JST Connectors (3, 4, & 6 position)
	- Quantity: 1 each 3-position, 4-position, and 6-position
	- Source(s): [Amazon](https://www.amazon.com/gp/product/B015Y6JOUG/ref=ppx_yo_dt_b_search_asin_image?ie=UTF8&psc=1) / [AliExpress](https://www.aliexpress.us/item/2251832778234720.html?spm=a2g0o.order_detail.order_detail_item.2.2908f19cEtDDFD&gatewayAdapt=glo2usa)
- 2.54mm (0.1") Female Headers
	- Quantity: 5 (4 20-position, 1 9-position)
	- Source(s): [Adafruit](https://www.adafruit.com/product/598)
- 2.54mm (0.1") Male Headers
	- Quantity: 5 (4 20-position, 1 9-position)
	- Source(s): [Adafruit](https://www.adafruit.com/product/2671)
- Momentary Push Button Switch (N.O.) 6mm slim
	- Quantity: 2
	- Source(s): [Adafruit](https://www.adafruit.com/product/1489)
- IR Receiver TSOP38238
	- Quantity: 1
	- Source(s): [Adafruit](https://www.adafruit.com/product/157) / [AliExpress](https://www.aliexpress.us/item/2251832658861424.html?spm=a2g0o.order_list.order_list_main.12.3df818029J6f3W&gatewayAdapt=glo2usa)
- IR LED 5mm
	- Quantity: 1
	- Source(s): [Adafruit](https://www.adafruit.com/product/388)
- Red LED 3mm
	- Quantity: 1
	- Source(s): [Adafruit](https://www.adafruit.com/product/777)
- CdS Photoresistor
	- Quantity: 1
	- Source(s): [Adafruit](https://www.adafruit.com/product/161)