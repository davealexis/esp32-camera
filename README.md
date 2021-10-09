# ESP32 Camera

This sketch came about from trying to understand how the ESP32-Cam and other ESP32-based
cameras work. Most of the examples out there show the code to get the camera running, but
do not really explain what the core camera code is doing, and why things are done the way 
they are.

I tried to boil down what I learned into an example that's (hopefully) understandable
and that can be extended to fit your own needs.

Some of the main resources I used were:
- https://github.com/espressif/esp32-camera/blob/master/driver/include/esp_camera.h
- https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/
- https://learn.circuit.rocks/esp32-cam-with-rtsp-video-streaming