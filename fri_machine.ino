#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AccelStepper.h>
#include <ezButton.h>

AccelStepper stepper_X1(1, 9, 10); 
AccelStepper stepper_X2(1, 11, 12); 
AccelStepper stepper_Y(1, 13, 14);

// Define a limit switch and the pin it uses
ezButton limitSwitch(17);

// SOFTWARE TEAM: PLEASE DEFINE X_MAX, Y_MAX, Y_HOME. X_HOME is assumed to be 0. 
const int Y_HOME = 0;
const int Y_MAX = 400000;
const int X_MAX = 400000;

const char* ssid = "MAKERSPACE";
const char* password = "12345678";

String message = "";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String steps_X;
String steps_Y;
bool newRequest = false;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Plotter Controls</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <div class="topnav">
    <h3>Plotter Controls</h3>
  </div>
  <!-- <div class="content">
        <form>
          <label for="steps_x">X Position:</label>
          <input type="number" id="steps_x" name="steps_x">
          <br><br>
          <label for="steps_y">Y Position:</label>
          <input type="number" id="steps_y" name="steps_y">
        </form>
        <br> 
        <button onclick="submitForm()">GO!</button>
  </div> -->
  <button onclick="home()">Home</button>
  <button onclick="circle()">Draw Circle</button>
  <button onclick="spiral()">Draw Spiral</button>
  <button onclick="line()">Draw Line</button>
</body>
<script>

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);
var dir;

function onload(event) {
    initWebSocket();
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function home() {
    websocket.send("home")
    console.log("Home")
}

function spiral() {
    websocket.send("spiral")
}

function line() {
    websocket.send("line")
}

function circle() {
    websocket.send("circle")
}

// function submitForm(){
//     var steps_x = document.getElementById("steps_x").value;
//     var steps_y = document.getElementById("steps_y").value;
//     websocket.send(steps_x+"&"+steps_y);
// }

function onMessage(event) {
    console.log(event.data);
}
</script>
</html>
)rawliteral";

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}


void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char*)data;

    if (message == "line") {
      steps_X = "0"; 
      steps_Y = String(-Y_MAX/2); 
    }
    else if (message == "circle") {
      steps_X = String(X_MAX); 
      steps_Y = "0"; 
    }
    else if (message == "sprial") {
      steps_X = String(X_MAX); 
      steps_Y = String(-Y_MAX/2); 
    }
    else if (message == "home") {
      home();
    }

    // switch (message) {
    //   // for a straight line, keep X constant and move across belt horizontally (from Y_HOME to Y_MAX)
    //   case "line":
    //     steps_X = "0"; 
    //     steps_Y = String(Y_MAX); 
    //     break;

    //   // for a circle, move to Y_HOME and keep Y constant. Rotate (move to X_MAX).
    //   case "circle":
    //     steps_X = String(X_MAX); 
    //     steps_Y = "0"; 
    //     break;

    //   // for a spiral, slowly move to Y_MAX/2 and rotate. 
    //   case "spiral":
    //     steps_X = String(X_MAX); 
    //     steps_Y = String(Y_MAX/2); 
    //     break;

    //   case "home":
    //     home();
    //     break;
    // }
    Serial.print("steps_X: ");
    Serial.println(steps_X);
    Serial.print("steps_Y: ");
    Serial.println(steps_Y);
    newRequest = true;
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      //Notify client of motor current state when it first connects
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String& var) {
  Serial.println("PROCESSOR");
  Serial.println(var);
  if (var == "STATE") {
    return "OFF";
  }
  return String();
}

void home() {
  //stepper_X1.moveTo(0);
  

  // set whatever position machine is as 0 position on the wheels
  stepper_X1.setCurrentPosition(0);
  stepper_X2.setCurrentPosition(0);

  //stepper_X2.moveTo(0);

  int state = limitSwitch.getState();
  while(state == HIGH) {
    stepper_Y.moveTo(100000);
  }
  stepper_Y.setCurrentPosition(Y_HOME);
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  initWiFi();
  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.begin();

  stepper_X1.setMaxSpeed(3000.0);
  stepper_X1.setAcceleration(1000.0);

  stepper_X2.setMaxSpeed(3000.0);
  stepper_X2.setAcceleration(1000.0);

  stepper_Y.setMaxSpeed(3000.0);
  stepper_Y.setAcceleration(1000.0);

  limitSwitch.setDebounceTime(50);

  stepper_Y.setCurrentPosition(Y_HOME);
  stepper_X1.setCurrentPosition(0);
  stepper_X2.setCurrentPosition(0);

}

int counter = 0;

void loop() {

  limitSwitch.loop();
  
  // if (counter == 0) {
  //   home();
  // }


  if (newRequest) {
    //home(); 
    stepper_X1.moveTo(steps_X.toInt());
    stepper_X2.moveTo(steps_X.toInt());
    stepper_Y.moveTo(steps_Y.toInt());
    newRequest = false;
    ws.cleanupClients();
  }

  if (stepper_X1.distanceToGo() == 0 && stepper_Y.distanceToGo() == 0) {
    //Serial.println("done");
  }

  stepper_X1.run();
  stepper_X2.run();
  stepper_Y.run();
  
  //counter = counter+1;

}