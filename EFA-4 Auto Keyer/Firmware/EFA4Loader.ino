/*
*******************************************************************************************************************************************************************************
  Written and developed by Costas Skordis 16 July 2024                                                                                                                              
  Concept from the ArduiTape Arduino 8-bit computer tape player 8-Bit wave player by tebl https://github.com/tebl and Phil_G http://philg.uk for Arduino Elf Serial Auto-Keyer

  This software will allow users to navigate a directory structure on the SD card looking for text files with hexadecimal values delimited by spaces

  The navigation buttons are Up, Down, Load and Root :
          Up and Down traverses through the directory/file strcture
          Load/Select will select the curtrent directory and if displayed item is a file it will load the contents to the Super Elf
          If loading and ROOT button is pressed, the load will be aborted.

  As the data bus is loading, the arduino will enable pin 19 of the 74LS245 Octal Bus Transceiver but will disable pin 19 when finished, detaching from the data bus.
  The 74LS245 Octal Bus Transceiver has pin 1 grounded to allow data to go from B TO A data lines

  Example of hex code is Blink :

  7A F8 10 B1 21 91 3A 04 31 00 7B 30 01

  There are no line terminator at the end of the file

  Do not use Class-10 cards as they are too fast for the SPI
  set cardType to 'oldCard' if using an old SD card (more than a few years old) or to 'newCard' if using a newly-purchase Class-4 card.
  int cardType = SPI_FULL_SPEED;

  SDFAT library version 1.14

  Connections

  OLED  A4 -> SDA
        A5 -> SCL

  SD    A0 -> Chip Select
        D13 -> SCK
        D11 -> MOSI
        D12 -> MISO


  Arduino  Super ELF
  D2    -> D0 Data Bus  (pin 50)
  D3    -> D1 Data Bus  (pin 25)
  D4    -> D2 Data Bus  (pin 49)
  D5    -> D3 Data Bus  (pin 24)
  D6    -> D4 Data Bus  (pin 48)
  D7    -> D5 Data Bus  (pin 23)
  D8    -> D6 Data Bus  (pin 47)
  D9    -> D7 Data Bus  (pin 22)
  GND   -> Ground (pin 1)

*******************************************************************************************************************************************************************************
*/

#include <SdFat.h>
#include "userconfig.h"
#include <Wire.h>

char line0[17];
char line1[17];
char line2[17];

SdFat sd;      //Initialise Sd card
SdFile entry;  //SD card file

#define filenameLength 100   //200

char fileName[filenameLength + 1];  //Current filename
char sfileName[13];
char prevSubDir[2][25];  ////[5][50];

int subdir = 0;

#define SD_CS A0    //SD chip select
#define DATABUS A1  //Enable Data Bus
#define btnUp A2    //Menu Up button
#define btnDown A3  //Menu Down button
#define btnLoad A6  //Load Button
#define btnStop A7  //Stop Button

#define D0 2             //Data Bus 0
#define D1 3             //Data Bus 1
#define D2 4             //Data Bus 2
#define D3 5             //Data Bus 3
#define D4 6             //Data Bus 4
#define D5 7             //Data Bus 5
#define D6 8             //Data Bus 6
#define D7 9             //Data Bus 7
#define InKey 10         //Input key signal
#define scrollSpeed 250  //text scroll delay
#define scrollWait 3000  //Delay before scrolling starts

byte scrollPos = 0;
unsigned long scrollTime = millis() + scrollWait;

int ix = 0;                  //iniatialize data only
int start = 0;               //Currently loading flag
int currentFile = 1;         //Current position in directory
int maxFile = 0;             //Total number of files in directory
int isDir = 0;               //Is the current file a directory
unsigned long timeDiff = 0;  //button debounce

byte UP = 0;  //Next File, down button pressed
char LoadBytes[16];
int pos = 0;

File fileData;
char bit;
String bits = "";
String data = "";
int dataValue = 0;
String hexValue = "";
int pin;
unsigned int addressCounter = 0;

void printString(const char* text, int line, int center = 0) {
  setXY(0, line);
  sendStr("                 ");
  if (center == 0) {
    pos = 0;
  } else {
    if (strlen(text) < 17) {
      pos = (16 - strlen(text)) / 2;
    }
  }
  setXY(pos, line);
  sendStr(text);
}

unsigned int hexToDec(String hexString) {
  unsigned int decValue = 0;
  int nextInt;
  for (int i = 0; i < hexString.length(); i++) {
    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);
    decValue = (decValue * 16) + nextInt;
  }
  return decValue;
}

String decToHex(long decValue, byte desiredStringLength) {
  char myHex[10] = "";
  ltoa(decValue, myHex, 16);  //convert to c string base 16
  String hexString = String(myHex);
  while (hexString.length() < desiredStringLength) hexString = "0" + hexString;
  return hexString;
}

void readFile(char* file) {
  if (SerialDebug == 1) {
    Serial.println(file);
  }
  if (file == NULL) {
    return;
  }
  fileData = sd.open(file);
  bit = "";

  if (fileData) {
    fileData.rewind();
    bits = "";
    addressCounter = 0;


    /*Reading the whole file*/
    while (fileData.available()) {
      if (analogRead(btnStop) <= 1) {
        stopFile();
        return;
      }
      bit = fileData.read();
      if (bit == ' ') {
        continue;
      }
      bits += bit;
      if (bits.length() == 2) {
        hexValue = String(bits);
        dataValue = hexToDec(hexValue);
        bits = "";
        keyin();
      }
      if (analogRead(btnLoad) <= 1) {
        delay(50);
        printString("Paused...", 0, 1);
        while ((analogRead(btnLoad) > 1)) {}
        printString("Programming...", 0, 1);
        continue;
      }
    }
    fileData.close();
  }
}

void keyin() {
  if (SerialDebug == 1) {
    Serial.println(dataValue);
    Serial.println(hexValue);
  }
 
  char buf[3];
  hexValue.toCharArray(buf, 3);
  char hexAddressString = "";
  sprintf(hexAddressString, "%04X", addressCounter);
  addressCounter++;
  if (addressCounter > 0xFFFF) { addressCounter = 0; }
  strcat(hexAddressString, " ");
  strcat(hexAddressString, buf);
  printString(hexAddressString, 2, 1);


  if (dataValue & 0b00000001) {
    digitalWrite(D0, HIGH);
  } else {
    digitalWrite(D0, LOW);
  }
  if (DataDelay > 0) { delay(DataDelay); }
  if (dataValue & 0b00000010) {
    digitalWrite(D1, HIGH);
  } else {
    digitalWrite(D1, LOW);
  }
  if (DataDelay > 0) { delay(DataDelay); }
  if (dataValue & 0b00000100) {
    digitalWrite(D2, HIGH);
  } else {
    digitalWrite(D2, LOW);
  }
  if (DataDelay > 0) { delay(DataDelay); }
  if (dataValue & 0b00001000) {
    digitalWrite(D3, HIGH);
  } else {
    digitalWrite(D3, LOW);
  }
  if (DataDelay > 0) { delay(DataDelay); }
  if (dataValue & 0b00010000) {
    digitalWrite(D4, HIGH);
  } else {
    digitalWrite(D4, LOW);
  }
  if (DataDelay > 0) { delay(DataDelay); }
  if (dataValue & 0b00100000) {
    digitalWrite(D5, HIGH);
  } else {
    digitalWrite(D5, LOW);
  }
  if (DataDelay > 0) { delay(DataDelay); }
  if (dataValue & 0b01000000) {
    digitalWrite(D6, HIGH);
  } else {
    digitalWrite(D6, LOW);
  }
  if (DataDelay > 0) { delay(DataDelay); }
  if (dataValue & 0b10000000) {
    digitalWrite(D7, HIGH);
  } else {
    digitalWrite(D7, LOW);
  }
  if (DataDelay > 0) { delay(DataDelay); }

  pinMode(InKey, OUTPUT);
  //if (InKeyDelay>0) {delay(InKeyDelay);}
  if (InvertInKey == 1) {
    digitalWrite(InKey, LOW);
  } else {
    digitalWrite(InKey, HIGH);
  }
  if (InKeyDelay > 0) { delay(InKeyDelay); }
  if (InvertInKey == 1) {
    digitalWrite(InKey, HIGH);
  } else {
    digitalWrite(InKey, LOW);
  }
  //if (InKeyDelay>0) {delay(InKeyDelay);}
  digitalWrite(InKey, HIGH);
  pinMode(InKey, INPUT);
}

void setup() {
  if (SerialDebug == 1) {
    Serial.begin(19200);
  }
  delay(1000);
  Wire.begin();

  /*Set up pin mode*/
  pinMode(btnLoad, INPUT);
  pinMode(btnStop, INPUT);
  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDown, INPUT_PULLUP);
  pinMode(SD_CS, OUTPUT);
  pinMode(DATABUS, OUTPUT);
  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);
  pinMode(InKey, INPUT);

  /*Initialize pins*/
  digitalWrite(btnUp, HIGH);
  digitalWrite(btnDown, HIGH);
  digitalWrite(DATABUS, HIGH); /*Disable Databus on transceiver*/

  /*Initialize Display*/
  init_OLED();
  delay(2500);     /*Show logo for 2.5 seconds*/
  reset_display(); /*Clear logo and load saved mode*/

  /*Start SD card and check it's working*/
  if (!sd.begin(SD_CS, SPI_FULL_SPEED)) {
    printString("No SD Card", 0, 1);
    return;
  }
  /*set SD to root directory*/
  sd.chdir();
  int pos = 0;
  printString("Initializing...", 0, 1);
  digitalWrite(DATABUS, HIGH); /*Disable Databus on transceiver*/
  getMaxFile();          /*Get the total number of files in the directory*/
  seekFile(currentFile); /*Move to the first file in the directory*/
}

void loop(void) {

  if ((millis() >= scrollTime) && start == 0 && (strlen(fileName) > 16)) {
    scrollTime = millis() + scrollSpeed;
    scrollText(fileName);
    scrollPos += 1;
    if (scrollPos > strlen(fileName)) {
      scrollPos = 0;
      scrollTime = millis() + scrollWait;
      scrollText(fileName);
    }
  }
  if (millis() - timeDiff > 50) {  // check switch every 100ms
    timeDiff = millis();           /*Get current millisecond count*/
    if (analogRead(btnLoad) <= 1) {
      if (start == 0) {
        loadFile();
        delay(50);
      } else {
        stopFile();
        while (analogRead(btnLoad) <= 1) {
          //delay(50);
        }
      }
    }
    if (analogRead(btnStop) <= 1 && start == 1) {
      stopFile();
      /*Prevent button repeats by waiting until the button is released.*/
      while (analogRead(btnStop) <= 1) {
        delay(50);
      }
    }
    if (analogRead(btnStop) <= 1 && start == 0 && subdir > 0) {
      fileName[0] = '\0';
      prevSubDir[subdir - 1][0] = '\0';
      subdir--;
      switch (subdir) {
        case 1:
          sd.chdir(strcat(strcat(fileName, "/"), prevSubDir[0]), true);
          break;
        case 2:
          sd.chdir(strcat(strcat(strcat(strcat(fileName, "/"), prevSubDir[0]), "/"), prevSubDir[1]), true);
          break;
        case 3:
          sd.chdir(strcat(strcat(strcat(strcat(strcat(strcat(fileName, "/"), prevSubDir[0]), "/"), prevSubDir[1]), "/"), prevSubDir[2]), true);
          break;
        default:
          sd.chdir("/", true);
      }
      getMaxFile();
      currentFile = 1;
      seekFile(currentFile);
      while (analogRead(btnStop) <= 1) {
        delay(50); /*Prevent button repeats by waiting until the button is released.*/
      }
    }
    /*Move up a file in the directory*/
    if (digitalRead(btnUp) == LOW && start == 0) {
      scrollTime = millis() + scrollWait;
      scrollPos = 0;
      upFile();

      while (digitalRead(btnUp) == LOW) {
        delay(50); /*Prevent button repeats by waiting until the button is released.*/
        upFile();
      }
    }
    /*Move down a file in the directory*/
    if (digitalRead(btnDown) == LOW && start == 0) {
      scrollTime = millis() + scrollWait;
      scrollPos = 0;
      downFile();
      while (digitalRead(btnDown) == LOW) {
        delay(50); /*Prevent button repeats by waiting until the button is released.*/
        downFile();
      }
    }
  }
}

/*Move up a file in the directory */
void upFile() {
  currentFile--;
  if (currentFile < 1) {
    getMaxFile();
    currentFile = maxFile;
  }
  UP = 1;
  seekFile(currentFile);
}

/*Mve down a file in the directory */
void downFile() {
  currentFile++;
  if (currentFile > maxFile) {
    currentFile = 1;
  }
  UP = 0;
  seekFile(currentFile);
}

/*Move to a set position in the directory, store the filename, and display the name on screen.*/
void seekFile(int pos) {
  if (UP == 1) {
    entry.cwd()->rewind();
    for (int i = 1; i <= currentFile - 1; i++) {
      entry.openNext(entry.cwd(), O_READ);
      entry.close();
    }
  }

  if (currentFile == 1) {
    entry.cwd()->rewind();
  }
  entry.openNext(entry.cwd(), O_READ);
  entry.getName(fileName, filenameLength);
  entry.getSFN(sfileName);
  if (entry.isDir() || !strcmp(sfileName, "ROOT")) {
    isDir = 1;
  } else {
    isDir = 0;
  }
  entry.close();

  LoadBytes[0] = '\0';
  if (isDir == 1) {
    strcat_P(LoadBytes, PSTR(VERSION));
  } else {
    strcat_P(LoadBytes, PSTR("Select File"));
  }
  printString(LoadBytes, 0, 1);

  scrollPos = 0;
  scrollText(fileName);
}
/*stop loading file*/
void stopFile() {
  if (start == 1) {
    printString("Completed", 0, 1);
    start = 0;
    digitalWrite(DATABUS, HIGH);
    delay(1000);
    printString("Ready", 0, 1);
    printString("", 2, 1);
  }
}
/*load file*/
void loadFile() {
  if (isDir == 1) {
    changeDir();
  } else {
    if (entry.cwd()->exists(sfileName)) {
      printString("Programming", 0, 1);
      start = 1;
      scrollPos = 0;
      scrollText(fileName);
      digitalWrite(DATABUS, LOW); /*Enable Databus on transceiver*/
      readFile(sfileName);
      stopFile();
    } else {
      printString("No File Selected", 0, 1);
    }
  }
}

/*Get the total files in the current directory and store the number in maxFile*/
void getMaxFile() {
  entry.cwd()->rewind();
  maxFile = 0;
  while (entry.openNext(entry.cwd(), O_READ)) {
    entry.getName(fileName, filenameLength);
    entry.close();
    maxFile++;
  }
  entry.cwd()->rewind();
}

/*
  Change directory, if fileName="ROOT" then return to the root directory
  SDFat has no easy way to move up a directory, so returning to root is the easiest way.
  each directory (except the root) must have a file called ROOT (no extension)
*/
void changeDir() {
  if (!strcmp(fileName, "ROOT")) {
    subdir = 0;
    sd.chdir(true);
  } else {
    if (subdir > 0) entry.cwd()->getName(prevSubDir[subdir - 1], filenameLength);  // Antes de cambiar
    sd.chdir(fileName, true);
    subdir++;
  }
  getMaxFile();
  currentFile = 1;
  seekFile(currentFile);
}

void scrollText(char* text) {
  /*Don't scroll if text length <17 and is not a directory*/
  if (isDir == 0 && strlen(text) < 17) {
    printString(text, 1, 1);
    return;
  }

  /*Text scrolling routine.  Setup for 16x2 screen so will only display 16 chars*/
  if (scrollPos < 0) scrollPos = 0;
  char outtext[17];
  if (isDir) {
    outtext[0] = 0x3E;
    for (int i = 1; i < 16; i++) {
      int p = i + scrollPos - 1;
      if (p < strlen(text)) {
        outtext[i] = text[p];
      } else {
        outtext[i] = '\0';
      }
    }
  } else {
    for (int i = 0; i < 16; i++) {
      int p = i + scrollPos;
      if (p < strlen(text)) {
        outtext[i] = text[p];
      } else {
        outtext[i] = '\0';
      }
    }
  }
  outtext[16] = '\0';
  printString(outtext, 1, 0);
}
