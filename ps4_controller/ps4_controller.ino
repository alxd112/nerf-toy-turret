#include <Bluepad32.h>
#include <AccelStepper.h>

const int stepPin = 14;
const int dirPin = 12;
const int enablePin = 13;

AccelStepper stepper(AccelStepper::DRIVER, stepPin, dirPin);

// Shared between cores
volatile float globalMotorSpeed = 0;

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// Runs on core 0 so step pulses are not delayed by Bluetooth
void MotorTask(void *pvParameters) {
  while (1) {
    stepper.setSpeed(globalMotorSpeed);
    stepper.runSpeed();
    vTaskDelay(1); // feed the watchdog
  }
}

void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      Serial.printf("Controller connected at index %d\n", i);
      myControllers[i] = ctl;
      break;
    }
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      Serial.printf("Controller disconnected from index %d\n", i);
      myControllers[i] = nullptr;
      break;
    }
  }
}

void processGamepad(ControllerPtr ctl) {
  int32_t axisY = ctl->axisY();       // left stick Y: -511..512
  int32_t throttle = ctl->throttle(); // R2: 0..1023

  if (abs(axisY) < 40) {
    globalMotorSpeed = 0;
    return;
  }

  float speedFactor = axisY < 0 ? axisY / 511.0f : axisY / 512.0f;
  float turboMultiplier = 1.0f + throttle / 1023.0f;

  // Negative so that stick up = forward
  globalMotorSpeed = speedFactor * -500.0f * turboMultiplier;
}

void setup() {
  Serial.begin(115200);

  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, LOW); // A4988 enable is active LOW

  stepper.setMaxSpeed(4000);

  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();
  BP32.enableVirtualDevice(false);

  xTaskCreatePinnedToCore(MotorTask, "MotorTask", 10000, NULL, 1, NULL, 0);

  Serial.println("System Ready. Pair your PS4 Controller (PS + Share)");
}

void loop() {
  BP32.update();

  for (auto myController : myControllers) {
    if (myController && myController->isConnected() && myController->hasData()) {
      processGamepad(myController);
    }
  }

  delay(10);
}
