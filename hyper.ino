#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <max6675.h>

byte BOILER_100W = 2;
byte BOILER_15W = 3;
byte COMPRESSOR = 7;
byte HEATER_SAFETY_RELAY = 8;
byte HEATER_SSR = 9;

byte thermoDO = 4;
byte thermoCS = 5;
byte thermoCLK = 6;

byte start_sw_state = 0;
byte start_sw_state_last = 0;
byte lim_sw_state = 0;

int temp_dial_value;
int temp_error;
int pwm_value;
int P_gain = 5;

double boiler_temp = 0;
double boiler_temp_array[] = {0,0,0,0,0,0,0,0,0,0};
double boiler_temp_average = 0; 

byte boiler_temp_index = 0;

double heater_temp;
int heater_max = 500;
int boiler_max = 130; 

long start_sw_hold_time = 0;
long start_sw_trigger_timestamp;
long runtime = 0;
int timeout_time = 21600; //6 hours

LiquidCrystal_I2C lcd(0x27,20,4);
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

void setup() 
{
 pinMode(BOILER_100W, OUTPUT); 
 pinMode(BOILER_15W, OUTPUT); 
 pinMode(COMPRESSOR, OUTPUT);
 pinMode(HEATER_SAFETY_RELAY, OUTPUT);
 pinMode(HEATER_SSR, OUTPUT);

 pinMode(10, INPUT_PULLUP); //Program start pushbutton
 pinMode(11, INPUT_PULLUP); //Gas Syringe Limit Switch

 Serial.begin(9600);

 lcd.init(); 
 lcd.backlight();
 lcd.setCursor(0,0);

 delay(300);
}

void loop()
{
  temp_dial_value = analogRead(A1);
  start_sw_state = 1-digitalRead(11);

  if(start_sw_state == 1 && start_sw_state_last == 0)
  {
    start_sw_trigger_timestamp = millis();    
  }

  if(start_sw_state == 1 && start_sw_state_last == 1)
  {
    start_sw_hold_time = millis() - start_sw_trigger_timestamp;
  }




  if(start_sw_hold_time > 1000)
  {
    digitalWrite(HEATER_SAFETY_RELAY, HIGH);
    digitalWrite(BOILER_100W, HIGH);
    digitalWrite(BOILER_15W, HIGH);
    
    while(1)
    {
      runtime = millis() - start_sw_trigger_timestamp;
      runtime = runtime/1000;
      lim_sw_state = 1-digitalRead(10);
      temp_dial_value = analogRead(A1);
      heater_temp = thermocouple.readCelsius();
      boiler_temp = analogRead(A0); 
      boiler_temp = log(boiler_temp)*(-33.27)+233.7;

      if(boiler_temp_index < 10) //10 period moving average for temperature 
      {
        boiler_temp_array[boiler_temp_index] = boiler_temp;
        boiler_temp_index++;
      }
      else
      {
        for(int i = 0; i<10; i++)
        {
          boiler_temp_array[i] = boiler_temp_array[i+1];
          boiler_temp_average = boiler_temp_average+boiler_temp_array[i];
        }
        boiler_temp_array[9] = boiler_temp;
      }

      boiler_temp_average = 0;
      
      for(int i =0; i<9; i++)
      {
        boiler_temp_average = boiler_temp_average + boiler_temp_array[i];
      }
      boiler_temp_average = boiler_temp_average / 9;
      Serial.print(boiler_temp_average);
      Serial.print(" ");
      Serial.println(boiler_temp);


      temp_dial_value = temp_dial_value / 2;
      temp_error = temp_dial_value - heater_temp;
    
      if(temp_error < 0)
      {
        temp_error = 0;
      }
      
      lcd.setCursor(0,0);
      lcd.print("Set Temp: ");
      lcd.print(temp_dial_value);
      lcd.setCursor(0,1);
      lcd.print("Pipe Temp: ");
      lcd.print(heater_temp);
      lcd.setCursor(0,2);
      lcd.print("Boiler Temp: ");
      lcd.print(boiler_temp_average);
      lcd.setCursor(0,3);
      lcd.print(runtime);
      
      if(temp_error > 0)
      {
        pwm_value = (temp_error+25)*P_gain;
      }
      else
      {
        pwm_value = 0;
      }
      if(pwm_value > 255)
      {
        pwm_value = 255;
      }

      if(boiler_temp_average >= 65) //Shut off 100W jump-start heater when boiling point is reached (this may change based on ethanol conentration)
      {
        digitalWrite(BOILER_100W, LOW);
      }
      else
      {
        digitalWrite(BOILER_100W, HIGH);
      }

      if(lim_sw_state == 1)
      {
        digitalWrite(HEATER_SSR, LOW); //Turn off heater because compressor inrush current + heater max current might trip a breaker
        delay(100);
        digitalWrite(COMPRESSOR, HIGH);
        delay(2500);
        digitalWrite(COMPRESSOR, LOW);
      }
      
      analogWrite(HEATER_SSR, pwm_value);  


      if(runtime > timeout_time || heater_temp > heater_max || boiler_temp_average > boiler_max) 
      {
        digitalWrite(HEATER_SSR, LOW);
        digitalWrite(HEATER_SAFETY_RELAY, LOW);
        digitalWrite(COMPRESSOR, LOW);
        digitalWrite(BOILER_100W, LOW);
        digitalWrite(BOILER_15W, LOW);
        if(runtime > timeout_time)
        {
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("TIMEOUT AT 6 HOURS");
        }
        if(heater_temp > heater_max)
        {
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("HEATER OVERTEMP");
        }
        if(boiler_temp_average > boiler_max)
        {
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("BOILER OVERTEMP");
        }       

        while(1)
        {
          lim_sw_state = 1-digitalRead(10);
          if(lim_sw_state == 1)
          {
            digitalWrite(COMPRESSOR, HIGH);
            delay(1000);
            digitalWrite(COMPRESSOR, LOW);
          }
          delay(1000);
        }
      }


      delay(1000);
      lcd.clear();
    }
  }








  //░░░░░░░░░░░░░░░░░░░░░ MAIN LOOP WHEN NOTHING ELSE IS HAPPENING ░░░░░░░░░░░░░░░░░░░░░░░░
  heater_temp = thermocouple.readCelsius();
  boiler_temp = analogRead(A0); 
  boiler_temp = log(boiler_temp)*(-33.27)+233.7;
  temp_dial_value = temp_dial_value / 2;
  lcd.setCursor(0,0);
  lcd.print("Set Temp: ");
  lcd.print(temp_dial_value);
  lcd.setCursor(0,1);
  lcd.print("Pipe Temp: ");
  lcd.print(heater_temp);
  lcd.setCursor(0,2);
  lcd.print("Boiler Temp: ");
  lcd.print(boiler_temp);
  lcd.setCursor(0,3);
  lcd.print("Press START to run");

  
  start_sw_state_last = start_sw_state;
  delay(1000);
  lcd.clear();
}
