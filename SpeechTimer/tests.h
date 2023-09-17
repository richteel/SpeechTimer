#ifndef TESTS
#define TESTS

#define TESTING 1  // SET TO 0 OUT TO REMOVE TESTS
#define PASS "PASS"
#define FAIL "FAIL"

#if TESTING

#include "Clock_SdCard.h"

uint currentTest = 0;
uint testCountFail = 0;


Clock_SdCard sd = Clock_SdCard();

void testSetup() {
  sd.begin();
}

void testEnd() {
  sd.end();
}

const char* testCardPresent(bool assertValue) {
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

const char* testFileExists(const char* fullFileName, bool assertValue) {
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

const char* testLoadConfig(const char* fullFileName, bool assertValue) {
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

const char* testReadFile(const char* fullFileName, bool assertValue) {
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

const char* testSaveConfig(const char* fullFileName, bool assertValue) {
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
  prompt("Insert the SD Card and send a char through the serial port.");
  Serial.printf("\t%d\tTest if card is present:\t%s\n", currentTest, testCardPresent(true));
  Serial.printf("\t%d\tTest File Exists (secrets.txt):\t%s\n", currentTest, testFileExists("secrets.txt", true));
  Serial.printf("\t%d\tTest File Does Not Exist (clowns.txt):\t%s\n", currentTest, testFileExists("clowns.txt", false));
  prompt("Remove the SD Card and send a char through the serial port.");
  Serial.printf("\t%d\tTest if card is removed:\t%s\n", currentTest, testCardPresent(false));
  Serial.printf("\t%d\tTest File Exists (secrets.txt):\t%s\n", currentTest, testFileExists("secrets.txt", false));
  prompt("Insert the SD Card and send a char through the serial port.");
  Serial.printf("\t%d\tTest if card is present:\t%s\n", currentTest, testCardPresent(true));
  Serial.printf("\t%d\tTest File Exists (secrets.txt):\t%s\n", currentTest, testFileExists("secrets.txt", true));
  Serial.printf("\t%d\tTest Folder Exists (wwwroot):\t%s\n", currentTest, testFileExists("wwwroot/", true));
  Serial.printf("\t%d\tRead file (secrets.txt):\t%s\n", currentTest, testReadFile("secrets.txt", true));
  Serial.printf("\t%d\tLoad config file (secrets.txt):\t%s\n", currentTest, testLoadConfig("secrets.txt", true));
  Serial.printf("\t%d\tLoad config file (config.txt):\t%s\n", currentTest, testLoadConfig("config.txt", true));
  Serial.printf("\t%d\tSave config file (test.txt):\t%s\n", currentTest, testSaveConfig("test.txt", true));
  Serial.printf("\t%d\tLoad config file (test.txt):\t%s\n", currentTest, testLoadConfig("test.txt", true));

  Serial.println();
  if (testCountFail == 0) {
    Serial.println("RESULT: All Tests Passed!");
  } else {
    Serial.printf("RESULT: %d Tests Failed out of %d Tests.\n", testCountFail, currentTest-1);
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