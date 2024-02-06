# Dodge-ESP32-Cam-Tripod
Dodge is an ESP32 Cam based novel tripod design.  Feel free to check out full build instructions on Instructables website - https://www.instructables.com/Dodge-ESP32-Cam-Based-Tripod/

## Setting Neutral Position for all six servos
You need to get the neutral positions for all servos set before uploading the code for Dodge to move correctly.  I have included an image with Dodge in its neutral position and the angles the servos should be at those positions.
Once done with setting the neutral angles the rest of the moves by Dodge will be calculated correctly.


<ins>Right Leg Top Servo (rAT)</ins>
Neutral Position - 60 degrees\
Leg Upwards - Increase degrees\
Leg Downwards - Decrease degrees

<ins> Left Leg Top Servo (lAT)</ins>
Neutral Position - 105 degrees\
Leg Upwards - Decrease degrees\
Leg Downwards - Increase degrees

<ins> Right Leg Bottom Servo (RAB)</ins>
Neutral Position - 90 degrees\
Leg Upwards - Increase degrees\
Leg Downwards - Decrease degrees

<ins> Left Leg Bottom Servo (lAB)</ins>
Neutral Position - 95 degrees\
Leg Upwards - Decrease degrees\
Leg Downwards - Increase degrees

<ins> Pinion Servo (mL)</ins>
Neutral Position - 107 degrees\
Leg Upwards - Decrease degrees\
Leg Downwards - Increase degrees

<ins>Support Foot</ins>
Neutral Position - 90 degrees\
Rotation angle between 20 degrees and 180 degrees


## Uploading Code
When uploading code to the ESP32 Cam module you need the ESP32 Cam-MB which has the boot loader to upload code. You need to press the IO 0 button on the ESP32 Cam-MB and then the RST button on the ESP32 module to put the board to accept code uploads. Apart from that you will also need to download ESP32Servo library by Kevin Harrington/John K. Bennett from the Library manager in Arduino IDE.

## Powering up
In the code file update to your desired password to connect to Dodge and once the code has been uploaded you can see it visible as Dodge-Tripod on the WIFI network. After you upload the code make sure to reset the ESP32 Cam module by pressing the RST button on the back. Connect to Dodge-Tripod and then open up a browser and in the url enter 192.168.4.1 on your phone/computer to connect to the Dodge Control App. Make sure to power up the Primary and Secondary Batteries and then your good to go. Power on the primary battery by connecting the barrel jack and turning the small slide switch on the bottom of Dodge's head, and then you can power on the secondary battery by connecting the battery to the buck converter module.


