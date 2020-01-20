/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution

 ClosedCube MAX30205 Human Body Temperature Sensor
 Written by AA for ClosedCube
 MIT License
*********************************************************************/
#include <bluefruit.h>

#include "IEEE11073float.h"

#include <Wire.h>

#include "ClosedCube_MAX30205.h"

#include "MAX30105.h"

MAX30105 ppgSensor;

/* HRM Service Definitions
 * Heart Rate Monitor Service:  0x180D
 * Health Thermometer Service: 0x1809
 * Heart Rate Measurement Char: 0x2A37
 * Body Sensor Location Char:   0x2A38
 * Temperature Measurement Characteristic: 0x2A1C
 * Temperature Type Characteristic: 0x2A1D
 */
BLEService        hrms = BLEService(UUID16_SVC_HEART_RATE);
BLEService        htms = BLEService(UUID16_SVC_HEALTH_THERMOMETER);
BLECharacteristic hrmc = BLECharacteristic(UUID16_CHR_HEART_RATE_MEASUREMENT);
BLECharacteristic bslc = BLECharacteristic(UUID16_CHR_BODY_SENSOR_LOCATION);
BLECharacteristic htmc = BLECharacteristic(UUID16_CHR_TEMPERATURE_MEASUREMENT);

BLEDis bledis;    // DIS (Device Information Service) helper class instance
BLEBas blebas;    // BAS (Battery Service) helper class instance

//  Heart rate
uint8_t bps = 0;

//  LiPo Voltage
int vBatPin = A6;
int rawValue = 0;
float mvPerLSB = 3600.0F/1024.0F; // 10-bit ADC with 4.2V input range
uint8_t vBatPer = 0;

//  Body Temperature
ClosedCube_MAX30205 max30205;
float tempValue = 0;

//  PPG Sensor
//Setup to sense a nice looking saw tooth on the plotter
byte ledBrightness = 0x1F; //Options: 0=Off to 255=50mA
byte sampleAverage = 8; //Options: 1, 2, 4, 8, 16, 32
byte ledMode = 3; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
int sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
int pulseWidth = 411; //Options: 69, 118, 215, 411
int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

void setup()
{
  Serial.begin(115200);
//  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  // Initialize the Temperature Sensor  
  initTempSensor(); //  Init Max30205 Temperature

  // Initialize PPG Sensor
  initPPG();
  setupPPG();

  Serial.println("Open Health Band");
  Serial.println("-----------------------\n");

  // Initialise the Bluefruit module
  Serial.println("Initialise Open Health Band");
  Bluefruit.begin();

  // Set the advertised device name (keep it short!)
  Serial.println("Setting Device Name to 'Open Health Band'");
  Bluefruit.setName("Open Health Band");

  // Set the connect/disconnect callback handlers
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // Configure and Start the Device Information Service
  Serial.println("Configuring the Device Information Service");
  bledis.setManufacturer("CRI");
  bledis.setModel("Open Health Band v0.2.5");
  bledis.begin();

  // Configure and Start the Battery Service
  Serial.println("Configuring the Battery Service");
  blebas.begin();

  // Setup the Heart Rate Monitor service using
  // BLEService and BLECharacteristic classes
  Serial.println("Configuring the Heart Rate Monitor Service");
  setupHRM();

  Serial.println("Configuring the Temperature Measurement Service");
  setupHTM();

  // Setup the advertising packet(s)
  Serial.println("Setting up the advertising payload(s)");
  startAdv();

  Serial.println("Ready Player One!!!");
  Serial.println("\nAdvertising");
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include HRM Service UUID
  Bluefruit.Advertising.addService(hrms);

  // Include Name
  Bluefruit.Advertising.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
  Serial.println("Advertising!");
}

void cccd_callback(uint16_t conn_hdl, BLECharacteristic* chr, uint16_t cccd_value)
{
    // Display the raw request packet
    Serial.print("CCCD Updated: ");
    //Serial.printBuffer(request->data, request->len);
    Serial.print(cccd_value);
    Serial.println("");

    // Check the characteristic this CCCD update is associated with in case
    // this handler is used for multiple CCCD records.
    if (chr->uuid == hrmc.uuid) {
        if (chr->notifyEnabled(conn_hdl)) {
            Serial.println("Heart Rate Measurement 'Notify' enabled");
        } else {
            Serial.println("Heart Rate Measurement 'Notify' disabled");
        }
    }
}

void loop()
{
//  digitalToggle(LED_RED);
  
  if ( Bluefruit.connected() ) {
    uint8_t hrmdata[2] = { 0b00000110, bps };           // Sensor connected, increment BPS value
    uint8_t htmdata[6] = { 0b00000100, 0,0,0,0, 2 }; // Fahrenheit unit, temperature type = body (2)
    
    // Read Battery Voltage
    getVoltage();
    blebas.write(vBatPer);
    Serial.println(vBatPer);
  
    // Note: We use .notify instead of .write!
    // If it is connected but CCCD is not enabled
    // The characteristic's value is still updated although notification is not sent

    // Heart Rate Measurement
    getPPG();
    if ( hrmc.notify(hrmdata, sizeof(hrmdata)) ){
      Serial.print("Heart Rate Measurement updated to: ");
      Serial.println(bps); 
    }else{
      Serial.println("ERROR: Notify not set in the CCCD or not connected!");
    }

    // Body Temperature Measurement
    getTemp();
    float2IEEE11073(tempValue, &htmdata[1]);
    if ( htmc.indicate(htmdata, sizeof(htmdata)) ){
      Serial.print("Temperature Measurement updated to: ");
      Serial.println(tempValue); 
    }else{
      Serial.println("ERROR: Indicate not set in the CCCD or not connected!");
    }
  }

  // Only send update once per second
  delay(10);
}
