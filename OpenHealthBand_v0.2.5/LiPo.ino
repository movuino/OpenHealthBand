void getVoltage(){
  rawValue = analogRead(vBatPin);
  delay(10);
  vBatPer = mvToPercent(rawValue * 2.0F * mvPerLSB); //  2.0F is the compensation factor for the VBAT divider
  delay(10);
}

uint8_t mvToPercent(float mvolts) {
  if(mvolts<3300)
    return 0;

  if(mvolts <3600) {
    mvolts -= 3300;
    return mvolts/30;
  }

  mvolts -= 3600;
  return 10 + (mvolts * 0.15F );  // thats mvolts /6.66666666
}
