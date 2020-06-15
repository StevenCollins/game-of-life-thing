#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "SSD1306Wire.h"
#include "WiFiSettings.h"

#ifndef STASSID
#define STASSID "your-ssid"
#define STAPSK  "your-password"
#endif

// prep for WiFi and display
const char* ssid = STASSID;
const char* password = STAPSK;
ESP8266WebServer server(80);
SSD1306Wire display(0x3c, SDA, SCL);

// constants
const bool DEBUG = false;
const byte X_RESOLUTION = 128; // actual screen x resolution
const byte Y_RESOLUTION = 64; // actual screen y resolution
bool BUFFER[2][X_RESOLUTION][Y_RESOLUTION]; // two buffers, each with enough bits for each pixel
// almost constants
byte CELL_SIZE = 1; // desired size of cell in pixels
byte WIDTH = X_RESOLUTION / CELL_SIZE; // apparent screen x resolution
byte HEIGHT = Y_RESOLUTION / CELL_SIZE; // apparent screen x resolution
unsigned long TARGET_FRAMETIME = 16000; // delay if a frame takes less time than this

// global variables
byte frontBuffer = 0;

void setup() {
  // setup serial, if debugging
  if (DEBUG) {
    Serial.begin(115200);
    delay(10);
    Serial.println('\n');
  }

  // setup display components
  pinMode(LED_BUILTIN, OUTPUT);
  display.init();
  display.flipScreenVertically();

  // setup network components
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (DEBUG) Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  MDNS.begin("esp8266");
  MDNS.addService("http", "tcp", 80);

  // setup server components
  server.on("/", handleRoot);
  server.on("/clear", handleClear);
  server.on("/random", handleRandom);
  server.on("/glider", handleGlider);
  server.begin();

  if (DEBUG) Serial.println("Ready!");

  // set initial state
  //initRandom();
  initSimpleGlider();
}

void loop() {
  // important things first
  unsigned long frameStartTime = micros();
  byte backBuffer = frontBuffer == 1 ? 0 : 1;

  // handle requests
  server.handleClient();
  MDNS.update();

  // update the back buffer
  unsigned long bufferStartTime = micros();
  for (int y = 0; y < HEIGHT; y++) {
    int yMinusOne = y == 0 ? HEIGHT - 1 : y - 1;
    int yPlusOne = y == HEIGHT - 1 ? 0 : y + 1;
    for (int x = 0; x < WIDTH; x++) {
      int xMinusOne = x == 0 ? WIDTH - 1 : x - 1;
      int xPlusOne = x == WIDTH - 1 ? 0 : x + 1;

      int liveNeighbours = 0;
      liveNeighbours += BUFFER[frontBuffer][xMinusOne][yMinusOne] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][x][yMinusOne] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][xPlusOne][yMinusOne] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][xMinusOne][y] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][xPlusOne][y] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][xMinusOne][yPlusOne] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][x][yPlusOne] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][xPlusOne][yPlusOne] ? 1 : 0;

      BUFFER[backBuffer][x][y] = getNewState(BUFFER[frontBuffer][x][y], liveNeighbours);
    }
  }
  unsigned long bufferEndTime = micros();

  // send the front buffer to the display
  display.clear();
  display.setColor(WHITE);
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      if (BUFFER[frontBuffer][x][y]) {
        display.fillRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
      }
    }
  }
  if (DEBUG) display.drawString(0, 0, String(bufferEndTime - bufferStartTime)); // draw buffer calculation time
  display.display();

  // swap the buffers
  frontBuffer = backBuffer;

  // delay to make the FPS consistent
  while ((micros() - frameStartTime) < TARGET_FRAMETIME);
}

// return the new state for a cell, based on Conway's Game of Life rules
bool getNewState(bool currentState, int liveNeighbours) {
  bool futureState = false;
  if (currentState == true && (liveNeighbours == 2 || liveNeighbours == 3)) {
    futureState = true;
  } else if (currentState == false && liveNeighbours == 3) {
    futureState = true;
  }
  return futureState;
}

// root returns a test string
void handleRoot() {
  handleArgs();
  digitalWrite(LED_BUILTIN, LOW);
  server.send(200, "text/plain", "Hello, world!");
  digitalWrite(LED_BUILTIN, HIGH);
}

// clear clears the game state
void handleClear() {
  handleArgs();
  initClear();
  server.send(200, "text/plain", "Cleared!");
}

// random randomizes the game state
void handleRandom() {
  handleArgs();
  initRandom();
  server.send(200, "text/plain", "Randomized!");
}

// glider launches a simple glider
void handleGlider() {
  handleArgs();
  initSimpleGlider();
  server.send(200, "text/plain", "Glider!");
}

// allow config from query parameters
void handleArgs() {
  for (int i = 0; i < server.args(); i++) {
    if (server.argName(i) == "size" && server.arg(i).toInt() != 0) {
      CELL_SIZE=server.arg(i).toInt();
      WIDTH = X_RESOLUTION / CELL_SIZE;
      HEIGHT = Y_RESOLUTION / CELL_SIZE;
    } else if (server.argName(i) == "frametime" && server.arg(i).toInt() != 0) {
      TARGET_FRAMETIME=server.arg(i).toInt() * 1000;
    }
  }
}

// mirrors entire buffer 0 to buffer 1
void mirrorBuffers() {
  memcpy(BUFFER[1], BUFFER[0], sizeof(BUFFER[0]));
}

// clear
void initClear() {
  memset(BUFFER[0],0,sizeof(BUFFER[0]));
  mirrorBuffers();
}
// init with a simple glider
void initSimpleGlider() {
  BUFFER[0][2][1] = true;
  BUFFER[0][3][2] = true;
  BUFFER[0][1][3] = true;
  BUFFER[0][2][3] = true;
  BUFFER[0][3][3] = true;
  mirrorBuffers();
}
// init with random
void initRandom() {
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      BUFFER[0][x][y] = random(2);
    }
  }
  mirrorBuffers();
}
