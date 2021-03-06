
#include <Wire.h>
#include "Arduino.h"
#include <avr/sleep.h>

//INPUTS

static const unsigned short accPin = 2;
static const unsigned short powerBtnPin = 3;
static const unsigned short rpi3VPin = 7;

static const unsigned short btn1Pin = A1;
static const unsigned short btn2Pin = A2;
static const unsigned short btn3Pin = A3;

static const unsigned short rotAPin = A0;
static const unsigned short rotBPin = 13;


//OUTPUTS

static const unsigned short rpi12VSupplyPin = 4;
static const unsigned short rpiShutdownPin = 5;


static const unsigned short led1Pin = 6;
static const unsigned short beepPin = 8;
static const unsigned short led2Pin = 9;

static const unsigned short ccled1Pin = 12;
static const unsigned short ccled2Pin = 11;
static const unsigned short ccled3Pin = 10;


//DEFINES

enum StateEnum {
  OFF,
  STARTING,
  IDLING,
  STOPPING,
  ON
};

enum relayEnum {
  POWER_OFF,
  POWER_ON
};

StateEnum powerState = OFF;

class AverageSampler {
  int numSamples;
  int SamplingDuration;
  int* SampleArray;
  bool FirstRun;
  int CurrentTime;
  int CurrentIndex;
  int Total;


  public:
    int Average;
    AverageSampler(int samples, int duration) :
      numSamples(samples),
      SamplingDuration(duration)
    {     
    }

  void start() {
    SampleArray = new int[numSamples];
    for (int i = 0; i < numSamples; i++) {
    SampleArray[i] = 0;
    }
    FirstRun = true;
    CurrentTime = 0;
    CurrentIndex = 0;
    Total = 0;
    Average = 0;
    }

  void SetSample(int sample, int timestep){
    if (FirstRun)
    {
      DoFirstRun(sample);
    }
    if (CurrentTime == 0)
    {
      SampleArray[CurrentIndex] = sample;
      CurrentIndex += 1;
  
      // if we're at the end of the array...
      if (CurrentIndex >= numSamples) {
        // ...wrap around to the beginning:
        CurrentIndex = 0;
      }
      //calculate total
      Total = 0;
      for (int i = 0; i < numSamples; i++) {
        Total += SampleArray[i];
      }
      //long average
      Average = int(floor(float(Total) / float(numSamples)));
    }
    CurrentTime += timestep;
    if (CurrentTime > SamplingDuration/numSamples)
    {
      CurrentTime = 0;
    }
    
  }

  
  String GetAverage(){
      float tempTotal = 0.0;
      for (int i = 0; i < numSamples; i++) {
        Total += float(SampleArray[i]);
      }
      //long average
      float tempAverage = float(Total) / float(numSamples);
    
    String temp = String(tempAverage);
    return temp;
  }

  void DoFirstRun(int sample){
    for (int i = 0; i < numSamples; i++) {
    SampleArray[i] = sample;
    }
    FirstRun = false;
  }
};

AverageSampler AccSampler(5, 500);
  

class ButtonTimer {
    unsigned short pin;
    unsigned short m_ledpin;
    unsigned long pushStart;
    unsigned long pushEnd;
    bool timerRunning;
    bool need2Release;
    int beepsdone = 0;
    bool m_hold = false;

  public:
    int pushDuration;
    int m_max;

    ButtonTimer(int p, bool p_hold=false, int p_max = 5000, unsigned short p_ledpin = 0):
      pin(p),m_hold(p_hold), m_max(p_max), m_ledpin(p_ledpin)
    {
      pinMode(pin, INPUT_PULLUP);
      if(m_ledpin != 0){
        pinMode(m_ledpin, OUTPUT);
      }
      pushStart = millis();
      pushEnd = millis();
      timerRunning = false;
      need2Release = false;
    }


    int Check() {
      int reading = digitalRead(pin);
      if(m_ledpin != 0 and powerState == ON){
        digitalWrite(m_ledpin, reading);
      }
      //Serial.println(reading);
      if (need2Release == false) {
        //we haven't already bounced out of max
        if (timerRunning) {
          //timer is running, we are tracking values
          if (reading == LOW) {
            //Button is still pressed while tracking, limit to max and do countdown beeps
            pushEnd = millis();
            pushDuration = pushEnd - pushStart;

            if (m_hold){
              if (pushDuration > m_max) {
                if (beepsdone == 0){
                  tone(beepPin, 2500);
                  delay(50);
                  noTone(beepPin);
                  beepsdone = 1;
                }
                return -1;
              }
              else{
                return 0;
              }
            }
            else{
              int beeps = (floor)(pushDuration / 1000);
              if (beeps > beepsdone) {
                tone(beepPin, (3500 / (m_max / 1000)) * (beeps + 1));
                delay(50);
                noTone(beepPin);
                beepsdone = beeps;
              }
              if (pushDuration > m_max) {
                tone(beepPin, 1000);
                delay(25);
                noTone(beepPin);
                need2Release = true;
                timerRunning = false;
                //Serial.println("log: push ended at max of " + String(pushDuration));
                return pushDuration;
              }
              else{
                return 0;
              }
            }
          }
          else {
            //button is released while tracking, stop counting and return duration
            pushEnd = millis();
            pushDuration = pushEnd - pushStart;
            if(m_hold and pushDuration > m_max){
              timerRunning = false;
              return 0;
            }
            else{
              //Serial.println("log: push ended at " + String(pushDuration));
              timerRunning = false;
              return pushDuration;
            }
          }
        }
        else
        {
          if (reading == LOW) {
            //Button is pressed when not tracking, so start tracking
            pushStart = millis();
            timerRunning = true;
            beepsdone = 0;
          }
          else {
            //button is not pressed, not tracking
            return 0;
          }
        }
      }
      else {
        //we've already maxed out, we can only go back to normal if the button goes high
        if (reading == HIGH) {
          need2Release = false;
        }
        return 0;
      }
    }

};

enum fadeMode {
  UP,
  DOWN,
  BOUNCE,
  ZERO,
  ONE
};

#define SMOOTHSTEP(x) ((x) * (x) * (3 - 2 * (x))) //SMOOTHSTEP expression.

class pwmFader {
    int m_duration;
    int m_min;
    int m_max;
    unsigned long m_end = 0;
    unsigned long m_start = -1;

    fadeMode m_mode = UP;
  public:
    pwmFader(fadeMode p_mode = BOUNCE, int p_duration = 1000, int p_min = 0, int p_max = 255):
      m_mode(p_mode), m_duration(p_duration), m_min(p_min), m_max(p_max) {

    }

    void Change(fadeMode p_mode, int p_duration,  int p_min, int p_max) {
      if (m_mode != p_mode) {
        m_mode = p_mode;
      }
      if (m_duration != p_duration) {
        m_duration = p_duration;
        m_start = -1;
      }
      if (m_min != p_min) {
        m_min = p_min;
        m_start = -1;
      }
      if (m_max != p_max) {
        m_max = p_max;
        m_start = -1;
      }


    }

    void Change(fadeMode p_mode) {
      m_mode = p_mode;
      m_start = -1;
    }

    void Change(fadeMode p_mode, int p_max) {
      m_mode = p_mode;
      m_max = p_max;
      m_start = -1;
    }

    int doFade(unsigned long p_millis) {
      if (m_start <0 or p_millis > m_end) {
        m_start = p_millis;
        m_end = m_start + m_duration;
      }

      int result = 0;
      float l_current = (float(p_millis) - float(m_start)) / float(m_duration);
      switch (m_mode) {
        case (UP):
          result = float(m_min) + (SMOOTHSTEP(l_current) * (float(m_max) - float(m_min)));
          break;
        case (DOWN):
          result = float(m_min) + (SMOOTHSTEP(1 - l_current) * (float(m_max) - float(m_min)));
          break;
        case (BOUNCE):
          result = float(m_min) + (SMOOTHSTEP(abs((l_current * 2) - 1)) * (float(m_max) - float(m_min)));
          break;
        case (ZERO):
          result = m_min;
          break;
        case (ONE):
          result = m_max;
          break;
      }
      return result;
    }
};



//variables



ButtonTimer powerButtonTimer(powerBtnPin);

ButtonTimer btn1Timer(btn1Pin, false, 4000, ccled1Pin);
ButtonTimer btn2Timer(btn2Pin, true , 1000, ccled2Pin);
ButtonTimer btn3Timer(btn3Pin, true , 1000, ccled3Pin);

//checkvars
bool acc_On = false;
bool rpi_On = false;
bool rpi_Supply = false;

//Chimes
static const int singleChime = 0;
static const int startupChime = 1;
static const int shutdownChime = 2;
static const int resetChime = 3;
static const int readyChime = 4;
static const int doneChime = 5;

void playTone(int chime) {
  switch (chime) {
    case 0:
      tone(beepPin, 2300);
      delay(50);
      noTone(beepPin);
      delay(15);
      tone(beepPin, 2300);
      delay(50);
      noTone(beepPin);
      break;
    case 1:
      tone(beepPin, 1900);
      delay(100);
      noTone(beepPin);
      delay(10);
      tone(beepPin, 1900);
      delay(100);
      noTone(beepPin);
      delay(10);
      tone(beepPin, 2200);
      delay(100);
      noTone(beepPin);
      delay(10);
      tone(beepPin, 2500);
      delay(100);
      noTone(beepPin);
      break;
    case 2:
      tone(beepPin, 2500);
      delay(100);
      noTone(beepPin);
      delay(10);
      tone(beepPin, 3000);
      delay(100);
      noTone(beepPin);
      delay(10);
      tone(beepPin, 3500);
      delay(100);
      noTone(beepPin);
      delay(10);
      break;
    case 3:
      tone(beepPin, 1500);
      delay(80);
      noTone(beepPin);
      delay(10);
      tone(beepPin, 1500);
      delay(80);
      noTone(beepPin);
      delay(10);
      tone(beepPin, 2500);
      delay(120);
      noTone(beepPin);
      delay(10);
      break;
    case 4:
      tone(beepPin, 2000);
      delay(80);
      noTone(beepPin);
      delay(20);
      tone(beepPin, 3000);
      delay(70);
      noTone(beepPin);
      break;
    case 5:
      tone(beepPin, 3000);
      delay(80);
      noTone(beepPin);
      delay(20);
      tone(beepPin, 2000);
      delay(70);
      noTone(beepPin);
      break;

  }
}

void initiateShutdown() {
  Serial.println("log: Shutting down...");
  digitalWrite(rpiShutdownPin, LOW);
  playTone(shutdownChime);
  powerState = STOPPING;
}

void initiateStartup() {
  if (powerState == OFF) {
        Serial.println("log: Starting from OFF");
        playTone(startupChime);
        setPowerState(POWER_ON);
        powerState = STARTING;
  }
}

unsigned long sleepCounter = 0;

void ResetSleepCounter() {
  sleepCounter = 0;
}

void goToSleep() {
    setPowerState(POWER_OFF);
    // Disable the ADC (Analog to digital converter, pins A0 [14] to A5 [19])
    static byte prevADCSRA = ADCSRA;
    ADCSRA = 0;

    /* Set the type of sleep mode we want. Can be one of (in order of power saving):
        SLEEP_MODE_IDLE (Timer 0 will wake up every millisecond to keep millis running)
        SLEEP_MODE_ADC
        SLEEP_MODE_PWR_SAVE (TIMER 2 keeps running)
        SLEEP_MODE_EXT_STANDBY
        SLEEP_MODE_STANDBY (Oscillator keeps running, makes for faster wake-up)
        SLEEP_MODE_PWR_DOWN (Deep sleep)
    */
    set_sleep_mode (SLEEP_MODE_PWR_DOWN);
    sleep_enable();

    // Turn of Brown Out Detection (low voltage)
    // Thanks to Nick Gammon for how to do this (temporarily) in software rather than
    // permanently using an avrdude command line.
    //
    // Note: Microchip state: BODS and BODSE only available for picoPower devices ATmega48PA/88PA/168PA/328P
    //
    // BODS must be set to one and BODSE must be set to zero within four clock cycles. This sets
    // the MCU Control Register (MCUCR)
    MCUCR = bit (BODS) | bit (BODSE);

    // The BODS bit is automatically cleared after three clock cycles so we better get on with it
    MCUCR = bit (BODS);

    // Ensure we can wake up again by first disabling interupts (temporarily) so
    // the wakeISR does not run before we are asleep and then prevent interrupts,
    // and then defining the ISR (Interrupt Service Routine) to run when poked awake
    noInterrupts();
    attachInterrupt(digitalPinToInterrupt(powerBtnPin), sleepISR, LOW);
    attachInterrupt(digitalPinToInterrupt(accPin), sleepISR, HIGH);

    // Send a message just to show we are about to sleep
    Serial.println("log: Going to sleep.");
    Serial.flush();

    // Allow interrupts now
    interrupts();

    // And enter sleep mode as set above
    sleep_cpu();
}

void sleepISR() {
  // Prevent sleep mode, so we don't enter it again, except deliberately, by code
  sleep_disable();

  // Detach the interrupt that brought us out of sleep
  detachInterrupt(digitalPinToInterrupt(powerBtnPin));
  detachInterrupt(digitalPinToInterrupt(accPin));
  ResetSleepCounter();
  // Now we continue running the main Loop() just after we went to sleep
}

unsigned long lastAccBroadcast = millis();
unsigned long lastAccCheck = millis();
unsigned long shutOffTime = millis();

void checkStates() {
  //check input pins

  unsigned long delta = millis() - lastAccCheck;
  
  int accvalue = digitalRead(accPin);
  AccSampler.SetSample(accvalue,delta);
  lastAccCheck = millis();
  if (AccSampler.Average == HIGH)
  {
    if(acc_On == false){
      Serial.println("power:acc_on");
      acc_On = true;
      initiateStartup();
    } 
  }
  else
  {
    if(acc_On == true){
      Serial.println("power:acc_off");
      acc_On = false;
    }
  }

  //broadcast ACC status every 10 seconds
  if ((millis() - lastAccBroadcast) >= 30000){
    if(acc_On){
      Serial.println("power:acc_on");
    }
    else{
      Serial.println("power:acc_off");
    }
    lastAccBroadcast = millis();
  }

  int shutoffDelta = millis()-shutOffTime;

  if (digitalRead(rpi3VPin) == LOW)
  {
    if (rpi_On == false) {
      rpi_On = true;
      powerState = ON;
      playTone(readyChime);
      Serial.println("log: Raspberry Pi started!");
    }
  }
  else if (digitalRead(rpi3VPin) == HIGH) {
    if (rpi_On == true) {
      rpi_On = false;
      if (powerState == STOPPING){
        powerState = IDLING;
        shutOffTime = millis();
      }
      else{
        powerState = OFF;
      }
      ResetSleepCounter();
      //digitalWrite(rpiShutdownPin, HIGH);
      playTone(doneChime);
      Serial.println("log: Raspberry Pi stopped!");
    }
    else if (rpi_On == false and shutoffDelta > 5000 and powerState == IDLING) {
      powerState = OFF;
      shutoffDelta = 0;
      setPowerState(POWER_OFF);
      Serial.println("log: Rpi Power switched off after shutdown.");
    }
    
  }

}


pwmFader led1Fader = pwmFader();
pwmFader led2Fader = pwmFader();

void doButtonLEDs(){
    if (powerState != ON) {
      digitalWrite(ccled1Pin, LOW);
      digitalWrite(ccled2Pin, LOW);
      digitalWrite(ccled3Pin, LOW);
    }
}

void doPowerLEDs() {

  switch (powerState) {
    case OFF:
      led1Fader.Change(BOUNCE, 2000, 0, 16);
      analogWrite(led1Pin, led1Fader.doFade(millis()) );
      led2Fader.Change(ZERO);
      analogWrite(led2Pin, led2Fader.doFade(millis()) );
      break;
    case STARTING:
      led1Fader.Change(ONE, 32);
      analogWrite(led1Pin, led1Fader.doFade(millis()) );
      led2Fader.Change(UP, 1000, 0, 64);
      analogWrite(led2Pin, led2Fader.doFade(millis()) );
      break;
    case IDLING:
      led1Fader.Change(BOUNCE, 2000, 0, 32);
      analogWrite(led1Pin, led1Fader.doFade(millis()) );
      led2Fader.Change(BOUNCE, 2000, 0, 16);
      analogWrite(led2Pin, led2Fader.doFade(millis()) );
      break;
    case STOPPING:
      led1Fader.Change(ONE, 64);
      analogWrite(led1Pin, led1Fader.doFade(millis()) );
      led2Fader.Change(DOWN, 1000, 0, 64);
      analogWrite(led2Pin, led2Fader.doFade(millis()) );
      break;
    case ON:
      led1Fader.Change(ONE, 32);
      analogWrite(led1Pin, led1Fader.doFade(millis()) );
      led2Fader.Change(ONE, 32);
      analogWrite(led2Pin, led2Fader.doFade(millis()) );
      break;
  }

}



void setPowerState(relayEnum p_state) {
  switch (p_state) {
    case (POWER_OFF):
      digitalWrite(rpi12VSupplyPin, LOW);
      break;
    case (POWER_ON):
      digitalWrite(rpi12VSupplyPin, LOW);
      delay(250);
      digitalWrite(rpi12VSupplyPin, HIGH);
      digitalWrite(rpiShutdownPin, HIGH);
      break;
  }
}

void doHardReset() {
  Serial.println("log: Doing hard reset");
  playTone(resetChime);
  setPowerState(POWER_OFF);
  powerState = OFF;
  rpi_On = false;
  delay(200);
  //digitalWrite(selfResetPin, LOW); //this doesnt seem to work.. prevents boot up of arduino!
}

void(* resetFunc) (void) = 0;

void interpretPowerButtonPush(int p_push) {
  if (p_push > 90) {
    //Serial.println("log: push registered at " + String(p_push));
    if (p_push > powerButtonTimer.m_max and powerState != OFF) {
      //hard reboot
      doHardReset();
    }
    else if (p_push > 3000 and powerState == ON) {
      initiateShutdown();
    }
    else if (p_push > 2000 and powerState == ON) {
      Serial.println("power:standby");
    }
    else if (p_push > 1000 and powerState == ON) {
      Serial.println("power:togglescreen");
    }
    
    else if (p_push > 100 and p_push < 5000) {
      initiateStartup();
      if (powerState == ON and p_push < 300) {
        //Serial.println("log: sending Mute signal");
        Serial.println("media:togglemute");
      }
    }
    else if (powerState == IDLING) {
      return;
    }

  }
}
int encoderPinALast = 0;

void encoderTrack() {
  int n = digitalRead(rotAPin);
  if ((encoderPinALast == LOW) && (n == HIGH)) {
    if (digitalRead(rotBPin) == LOW) {
      Serial.println("media:rot-");
    } else {
      Serial.println("media:rot+");
    }
  }
  encoderPinALast = n;
}

void checkTriButtons(int p_btn1, int p_btn2, int p_btn3){
  if(p_btn1 != 0){
    //Serial.println("log: " + Stringp_btn1);
    if(p_btn1 > btn1Timer.m_max)
    {
      Serial.println("media:clearlist"); 
    }
    else if (p_btn1 > 2000)
    {
      Serial.println("media:togglestop");
    }
    else
    {
      Serial.println("media:toggleplay");
    }
  }

  if(p_btn2 != 0){
    if (p_btn2 > 0)
    {
      Serial.println("media:next");
    }
    else
    {
      Serial.println("media:skipfwd");
    }
  }

  if(p_btn3 != 0){
    if (p_btn3 > 0)
    {
      Serial.println("media:prev");
    }
    else
    {
      Serial.println("media:skipbwd");
    }
  }
}

void setup() {

  Serial.begin(9600);

  pinMode(accPin, INPUT_PULLUP);
  pinMode(powerBtnPin, INPUT_PULLUP);
  pinMode(rpi3VPin, INPUT_PULLUP);

  pinMode(btn1Pin, INPUT_PULLUP);
  pinMode(btn2Pin, INPUT_PULLUP);
  pinMode(btn3Pin, INPUT_PULLUP);

  pinMode(rotAPin, INPUT_PULLUP);
  pinMode(rotBPin, INPUT_PULLUP);

  pinMode(rpi12VSupplyPin, OUTPUT);
  pinMode(rpiShutdownPin, OUTPUT);

  pinMode(led1Pin, OUTPUT);
  pinMode(led2Pin, OUTPUT);

  pinMode(beepPin, OUTPUT);

  pinMode(ccled1Pin, OUTPUT);
  pinMode(ccled2Pin, OUTPUT);
  pinMode(ccled3Pin, OUTPUT);

  playTone(singleChime);

  digitalWrite(rpiShutdownPin, HIGH);
  digitalWrite(rpi12VSupplyPin, LOW);

  AccSampler.start();
}

unsigned long lastTime = 0;

void CheckForSleep(){
  if(powerState == OFF){
    int d = millis() - lastTime;
    sleepCounter += d;
    if(sleepCounter > 60000){
      goToSleep();
    }
  }
  lastTime = millis();
}
  
void loop() {
  checkStates();
  encoderTrack();
  checkTriButtons(btn1Timer.Check(),btn2Timer.Check(),btn3Timer.Check());
  interpretPowerButtonPush(powerButtonTimer.Check());
  doPowerLEDs();
  doButtonLEDs();
  CheckForSleep();
}
