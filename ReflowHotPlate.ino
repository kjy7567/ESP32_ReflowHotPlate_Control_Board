//rev.0

//For st7735 display
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <TimerEvent.h>
// #include <analogWrite.h>

//These pins will also work for the 1.8" TFT shield
//ESP32-WROOM
#define TFT_DC 14 //A0
#define TFT_CS 12 //CS
#define TFT_MOSI 27 //SDA
#define TFT_CLK 26 //SCK
#define TFT_RST 0
#define TFT_MISO 0

//#define TFT_DC 12 //A0
//#define TFT_CS 13 //CS
//#define TFT_MOSI 14 //SDA
//#define TFT_CLK 27 //SCK
//#define TFT_RST 0
//#define TFT_MISO 0

//NTC3950 100k
int ThermistorPin = 34;

//Solid State Relay
int SSR = 25;

//TimerEvent Interval ms
const int mainInterval = 1;   //Read temperature interval
const int plotInterval = 2000;    //plot graph, plot temperature value interval

TimerEvent main;
TimerEvent plot_temp;

double setTemp;
double currentTemp;
double error;

//Ambient Temperature
double ambTemp = 25;

//For Pb reflow profile
typedef struct
{
  int rampup;
  int preheat;
  int peak;

  int heatup_time;
  int preheat_time;
  int peak_time;
  int soak_time;

} reflow_profile;

reflow_profile profile = {60, 150, 250, 60, 60, 60, 60};

int t1 = profile.heatup_time * 1000;  //ms to sec
int t2 = t1 + (profile.preheat_time * 1000);
int t3 = t2 + (profile.peak_time * 1000);
int t4 = t3 + (profile.soak_time * 1000);

unsigned long startTime, currentTime, previousTime, elapsedTime;

//ADC Setup
double adcMax = 4095.0;
double Vs = 3.3;

double R1 = 10000.0;    //voltage divider resistor value
double Beta = 3950.0;   //Beta value
double To = 298.15;     //Temperature in Kelvin for 25 degree Celsius
double Ro = 100000.0;   //Resistance of Thermistor at 25 degree Celsius

double Vout, Rt = 0;
double T, Tc, Tf = 0;

double adcValue = 0;

//for Plot Graph & Display Current Temperature, Display Current Setpoint
int X = 1;
int Y = 0;

//Cursor position for Display Current Temperature
int Current_tempX = 10;
int Current_tempY = 10;

//Set Graph
int Graph_originX = 28;
int Graph_originY = 117;
int Graph_width = 125;
int Graph_height = 80;

bool start = false;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);

void setup() {
  
//  Serial.begin(9600);
//  Serial.println("hello");

//  delay(1000);

  int Margin_left = 5;

  int ambTempY = ambTemp / 3.125;                 //1 Pixel is 3.125 Degree Celcius
  int Initial_time = profile.heatup_time / 2;     //1 Pixel is 2 sec, initial_time is 60s = 30 Pixel
  int Initial_start = Graph_originX + 1;
  int Initial_end = Initial_start + Initial_time;

  int preheatY = profile.preheat / 3.125;
  int Preheat_time = profile.preheat_time / 2;
  int Preheat_start = Initial_end + 1;
  int Preheat_end = Preheat_start + Preheat_time;

  int peakY = profile.peak / 3.125;
  int Peak_time = profile.peak_time / 2;
  int Peak_start = Preheat_end + 1;
  int Peak_end = Peak_start + Peak_time;

  int Soak_time = profile.soak_time / 2;
  int Soak_start = Peak_end + 1;
  int Soak_end = Soak_start + Soak_time;

  tft.initR(INITR_BLACKTAB);    //You will need to do this in every sketch
  tft.setRotation(3);           //Rotate Image 270 Degree
  tft.fillScreen(ST7735_BLACK); //Display Fill Black

  //Y Axis Temperature Value Display
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  tft.setCursor(Margin_left, Graph_originY-Graph_height);
  tft.print("250");
  tft.setCursor(Margin_left, tft.getCursorY()+16);
  tft.print("200");
  tft.setCursor(Margin_left, tft.getCursorY()+16);
  tft.print("150");
  tft.setCursor(Margin_left, tft.getCursorY()+16);
  tft.print("100");
  tft.setCursor(Margin_left, tft.getCursorY()+16);
  tft.print(" 50");
  tft.setCursor(Margin_left, tft.getCursorY()+16);
  tft.print("  0");

  //Graph Vertical Line, Graph Horizontal Line
  tft.drawFastVLine(Graph_originX, Graph_originY-Graph_height, Graph_height, ST7735_WHITE);   //Vertical Line
  tft.drawFastHLine(Graph_originX, Graph_originY, Graph_width, ST7735_WHITE);                 //Horizontal Line

  //Graph Temperature bar
  tft.drawFastHLine(25, Graph_originY-Graph_height, 3, ST7735_WHITE);         //250
  tft.drawFastHLine(25, Graph_originY-Graph_height+(16*1), 3, ST7735_WHITE);  //200
  tft.drawFastHLine(25, Graph_originY-Graph_height+(16*2), 3, ST7735_WHITE);  //150
  tft.drawFastHLine(25, Graph_originY-Graph_height+(16*3), 3, ST7735_WHITE);  //100
  tft.drawFastHLine(25, Graph_originY-Graph_height+(16*4), 3, ST7735_WHITE);  // 50
  tft.drawFastHLine(25, Graph_originY-Graph_height+(16*5), 3, ST7735_WHITE);  //  0

  //Ramp_up
  tft.drawLine(Initial_start, Graph_originY-ambTempY, Initial_end, Graph_originY-preheatY, ST7735_WHITE);

  //150°
  tft.drawLine(Preheat_start, Graph_originY-preheatY, Preheat_end, Graph_originY-preheatY, ST7735_WHITE);
  
  //Ramp_up
  tft.drawLine(Peak_start, Graph_originY-preheatY, Peak_end, Graph_originY-peakY, ST7735_WHITE);

  //250°
  tft.drawLine(Soak_start, Graph_originY- peakY, Soak_end, Graph_originY-peakY, ST7735_WHITE);

  //Set SSR pin OUTPUT & HIGH for Active Low SSR
  pinMode(SSR, OUTPUT);
  digitalWrite(SSR, LOW);

  //TimerEvent
  main.set(mainInterval, main_process);
  plot_temp.set(plotInterval, plot_current_temp);
}

void loop() {
  //Update TimerEvent
  main.update();
  plot_temp.update();
}

double ReadTemperature(int ThermistorPin) {
  //Calculate Temperature
  adcValue = analogRead(ThermistorPin);

  Vout = adcValue * Vs/adcMax;
  Rt = R1 * Vout / (Vs - Vout);

  T = 1/(1/To + log(Rt/Ro)/Beta);   //Temperature in Kelvin
  Tc = T - 273.15;                  //Celsius

  return Tc;
}

void plot_current_temp() {

  static int previousTemp = 0;
  static int previousSetTemp = 0;
  
  //Clear Previous Temperature
  tft.setTextColor(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println(String(previousTemp));
  
  //Display Current Temperature
  tft.setTextColor(ST7735_RED);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print(String(int(currentTemp)));

  //Clear Previous Setpoint
  tft.setTextColor(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setCursor(50, 10);
  tft.print(String(previousSetTemp));
  
  //Display Setpoint
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(50, 10);
  tft.print(String(int(setTemp)));

  previousTemp = int(currentTemp);
  previousSetTemp = int(setTemp);

  //Draw Dot Current Temperature
  Y = int(currentTemp / 3.125);
  tft.drawPixel(Graph_originX+X, Graph_originY-Y, ST7735_GREEN);
  X++;

//  Serial.print(currentTemp);
//  Serial.print("     ");
//  Serial.println(setTemp);
}

void main_process() {
  
  if(!start) {
    startTime = millis();
    start = true;
  }
  
  currentTime = millis();
  elapsedTime = (currentTime - previousTime);
  
  if (currentTime - startTime < t1) {
    setTemp = profile.preheat;
  }
  if (currentTime - startTime > t1 && currentTime - startTime < t2) {
    setTemp = profile.preheat;
  }
  if (currentTime - startTime > t2 && currentTime - startTime < t3) {
      setTemp = profile.peak;
  }
  if (currentTime - startTime > t3 && currentTime - startTime < t4) {
    setTemp = profile.peak;
  }
  if (currentTime - startTime > t4) {
    setTemp = ambTemp;

    if (currentTemp < ambTemp + 10 && currentTemp > 0) {
      //Off
      digitalWrite(SSR, LOW);
      setTemp = 0; 
      tft.fillScreen(ST7735_BLACK);
      while(true);
    }
  }
  
  currentTemp = ReadTemperature(ThermistorPin);
  error = setTemp - currentTemp;

  if(error < 0){
    digitalWrite(SSR, LOW);
//    Serial.println("off");
  }
  else{
    digitalWrite(SSR, HIGH);
//    Serial.println("on");
  }

//PID Control
//  pid.p_term = error;
//
//  if(error > -3 && error < 3)
//    pid.i_term += constrain(pid.i_term, 0, 100);
//
//  pid.d_term = (error - lastError) / elapsedTime;
//
//  PID_value = pid.Kp * pid.p_term + pid.Ki * pid.i_term + pid.Kd * pid.d_term;
//
//  PID_value = constrain(PID_value, 0, 255);
//
//  PID_value = 255 - PID_value;
//  analogWrite(SSR, PID_value);  //Change the Duty Cycle applied to the SSR
//
//  lastError = error;
//  previousTime = currentTime;

}
