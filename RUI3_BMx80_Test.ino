#include <Wire.h>
#include "rak1906.h" // https://downloads.rakwireless.com/RUI/RUI3/Library/
#include "Seeed_BME280.h" // http://librarymanager/All#Seeed_BME280

rak1906 rak1906;
BME280 bme280;
bool hasBME680 = false, hasBME280 = false;

/*
  WARNING

  In order to work this code needs a couple of fixes:

  In rak1906.h add the following code just under "float gas(void);"

    //@brief  This function will return the Chip ID
    // @return uint8_t
    uint8_t chipID(void);

  In rak1906.cpp add the following code at the bottom of the file:

  uint8_t rak1906::chipID(void) {
    return readByte(RAK1906_CHIPID_REGISTER);
  }

  ----> This change will be reflected in the official library soon

  In Seeed_BME280.cpp I modified the code to return float and not uint32_t for
  getPressure() and getHumidity (getTemperature() already does)

  float BME280::getPressure(void) {
    int64_t var1, var2, p;
    // Call getTemperature to get t_fine
    getTemperature();
    // Check if the last transport successed
    if (!isTransport_OK) {
      return 0;
    }
    int32_t adc_P = BME280Read24(BME280_REG_PRESSUREDATA);
    adc_P >>= 4;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    if (var1 == 0) {
      return 0; // avoid exception caused by division by zero
    }
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (float)(p / 256.0);
  }

  float BME280::getHumidity(void) {
    int32_t v_x1_u32r, adc_H;
    // Call getTemperature to get t_fine
    getTemperature();
    // Check if the last transport successed
    if (!isTransport_OK) {
      return 0;
    }
    adc_H = BME280Read16(BME280_REG_HUMIDITYDATA);
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (
                  ((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1_u32r)) +
                    ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) *
                        (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) *
                        ((int32_t)dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    v_x1_u32r = v_x1_u32r >> 12;
    float h = v_x1_u32r / 1024.0;
    return h;
  }

  Seeed_BME280.h reflects these changes:

    float getPressure(void);
    float getHumidity(void);


*/

uint8_t myBus[128];
void i2cScan() {
  byte error, addr;
  memset(myBus, 0, 128);
  uint8_t nDevices, ix = 0;
  Serial.println("\nI2C scan in progress...");
  nDevices = 0;
  Serial.print("   |   .0   .1   .2   .3   .4   .5   .6   .7   .8   .9   .A   .B   .C   .D   .E   .F\n");
  Serial.print("-------------------------------------------------------------------------------------\n0. |   .  ");
  char memo[64];
  char buff[32];
  char msg[128];
  uint8_t px = 0;
  for (addr = 1; addr < 128; addr++) {
    Wire.beginTransmission(addr);
    error = Wire.endTransmission();
    if (error == 0) {
      sprintf(msg + px, "0x%2x      ", addr);
      // more spaces than required to be sure to erase "Scanning"
      msg[px + 5] = 0;
      Serial.print(msg + px);
      msg[px + 5] = ' ';
      // save the addr in the table
      // makes it easier to detect available devices during setup()
      myBus[addr] = addr;
      nDevices++;
      px += 5;
    } else {
      Serial.print("  .  ");
    }
    if (addr > 0 && (addr + 1) % 16 == 0 && addr < 127) {
      Serial.write('\n');
      Serial.print(addr / 16 + 1);
      Serial.print(". | ");
    }
  }
  msg[px] = 0;
  Serial.println("\n-------------------------------------------------------------------------------------");
  Serial.println("I2C devices found: " + String(nDevices));
}

void show680() {
  if (rak1906.update()) {
    Serial.printf("Temperature = %.2f。C\r\n", rak1906.temperature());
    Serial.printf("Humidity = %.2f%%\r\n", rak1906.humidity());
    Serial.printf("Pressure = %.2f hPa\r\n", rak1906.pressure());
    Serial.printf("Air quality = %.2f\r\n", rak1906.gas());
  } else {
    Serial.println("Please plug in the sensor RAK1906 and Reboot");
  }
}

void show280() {
  float pressure = bme280.getPressure(), temp = bme280.getTemperature(), humidity = bme280.getHumidity();
  Serial.printf("Temperature = %.2f C\n", temp);
  Serial.printf("Humidity = %.2f%%\n", humidity);
  Serial.printf("Pressure = %.2f hPa\n", (pressure / 100.0));
}

void setup() {
  Serial.begin(115200, RAK_CUSTOM_MODE);
  // RAK_CUSTOM_MODE disables AT firmware parsing
  time_t timeout = millis();
  while (!Serial) {
    // on nRF52840, Serial is not available right away.
    // make the MCU wait a little
    if ((millis() - timeout) < 5000) {
      delay(100);
    } else {
      break;
    }
  }
  uint8_t x = 5;
  while (x > 0) {
    Serial.printf("%d, ", x--);
    delay(500);
  } // Just for show
  Serial.println("0!");
  Serial.println("RAKwireless BM*80 Test");
  Serial.println("------------------------------------------------------");
  Wire.begin();
  //  Wire.setClock(100000);
  Serial.println("I2C scan...");
  i2cScan();
  // Test for rak1906
  uint8_t error = myBus[0x76];
  hasBME280 = false;
  if (error != 0) {
    Serial.println("BME device present!");
    Serial.print("Chip ID=0x");
    uint8_t id = rak1906.chipID();
    // works for both modules
    Serial.println(id, HEX);
    if (id == 0x61) {
      hasBME680 = rak1906.init();
      if (hasBME680) Serial.println("RAK1906 init success!");
      else Serial.println("* RAK1906 startup error!");
    } else if (id == 0x60) {
      hasBME280 = bme280.init();
      if (!hasBME280) {
        Serial.println("* bme280 startup error!");
      } else {
        Serial.println("* bme280 init success!");
      }
    } else {
      Serial.println("BMEx80 init fail!");
    }
  }
}

void loop() {
  if (hasBME680) show680();
  if (hasBME280) show280();
  delay(10000);
}
