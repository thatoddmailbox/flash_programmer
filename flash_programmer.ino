#define CMD_BYTE_PROGRAM 0xA0
#define CMD_SECTOR_CHIP_ERASE 0x80
#define CMD_SOFTWARE_ID_ENTRY 0x90
#define CMD_SOFTWARE_ID_EXIT 0xF0

#define REG_MANUFACTURER 0x0
#define REG_DEVICE_ID 0x1

#define MFG_SST 0xBF

#define DEVICE_010A 0xB5
#define DEVICE_020A 0xB6
#define DEVICE_040 0xB7

int latchPin = A1;   // pin  12 on the 74hc595
int dataPin = A0;   // pin 14 on the 74hc595
int clockPin = A2;  // pin 11 on the 74hc595

int oe = 2;
int we = 3;

void setup() {
  Serial.begin(9600);

  Serial.println(F("Flash programmer"));

  DDRB = B11111111; // bus B = data bus
  PORTB = B00000000;
  
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);

  pinMode(oe, OUTPUT);
  pinMode(we, OUTPUT);

  digitalWrite(oe, HIGH);
  digitalWrite(we, HIGH);

  softwareCommand(CMD_SOFTWARE_ID_ENTRY);
  printChipInfo();
  softwareCommand(CMD_SOFTWARE_ID_EXIT);

  Serial.println();
}

void printChipInfo() {
  Serial.println(F("==== Chip info ===="));
  byte mfg = readByte(REG_MANUFACTURER);
  if (mfg != MFG_SST) {
    Serial.print(F("Unknown manufacturer value 0x"));
    Serial.print(mfg, HEX);
    Serial.println(F("!"));
    return;
  }
  Serial.println(F("Manufacturer: SST"));

  byte device = readByte(REG_DEVICE_ID);
  Serial.print(F("Device: "));
  if (device == DEVICE_010A) {
    Serial.println(F("SST39SF010A"));
  } else if (device == DEVICE_020A) {
    Serial.println(F("SST39SF020A"));
  } else if (device == DEVICE_040) {
    Serial.println(F("SST39SF040"));
  } else {
    Serial.println(F("Unknown"));
  }

  Serial.println(F("==================="));
}

void assertAddress(short address) {
  digitalWrite(latchPin, LOW);

  shiftOut(dataPin, clockPin, MSBFIRST, address >> 8); // sr 2
  shiftOut(dataPin, clockPin, MSBFIRST, address & 0xFF); // sr 1
  
  digitalWrite(latchPin, HIGH);
  delay(1);
}

byte readByte(short address) {
  digitalWrite(oe, LOW);
  digitalWrite(we, HIGH);
  DDRB = B00000000;  
  assertAddress(address);
  return PINB;
}

void writeByte(short address, byte data) {
  digitalWrite(oe, HIGH);
  digitalWrite(we, HIGH);
  assertAddress(address);
  DDRB = B11111111;
  PORTB = data;
  digitalWrite(we, LOW);
  delay(1);
  digitalWrite(we, HIGH);
}

void sectorErase(short sector) {
  softwareCommand(CMD_SECTOR_CHIP_ERASE);
  writeByte(0x5555, 0xAA);
  writeByte(0x2AAA, 0x55);
  writeByte(sector, 0x30);
  delay(200); 
}

void chipErase() {
  softwareCommand(CMD_SECTOR_CHIP_ERASE);
  writeByte(0x5555, 0xAA);
  writeByte(0x2AAA, 0x55);
  writeByte(0x5555, 0x10);
  delay(300);
}

void softwareCommand(byte cmd) {
  writeByte(0x5555, 0xAA);
  writeByte(0x2AAA, 0x55);
  writeByte(0x5555, cmd);
}

void menu() {
  Serial.println("b: bulk write bytes"); 
  Serial.println("c: erase chip");
  Serial.println("e: erase a sector");
  Serial.println("i: identify chip");
  Serial.println("r: read a byte");
  Serial.println("w: write a byte"); 
  Serial.println("x: examine memory block"); 
  Serial.println();
}

void serialClear() {  
  while (Serial.available() > 0) {
    Serial.read();
  }
}

void serialWait() {
  while (Serial.available() == 0) {}
  if (Serial.peek() == '\n') {
    Serial.read();
    serialWait();
  }
}

void serialRead(char * buf, int bufLen) {
  int i = 0;
  while (i < bufLen) {
    while (Serial.available() == 0) {}
    while (Serial.available() > 0) {
      char c = Serial.read();
      if (c == '\n') {
        return;
      }
      buf[i] = c;
      i++;
      if (i >= bufLen) {
        return;
      }
    }
  }
}

void commandRead() {
  Serial.print(F("Byte to read: "));
  serialClear();
  serialWait();
  char readIn[9];
  serialRead(readIn, 8);
  short readAddress = (short)strtoul(readIn, NULL, 16);
  Serial.print(F("0x"));
  Serial.println(readAddress, HEX);

  Serial.print(F("Value: 0x"));
  Serial.println(readByte(readAddress), HEX);
}

void commandWrite() {
  Serial.print(F("Byte to write: "));
  serialClear();
  serialWait();
  char writeIn[9];
  writeIn[8] = '\0';
  serialRead(writeIn, 8);
  short writeAddress = (short)strtoul(writeIn, NULL, 16);
  Serial.print(F("0x"));
  Serial.println(writeAddress, HEX);
  memset(writeIn, '\0', 9);
  
  serialClear();

  Serial.print(F("Value to write: "));
  serialClear();
  serialWait();
  serialRead(writeIn, 8);
  byte writeData = (byte)strtoul(writeIn, NULL, 16);
  Serial.print(F("0x"));
  Serial.println(writeData, HEX);

  softwareCommand(CMD_BYTE_PROGRAM);
  writeByte(writeAddress, writeData);
}

void commandBulkWrite() {
  Serial.print(F("Byte to start write: "));
  serialClear();
  serialWait();
  char writeIn[9];
  writeIn[8] = '\0';
  serialRead(writeIn, 8);
  short startWriteAddress = (short)strtoul(writeIn, NULL, 16);
  Serial.print(F("0x"));
  Serial.println(startWriteAddress, HEX);

  Serial.print(F("Bytes to write: "));
  serialClear();
  serialWait();
  
  char byteBuf[3];
  byte bufPos = 0;
  short writeAddress = startWriteAddress;
  while (1) {
    while (Serial.available() == 0) {}
    char c = Serial.read();
    if (c == ' ' || c == '\n') {
      byte data = (byte)strtoul(byteBuf, NULL, 16);
      memset(byteBuf, '\0', 3);
      bufPos = 0;
      Serial.print(F("0x"));
      Serial.print(data, HEX);
      Serial.print(" ");
      softwareCommand(CMD_BYTE_PROGRAM);
      writeByte(writeAddress, data);
      writeAddress++;
    } else {
      byteBuf[bufPos] = c;
      bufPos++;
      if (bufPos > 1) {
        bufPos = 0;
      }
    }
    if (c == '\n') {
      Serial.println();
      Serial.print(F("Done. Programmed "));
      Serial.print(writeAddress - startWriteAddress, DEC);
      Serial.println(F(" byte(s)."));
      return;
    }
  }
}

void commandExamine() {  
  Serial.print(F("Byte to start examining: "));
  serialClear();
  serialWait();
  char readIn[9];
  readIn[8] = '\0';
  serialRead(readIn, 8);
  short startAddress = (short)strtoul(readIn, NULL, 16);
  Serial.print(F("0x"));
  Serial.println(startAddress, HEX);

  memset(readIn, '\0', 9);
  
  Serial.print(F("Chunks to read: "));
  serialClear();
  serialWait();
  readIn[8] = '\0';
  serialRead(readIn, 8);
  short chunks = (short)strtoul(readIn, NULL, 10);
  Serial.println(startAddress, DEC);

  Serial.println();

  short chunk = 0;
  short address = startAddress;
  while (chunk < chunks) {
    Serial.print(F("0x"));
    if (address < 0x10) { Serial.print(F("0")); }
    if (address < 0x100) { Serial.print(F("0")); }
    if (address < 0x1000) { Serial.print(F("0")); }
    Serial.print(address, HEX);
    Serial.print(F(": "));

    for (byte i = 0; i < 16; i++) {
      byte b = readByte(address + i);
      if (b < 16) {Serial.print("0");}
      Serial.print(b, HEX);
      Serial.print(F(" "));
    }
    
    for (byte i = 0; i < 16; i++) {
      byte b = readByte(address + i);
      if (b >= 0x20 && b < 0x7F) { // printable range
        Serial.print((char)b);
      } else {
        Serial.print(F("."));
      }
    }

    Serial.println();

    address += 16;
    chunk++;
  }
}

void commandSectorErase() {
  Serial.print(F("Sector to erase: "));
  serialClear();
  serialWait();
  char readIn[9];
  readIn[8] = '\0';
  serialRead(readIn, 8);
  short eraseAddress = (short)strtoul(readIn, NULL, 16);
  Serial.print(F("0x"));
  Serial.println(eraseAddress, HEX);

  if ((eraseAddress & 0xF000) != eraseAddress) {
    Serial.println(F("Erase address must be aligned to 4 KByte page!"));
    Serial.println(F("For example: 0x0000, 0x1000, 0x2000, ..."));
    return;
  }

  Serial.print(F("Erasing sector..."));
  sectorErase(eraseAddress);
  Serial.println(F("done!"));
}

void loop() {
  serialClear();
  menu();

  serialWait();
  
  char c = Serial.read();

  serialClear();

  switch (c) {
    case 'b':
      commandBulkWrite();
      break;
    case 'c':
      Serial.print(F("Erasing chip..."));
      chipErase();
      Serial.println(F("done!"));
      break;
    case 'e':
      commandSectorErase();
      break;
    case 'i':
      softwareCommand(CMD_SOFTWARE_ID_ENTRY);
      printChipInfo();
      softwareCommand(CMD_SOFTWARE_ID_EXIT);
      break;
    case 'r':
      commandRead();
      break;
    case 'w':
      commandWrite();
      break;
    case 'x':
      commandExamine();
      break;
    default:
      Serial.println(F("Unknown command!"));
      break;
  }

  Serial.println();
  
  while (Serial.available() > 0) { Serial.read(); }
}
