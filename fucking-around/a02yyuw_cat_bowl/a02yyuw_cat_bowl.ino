#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

/*
  A02YYUW Cat Bowl Water-Level Experiment
  Board: Seeed XIAO nRF52840
  Sensor: DFRobot A02YYUW / SEN0311

  Wiring:
    A02YYUW VCC -> XIAO 3V3
    A02YYUW GND -> XIAO GND
    A02YYUW TX  -> XIAO D7 / RX
    A02YYUW RX  -> leave disconnected (processed/stable mode)

  Calibration from bench test:
    100% full = 224 mm (8.82 in) sensor-to-water
      0% full = 406.4 mm (16.00 in) sensor-to-bottom

  Behavior:
    - Samples every 1 minute.
    - Each sample is the median of 9 valid UART readings.
    - Prints distance and estimated percent full.
    - Emits an ALERT line whenever the level crosses downward through
      90%, 80%, 70%, ... 10%, 0%.
    - If the bowl is refilled substantially, the alert ladder resets.

  NOTE:
    This standalone sketch has no Internet connection. The ALERT line is
    designed to be forwarded later by a Meshtastic-connected gateway.
*/

static const uint32_t SENSOR_BAUD = 9600;
static const uint32_t SAMPLE_INTERVAL_MS = 1UL * 60UL * 1000UL; // 1 minute
static const uint8_t READINGS_PER_SAMPLE = 9;
static const uint32_t READING_TIMEOUT_MS = 500;

static const float FULL_DISTANCE_MM = 224.0f;
static const float EMPTY_DISTANCE_MM = 406.4f;
static const float REFILL_RESET_DELTA_PERCENT = 15.0f;

unsigned long lastSampleMillis = 0;
bool firstSample = true;
float previousPercent = -1.0f;
int nextAlertThreshold = 90;

bool readA02YYUWFrame(uint16_t &distanceMM)
{
  unsigned long start = millis();

  while (millis() - start < READING_TIMEOUT_MS)
  {
    if (!Serial1.available())
      continue;

    uint8_t b = Serial1.read();
    if (b != 0xFF)
      continue;

    uint8_t frame[4];
    frame[0] = b;

    unsigned long frameStart = millis();
    int index = 1;
    while (index < 4 && millis() - frameStart < 50)
    {
      if (Serial1.available())
        frame[index++] = Serial1.read();
    }

    if (index != 4)
      continue;

    uint8_t checksum = (frame[0] + frame[1] + frame[2]) & 0xFF;
    if (checksum != frame[3])
      continue;

    distanceMM = ((uint16_t)frame[1] << 8) | frame[2];
    return true;
  }

  return false;
}

void sortUint16(uint16_t *values, uint8_t count)
{
  for (uint8_t i = 1; i < count; i++)
  {
    uint16_t key = values[i];
    int j = i - 1;
    while (j >= 0 && values[j] > key)
    {
      values[j + 1] = values[j];
      j--;
    }
    values[j + 1] = key;
  }
}

bool getMedianDistance(uint16_t &medianMM, uint8_t &validCount)
{
  uint16_t readings[READINGS_PER_SAMPLE];
  validCount = 0;

  while (validCount < READINGS_PER_SAMPLE)
  {
    uint16_t mm;
    if (readA02YYUWFrame(mm))
    {
      readings[validCount++] = mm;
    }
    else
    {
      break;
    }
  }

  if (validCount < 3)
    return false;

  sortUint16(readings, validCount);
  medianMM = readings[validCount / 2];
  return true;
}

float distanceToPercent(float distanceMM)
{
  float percent = 100.0f * (EMPTY_DISTANCE_MM - distanceMM) /
                  (EMPTY_DISTANCE_MM - FULL_DISTANCE_MM);

  if (percent > 100.0f) percent = 100.0f;
  if (percent < 0.0f) percent = 0.0f;
  return percent;
}

void resetAlertThreshold(float percent)
{
  if (percent >= 90.0f) nextAlertThreshold = 90;
  else if (percent >= 80.0f) nextAlertThreshold = 80;
  else if (percent >= 70.0f) nextAlertThreshold = 70;
  else if (percent >= 60.0f) nextAlertThreshold = 60;
  else if (percent >= 50.0f) nextAlertThreshold = 50;
  else if (percent >= 40.0f) nextAlertThreshold = 40;
  else if (percent >= 30.0f) nextAlertThreshold = 30;
  else if (percent >= 20.0f) nextAlertThreshold = 20;
  else if (percent >= 10.0f) nextAlertThreshold = 10;
  else nextAlertThreshold = 0;
}

void checkAlerts(float percent, uint16_t distanceMM)
{
  if (previousPercent >= 0.0f && percent >= previousPercent + REFILL_RESET_DELTA_PERCENT)
  {
    resetAlertThreshold(percent);
    Serial.print("REFILL detected. Alert ladder reset at ");
    Serial.print(percent, 1);
    Serial.println("%");
  }

  while (nextAlertThreshold >= 0 && percent <= nextAlertThreshold)
  {
    Serial.print("ALERT|CAT_BOWL|");
    Serial.print(nextAlertThreshold);
    Serial.print("|percent=");
    Serial.print(percent, 1);
    Serial.print("|distance_mm=");
    Serial.println(distanceMM);

    if (nextAlertThreshold == 0)
    {
      nextAlertThreshold = -10;
      break;
    }

    nextAlertThreshold -= 10;
  }

  previousPercent = percent;
}

void takeSample()
{
  uint16_t medianMM;
  uint8_t validCount;

  if (!getMedianDistance(medianMM, validCount))
  {
    Serial.println("SAMPLE ERROR: not enough valid readings");
    return;
  }

  float percent = distanceToPercent((float)medianMM);
  float inches = medianMM / 25.4f;

  Serial.print("Water: ");
  Serial.print(percent, 1);
  Serial.print("%   Distance: ");
  Serial.print(medianMM);
  Serial.print(" mm   ");
  Serial.print(inches, 2);
  Serial.print(" in   Median of ");
  Serial.print(validCount);
  Serial.println(" readings");

  checkAlerts(percent, medianMM);
}

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial1.begin(SENSOR_BAUD);

  Serial.println();
  Serial.println("======================================");
  Serial.println(" A02YYUW CAT BOWL MONITOR");
  Serial.println("======================================");
  Serial.println("Sample interval: 1 minute");
  Serial.println("100% = 224 mm / 8.82 in");
  Serial.println("0%   = 406.4 mm / 16.00 in");
  Serial.println();

  takeSample();
  lastSampleMillis = millis();
  firstSample = false;
}

void loop()
{
  if (millis() - lastSampleMillis >= SAMPLE_INTERVAL_MS)
  {
    lastSampleMillis = millis();
    takeSample();
  }
}
