void setupHTM(void)
{
  // Configure the Health Thermometer service
  // See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.health_thermometer.xml
  // Supported Characteristics:
  // Name                         UUID    Requirement Properties
  // ---------------------------- ------  ----------- ----------
  // Temperature Measurement      0x2A1C  Mandatory   Indicate
  //
  // Temperature Type             0x2A1D  Optional    Read                  <-- Not used here
  // Intermediate Temperature     0x2A1E  Optional    Read, Notify          <-- Not used here
  // Measurement Interval         0x2A21  Optional    Read, Write, Indicate <-- Not used here
  htms.begin();

  htmc.setProperties(CHR_PROPS_INDICATE);
  htmc.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  htmc.setFixedLen(6);
  htmc.setCccdWriteCallback(cccd_callback);  // Optionally capture CCCD updates
  htmc.begin();
  uint8_t htmdata[6] = { 0b00000100, 0, 0 ,0 ,0, 2 }; // Set the characteristic to use Fahrenheit, with type (body) but no timestamp field
  htmc.write(htmdata, sizeof(htmdata)); // Use .write for init data
}

void initTempSensor(){
  max30205.begin(0x48);
//  if(!max30205.begin(0x48)){
//    Serial.println("MAX30205 was not found. Please check wiring/power... ");
//    while (1);
//  }
}

void getTemp(){
  tempValue = max30205.readTemperature();
}
