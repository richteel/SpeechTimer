#ifndef TESTS
#define TESTS

#define TESTING 0  // SET TO 0 OUT TO REMOVE TESTS
#define PASS "PASS"
#define FAIL "FAIL"

#if TESTING

#include "Clock_SdCard.h"
#include "Clock_Wifi.h"
#include "Clock_Rtc.h"
#include "Clock_Output.h"

uint currentTest = 0;
uint testCountFail = 0;


Clock_SdCard sd = Clock_SdCard();
Clock_Wifi wifi = Clock_Wifi(&sd);
Clock_Rtc rtc = Clock_Rtc(&sd, &wifi);
Clock_Output clockOutput = Clock_Output();

void printDateTime() {
  char timeString[40];

  rtc.getIsoDateString(timeString);
  Serial.printf("\t\t%s\n", timeString);
}

void printDateTime(const char* message) {
  char timeString[40];

  rtc.getIsoDateString(timeString);
  Serial.printf("\t\t%s %s\n", message, timeString);
}

void testSetup() {
  clockOutput.begin();
  rtc.begin();
  sd.begin();
  wifi.begin();
}

void testEnd() {
  sd.end();
}

const char* testSdCardCardPresent(bool assertValue) {
  currentTest++;
  const char* result = FAIL;

  bool val = sd.isCardPresent();

  if (val == assertValue) {
    result = PASS;
  } else {
    testCountFail++;
  }
  return result;
}

const char* testSdCardFileExists(const char* fullFileName, bool assertValue) {
  currentTest++;
  const char* result = FAIL;

  bool val = sd.fileExists(fullFileName);

  if (val == assertValue) {
    result = PASS;
  } else {
    testCountFail++;
  }
  return result;
}

const char* testSdCardLoadConfig(const char* fullFileName, bool assertValue) {
  currentTest++;
  const char* result = FAIL;

  bool val = sd.loadConfig(fullFileName);

  if (val == assertValue) {
    result = PASS;
  } else {
    testCountFail++;
  }
  return result;
}

const char* testSdCardReadFile(const char* fullFileName, bool assertValue) {
  currentTest++;
  const char* result = FAIL;

  File f = sd.readFile(fullFileName);

  bool val = f;

  if (val == assertValue) {
    result = PASS;
    f.close();
  } else {
    testCountFail++;
  }
  return result;
}

const char* testSdCardSaveConfig(const char* fullFileName, bool assertValue) {
  currentTest++;
  const char* result = FAIL;

  bool val = sd.saveConfig(fullFileName);

  if (val == assertValue) {
    result = PASS;
  } else {
    testCountFail++;
  }
  return result;
}

const char* testSdCardWriteToLog(const char* fullFileName, bool assertValue) {
  currentTest++;
  const char* result = FAIL;

  uint linesInFileBefore = sd.countLinesInFile(fullFileName);
  sd.writeLogEntry(fullFileName, "Test Entry", LOG_INFO);
  uint linesInFileAfter = sd.countLinesInFile(fullFileName);

  bool val = linesInFileAfter == (linesInFileBefore + 1);

  if (val == assertValue) {
    result = PASS;
  } else {
    testCountFail++;
  }
  return result;
}

const char* testRtc_getInternetTime(bool assertValue) {
  currentTest++;
  const char* result = FAIL;

  bool val = rtc.getInternetTime() > (rtc.SECONDS_FROM_1970_TO_2000 + 3600);

  if (val == assertValue) {
    result = PASS;
  } else {
    testCountFail++;
  }
  return result;
}

const char* testRtc_getTimeString(bool assertValue) {
  currentTest++;
  const char* result = FAIL;

  char stringBuffer[10] = {};
  rtc.getTimeString(stringBuffer);
  Serial.printf("\t\tTime String -> %s\n", stringBuffer);

  bool val = strlen(stringBuffer) == 8;

  if (val == assertValue) {
    result = PASS;
  } else {
    testCountFail++;
  }

  char currentTime[10] = {};
  rtc.getTimeString(currentTime);

  clockOutput.updateTime(currentTime);

  return result;
}

void waitForSerial() {
  bool ready = false;

  while (!ready) {
    if (Serial.available() > 0) {
      while (Serial.available() > 0) {
        Serial.read();
        ready = true;
      }
    }
  }
}

void prompt(const char* message) {
  Serial.println(message);
  waitForSerial();
}

bool runTests() {
  if (!Serial) {
    return true;
  }

  testSetup();

  currentTest = 1;
  testCountFail = 0;

  Serial.println();
  Serial.println();
  Serial.println();
  Serial.println("=== STARTING TESTS ===");
  Serial.println(F("** START: SD Card Tests **"));
  prompt("Insert the SD Card and send a char through the serial port.");
  Serial.printf("\t%d\tTest if card is present:\t%s\n", currentTest, testSdCardCardPresent(true));
  Serial.printf("\t%d\tTest File Exists (config.txt):\t%s\n", currentTest, testSdCardFileExists("config.txt", true));
  Serial.printf("\t%d\tTest File Does Not Exist (clowns.txt):\t%s\n", currentTest, testSdCardFileExists("clowns.txt", false));
  prompt("Remove the SD Card and send a char through the serial port.");
  Serial.printf("\t%d\tTest if card is removed:\t%s\n", currentTest, testSdCardCardPresent(false));
  Serial.printf("\t%d\tTest File Does Not Exist (config.txt):\t%s\n", currentTest, testSdCardFileExists("config.txt", false));
  prompt("Insert the SD Card and send a char through the serial port.");
  Serial.printf("\t%d\tTest if card is present:\t%s\n", currentTest, testSdCardCardPresent(true));
  Serial.printf("\t%d\tTest File Exists (config.txt):\t%s\n", currentTest, testSdCardFileExists("config.txt", true));
  Serial.printf("\t%d\tTest Folder Exists (wwwroot):\t%s\n", currentTest, testSdCardFileExists("wwwroot/", true));
  Serial.printf("\t%d\tRead file (config.txt):\t%s\n", currentTest, testSdCardReadFile("config.txt", true));
  Serial.printf("\t%d\tLoad config file (config.txt):\t%s\n", currentTest, testSdCardLoadConfig("config.txt", true));
  Serial.printf("\t%d\tSave config file (test.txt):\t%s\n", currentTest, testSdCardSaveConfig("test.txt", true));
  Serial.printf("\t%d\tLoad config file (test.txt):\t%s\n", currentTest, testSdCardLoadConfig("test.txt", true));
  Serial.printf("\t%d\tWrite to log file (samtest.log):\t%s\n", currentTest, testSdCardWriteToLog("samtest.log", true));


  Serial.println(F("** START: Wi-Fi Tests **"));
  printDateTime("BEFORE:");
  Serial.printf("\t%d\tGet internet time:\t%s\n", currentTest, testRtc_getInternetTime(true));
  Serial.printf("\t\tURL for Time: %s\n", rtc.timeUpdateUrl);
  printDateTime("AFTER:");
  Serial.printf("\t%d\tGet time string:\t%s\n", currentTest, testRtc_getTimeString(true));




  Serial.println();
  if (testCountFail == 0) {
    Serial.printf("RESULT: All %d Tests Passed!\n", currentTest - 1);
  } else {
    Serial.printf("RESULT: %d Test(s) Failed out of %d Tests.\n", testCountFail, currentTest - 1);
  }

  Serial.println("=== END TESTS ===");

  testEnd();

  return testCountFail == 0;
}

#else
bool runTests() {
  return true;
}
#endif

#endif