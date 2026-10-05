#include <Wire.h>

#include <SPI.h>

#include <PCA9536D.h>

#include <PCA9538.h>   // <--- Using your external library!



#define LGFX_USE_V1

#include <LovyanGFX.hpp>



// --- CRUCIAL FIX: EXPANSION BUS RESET PIN ---

#define PCA_RESET 21 

// --------------------------------------------



// --- LovyanGFX Display Configuration for NORVI X ---

class LGFX : public lgfx::LGFX_Device {

  lgfx::Panel_ST7789 _panel_instance;

  lgfx::Bus_SPI      _bus_instance;



public:

  LGFX(void) {

    {

      auto cfg = _bus_instance.config();

      cfg.spi_host = SPI2_HOST;

      cfg.spi_mode = 0;

      cfg.freq_write = 40000000;

      cfg.freq_read  = 16000000;

      cfg.spi_3wire  = false;

      cfg.use_lock   = true;

      cfg.dma_channel = SPI_DMA_CH_AUTO;

      

      cfg.pin_sclk = 12; 

      cfg.pin_mosi = 11; 

      cfg.pin_miso = 13; 

      cfg.pin_dc   = 46; 

      _bus_instance.config(cfg);

      _panel_instance.setBus(&_bus_instance);

    }

    {

      auto cfg = _panel_instance.config();

      cfg.pin_cs           = 45; 

      cfg.pin_rst          = 47; 

      cfg.pin_busy         = -1;

      cfg.panel_width      = 240;

      cfg.panel_height     = 320;

      cfg.offset_x         = 0;

      cfg.offset_y         = 0;

      cfg.offset_rotation  = 0;

      cfg.dummy_read_pixel = 8;

      cfg.dummy_read_bits  = 1;

      cfg.readable         = true;

      cfg.invert           = true; 

      cfg.rgb_order        = false;

      cfg.dlen_16bit       = false;

      cfg.bus_shared       = true; 

      _panel_instance.config(cfg);

    }

    setPanel(&_panel_instance);

  }

};



LGFX tft; 



// --- Pin Definitions ---

#define SDA_PIN 8

#define SCL_PIN 9



// PCA9536 Built-in Buttons (I2C 0x41)

#define IO_PB1  0  // "Next Page" Button

#define IO_PB2  3  // "Toggle Outputs" Button



// ==========================================

// OUTPUT EXPANSION MODULE ADDRESSES 

// ==========================================

#define R4_ADDR  0x70

#define R8_ADDR  0x71

#define Q8_ADDR  0x72

#define Q16_ADDR 0x27  

// ==========================================



// --- Objects & State Variables ---

PCA9536 io;                 

PCA9538 module_r4(R4_ADDR); 

PCA9538 module_r8(R8_ADDR); 

PCA9538 module_q8(Q8_ADDR); 



int currentPage = 0; 

uint8_t  r4_state  = 0x00;

uint8_t  r8_state  = 0x00;

uint8_t  q8_state  = 0x00;

uint16_t q16_state = 0x0000;



bool lastPb1State = HIGH;

bool lastPb2State = HIGH;

unsigned long lastDisplayUpdate = 0;



void write16(uint8_t addr, uint8_t reg, uint16_t data) {

  Wire.beginTransmission(addr);

  Wire.write(reg);

  Wire.write(data & 0xFF);         

  Wire.write((data >> 8) & 0xFF);  

  Wire.endTransmission();

}



// --- I2C Scanner Function ---

void I2C_SCAN() {

    byte error, address;

    int deviceCount = 0;

    Serial.println("\n--- Scanning I2C Bus ---");

    for (address = 1; address < 127; address++) {

        Wire.beginTransmission(address);

        error = Wire.endTransmission();

        if (error == 0) {

            Serial.print("I2C device found at address 0x");

            if (address < 16) Serial.print("0");

            Serial.print(address, HEX);

            Serial.println(" !");

            deviceCount++;

            delay(1);

        }

    }

    if (deviceCount == 0) Serial.println("No I2C devices found\n");

    else Serial.println("--- Scanning complete ---\n");

}



void setup() {

  Serial.begin(115200);

  delay(1000);



  // ==========================================================

  // WAKE UP EXPANSION MODULES 

  // This is what fixes the 0x70, 0x71 not detecting!

  // ==========================================================

  pinMode(PCA_RESET, OUTPUT);

  digitalWrite(PCA_RESET, LOW);   // Brief low pulse to reset

  delay(50);

  digitalWrite(PCA_RESET, HIGH);  // Pull HIGH to wake up the modules

  delay(100);                     // Give them time to wake up

  Serial.println("PCA Expanders Awakened on GPIO 21");



  // Now that they are awake, start I2C and scan

  Wire.begin(SDA_PIN, SCL_PIN);

  I2C_SCAN();



  // --- Initialize Modules using the PCA9538 library ---

  for (int i = 0; i < 8; i++) {

    module_r4.pinMode(i, OUTPUT); module_r4.digitalWrite(i, LOW); 

    module_r8.pinMode(i, OUTPUT); module_r8.digitalWrite(i, LOW); 

    module_q8.pinMode(i, OUTPUT); module_q8.digitalWrite(i, LOW); 

  }

  

  // Q16 Module (Still uses direct Wire commands since it's 16-bit)

  write16(Q16_ADDR, 0x06, 0x0000); 

  write16(Q16_ADDR, 0x02, 0x0000); 



  if (io.begin()) {

    io.pinMode(IO_PB1, INPUT);

    io.pinMode(IO_PB2, INPUT);

  }



  tft.init();

  tft.setRotation(0); 

  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(2); 

}



void loop() {

  bool currentPb1 = io.digitalRead(IO_PB1); 

  bool currentPb2 = io.digitalRead(IO_PB2); 



  if (currentPb1 == LOW && lastPb1State == HIGH) {

    currentPage++;

    if (currentPage > 3) currentPage = 0; 

    tft.fillScreen(TFT_BLACK); 

    delay(50); 

  }

  lastPb1State = currentPb1;



  if (currentPb2 == LOW && lastPb2State == HIGH) {

    if (currentPage == 0) {

      r4_state = (r4_state == 0x00) ? 0x0F : 0x00; 

      for (int i = 0; i < 4; i++) module_r4.digitalWrite(i, bitRead(r4_state, i) ? HIGH : LOW);

    } 

    else if (currentPage == 1) {

      r8_state = (r8_state == 0x00) ? 0xFF : 0x00; 

      for (int i = 0; i < 8; i++) module_r8.digitalWrite(i, bitRead(r8_state, i) ? HIGH : LOW);

    } 

    else if (currentPage == 2) {

      q8_state = (q8_state == 0x00) ? 0xFF : 0x00; 

      for (int i = 0; i < 8; i++) module_q8.digitalWrite(i, bitRead(q8_state, i) ? HIGH : LOW);

    } 

    else if (currentPage == 3) {

      q16_state = (q16_state == 0x0000) ? 0xFFFF : 0x0000; 

      write16(Q16_ADDR, 0x02, q16_state);

    }

    delay(50); 

  }

  lastPb2State = currentPb2;



  if (millis() - lastDisplayUpdate >= 100) {

    lastDisplayUpdate = millis();

    tft.setCursor(0, 5);



    if (currentPage == 0) displayR4();

    else if (currentPage == 1) displayR8();

    else if (currentPage == 2) displayQ8();

    else if (currentPage == 3) displayQ16();

    

    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.setCursor(0, 260);

    tft.println("--------------------");

    tft.println("[B2:TOGGLE] [B1:NXT]");

  }

}



// --- Display Functions ---

bool checkModule(uint8_t addr) {

  Wire.beginTransmission(addr);

  if (Wire.endTransmission() != 0) {

    tft.setTextColor(TFT_RED, TFT_BLACK);

    tft.println(" Module Not Found!  ");

    tft.println(" Check DIP Switches!");

    for(int i=0; i<5; i++) tft.println("                    "); 

    return false;

  }

  return true;

}



void displayR4() {

  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  tft.println("   X-R4 Relays      ");

  tft.println("--------------------");

  if (!checkModule(R4_ADDR)) return;

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  for (int i = 0; i < 4; i++) {

    tft.printf(" RELAY %d: %s \n", i + 1, bitRead(r4_state, i) ? "ON " : "OFF");

  }

  for(int i=0; i<4; i++) tft.println("                    "); 

}



void displayR8() {

  tft.setTextColor(TFT_CYAN, TFT_BLACK);

  tft.println("   X-R8 Relays      ");

  tft.println("--------------------");

  if (!checkModule(R8_ADDR)) return;

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  for (int i = 0; i < 8; i++) {

    tft.printf(" RELAY %d: %s \n", i + 1, bitRead(r8_state, i) ? "ON " : "OFF");

  }

}



void displayQ8() {

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);

  tft.println("   X-Q8 Outputs     ");

  tft.println("--------------------");

  if (!checkModule(Q8_ADDR)) return;

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  for (int i = 0; i < 8; i++) {

    tft.printf(" OUT %d: %s \n", i + 1, bitRead(q8_state, i) ? "ON " : "OFF");

  }

}



void displayQ16() {

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  tft.println("   X-Q16 Outputs    ");

  tft.println("--------------------");

  if (!checkModule(Q16_ADDR)) return;

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  for (int i = 0; i < 8; i++) {

    tft.printf(" Q%02d:%-3s   Q%02d:%-3s\n", 

                i + 1, bitRead(q16_state, i) ? "ON" : "OFF", 

                i + 9, bitRead(q16_state, i + 8) ? "ON" : "OFF");

  }

}