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
const int X_RESOLUTION = 128; // actual screen x resolution
const int Y_RESOLUTION = 64; // actual screen y resolution
// almost constants
int CELL_SIZE = 1; // desired size of cell in pixels
int WIDTH = X_RESOLUTION / CELL_SIZE; // apparent screen x resolution
int HEIGHT = Y_RESOLUTION / CELL_SIZE; // apparent screen x resolution
int TARGET_FRAMETIME = 16; // delay if a frame takes less time than this

// global variables
bool BUFFER[2][X_RESOLUTION][Y_RESOLUTION]; // very inefficient, takes 16kbytes RAM instead of 16kbits
int frontBuffer = 0;

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
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
  }
  MDNS.begin("esp8266");
  MDNS.addService("http", "tcp", 80);

  // setup server components
  server.on("/", handleRoot);
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
  int backBuffer = frontBuffer == 1 ? 0 : 1;
  unsigned long frameStartTime = millis();

  // handle requests
  server.handleClient();
  MDNS.update();

  // update the back buffer
  for (int y = 0; y < HEIGHT; y++) {
    int yminusone = y == 0 ? HEIGHT - 1 : y - 1;
    int yplusone = y == HEIGHT - 1 ? 0 : y + 1;
    for (int x = 0; x < WIDTH; x++) {
      int xminusone = x == 0 ? WIDTH - 1 : x - 1;
      int xplusone = x == WIDTH - 1 ? 0 : x + 1;

      int liveNeighbours = 0;
      liveNeighbours += BUFFER[frontBuffer][xminusone][yminusone] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][x][yminusone] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][xplusone][yminusone] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][xminusone][y] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][xplusone][y] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][xminusone][yplusone] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][x][yplusone] ? 1 : 0;
      liveNeighbours += BUFFER[frontBuffer][xplusone][yplusone] ? 1 : 0;

      BUFFER[backBuffer][x][y] = getNewState(BUFFER[frontBuffer][x][y], liveNeighbours);
    }
  }

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
  if (DEBUG) display.drawString(0, 0, String(millis() - frameStartTime)); // draw frametime
  display.display();

  // swap the buffers
  frontBuffer = backBuffer;

  // delay to make the FPS consistent
  while ((millis() - frameStartTime) < TARGET_FRAMETIME);
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
  digitalWrite(LED_BUILTIN, HIGH);
  //  char state[8257];
  //  for (int j = 0; j < 64; j++) {
  //    for (int i = 0; i < 128; i++) {
  //      state[i+j*129]=BUFFER[0][i][j]?1:0;
  //    }
  //    state[129+j*129]='\t';
  //  }
  //  state[8256]='\0';
  //  server.send(200, "application/octet-stream", buffersx[0][0][0]);
  server.send(200, "text/plain", "Hello, world!");
  digitalWrite(LED_BUILTIN, LOW);
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
      bool cell = random(2) == 1;
      BUFFER[0][x][y] = cell;
    }
  }
  mirrorBuffers();
}
