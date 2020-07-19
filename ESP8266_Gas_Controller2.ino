#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define D9 3  //RX
#define D10 1 //TX
#define D11 9
#define D12 10

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h> //  https://github.com/kakopappa/sinric/wiki/How-to-add-dependency-libraries
#include <ArduinoJson.h> // https://github.com/kakopappa/sinric/wiki/How-to-add-dependency-libraries (use the correct version)
#include <StreamString.h>

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WiFiClient client;

//Adafruit_PCD8544(CLK,DIN,D/C,CE,RST);
//TFT
//---
//CS  - D8 - 15
//SDC - D5 - 14
//SDA - D7 - 13
//DC  - D0 - 16
//RST - Rt - -1

Adafruit_PCD8544 display = Adafruit_PCD8544(14, 13, 3, 15, -1);

//Button
//------
//B1 - D3 - 0
//B2 - D2 - 4
//B3 - D1 - 5

#define RELAY 2
#define SENSOR 0
#define BUZZER 4
#define Button  A0    // button B1 Set
#define Solinoid  5 //Solinoid Valve

#define MyApiKey "de289dfg5-2fre-420a-8a54-5f2094fdsagrt" // TODO: Change to your sinric API Key. Your API Key is displayed on sinric.com dashboard
#define MySSID "Shajahan" // TODO: Change to your Wifi network SSID
#define MyWifiPassword "!ABCD@123#" // TODO: Change to your Wifi network password
#define HEARTBEAT_INTERVAL 300000 // 5 Minutes 

uint64_t heartbeatTimestamp = 0;
bool isConnected = false;

int MillisCount = 0;
int SolStateChange = 0;
int Blink = 0;
int SensorDelay;

int OffHour = 0;
int OffMin = 30;
int OffSec = 0;
int OffDelaym = 0;
int OffDelays = 0;
int Delay = 30;
int nDelay;
int sDelay;
int aDelay;
int SysOffDelay = 300;
int SysState = 1;

int button1 = 0;
int button2 = 0;
int button3 = 0;
int button4 = 0;
int button5 = 0;

int SolState = 0;
int SAlerted = 0; // Secnsor Alert Status
int TAlerted = 0; // Timer Alert Status
int AAlerted = 0; // Alexa Alert Status
int checking = 0;
int WAlerted = 0;
int BuzzerOn = 0;
int Repeat;

int hourx = 0;
int houry = 16;
int minx = 28;
int miny = 16;
int secx = 57;
int secy = 16;

void BuzzerEnable();

int buttonValue;

void turnOff(String deviceId) {
   if (deviceId == "5ef32efa6733ac446gdg45e4sf567gt33dg5fg653") // Device ID of Living Room Light
   {  
     Serial.print("Turn off Device ID: ");
     Serial.println(deviceId);
     BuzzerOn = 3;
     aDelay = Delay;
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      isConnected = false;    
      Serial.printf("[WSc] Webservice disconnected from sinric.com!\n");
      break;
    case WStype_CONNECTED: {
      isConnected = true;
      Serial.printf("[WSc] Service connected to sinric.com at url: %s\n", payload);
      Serial.printf("Waiting for commands from sinric.com ...\n");        
      }
      break;
    case WStype_TEXT: {
        Serial.printf("[WSc] get text: %s\n", payload);
        // Example payloads

        // For Switch or Light device types
        // {"deviceId": xxxx, "action": "setPowerState", value: "ON"} // https://developer.amazon.com/docs/device-apis/alexa-powercontroller.html

        // For Light device type
        // Look at the light example in github
#if ARDUINOJSON_VERSION_MAJOR == 5
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject((char*)payload);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
        DynamicJsonDocument json(1024);
        deserializeJson(json, (char*) payload);      
#endif        
        String deviceId = json ["deviceId"];     
        String action = json ["action"];
        
        if(action == "setPowerState") { // Switch or Light
            String value = json ["value"];
            if(value == "OFF") {
                turnOff(deviceId);
            }
        }
        else if (action == "SetTargetTemperature") {
            String deviceId = json ["deviceId"];     
            String action = json ["action"];
            String value = json ["value"];
        }
        else if (action == "test") {
            Serial.println("[WSc] received test command from sinric.com");
        }
      }
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
  }
}

void setup() {
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);
  Serial.begin(115200);
  
  //Initialize Display
  display.begin();

  // you can change the contrast around to adapt the display for the best viewing!
  display.setContrast(30);

  // Clear the buffer.
  display.clearDisplay();
  
  pinMode(Solinoid, OUTPUT);
  pinMode(SENSOR, INPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(Solinoid, LOW);
  digitalWrite(BUZZER, LOW);

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(MySSID, MyWifiPassword);
  Serial.println();
  Serial.print("Connecting to Wifi: ");
  Serial.println(MySSID);  

  // Waiting for Wifi connect
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if(WiFiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("WiFi connected. ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // server address, port and URL
  webSocket.begin("iot.sinric.com", 80, "/");
  Serial.println("Connected to Web Socket");

  // event handler
  webSocket.onEvent(webSocketEvent);
  Serial.println("On Event to Web Socket");
  webSocket.setAuthorization("apikey", MyApiKey);
  Serial.println("Authentication to Web Socket");
  
  // try again every 5000ms if connection has failed
  webSocket.setReconnectInterval(5000);   // If you see 'class WebSocketsClient' has no member named 'setReconnectInterval' error update arduinoWebSockets
}

void loop() {
  
  webSocket.loop();
  
  if(isConnected) {
      uint64_t now = millis();
      // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night. Thanks @MacSass
      if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
          heartbeatTimestamp = now;
          webSocket.sendTXT("H");          
      }
  }   
  
  if (MillisCount == 10)
    {
      if ( SolState && ! SAlerted && ! TAlerted && ! AAlerted )
        ConsoleDisplayActive();

      if ( SolState && SAlerted )
        ConsoleDisplaySensor();

      if ( SolState && TAlerted )
        ConsoleDisplayTimer();

      if ( SolState && AAlerted )
        {
          if (!aDelay)
              SolinoidOff();
          else
              aDelay -= 1;
              
          ConsoleDisplayAlexa();
       }

      if ( ! SolState )
        ConsoleDisplayIdle();

      if (SolState && SensorDelay < 121)
          SensorDelay += 1;
   
      if((digitalRead(SENSOR)==LOW || SAlerted) && SolState && SensorDelay > 120)
        {
          Serial.print(OffDelays);
          Serial.print("-OffDelays-");
          Serial.print(OffDelaym);
          Serial.println("-OffDelaym-Sensor detected");
          if (!SAlerted)
            BuzzerOn = 2;
          else
            if (!OffDelays)
              {
                if(!OffDelaym)
                  if(digitalRead(SENSOR)==LOW && SolState )
                    {
                      if (!WAlerted)
                        {
                          WAlerted = 1;
                          BuzzerOn = 2;
                          sDelay = Delay;
                        }
                      else
                         {
                          sDelay -= 1;
                         }
                      if (!sDelay)
                          SolinoidOff();
                    }
                  else
                    {
                      SAlerted = 0;
                      OffDelays = 30;
                      OffDelaym = 0;
                    }
                else
                  {
                    OffDelaym -=1;
                    OffDelays = 59;
                  }
              }
            else
              {
                OffDelays -= 1;
              }
       }
     else if (WAlerted)
       {
          SAlerted = 0;
          WAlerted = 0;
          OffDelays = 30;
          OffDelaym = 0;
       }
     //     SAlerted = 0;
     //     OffDelays = 30;
     //     OffDelaym = 0;
     //  }

      if ( !OffSec )
        {
          if ( !OffMin )
            {
              if ( !OffHour )
                {
                  if ( SolState )
                    {
                      Serial.println("Time Reached, Switching off Solinoid");
                      if (!TAlerted )
                        {
                          BuzzerOn = 1;
                          nDelay = Delay;
                        }
                      else
                          nDelay -= 1;
                      if (!nDelay)
                          SolinoidOff();
                    }
                      else
                      Serial.println("Time Reached zero, But Soliniod is already off, So No action Taken");
                }
                  else
                    {
                      OffHour -= 1;
                      OffMin = 59;
                      OffSec = 59;
                      TAlerted = 0;
                    }
            }
              else
                {
                  OffMin -= 1;
                  OffSec = 59;
                  TAlerted = 0;
                }
        }
       else
        {
          OffSec -= 1;
          TAlerted = 0;
        }
        
    if (!Blink)
      {
         Blink = 1;
         //digitalWrite(LED, HIGH);
      }
    else
      {
        Blink = 0;
        //digitalWrite(LED, LOW);
      }
     MillisCount = 0;
     SolStateChange = 0;

    if (!SysOffDelay && !SolState && SysState) 
      {
        Serial.println("Shutting Down the System after Idle Timeout");
        digitalWrite(BUZZER, HIGH);
        delay(500);
        digitalWrite(BUZZER, LOW);
        digitalWrite(RELAY, LOW);
        SysState = 0;
      }
    else if (SysOffDelay && !SolState && SysState)
      {
        SysOffDelay -= 1;
        Serial.print("Counting Down Idle Time:- ");
        Serial.println(SysOffDelay);
      }
    }                                                               // 1 Second Operation End here

  ReadButton();
  BuzzerEnable();

  if(button5 && !SolState && !SolStateChange) // if B4 is pressed
    {
          Serial.println("Setting Solinoid On");
          digitalWrite(Solinoid, HIGH);
          SolState = 1;
          OffHour = 0;
          OffMin = 30;
          OffSec = 0;
          SolStateChange = 1;
          SensorDelay = 1;
          digitalWrite(BUZZER, HIGH);
          delay(100);
          digitalWrite(BUZZER, LOW);
    }

  if(button5 && SolState && !SolStateChange && !AAlerted) // if B4 is pressed
    {
      SolinoidOff();
      SolStateChange = 1;
    }
          
  if(button5 && SolState && !SolStateChange && AAlerted) // if B4 is pressed
     AAlerted = 0;
              
  if ((button3 || button4) && SAlerted )
    {
      DisplaySetDelay();
    }
    
  if (button1 || button2)
    {
        //DisplayBlank(2, hourx, houry, Buty(OffHour));
        Serial.println("In Menu 1 for Edit Hour");
        DisplaySetHour();
    }
  
  if ((button3 || button4) && !SAlerted)
    {
      //DisplayBlank(2, minx, miny, Buty(OffMin));
      Serial.println("In Menu 2 for Edit Minute");
      DisplaySetMinute();
    }


  //DisplayBlank(2, hourx, houry, Buty(OffHour));  
  //Serial.print("Timer Values :- ");
  //Serial.print(OffHour);
  //Serial.print(":");
  //Serial.print(OffMin);
  //Serial.print(":");
  //Serial.println(OffSec);
  MillisCount += 1;
  delay(100);
}

void ConsoleDisplayIdle()
 {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  display.println("LPG Controller");
  display.setCursor(0,6);
  display.println("--------------");
  display.setCursor(5,16);
  display.println("Developed By");
  display.setCursor(10,27);
  display.println("Shajahan S");
  display.setCursor(9,39);
  display.println("Valve - ");
  display.setCursor(56,39);
  display.println("OFF");
  display.display();
  DisplayBlank(1, 56, 39, "OFF");
 }

void ConsoleDisplayActive()
 {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  display.println("LPG Controller");
  display.setCursor(0,6);
  display.println("--------------");
  display.setCursor(hourx,houry);
  display.setTextSize(2);
  display.println(Buty(OffHour));
  display.setCursor(20,houry);
  display.println(":");
  display.setCursor(minx,miny);
  display.println(Buty(OffMin));
  display.setCursor(49,houry);
  display.println(":");
  display.setCursor(secx,secy);
  display.println(Buty(OffSec));
  display.setTextSize(1);
  display.setCursor(5,39);
  display.println("Valve - ");
  display.setCursor(52,35);
  display.setTextSize(2);
  display.println("ON");
  display.display();
 }

void ConsoleDisplaySensor()
 {
  Serial.println("Console Display Sensor");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  display.println("LPG Controller");
  display.setCursor(0,6);
  display.println("--------------");
  display.setCursor(hourx,houry);
  display.setTextSize(2);
  display.println("Se");
  display.setCursor(30,houry);
  display.println(Buty(OffDelaym));
  display.setCursor(50,houry);
  display.println(":");
  display.setCursor(57,houry);
  if (!WAlerted)
    display.println(Buty(OffDelays));
  else
    display.println(Buty(sDelay));
  display.setTextSize(1);
  display.setCursor(5,38);
  display.println("Valve - ");
  display.setCursor(52,34);
  display.setTextSize(2);
  display.println("ON");
  display.display();
  DisplayBlank(2, hourx, houry, "Se");
 }

void ConsoleDisplayTimer()
 {
  Serial.println("Console Display Timer");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  display.println("LPG Controller");
  display.setCursor(0,6);
  display.println("--------------");
  display.setCursor(hourx,houry);
  display.setTextSize(2);
  display.println("Time");
  display.setCursor(49,houry);
  display.println(":");
  display.setCursor(secx,secy);
  display.println(Buty(nDelay));
  display.setTextSize(1);
  display.setCursor(5,39);
  display.println("Valve - ");
  display.setCursor(52,35);
  display.setTextSize(2);
  display.println("ON");
  display.display();
  DisplayBlank(2, hourx, houry, "Time");
 }

void ConsoleDisplayAlexa()
 {
  Serial.println("Console Display Alexa");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  display.println("LPG Controller");
  display.setCursor(0,6);
  display.println("--------------");
  display.setCursor(hourx,houry);
  display.setTextSize(2);
  display.println("Alex");
  display.setCursor(49,houry);
  display.println(":");
  display.setCursor(secx,secy);
  display.println(Buty(aDelay));
  display.setTextSize(1);
  display.setCursor(5,39);
  display.println("Valve - ");
  display.setCursor(52,35);
  display.setTextSize(2);
  display.println("ON");
  display.display();
  DisplayBlank(2, hourx, houry, "Alex");
 }
void DisplaySetHour()
{
    if(button1)
      {
        if(OffHour==23)
          {
            OffHour=0;
          }
        else
          {
            OffHour=OffHour+1;
          }
      }
    if(button2)
      {
        if(!OffHour)
          {
            OffHour=23;
          }
        else
          {
            OffHour=OffHour-1;
          }
      }
     ConsoleDisplayActive();
}

void DisplaySetMinute()
{
    if(button3)
      {
        if(OffMin==59)
          {
            OffMin=0;
          }
        else
          {
            OffMin=OffMin+1;
          }
      }
    if(button4)
      {
        if(!OffMin)
          {
            OffMin=59;
          }
        else
          {
            OffMin=OffMin-1;
          }
      }
   ConsoleDisplayActive();
}

void DisplaySetDelay()
{
    if(button3)
      {
        if(OffDelaym==59)
          {
            OffDelaym=0;
          }
        else
          {
            OffDelaym += 1;
          }
      }
    if(button4)
      {
         if(OffDelaym==0)
          {
            OffDelaym=59;
          }
        else
          {
            OffDelaym -= 1;
          }
      }
   ConsoleDisplaySensor();
}

void BuzzerEnable()
  {
    if (BuzzerOn == 1)
      {
        if (!TAlerted)
          {
            Repeat = 5;
            TAlerted = 1;
          }
        else
          Repeat -= 1;
          
        ConsoleDisplayTimer();
        Serial.println("Enabiling Buzzer");
        if (Repeat > 0)
          {
            digitalWrite(BUZZER, HIGH);
            delay(300);
            digitalWrite(BUZZER, LOW);
          }
        else
          {
           BuzzerOn = 0; 
          }
      }
     else if (BuzzerOn == 2)
      {
        ConsoleDisplaySensor();
        if (!SAlerted)
          {
            Repeat = 10;
            SAlerted = 1;
          }
        else
          Repeat -= 1;
          
        Serial.println("Long Enabiling Buzzer");
        if ( Repeat > 0 )
          {
            digitalWrite(BUZZER, HIGH);
            delay(200);
            digitalWrite(BUZZER, LOW);
          }
        else
          {
           BuzzerOn = 0; 
          }
      }

      else if (BuzzerOn == 3)
        {
          ConsoleDisplayAlexa();
          if (!AAlerted)
            {
              Repeat = 10;
              AAlerted = 1;
            }
          else
            Repeat -= 1;

      Serial.println("Long Enabiling Buzzer");
      if ( Repeat > 0 )
        {
          digitalWrite(BUZZER, HIGH);
          delay(100);
          digitalWrite(BUZZER, LOW);
        }
      else
        {
          BuzzerOn = 0; 
        }        
    }
  }

String Buty(int Dtime)
 {
  String Ntime;
  if (Dtime < 10)
  {
    Ntime = "0";
    Ntime += String(Dtime);  
  }
  else
  {
    Ntime = String(Dtime);
  }
  return Ntime;
 }

void SolinoidOff()
  {
    Serial.println("Setting Solinoid off");
    digitalWrite(Solinoid, LOW);
    SolState = 0;
    OffHour = 0;
    OffMin = 0;
    OffSec = 0;
    TAlerted = 0;
    SAlerted = 0;
    WAlerted = 0;
    AAlerted = 0;
    SysOffDelay = 300;
    SysState = 1;
    digitalWrite(BUZZER, HIGH);
    delay(300);
    digitalWrite(BUZZER, LOW);
 }

void DisplayBlank(int tsize, int xstart, int ystart, String text)
  {
    if (!Blink && !BuzzerOn)
      {
        display.setTextSize(tsize);
        display.setTextColor(WHITE);
        display.setCursor(xstart,ystart);
        display.println(text);
        display.display();
      }
  }
  
bool debounce ()
{
  byte count = 0;
  for(byte i = 0; i < 5; i++)
  {
    if ( !digitalRead(button1) )
      count++;
    delay(10);
  }
 
  if(count > 2)  return 1;
  else           return 0;
}

void ReadButton()
  {
    byte count = 0;
    buttonValue = analogRead(Button);
    //Serial.print("Some button pressed- ");
    //Serial.println(buttonValue);

    if (buttonValue > 50)
      {
        BuzzerOn = 0;
        for(byte i = 0; i < 5; i++)
          {
            buttonValue = analogRead(Button);
            if (buttonValue > 50)
              count++;
            delay(10);
          }
        if (count < 2)
            buttonValue = 0;
            
        Serial.print("Analog Value Read :- ");
        Serial.println(buttonValue);
       }
     
     if(buttonValue>=750 && buttonValue<=900)
      {
        Serial.print("B1 button pressed- ");
        Serial.println(buttonValue);
        button1 = 1;
        button2 = 0;
        button3 = 0;
        button4 = 0;
        button5 = 0;
      }
     else if(buttonValue>=600  && buttonValue<=700)
      {
       Serial.print("B2 button pressed- ");
       Serial.println(buttonValue);
        button1 = 0;
        button2 = 2;
        button3 = 0;
        button4 = 0;
        button5 = 0;
      }
    else if(buttonValue>=450  && buttonValue<=550)
      {
       Serial.print("B3 button pressed- ");
       Serial.println(buttonValue);
        button1 = 0;
        button2 = 0;
        button3 = 1;
        button4 = 0;
        button5 = 0;
      }
    else if(buttonValue>=300  && buttonValue<=350)
      {
       Serial.print("B4 button pressed- ");
       Serial.println(buttonValue);
        button1 = 0;
        button2 = 0;
        button3 = 0;
        button4 = 1;
        button5 = 0;
      }
     else if(buttonValue>=150  && buttonValue<=200)
      {
       Serial.print("B5 button pressed- ");
       Serial.println(buttonValue);
        button1 = 0;
        button2 = 0;
        button3 = 0;
        button4 = 0;
        button5 = 1;
      }
     else
      {
        button1 = 0;
        button2 = 0;
        button3 = 0;
        button4 = 0;
        button5 = 0;   
      }
  }
