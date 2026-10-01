#include "DotFrameMode.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

namespace DotFrameMode {

static Adafruit_ST7735 *tft = nullptr;
static ESP8266WebServer http(80);
static WebSocketsServer sockets(81);
static bool active = false;
static int8_t owner = -1;
static uint32_t lastFrameAt = 0;
static uint16_t stripe[160 * 8];

constexpr char AP_NAME[] = "Varuna-DotFrame";
constexpr char AP_PASSWORD[] = "varuna01";
constexpr size_t FRAME_BYTES = 7 + 160 * 8 * 2;

// Browser-side player. Frames are scaled to 160x128 and sent as eight-row
// RGB565 strips. Every strip waits for an acknowledgement before the next one.
static const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Varuna DotFrame</title><style>
*{box-sizing:border-box}body{margin:0;background:#071018;color:#eef8ff;font:15px system-ui}main{max-width:580px;margin:auto;padding:20px}
.brand{color:#52dcff;letter-spacing:.18em}.card{background:#101f2c;border:1px solid #244052;border-radius:18px;padding:18px;margin-top:16px}
h1{margin:5px 0 2px}video,canvas,input,button,select{width:100%;margin-top:12px}canvas{image-rendering:pixelated;background:#000;border-radius:9px}
button,select,input{padding:12px;border-radius:10px;border:1px solid #36566c;background:#142b3b;color:#fff}button{background:#167fa3;font-weight:700}
.muted{color:#8ea8b9;font-size:13px}#state{color:#62e6bb}</style></head><body><main>
<div class="brand">DOTBYTE × PVN</div><h1>DotFrame</h1><div class="muted">Project Varuna wireless display</div>
<div class="card"><div id="state">Connecting…</div><input id="file" type="file" accept="video/*"><video id="video" controls playsinline></video>
<select id="fit"><option value="cover">Fill display</option><option value="contain">Fit entire video</option></select>
<button id="play" disabled>Stream to Varuna</button><button id="stop">Stop</button><canvas id="canvas" width="160" height="128"></canvas>
<div class="muted" id="stats">160×128 RGB565 • acknowledged strip streaming</div></div></main><script>
const q=x=>document.getElementById(x),v=q('video'),c=q('canvas'),g=c.getContext('2d',{alpha:false,willReadFrequently:true});
let ws,run=false,img,y=0,seq=0,t0=0,frames=0,url;
function connect(){ws=new WebSocket('ws://'+location.hostname+':81/');ws.binaryType='arraybuffer';
ws.onopen=()=>{q('state').textContent='Connected';q('play').disabled=!v.src};
ws.onclose=()=>{run=false;q('state').textContent='Disconnected — reconnecting';setTimeout(connect,800)};
ws.onmessage=e=>{let m;try{m=JSON.parse(e.data)}catch(_){return}if(m.type==='ack'&&run)sendStrip()}}
function paint(){g.fillStyle='#000';g.fillRect(0,0,160,128);let s=q('fit').value==='cover'?Math.max(160/v.videoWidth,128/v.videoHeight):Math.min(160/v.videoWidth,128/v.videoHeight);
g.drawImage(v,(160-v.videoWidth*s)/2,(128-v.videoHeight*s)/2,v.videoWidth*s,v.videoHeight*s);img=g.getImageData(0,0,160,128).data}
function frame(){if(!run)return;if(v.ended){stop();return}if(v.paused||v.readyState<2){setTimeout(frame,20);return}paint();y=0;sendStrip()}
function sendStrip(){if(!run||ws.readyState!==1)return;if(y>=128){frames++;q('stats').textContent=(frames*1000/(performance.now()-t0)).toFixed(1)+' FPS';requestAnimationFrame(frame);return}
let p=new Uint8Array(2567),d=new DataView(p.buffer);p[0]=0xA7;p[1]=1;d.setUint32(2,++seq,false);p[6]=y;
for(let n=0,i=y*640;n<1280;n++,i+=4){let rgb=((img[i]&248)<<8)|((img[i+1]&252)<<3)|(img[i+2]>>3);p[7+n*2]=rgb>>8;p[8+n*2]=rgb}y+=8;ws.send(p)}
function stop(){run=false;v.pause();q('play').textContent='Stream to Varuna';if(ws&&ws.readyState===1)ws.send('HOME')}
q('file').onchange=()=>{let f=q('file').files[0];if(!f)return;if(url)URL.revokeObjectURL(url);url=URL.createObjectURL(f);v.src=url;v.load();q('play').disabled=!ws||ws.readyState!==1};
q('play').onclick=async()=>{if(run){stop();return}run=true;frames=0;t0=performance.now();q('play').textContent='Pause stream';await v.play();frame()};q('stop').onclick=stop;connect();
</script></body></html>)HTML";

static void showHome() {
  if (!tft) return;
  uint16_t bg = tft->color565(5, 13, 21);
  uint16_t panel = tft->color565(13, 31, 44);
  uint16_t cyan = tft->color565(65, 218, 250);
  uint16_t muted = tft->color565(125, 151, 170);
  tft->setRotation(1);
  tft->fillScreen(bg);
  tft->fillRoundRect(8, 8, 30, 30, 7, cyan);
  tft->fillTriangle(19, 15, 19, 31, 30, 23, ST77XX_WHITE);
  tft->setTextSize(2);
  tft->setTextColor(ST77XX_WHITE, bg);
  tft->setCursor(45, 9);
  tft->print(F("DotFrame"));
  tft->setTextSize(1);
  tft->setTextColor(muted, bg);
  tft->setCursor(46, 28);
  tft->print(F("VARUNA EDITION"));
  tft->fillRoundRect(8, 48, 144, 61, 7, panel);
  tft->setTextColor(cyan, panel);
  tft->setCursor(16, 56);
  tft->print(F("CONNECT WI-FI"));
  tft->setTextColor(ST77XX_WHITE, panel);
  tft->setCursor(16, 70);
  tft->print(F("Varuna-DotFrame"));
  tft->setTextColor(muted, panel);
  tft->setCursor(16, 84);
  tft->print(F("PASS: varuna01"));
  tft->setCursor(16, 98);
  tft->print(F("OPEN: 192.168.4.1"));
  tft->setCursor(8, 117);
  tft->print(F("Hold SELECT 5s to exit"));
}

static void socketEvent(uint8_t client, WStype_t type, uint8_t *payload,
                        size_t length) {
  if (type == WStype_DISCONNECTED) {
    if (owner == (int8_t)client) owner = -1;
    return;
  }
  if (type == WStype_TEXT && length == 4 &&
      memcmp(payload, "HOME", 4) == 0) {
    if (owner == -1 || owner == (int8_t)client) {
      owner = client;
      showHome();
      sockets.sendTXT(client, "{\"type\":\"ready\"}");
    }
    return;
  }
  if (type != WStype_BIN) return;
  if (owner != -1 && owner != (int8_t)client) {
    sockets.sendTXT(client, "{\"type\":\"error\",\"message\":\"Display busy\"}");
    return;
  }
  if (length != FRAME_BYTES || payload[0] != 0xA7 || payload[1] != 1 ||
      payload[6] > 120 || (payload[6] % 8) != 0) {
    sockets.sendTXT(client, "{\"type\":\"error\",\"message\":\"Invalid strip\"}");
    return;
  }

  owner = client;
  uint32_t id = ((uint32_t)payload[2] << 24) |
                ((uint32_t)payload[3] << 16) |
                ((uint32_t)payload[4] << 8) | payload[5];
  for (size_t i = 0; i < 1280; ++i) {
    stripe[i] = ((uint16_t)payload[7 + i * 2] << 8) | payload[8 + i * 2];
  }
  tft->startWrite();
  tft->setAddrWindow(0, payload[6], 160, 8);
  tft->writePixels(stripe, 1280);
  tft->endWrite();
  lastFrameAt = millis();

  char reply[54];
  snprintf(reply, sizeof(reply), "{\"type\":\"ack\",\"id\":%lu}",
           (unsigned long)id);
  sockets.sendTXT(client, reply);
}

bool begin(Adafruit_ST7735 &display) {
  if (active) return true;
  tft = &display;
  WiFi.forceSleepWake();
  delay(1);
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_NAME, AP_PASSWORD)) return false;

  http.on("/", HTTP_GET, []() {
    http.sendHeader("Cache-Control", "no-store");
    http.send_P(200, "text/html; charset=utf-8", PAGE);
  });
  http.on("/ping", HTTP_GET, []() {
    http.send(200, "text/plain", "Varuna DotFrame ready");
  });
  http.begin();
  sockets.begin();
  sockets.onEvent(socketEvent);
  active = true;
  showHome();
  return true;
}

void loop() {
  if (!active) return;
  sockets.loop();
  http.handleClient();
  yield();
}

bool isActive() { return active; }

} // namespace DotFrameMode
