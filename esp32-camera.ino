/**
 * 
 * 
 * https://github.com/espressif/esp32-camera/blob/master/driver/include/esp_camera.h
 * https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/
 * https://learn.circuit.rocks/esp32-cam-with-rtsp-video-streaming
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include "esp_camera.h"
#include "wificredentials.h"

#define CAMERA_MODEL_M5STACK_WITHOUT_PSRAM
#include "camera_defs.h"

#define DEBUG

const char *HostName PROGMEM = "BugEye";

const char *HomePage PROGMEM =
    "<!DOCTYPE html>\r\n"
    "<html>\r\n"
    "    <head>\r\n"
    "        <title>Streaming Camera</title>\r\n"
    "    </head>\r\n"
    "    <body>\r\n"
    "        <h1>Streaming Camera</h1>\r\n"
    "        <h2>Options</h2>\r\n"
    "        <ul>\r\n"
    "            <li><a href=\"/video\">Video Stream</a></li>\r\n"
    "            <li><a href=\"/snap\">Take a picture</a></li>\r\n"
    "        </ul>\r\n"
    "    </body>\r\n"
    "</html>";
    
const char *StreamHeader PROGMEM = "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
const char *StreamFrameHeader PROGMEM = "--frame\r\nContent-Type: image/jpeg\r\n\r\n";
const char *SnapshotHeader PROGMEM = 
    "HTTP/1.1 200 OK\r\n"
    "Content-disposition: inline; filename=capture.jpg\r\n"
    "Content-type: image/jpeg\r\n\r\n";
    
WebServer server(80);

// -----------------------------------------------------------------------
// Web server configuration
// -----------------------------------------------------------------------

/** .......................................................................
 *  startWebServer() initializes a concurrent task for our web server.
 *  The task mechanism used here is the one provided as part of the
 *  FreeRTOS libraries embedded in the ESP32 platform.
 *  https://www.freertos.org/a00019.html
 *  There is no need to #include anything additoinal to use tasks.
 *  .......................................................................
 */
void startWebServer()
{
    // Fire up the task that handles setting up the web server
    TaskHandle_t webTaskHandle;
    xTaskCreate(webTask, "WebServer", 4096, NULL, 1, &webTaskHandle);

    if (webTaskHandle == NULL)
    {
        #ifdef DEBUG
        Serial.println("ERROR: Could not start web server!");
        #endif
    }
    else
    {
        #ifdef DEBUG
        Serial.println("Web server started");
        #endif
    }
}

/** .......................................................................
 *  webTask() is the function that is run as a background task to set up
 *  and run our web server.
 *  It sets up the routes for the home page, video streaming page, and
 *  snapshot (single photo) page.
 *  It then starts the web server and starts listening for incoming
 *  requests.
 *  .......................................................................
 */
void webTask(void *parameters)
{
    //--- Set up server routes

    // Home route - display usage info
    server.on("/", HTTP_GET, homeRoute);
    
    // Video stream route
    server.on("/video", HTTP_GET, streamRoute);

    // Single picture route
    server.on("/snap", HTTP_GET, pictureRoute);

    server.begin();
    
    while (true)
    {
        // Check if there are any incoming requests to be handled
        server.handleClient();
//        delay(50);
    }
}

/** .......................................................................
 *  homeRoute() renders the home page of the web site, which contains links
 *  to the video and snapshot pages.
 *  .......................................................................
 */
void homeRoute()
{
    WiFiClient webClient = server.client();
    server.sendContent(HomePage);
}

/** .......................................................................
 *  streamRoute() starts returning a video stream to the client. The stream
 *  will continue until the client disconnects or navigates to a different
 *  route.
 *  .......................................................................
 */
void streamRoute()
{
    #ifdef DEBUG
    Serial.println("Starting video stream");
    #endif
    
    camera_fb_t *imgBuffer = NULL;
    WiFiClient webClient = server.client();

    // To send a video stream, first we'll send the HTTP response header that
    // tells the browser that what follows is a stream of images separated by
    // a boundry marker. Each image we send will be a single frame of the video.
    server.sendContent(StreamHeader);

    // Now we just continuously send images taken by the camera to the client.
    // Taken all together, they act like a video stream.
    while (true)
    {
        if (!webClient.connected())
        {
            #ifdef DEBUG
            Serial.println("Client disconnected");
            #endif
            break;
        }

        // This is the esp_camera function that snaps a picture with the camera
        // and returns a frame buffer - the image as an array of bytes.
        imgBuffer = esp_camera_fb_get();

        if (!imgBuffer)
        {
            #ifdef DEBUG
            Serial.println("ERROR: Image capture failed!");
            #endif
            delay(100);
            continue;
        }

        // Send the frame boundary marker, then the image, followed by a blank line
        // so that the browser interprets it as a frame of the video.
        server.sendContent(StreamFrameHeader);
        webClient.write((char *)imgBuffer->buf, imgBuffer->len);
        server.sendContent("\r\n");

        // We've sent the image. We must now free up the memory used by the buffer.
        // The esp_camera library takes care of that for us with this function call.
        esp_camera_fb_return(imgBuffer);
        imgBuffer = NULL;
        // delay(15);
    }

    #ifdef DEBUG
    Serial.println("Done streaming");
    #endif
}

/** .......................................................................
 *  pictureRoute() handles requests to take a snapshot from the camera and
 *  return the single image.
 *  .......................................................................
 */
void pictureRoute()
{
    #ifdef DEBUG
    Serial.println("Sending single picture");
    #endif
    
    WiFiClient webClient = server.client();

    if (!webClient.connected())
    {
        return;
    }

    camera_fb_t *imgBuffer = esp_camera_fb_get();
    
    if (!imgBuffer)
    {
        #ifdef DEBUG
        Serial.println("ERROR: Image capture failed!");
        #endif
        return;
    }
    
    server.sendContent(SnapshotHeader);
    webClient.write((char *)imgBuffer->buf, imgBuffer->len);
    
    esp_camera_fb_return(imgBuffer);
    imgBuffer = NULL;
}

/** .......................................................................
 *  In setup(), we're going to configure the camera using the pinout for
 *  the specific model we're using (depends on your camera module).
 *  Then we'll fire up the web server, sit back, and wait for incoming
 *  requests for a video stream or a picture.
 *  .......................................................................
 */
void setup() 
{
    #ifdef DEBUG
    Serial.begin(115200);
    Serial.setDebugOutput(false);
    #endif
    
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG; 

    // If the ESP32 board we're using has additional PSRAM, the cammera can
    // grab a larger image.
    if (psramFound())
    {
        // We can grab a 1600x1200 image.
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 6;
        config.fb_count = 2;
    } 
    else 
    {
        // No PSRAM? The largest image we can handle is 800x600.
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 6;
        config.fb_count = 1;
    }
  
    // Camera init
    esp_err_t err = esp_camera_init(&config);
    
    if (err != ESP_OK) 
    {
        #ifdef DEBUG
        Serial.printf("Camera init failed with error 0x%x", err);
        #endif
        return;
    }

    // Camera settings to try to get a better image
    // The OV2640 camera seems to capture images upside down, so we need to
    // flip the image. 
    // See https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/ for
    // a great writeup on the available settings.
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
    s->set_brightness(s, 1);    // up the brightness just a bit
    s->set_saturation(s, -2);   // lower the saturation
    s->set_dcw(s, 0);

    WiFi.setHostname(HostName);
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        
        #ifdef DEBUG
        Serial.print(".");
        #endif
    }

    #ifdef DEBUG
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("Camera Stream Ready! Go to: http://");
    Serial.println(WiFi.localIP());
    #endif
    
    // Start the web server
    startWebServer();
}

/** .......................................................................
 *  We don't really need to do anything in loop(), since all our functionality
 *  is already running as a background task.
 *  .......................................................................
 */
void loop() 
{
    delay(1);
}
