#include <WiFiNINA.h>
#include <LiquidCrystal.h>
#include "List.hpp"
#include "IRremote.hpp"

/* setup the IR remote pin */
int RECV_PIN = 10;

/* set up the LCD for display */
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

/* define the wifi credentials */
char wifi_ssid[] = "mywifi-ssid";
char wifi_pass[] = "mywifi-password";
int wifi_status = WL_IDLE_STATUS;

/* define the client and the url */
WiFiSSLClient sslClient;
char server_domain[] = "www.tagesschau.de";
char content_url[] = "/newsticker.rdf";

/* indicates when app is ready */
bool app_ready = false;

/* list with all news titles */
List<String*> titleList;
int activeTitle = 0;
int activeLine = 0;

void setup() {
  /* open serial port and wait for it */
  Serial.begin(9600);
  while (!Serial) {;}

  /* say hello to the serial and give it 
    a little time to get things sorted */
  Serial.println("Nano RP2040 alive.");
  delay(1000);
  
  lcd.begin(16, 2);
  lcd.print("System booted.");
  lcd.setCursor(0,1);
  lcd.print("Please wait...");
  delay(2000);

  /* initialise the IR receiver */
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);

  int attempt = 1;
  while (wifi_status != WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connecting Wi-Fi...");
    lcd.setCursor(0,1);
    lcd.print(wifi_ssid);

    wifi_status = WiFi.begin(wifi_ssid,wifi_pass);

    Serial.println("Wi-Fi connection attempt #"+String(attempt));
    Serial.println("Wi-Fi status code: " + String(wifi_status));

    if(wifi_status != WL_CONNECTED){
      attempt++;
      delay(10000); 
    }
  }

  delay(1000);

  /* display the connection status */
  IPAddress ip = WiFi.localIP();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Server connecting...");
  lcd.setCursor(0,1);
  lcd.print(ip);

  /* connect to the server and get the content */
  int connect_result = sslClient.connect(server_domain, 443);
  if (connect_result) {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Connected to server.");
      lcd.setCursor(0,1);
      lcd.print(server_domain);

      /* request the content from the server */
      sslClient.println("GET " + String(content_url) + " HTTP/2");
      Serial.println("GET " + String(content_url) + " HTTP/2");
      sslClient.println("Host: " + String(server_domain));
      Serial.println("Host: " + String(server_domain));
      sslClient.println("Connection: close");
      Serial.println("Connection: close");
      sslClient.println("User-Agent: ArduinoNanoRP2040/1.0");
      Serial.println("User-Agent: ArduinoNanoRP2040/1.0");
      sslClient.println("Accept: */*");
      Serial.println("Accept: */*");
      sslClient.println();

      delay(2000);

      titleList.clear();
      bool itemListStarted = false;
      while (sslClient.available()) {
        String line = sslClient.readStringUntil('\n');
        line.trim();

        if(line.startsWith("<item>") == true){
          itemListStarted = true;
        }        

        if(itemListStarted == true){
          if(line.startsWith("<title>") == true){
              String titleText = line.substring(7,line.length()-8);

              /* replace some special characters the LCD cannot display */
              if(titleText.endsWith("&quot;")){
                titleText = titleText.substring(0,titleText.length()-6) + "\"";
              }
              
              while(titleText.indexOf("&quot;") != -1){
                titleText = titleText.substring(0,titleText.indexOf("&quot;"))
                          + "\"" + titleText.substring(titleText.indexOf("&quot;")+6);
              }
              
              titleText.replace("ä","ae");
              titleText.replace("ö","oe");
              titleText.replace("ü","ue");
              titleText.replace("ß","ss");
              titleText.replace("Ä","Ae");
              titleText.replace("Ö","Oe");
              titleText.replace("Ü","Ue");
              
              Serial.println(titleText);
              String* titlePtr = new String(titleText);
              titleList.add(titlePtr);
          }
        }
      }

      Serial.println("Number of titles: " + String(titleList.getSize()));

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Content received.");
      lcd.setCursor(0,1);
      lcd.print(titleList.getSize());

      app_ready = true;
      renderReader();
  }else{
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Failed: " + String(connect_result));
    lcd.setCursor(0,1);
    lcd.print(server_domain);
  }
}

/**
 * Handles the IR button input from
 * the remote control to provide the
 * navigation through the titles
 */
void loop() {
  if(app_ready == true){
    if(IrReceiver.decode()){
      int buttonCode = IrReceiver.decodedIRData.decodedRawData;
  
      Serial.print("Button pressed: ");
      Serial.print(buttonCode);
      Serial.println();

      /* get the currently selected title text */
      String* titleText = titleList.getValue(activeTitle);

      if(buttonCode == 245){
        /* content scroll down */
        if((titleText->length()-(activeLine*16))>32){
          activeLine++;
          renderReader();
        }
      }if(buttonCode == 244){
        /* content scroll up */
        if(activeLine > 0){
          activeLine--;
          renderReader();
        }
      }if(buttonCode == 179){
        /* next page */
        if(activeTitle == (titleList.getSize()-1)){
            activeTitle = 0;
        }else{
            activeTitle++;
        }

        activeLine = 0;
        renderReader();  
      }if(buttonCode == 180){
        /* previous page */
        if(activeTitle == 0){
            activeTitle = (titleList.getSize()-1);
        }else{
            activeTitle--;
        }

        activeLine = 0;
        renderReader();
      }if(buttonCode == 2979 || buttonCode == 224 || buttonCode == 3452){
        /* back, home, start etc. */
        activeTitle = 0;
        activeLine = 0;
        renderReader();
      }
      
      delay(200);  
      IrReceiver.resume();
    }  
  }else{
    delay(500);  
  }
}

/**
 * Renders the reader of the titles
 * based on the defined naviation
 */
void renderReader(){
    /* get the currently selected title text */
    String* titleText = titleList.getValue(activeTitle);
    Serial.println("Rendering news item: " + *titleText);

    String topLine = "";
    String bottomLine = ""; 
    int activeTextPos = (activeLine*16);
    if((titleText->length()-activeTextPos)<32){
      if((titleText->length()-activeTextPos)<16){
        topLine = titleText->substring(activeLine);
      }else{
        topLine = titleText->substring(activeTextPos,activeTextPos+16);
        bottomLine = titleText->substring(activeTextPos+16);
      }
    }else{
      topLine = titleText->substring(activeTextPos,activeTextPos+16);
      bottomLine = titleText->substring(activeTextPos+16,activeTextPos+32); 
    }   

    /* render the text on the LCD */
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(topLine);
    lcd.setCursor(0,1);
    lcd.print(bottomLine);
}
