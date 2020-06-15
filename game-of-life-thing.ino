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
const bool DEBUG = true;
const byte X_RESOLUTION = 128; // actual screen x resolution
const byte Y_RESOLUTION = 64; // actual screen y resolution
const byte BUFFER_TYPE_SIZE = 32; // size of the type of the buffer
const byte BUFFER_TYPE_POWR = 5; // 2 ^ x = BUFFER_TYPE_SIZE
const byte BUFFER_TYPE_MASK = 0b00011111; // BUFFER_TYPE_MASK >> BUFFER_TYPE_BITS == 0
unsigned long BUFFER[2][X_RESOLUTION/BUFFER_TYPE_SIZE][Y_RESOLUTION]; // two buffers, each with enough bits for each pixel. for mental reasons, x is the direction with the datatype bits.
// almost constants
byte CELL_SIZE = 1; // desired size of cell in pixels
byte WIDTH = X_RESOLUTION / CELL_SIZE; // apparent screen x resolution
byte HEIGHT = Y_RESOLUTION / CELL_SIZE; // apparent screen x resolution
byte TARGET_FRAMETIME = 16000; // delay if a frame takes less time than this

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
    int yminusone = y == 0 ? HEIGHT - 1 : y - 1;
    int yplusone = y == HEIGHT - 1 ? 0 : y + 1;
    for (int x = 0; x < WIDTH; x++) {
      int liveNeighbours = 0;
      
      int xactual = x >> BUFFER_TYPE_POWR;
      int xoffset = x & BUFFER_TYPE_MASK;
      int xminusone = x == 0 ? WIDTH - 1 : x - 1;
      int xminusoneactual = xminusone >> BUFFER_TYPE_POWR;
      int xminusoneoffset = xminusone & BUFFER_TYPE_MASK;
      int xplusone = x == WIDTH - 1 ? 0 : x + 1;
      int xplusoneactual = xplusone >> BUFFER_TYPE_POWR;
      int xplusoneoffset = xplusone & BUFFER_TYPE_MASK;

      // get bytes from the buffer - elses below fallback if the byte doesn't contain the correct data
      unsigned long topRow = BUFFER[frontBuffer][xactual][yminusone];
      unsigned long midRow = BUFFER[frontBuffer][xactual][y];
      unsigned long botRow = BUFFER[frontBuffer][xactual][yplusone];

      // get neighbours above and below
      liveNeighbours += bitRead(topRow, xoffset); // neighbour above
      liveNeighbours += bitRead(botRow, xoffset); // neighbour below
      // left side neighbours
      if (xminusoneactual == xactual) {
        liveNeighbours += bitRead(topRow, xminusoneoffset);
        liveNeighbours += bitRead(midRow, xminusoneoffset);
        liveNeighbours += bitRead(botRow, xminusoneoffset);
      } else {
        liveNeighbours += bitRead(BUFFER[frontBuffer][xminusoneactual][yminusone], xminusoneoffset);
        liveNeighbours += bitRead(BUFFER[frontBuffer][xminusoneactual][y], xminusoneoffset);
        liveNeighbours += bitRead(BUFFER[frontBuffer][xminusoneactual][yplusone], xminusoneoffset);
      }
      // right side neighbours
      if (xplusoneactual == xactual) {
        liveNeighbours += bitRead(topRow, xplusoneoffset);
        liveNeighbours += bitRead(midRow, xplusoneoffset);
        liveNeighbours += bitRead(botRow, xplusoneoffset);
      } else {
        liveNeighbours += bitRead(BUFFER[frontBuffer][xplusoneactual][yminusone], xplusoneoffset);
        liveNeighbours += bitRead(BUFFER[frontBuffer][xplusoneactual][y], xplusoneoffset);
        liveNeighbours += bitRead(BUFFER[frontBuffer][xplusoneactual][yplusone], xplusoneoffset);
      }

      bool thisCell = bitRead(midRow, xoffset); // the current cell
      bitWrite(BUFFER[backBuffer][xactual][y], xoffset, getNewState(thisCell, liveNeighbours));
    }
  }
  unsigned long bufferEndTime = micros();

  // send the front buffer to the display
  display.clear();
  display.setColor(WHITE);
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < (WIDTH >> BUFFER_TYPE_POWR); x++) {
      unsigned long cellGroup = BUFFER[frontBuffer][x][y];
      for (int i = 0; i < BUFFER_TYPE_SIZE; i++) {
        if (bitRead(cellGroup, i) == 1) {
          display.fillRect(((x << BUFFER_TYPE_POWR) + i) * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
        }
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
      TARGET_FRAMETIME=server.arg(i).toInt();
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
  bitWrite(BUFFER[0][0][1], 2, 1);
  bitWrite(BUFFER[0][0][2], 3, 1);
  bitWrite(BUFFER[0][0][3], 1, 1);
  bitWrite(BUFFER[0][0][3], 2, 1);
  bitWrite(BUFFER[0][0][3], 3, 1);
  mirrorBuffers();
}
// init with random
void initRandom() {
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH / BUFFER_TYPE_SIZE; x++) {
      for (int i = 0; i < BUFFER_TYPE_SIZE; i++) {
        bitWrite(BUFFER[0][x][y], i, random(2));
      }
    }
  }
  mirrorBuffers();
}
