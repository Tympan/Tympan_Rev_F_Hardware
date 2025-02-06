
// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This code controls the LED that is under the control of the nRF52840.
//
// MIT License.  Use at your own risk.
//
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#define LED_0 14  // red
#define LED_1 12  // blue
#define LED_2 15  // green

// LED fade stuff
#define FADE_DELAY_SLOW 50
#define FADE_DELAY_FAST 5
#define FADE_RATE 8
#define FADE_MAX 127
#define FADE_MIN 255

class LED_controller {
  public:
    LED_controller() {
      setupLEDs();
    };
    const int red = LED_0;
    const int blue = LED_1;
    const int green = LED_2;
    unsigned int fadeDelay = FADE_DELAY_SLOW;
    int fadeRate = 0 - FADE_RATE;
    int fadeValue = FADE_MIN;
    int ledToFade = blue;
    unsigned long lastShowTime;
    
    void setupLEDs(void) { for(int i=0; i<3; i++)  pinMode(ledPin[i],OUTPUT); }
    void disableLEDs(void) { ledToFade = 0; LEDsOff(); }
    void LEDsOff(void ){ 
      fadeValue = FADE_MIN; // keep in bounds
      for(int i=0; i<3; i++) analogWrite(ledPin[i],fadeValue);
    }

    void setLedColor(int color_ind) {
      if (color_ind != ledToFade) LEDsOff();
      if ((color_ind == red) || (color_ind == blue) || (color_ind == green)) {
        ledToFade = color_ind;
        fadeDelay = FADE_DELAY_SLOW;
        lastShowTime = millis();
      }
    }

    void showRGB_LED(unsigned long m) {
      if (m < lastShowTime) lastShowTime = 0; //handle the case when time wraps around
      if ((ledToFade != red) && (ledToFade != green) && (ledToFade != blue)) return; //leds are disabled

      if(m > lastShowTime + fadeDelay) { //is it time to update?
        lastShowTime = m; // keep time

        fadeValue += fadeRate;  // adjust brightness
        if(fadeValue >= FADE_MIN){ // LEDs are Common Anode
          fadeValue = FADE_MIN; // keep in bounds
          fadeRate = 0 - FADE_RATE; // change direction
        } else if(fadeValue <= FADE_MAX){ // LEDs are Common Anode
          fadeValue = FADE_MAX; // keep in bounds
          fadeRate = 0 + FADE_RATE; // change direction
        }
        analogWrite(ledToFade,fadeValue);
      }
    }

  private:
    int ledPin[3] = {LED_0, LED_1, LED_2}; // red, blue, green
  
};




#endif
