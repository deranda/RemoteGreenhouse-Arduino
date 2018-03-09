#include <SPI.h>
#include <Wire.h>
#include <BMP180.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BH1750FVI.h>

#define dht_bodenPin 4
#define dht_luftPin 5
#define relais1 6
#define relais2 7
#define relais3 8
#define relais4 9
#define mosfet_led 10
#define mosfet_fan 11

void setLedLevel(int val);
long getPressure();
float getTemperature();
float getHumidity();
uint16_t getLightIntensity();
void displayText();
String readComPort();
String AsciiUmwandlung(int mAscii);
double getWassergehalt();

BMP180 barometer;                           //Barometer BMP180
Adafruit_SSD1306 display(4);                //Display 
DHT dht_Luft(DHT22, dht_luftPin);           //DHT22 Luftsensor an Pin D5
DHT dht_Boden(DHT22, dht_bodenPin);         //DHT22 Bodensensor an Pin D4
BH1750FVI lichtSensor;                      //Lichtesensorobjekt

unsigned long curMillis;
unsigned long prevMillis;
unsigned long delayMillis;

boolean ledStatus;        // Zustand LED Stripes          (True: ON, False: OFF)
int ledLevel;             // Helligkeit des LED Stripes   (0% - 100%)
long pressure;            // Druckmesswert                [Pa]
long lux;                 // Helligkeitsmesswert          [lux]
float temperature;        // Temperaturmesswert           [°C]
float humidity;           // Luftfeuchtemesswert          [%]
float bodenfeuchte;       // Bodenfeuchte                 [%]
String readString;        // Lesedaten der Seriellen Schnittstelle
String druckLimiter, hLimiter, temperaturdruckLimiter, bodenfeuchtedruckLimiter, luftfeuchtedruckLimiter, lichtlevelLimiter;

void setup() {
  /* 
   * -----------------------------------------
   * ***Serielle Schnittstellen und I2C Bus***
   * -----------------------------------------
   */
  Wire.begin(0x77);                           //Starte I²C des Barometers (0x77)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  //Starte I²C des Displays (0x3C)
  dht_Luft.begin();                           //Starte Luftsensor
  dht_Boden.begin();                          //Starte Bodensensor
  lichtSensor.begin();                        //Starte Lichtsensor
  lichtSensor.SetAddress(0x23);               //I²C Adresse des Sensors
  Serial.begin(9600);

  /* 
   * --------------------
   * ***Sensoren Setup***
   * --------------------
   */
  //Barometer Setup()
  barometer = BMP180();
  if(barometer.EnsureConnected()){
    barometer.SoftReset();
    barometer.Initialize();
  }
  //Display Setup()
  display.setTextColor(WHITE);
  display.clearDisplay();
  //Lichtsensor Setup()
  lichtSensor.SetMode(Continuous_H_resolution_Mode);

  /*
   * -------------------
   * ***Pinmode Setup***
   * -------------------
   */
  pinMode(relais1, OUTPUT);
  pinMode(relais2, OUTPUT);
  pinMode(relais3, OUTPUT);
  pinMode(relais4, OUTPUT);
  pinMode(mosfet_fan, OUTPUT);  

  digitalWrite(relais1, LOW);
  digitalWrite(relais2, LOW);
  digitalWrite(relais3, LOW);
  digitalWrite(relais4, LOW);
  digitalWrite(mosfet_fan, LOW); 
  analogWrite(mosfet_led, 0);

  /*
   * ---------------------
   * ***Variablen Setup***
   * ---------------------
   */
  ledStatus     = false;
  ledLevel      = 0;
  lux           = 0;
  pressure      = 0;
  temperature   = 0; 
  humidity      = 0;
  bodenfeuchte  = 0;
  druckLimiter              = "p";
  hLimiter                  = "h"; 
  temperaturdruckLimiter    = "t";
  bodenfeuchtedruckLimiter  = "b";
  luftfeuchtedruckLimiter   = "l";
  lichtlevelLimiter         = "g";
  
  readString = "";
}

void loop() {
  readString = readComPort();
  if(readString.length() > 0){
    int iType = readString[0];
    if(iType == 97){                              // Empfangsdaten: a (ASCII: 97)  LED AN/AUS
      ledLevel = (AsciiUmwandlung(readString[1])+AsciiUmwandlung(readString[2])+AsciiUmwandlung(readString[3])).toInt();
      setLedLevel(ledLevel);
    }
    else if(iType == 104){
      lux           = getLightIntensity();          // Helligkeit messen
      humidity      = getWassergehalt();            // Luftfeuchte messen
      pressure      = getPressure();                // Druckmessung
      temperature   = getTemperature();             // Temperatur messen
      
      String sSendString = hLimiter + lux + luftfeuchtedruckLimiter + humidity + druckLimiter + pressure + temperaturdruckLimiter + temperature + lichtlevelLimiter + ledLevel;
      Serial.println(sSendString);
      sSendString = "";
    }
    /*
    else if(iType == 104){                        // Emfpangsdaten: h (ASCII: 104) Helligkeit
      lux = getLightIntensity();                  // Helligkeit messen
      Serial.print("h");                          // Startbyte
      Serial.println(lux);                        // Daten zurück senden
      lux = 0;
    }
    else if(iType == 108){                        // Empfangsdaten: l (ASCII: 108) Luftfeuchtigkeit
      humidity = getHumidity();                   // Luftfeuchte messen
      Serial.print("l");                          // Startbyte
      Serial.println(humidity);                   // Daten zurück senden
      humidity = 0;
    }
    else if(iType == 112){                        // Empfangsdaten: p (ASCII: 112) Druck
      pressure = getPressure();                   // Druckmessung
      Serial.print("p");                          // Startbyte
      Serial.println(pressure);                   // Daten zurück senden
      pressure = 0;
    }  
    else if(iType == 116){                        // Empfangsdaten: t (ASCII: 116) Temperatur
      temperature = getTemperature();             // Temperatur messen
      Serial.print("t");                          // Startbyte
      Serial.println(temperature);                // Daten zurück senden
      temperature = 0;
    }*/
    iType = 0;
    readString = "";
  }
}


// ------------------------------------
// ------------ Funktionen ------------
// ------------------------------------

void setLedLevel(int val){
  double lightValue = val*2.55;
  analogWrite(mosfet_led, (int) lightValue);
}

long getPressure(){
  pressure = 0;
  if(barometer.IsConnected){
    barometer.SoftReset();
    barometer.Initialize();
    for(int i = 0; i < 10; i++){
      pressure += barometer.GetPressure();
    }
    pressure /= 10;
  }
  else{
    pressure = 0;  
  }
  return pressure;
}

float getTemperature(){
  return dht_Luft.readTemperature();
}

float getHumidity(){
  return dht_Luft.readHumidity();
}

uint16_t getLightIntensity(){
  return lichtSensor.GetLightIntensity();
}

void displayText(){
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(1,0);
  display.println("1st Row");
  display.print("2nd Row");
}

double getWassergehalt(){
  double  pws = 6.11657*exp(17.2799-(4102.99/(dht_Luft.readTemperature()+237.431)));
  double  mw = 1000*0.622*pws/(1013.25*100/dht_Luft.readHumidity() - pws);
  return mw;
}

String readComPort(){
  while (Serial.available()) {
    delay(10);  
    if (Serial.available() >0) {
      char c = Serial.read();
      readString += c;
    }
  }
  return readString;
}

String AsciiUmwandlung(int mAscii){
  String outString;
   if (mAscii == 48) outString = "0";
   else if (mAscii == 49) outString = "1";
   else if (mAscii == 50) outString = "2";
   else if (mAscii == 51) outString = "3";
   else if (mAscii == 52) outString = "4";
   else if (mAscii == 53) outString = "5";
   else if (mAscii == 54) outString = "6";
   else if (mAscii == 55) outString = "7";
   else if (mAscii == 56) outString = "8";
   else if (mAscii == 57) outString = "9";
   else if (mAscii == 107) outString = "a";
   else if (mAscii == 108) outString = "b";
   else if (mAscii == 109) outString = "c";
   else if (mAscii == 110) outString = "d";
   else if (mAscii == 111) outString = "e";
   else if (mAscii == 112) outString = "f";
  return outString;
}
