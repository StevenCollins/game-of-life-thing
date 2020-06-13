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
const int X_RESOLUTION = 128; // actual screen x resolution
const int Y_RESOLUTION = 64; // actual screen y resolution
const unsigned int BUFFER_TYPE_SIZE = 8; // size of the type of the buffer
byte BUFFER[2][X_RESOLUTION/BUFFER_TYPE_SIZE][Y_RESOLUTION]; // two buffers, each with enough bits for each pixel. for mental reasons, x is the direction with the datatype bits.
// almost constants
int CELL_SIZE = 1; // desired size of cell in pixels
int WIDTH = X_RESOLUTION / CELL_SIZE; // apparent screen x resolution
int HEIGHT = Y_RESOLUTION / CELL_SIZE; // apparent screen x resolution
int TARGET_FRAMETIME = 16; // delay if a frame takes less time than this

// global variables
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
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
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
      int liveNeighbours = 0;
      
      // get neighbours above and below
      int xactual = x / 8;
      int xoffset = x % 8;
      liveNeighbours += bitRead(BUFFER[frontBuffer][xactual][yminusone], xoffset);
      liveNeighbours += bitRead(BUFFER[frontBuffer][xactual][yplusone], xoffset);

      // get leftside neighbours
      int xminusone = x == 0 ? WIDTH - 1 : x - 1;
      int xminusoneactual = xminusone / 8;
      int xminusoneoffset = xminusone % 8;
      liveNeighbours += bitRead(BUFFER[frontBuffer][xminusoneactual][yminusone], xminusoneoffset);
      liveNeighbours += bitRead(BUFFER[frontBuffer][xminusoneactual][y], xminusoneoffset);
      liveNeighbours += bitRead(BUFFER[frontBuffer][xminusoneactual][yplusone], xminusoneoffset);

      // get rightside neighbours
      int xplusone = x == WIDTH - 1 ? 0 : x + 1;
      int xplusoneactual = xplusone / 8;
      int xplusoneoffset = xplusone % 8;
      liveNeighbours += bitRead(BUFFER[frontBuffer][xplusoneactual][yminusone], xplusoneoffset);
      liveNeighbours += bitRead(BUFFER[frontBuffer][xplusoneactual][y], xplusoneoffset);
      liveNeighbours += bitRead(BUFFER[frontBuffer][xplusoneactual][yplusone], xplusoneoffset);

      int thisCell = bitRead(BUFFER[frontBuffer][xactual][y], xoffset);
      bitWrite(BUFFER[backBuffer][xactual][y], xoffset, getNewState(thisCell, liveNeighbours));
    }
  }

  // send the front buffer to the display
  display.clear();
  display.setColor(WHITE);
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH / BUFFER_TYPE_SIZE; x++) {
      byte cellGroup = BUFFER[frontBuffer][x][y];
      for (int i = 0; i < BUFFER_TYPE_SIZE; i++) {
        if (bitRead(cellGroup, i) == 1) {
          display.fillRect((x * 8 + i) * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
        }
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
