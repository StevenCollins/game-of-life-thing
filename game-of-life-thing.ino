#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
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
  SPIFFS.begin();

  // setup server components
  server.on("/", handleRoot);
  server.onNotFound(handleRoot);
  server.on("/index.js", handleRootJs);
  server.on("/clear", handleClear);
  server.on("/random", handleRandom);
  server.on("/glider", handleGlider);
  server.on("/getGrid", handleGetGrid);
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

// send the specified file as the specified type
void sendFile(char* name, char* type) {
  if (SPIFFS.exists(name)) {
    File file = SPIFFS.open(name, "r");
    size_t sent = server.streamFile(file, type);
    file.close();
  } else {
    server.send(500, "text/plain", "Something messed up :(");
  }
}

// root returns index.html
void handleRoot() {
  digitalWrite(LED_BUILTIN, LOW);
  handleArgs();
  sendFile("/index.html", "text/html");
  digitalWrite(LED_BUILTIN, HIGH);
}

// rootJs returns index.js
void handleRootJs() {
  sendFile("/index.js", "application/javascript");
}

// clear clears the game state
void handleClear() {
  initClear();
  handleRoot();
}

// random randomizes the game state
void handleRandom() {
  initRandom();
  handleRoot();
}

// glider launches a simple glider
void handleGlider() {
  initSimpleGlider();
  handleRoot();
}

// getGrid returns grid data in JSON format
void handleGetGrid() {
  // response will be chunked - not enough memory to send it all at once
  server.chunkedResponseModeStart(200, "application/json");
  const int rowsPerChunk = 8;
  const int chunkSize = 258 * rowsPerChunk + 2; // each row is 128 '1'/'0', 128 ',', 2 '[]', and maybe 2 more of ',[]'
  char chunkData[chunkSize] = "["; // initialize the very first chunk with the outer [
  for (int chunk = 0; chunk < HEIGHT / rowsPerChunk; chunk++) {
    for (int row = 0; row < rowsPerChunk; row++) {
      int y = chunk * rowsPerChunk + row;
      strcat(chunkData, BUFFER[0][0][y] ? "[1" : "[0");
      for (int x = 1; x < WIDTH; x++) {
        strcat(chunkData, BUFFER[0][x][y] ? ",1" : ",0");
      }
      strcat(chunkData, y != HEIGHT - 1 ? "]," : "]]");
    }
    server.sendContent(chunkData);
    chunkData[0] = '\0'; // "erase" the string
  }
  server.chunkedResponseFinalize();
}

// allow config from query parameters
void handleArgs() {
  for (int i = 0; i < server.args(); i++) {
    if (server.argName(i) == "size" && server.arg(i).toInt() != 0) {
      CELL_SIZE = server.arg(i).toInt();
      WIDTH = X_RESOLUTION / CELL_SIZE;
      HEIGHT = Y_RESOLUTION / CELL_SIZE;
    } else if (server.argName(i) == "frametime" && server.arg(i).toInt() != 0) {
      TARGET_FRAMETIME = server.arg(i).toInt() * 1000;
    }
  }
}

// mirrors entire buffer 0 to buffer 1
void mirrorBuffers() {
  memcpy(BUFFER[1], BUFFER[0], sizeof(BUFFER[0]));
}

// clear
void initClear() {
  memset(BUFFER[0], 0, sizeof(BUFFER[0]));
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
