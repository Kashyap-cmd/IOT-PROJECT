#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#define ONE_WIRE_BUS 4
#define pulsePin 34

const char* ssid = "Rehan";
const char* password = "12345678";

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

LiquidCrystal_I2C lcd(0x27,16,2);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

int BPM = 0;
bool beat = false;
int threshold = 2000;

unsigned long lastBeat = 0;
unsigned long lastUpdate = 0;

/* Moving average filter */
int signalBuffer[10];
int bufferIndex = 0;

String webpage = R"====(
<!DOCTYPE html>
<html>
<head>
<title>Health Monitor</title>
<style>
body {font-family: Arial; text-align:center; margin-top:50px;}
.card {font-size:30px; margin:20px;}
</style>
</head>

<body>

<h1>Health Monitor</h1>

<div class="card">Temperature: <span id="temp">--</span> F</div>
<div class="card">Heart Rate: <span id="bpm">--</span> BPM</div>

<script>
var ws = new WebSocket("ws://" + location.hostname + ":81/");

ws.onmessage = function(event){
var d = event.data.split(",");
document.getElementById("temp").innerHTML = d[0];
document.getElementById("bpm").innerHTML = d[1];
};
</script>

</body>
</html>
)====";

void setup()
{
Serial.begin(115200);

Wire.begin(21,22);

lcd.init();
lcd.backlight();

sensors.begin();

lcd.setCursor(0,0);
lcd.print("Health Monitor");
delay(2000);
lcd.clear();

WiFi.begin(ssid,password);

while(WiFi.status()!=WL_CONNECTED){
delay(500);
}

Serial.print("IP: ");
Serial.println(WiFi.localIP());

server.on("/",[](){
server.send(200,"text/html",webpage);
});

server.begin();
webSocket.begin();
}

void loop()
{

server.handleClient();
webSocket.loop();

/* -------- Pulse Sensor -------- */

int rawSignal = analogRead(pulsePin);

/* moving average filter */
signalBuffer[bufferIndex++] = rawSignal;
if(bufferIndex >= 10) bufferIndex = 0;

long sum = 0;
for(int i=0;i<10;i++) sum += signalBuffer[i];

int signal = sum / 10;

unsigned long now = millis();

/* Beat detection */

if(signal > threshold && !beat)
{
beat = true;

unsigned long interval = now - lastBeat;

if(interval > 300 && interval < 1500)
{
BPM = 60000 / interval;
}

lastBeat = now;
}

if(signal < threshold)
beat = false;

/* -------- Temperature -------- */

if(now - lastUpdate > 1000)
{

lastUpdate = now;

sensors.requestTemperatures();

float tempC = sensors.getTempCByIndex(0);

if(tempC == DEVICE_DISCONNECTED_C)
return;

float tempF = tempC * 9.0 / 5.0 + 32.0;

/* -------- LCD -------- */

lcd.setCursor(0,0);
lcd.print("Temp:");
lcd.print(tempF,1);
lcd.print("F   ");

lcd.setCursor(0,1);
lcd.print("Pulse:");

if(BPM > 40 && BPM < 180)
{
lcd.print(BPM);
lcd.print(" BPM  ");
}
else
{
lcd.print("No Finger ");
}

/* -------- WebSocket -------- */

String data = String(tempF,1) + "," + String(BPM);
webSocket.broadcastTXT(data);

}

}