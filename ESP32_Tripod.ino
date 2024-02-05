#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems
#include "esp_http_server.h"
#include <ESP32Servo.h>

// Replace with your preffered password
const char* ssid = "Dodge-Tripod";
const char* password = "hannu1234";

#define PART_BOUNDARY "123456789000000000000987654321"

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Servo definitions

/* 
  bS -> Support Feet Base Servo
  rAT -> Right Leg Top Servo
  lAT -> Left Leg Top Servo
  rAB -> Right Leg Bottom Servo
  lAB -> Left Leg Bottom Servo
  mL -> Pinion Servo

*/

Servo bS;
Servo rAT;
Servo lAT;
Servo rAB;
Servo lAB;
Servo mL;

int nRAT = 60;
int nRAB = 90;
int nLAT = 105;
int nLAB = 95;
int mLD = 165;
int mLU = 50;

int rotPos = 90;
bool flip = false;
bool rightFlip = false;
bool leftFlip = true;
bool moveUp = false;
bool moveDown = false;
bool showMoves = false;

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<html>
  <head>
    <title>ESP32-CAM Robot</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { font-family: Arial; text-align: center; margin:0px auto; padding-top: 30px;}
      table { margin-left: auto; margin-right: auto; }
      td { padding: 8 px; }
      .button {
        background-color: #2f4468;
        border: none;
        color: white;
        padding: 10px 20px;
        text-align: center;
        text-decoration: none;
        display: inline-block;
        font-size: 18px;
        margin: 6px 3px;
        cursor: pointer;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        -khtml-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
        user-select: none;
        -webkit-tap-highlight-color: rgba(0,0,0,0);
      }
      img {  width: auto ;
        max-width: 100% ;
        height: auto ; 
      }
    </style>
  </head>
  <body>
    <h1>Dodge-Tripod</h1>
    <img src="" id="photo" >
    <table>
      <tr><td colspan="3" align="center"><button class="button" onmousedown="toggleCheckbox('up');" ontouchstart="toggleCheckbox('up');">Up</button></td></tr>
      <tr><td align="center"><button class="button" onmousedown="toggleCheckbox('left');" ontouchstart="toggleCheckbox('left');">Left</button></td><td align="center"></td><td align="center"><button class="button" onmousedown="toggleCheckbox('right');" ontouchstart="toggleCheckbox('right');">Right</button></td></tr>
      <tr><td colspan="3" align="center"><button class="button" onmousedown="toggleCheckbox('down');" ontouchstart="toggleCheckbox('down');">Down</button></td></tr>
      <tr><td align="center"><button class="button" onmousedown="toggleCheckbox('l-turn');" ontouchstart="toggleCheckbox('l-turn');">L-Turn</button></td><td align="center"></td><td align="center"><button class="button" onmousedown="toggleCheckbox('r-turn');" ontouchstart="toggleCheckbox('r-turn');">R-Turn</button></td></tr>
      <tr><td align="center"><button class="button" onmousedown="toggleCheckbox('moves');" ontouchstart="toggleCheckbox('moves');">Moves</button></td><td align="center"></td><td align="center"><button class="button" onmousedown="toggleCheckbox('flip');" ontouchstart="toggleCheckbox('flip');">Flip</button></td></tr>                  
    </table>
   <script>
   function toggleCheckbox(x) {
     var xhr = new XMLHttpRequest();
     xhr.open("GET", "/action?go=" + x, true);
     xhr.send();
   }
   window.onload = document.getElementById("photo").src = window.location.href.slice(0, -1) + ":81/stream";
  </script>
  </body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
  }
  return res;
}

static esp_err_t cmd_handler(httpd_req_t *req){
  char*  buf;
  size_t buf_len;
  char variable[32] = {0,};
  
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char*)malloc(buf_len);
    if(!buf){
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "go", variable, sizeof(variable)) == ESP_OK) {
      } else {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  sensor_t * s = esp_camera_sensor_get();

  int res = 0;
  
  if(!strcmp(variable, "up")) {
    showMoves = false;
    if (moveUp == false){
      move_up();
    }
    
  }
  else if(!strcmp(variable, "left")) {
    showMoves = false;
    reset_vertical_move();
    walk_left();
  }
  else if(!strcmp(variable, "right")) {
    showMoves = false;
    reset_vertical_move();
    walk_right();
  }
  else if(!strcmp(variable, "l-turn")) {
    showMoves = false;
    reset_vertical_move();
    turn_left();
    flip = false;
  }
  else if(!strcmp(variable, "r-turn")) {
    showMoves = false;
    reset_vertical_move();
    turn_right();
    flip = false;
  }
  else if(!strcmp(variable, "down")) {
    showMoves = false;
    if (moveDown == false){
      move_down();
    }
    
  }
  else if(!strcmp(variable, "flip")) {
    showMoves = false;
    reset_vertical_move();
    if (flip == false){
      if (rightFlip){
        flip_right();
      }
      else {
      flip_left();
      }
    }
    
  }
  else if(!strcmp(variable, "moves")) {
    moveUp = false;
    moveDown = false;
    if (showMoves == false){
      show_moves();
    }
  }
  else {
    res = -1;
  }

  if(res){
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri       = "/action",
    .method    = HTTP_GET,
    .handler   = cmd_handler,
    .user_ctx  = NULL
  };
  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
  }
  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  bS.setPeriodHertz(50);    
  rAT.setPeriodHertz(50);
  lAT.setPeriodHertz(50);
  rAB.setPeriodHertz(50);
  lAB.setPeriodHertz(50);
  mL.setPeriodHertz(50);

	bS.attach(4, 500, 2500);
  rAT.attach(12, 500, 2500);
  lAT.attach(13, 500, 2500);
  rAB.attach(14, 500, 2500);
  lAB.attach(15, 500, 2500);
  mL.attach(2, 500, 2500);
  delay(2000);

  rAT.write(nRAT+36);
  rAB.write(nRAB-16);
  lAT.write(nLAT-36);
  lAB.write(nLAB+16);
  mL.write(165);
  
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
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  WiFi.softAP(ssid, password);
  
  startCameraServer();
}

void loop() {

}

void reset_vertical_move(){
  moveUp = false;
  moveDown = false;
}

void flip_left(){
  mL.write(165);
  delay(200);
  rAB.write(nRAB-89);
  lAB.write(nLAB+89);
  rAT.write(nRAT+40);
  lAT.write(nLAT-40);
  delay(500);
  for (int i = rotPos; i>20; i--){
    bS.write(i);
    delay(15);
  }
  delay(1000);
  rAT.write(nRAT+36);
  lAT.write(nLAT-36);
  for (int i=1; i<74; i++){
    rAB.write(i);
    delay(5);
  }
  for (int i=184; i>111; i--){
    lAB.write(i);
    delay(5);
  }

  delay(1000);
  for (int i=165; i>49; i--){
    mL.write(i);
    delay(5);
  }
  delay(200);
  bS.write(180);
  delay(500);
  mL.write(165);
  rotPos = 180;
  leftFlip = true;
  rightFlip = false;
  flip = true;
}

void flip_right(){
  mL.write(165);
  delay(200);
  rAB.write(nRAB-89);
  lAB.write(nLAB+89);
  rAT.write(nRAT+40);
  lAT.write(nLAT-40);
  delay(500);
  for (int i=rotPos; i<181; i++){
    bS.write(i);
    delay(15);
  }
  delay(1000);
  rAT.write(nRAT+36);
  lAT.write(nLAT-36);
  for (int i=1; i<74; i++){
    rAB.write(i);
    delay(5);
  }
  for (int i=184; i>111; i--){
    lAB.write(i);
    delay(5);
  }
  delay(1000);
  for (int i=165; i>49; i--){
    mL.write(i);
    delay(5);
  }
  delay(1000);
  bS.write(20);
  delay(500);
  mL.write(165);
  rotPos = 20;
  leftFlip = false;
  rightFlip = true;
  flip = true;
}

void turn_left(){
  mL.write(165);
  delay(200);
  rAB.write(nRAB-89);
  lAB.write(nLAB+89);
  rAT.write(nRAT+40);
  lAT.write(nLAT-40);
  delay(500);
  if (rotPos > 20){
    for (int i=rotPos; i>rotPos-20; i--){
      bS.write(i);
      delay(15);
    }
    rotPos = rotPos - 20;
  }
}

void turn_right(){
  mL.write(165);
  delay(200);
  rAB.write(nRAB-89);
  lAB.write(nLAB+89);
  rAT.write(nRAT+40);
  lAT.write(nLAT-40);
  delay(500);
  if (rotPos < 180){
    for (int i=rotPos; i<rotPos+20; i++){
      bS.write(i);
      delay(15);
    }
    rotPos = rotPos + 20;
  }
}

void walk_left(){
  rAT.write(nRAT+36);
  rAB.write(nRAB-16);
  lAT.write(nLAT-36);
  lAB.write(nLAB+16);
  mL.write(155);
  delay(1000);
  for (int i=0; i<1; i++){
    rAT.write(nRAT+43);
    rAB.write(nRAB-54);

    delay(500);
    for (int i=155; i>60; i--){
      mL.write(i);
      delay(10);
    }

    delay(200);

    rAB.write(nRAB-16);
    lAB.write(nLAB+54);

    delay(500);
    for (int i=60; i<155; i++){
      mL.write(i);
      delay(10);
    }

    delay(500);
    lAT.write(nLAT-61);
    lAB.write(nLAB+43);
    delay(200);
    lAT.write(nLAT-36);
    lAB.write(nLAB+16);
    delay(200);
    rAT.write(nRAT+61);
    rAB.write(nRAB-43);
    delay(200);
    rAT.write(nRAT+36);
    rAB.write(nRAB-16);

  }
  
}

void walk_right(){
    rAT.write(nRAT+36);
    rAB.write(nRAB-16);
    lAT.write(nLAT-36);
    lAB.write(nLAB+16);
    mL.write(155);
    delay(1000);
  for (int i=0; i<1; i++){
    lAT.write(nLAT-43);
    lAB.write(nLAB+54);

    delay(500);
    for (int i=155; i>60; i--){
      mL.write(i);
      delay(10);
    }

    delay(200);

    lAB.write(nLAB+16);
    rAB.write(nRAB-54);

    delay(500);

    for (int i=60; i<155; i++){
      mL.write(i);
      delay(10);
    }

    delay(500);
    rAT.write(nRAT+61);
    rAB.write(nRAB-43);
    delay(200);
    rAT.write(nRAT+36);
    rAB.write(nRAB-16);
    delay(200);
    lAT.write(nLAT-61);
    lAB.write(nLAB+43);
    delay(200);
    lAT.write(nLAT-36);
    lAB.write(nLAB+16);
  }
}

void move_down(){
  rAT.write(nRAT+80);
  rAB.write(nRAB-45);
  lAT.write(nLAT-80);
  lAB.write(nLAB+45);
  delay(1000);
  for (int i=165; i>49; i--){
    mL.write(i);
    delay(10);
  }
  moveDown = true;
  moveUp = false;
}

void move_up(){
  rAT.write(nRAT+80);
  rAB.write(nRAB-45);
  lAT.write(nLAT-80);
  lAB.write(nLAB+45);
  delay(1000);
  for (int i=50; i<165; i++){
    mL.write(i);
    delay(10);
  }
  moveUp = true;
  moveDown = false;
}

void show_moves(){
  rAT.write(nRAT+36);
  rAB.write(nRAB-16);
  lAT.write(nLAT-36);
  lAB.write(nLAB+16);
  mL.write(165);
  delay(1000);
  for (int i=165; i<61; i--){
    mL.write(i);
    delay(10);
  }
  delay(500);
  bS.write(180);
  delay(1000);
  for (int i=60; i>165; i++){
    mL.write(i);
    delay(10);
  }
  delay(1000);
  rAT.write(nRAT+45);
  rAB.write(nRAB+85);
  lAT.write(nLAT-45);
  lAB.write(nLAB-85);
  delay(1000);
  for (int j=0; j<3; j++){
    for (int i=180; i>151; i--){
      bS.write(i);
      delay(10);
    }
    delay(200);
    for (int i=150; i<181; i++){
      bS.write(i);
      delay(10);
    }
    delay(200);
  }
  delay(1000);
  rAT.write(nRAT+75);
  rAB.write(nRAB-10);
  lAT.write(nLAT-75);
  lAB.write(nLAB+10);
  delay(1000);
  for (int j=0; j<3; j++){
    for (int i=105; i<121; i++){
      lAB.write(i);
      delay(15);
    }
    for (int i=30; i>10; i--){
      lAT.write(i);
      delay(15);
    }
    for (int i=120; i>105; i--){
      lAB.write(i);
      delay(15);
    }
    for (int i=9; i<31; i++){
      lAT.write(i);
      delay(15);
    }
    for (int i=135; i<155; i++){
      rAT.write(i);
      delay(15);
    }
    for (int i=80; i>65; i--){
      rAB.write(i);
      delay(15);
    }
    for (int i=155; i>136; i--){
      rAT.write(i);
      delay(15);
    }
    for (int i=64; i<81; i++){
      rAB.write(i);
      delay(15);
    }
    for (int i=30; i>10; i--){
      lAT.write(i);
      delay(15);
    }
    for (int i=120; i>105; i--){
      lAB.write(i);
      delay(15);
    }
    for (int i=9; i<31; i++){
      lAT.write(i);
      delay(15);
    }
    for (int i=105; i<121; i++){
      lAB.write(i);
      delay(15);
    }
    delay(300);
    rAT.write(nRAT+75);
    rAB.write(nRAB-10);
    lAT.write(nLAT-75);
    lAB.write(nLAB+10);
  }


  delay(1000);

  rAT.write(nRAT+75);
  rAB.write(nRAB+80);
  lAT.write(nLAT-75);
  lAB.write(nLAB+80);
  delay(500);
  for (int j=0; j<2; j++){
    for (int i=180; i>91; i--){
      bS.write(i);
      delay(15);
    }
    for (int i=90; i<181; i++){
      bS.write(i);
      delay(15);
    }
  }
  rAT.write(nRAT+75);
  rAB.write(nRAB-10);
  lAT.write(nLAT-75);
  lAB.write(nLAB+10);

  delay(1000);

  rAT.write(nRAT+36);
  rAB.write(nRAB-16);
  lAT.write(nLAT-36);
  lAB.write(nLAB+16);
  mL.write(165);
  delay(1000);
  for (int i=165; i>61; i--){
    mL.write(i);
    delay(10);
  }
  delay(500);
  bS.write(20);
  delay(1000);
  for (int i=60; i<165; i++){
    mL.write(i);
    delay(10);
  }
  delay(1000);
  rAT.write(nRAT+75);
  rAB.write(nRAB-80);
  lAT.write(nLAT-75);
  lAB.write(nLAB-80);
  delay(500);
  for (int j=0; j<2; j++){
    for (int i=20; i<111; i++){
      bS.write(i);
      delay(15);
    }
    for (int i=110; i>21; i--){
      bS.write(i);
      delay(15);
    }
  }
  delay(1000);
  for (int i=15; i<111; i++){
    lAB.write(i);
    delay(10);
  }
  for (int i=10; i<74; i++){
    rAB.write(i);
    delay(10);
  }
  for (int i=30; i<69; i++){
    lAT.write(i);
    delay(10);
  }
  for (int i=135; i>96; i--){
    rAT.write(i);
    delay(10);
  }
  rAT.write(nRAT+36);
  rAB.write(nRAB-16);
  lAT.write(nLAT-36);
  lAB.write(nLAB+16);
  delay(1000);
  for (int i=165; i>61; i--){
    mL.write(i);
    delay(10);
  }
  delay(500);
  bS.write(180);
  delay(1000);
  for (int i=60; i<165; i++){
    mL.write(i);
    delay(10);
  }
  showMoves = true;
}