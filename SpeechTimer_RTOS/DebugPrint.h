#ifndef DEBUGPRINT
#define DEBUGPRINT

#define DEBUG 1          // SET TO 0 OUT TO REMOVE TRACES
#define USE_PICOPROBE 1  // Set to 1 if using Picoprobe
#define SHOW_HEAP 0      // Show the free heap on serial output if enabled

#if DEBUG
#if USE_PICOPROBE
#define D_SerialBegin(...) Serial1.begin(__VA_ARGS__);
#define D_SerialEnd(...) Serial1.end(__VA_ARGS__);
#define D_print(...) Serial1.print(__VA_ARGS__)
#define D_println(...) Serial1.println(__VA_ARGS__)
#define D_printf(...) Serial1.printf(__VA_ARGS__)
#define D_printlnf(...) Serial1.printlnf(__VA_ARGS__)
#define D_write(...) Serial1.write(__VA_ARGS__)
#else
#define D_SerialBegin(...) Serial.begin(__VA_ARGS__);
#define D_SerialEnd(...) Serial.end(__VA_ARGS__);
#define D_print(...) Serial.print(__VA_ARGS__)
#define D_println(...) Serial.println(__VA_ARGS__)
#define D_printf(...) Serial.printf(__VA_ARGS__)
#define D_printlnf(...) Serial.printlnf(__VA_ARGS__)
#define D_write(...) Serial.write(__VA_ARGS__)
#endif
#else
#define D_SerialBegin(...)
#define D_SerialEnd(...)
#define D_print(...)
#define D_println(...)
#define D_printf(...)
#define D_printlnf(...)
#define D_write(...)
#endif


// #include <FreeRTOS.h>
// #include <task.h>


// void taskDebugInfo() {
//   int numberOfTasks = uxTaskGetNumberOfTasks();
//   TaskStatus_t *pxTaskStatusArray = new TaskStatus_t[numberOfTasks];
//   unsigned long runtime;
//   numberOfTasks = uxTaskGetSystemState(pxTaskStatusArray, numberOfTasks, &runtime);
//   D_printf("# Tasks: %d\n", numberOfTasks);
//   for (int i = 0; i < numberOfTasks; i++) {
//     D_printf("ID: %d %-7s", i, pxTaskStatusArray[i].pcTaskName);
//     int currentState = pxTaskStatusArray[i].eCurrentState;
//     switch (currentState) {
//       case 0:
//         D_printf(" Current State running  ");
//         break;
//       case 1:
//         D_printf(" Current State ready    ");
//         break;
//       case 2:
//         D_printf(" Current State blocked  ");
//         break;
//       case 3:
//         D_printf(" Current State suspended");
//         break;
//       case 4:
//         D_printf(" Current State deleted  ");
//         break;
//     }


//     D_printf(" Priority %-2d", pxTaskStatusArray[i].uxCurrentPriority);
//     D_printf(" Freestack %-3d", pxTaskStatusArray[i].usStackHighWaterMark);
//     D_printf(" Runtime %-9d\n", pxTaskStatusArray[i].ulRunTimeCounter);
//   }
//   delete[] pxTaskStatusArray;
// }

#endif  // DEBUGPRINT