/*
 * Pico OpenFlexure Controller
 * 
 * Based on Sangaboard firmware ported to Raspberry Pi Pico (RP2040)
 * 
 * This firmware was written by
 * Richard Bowman
 * Julian Stirling
 * Boyko Vodenicharski
 * Filip Ayazi
 * 
 * Much of the code is based on older code written by
 * James Sharkey and Fergus Riche
 * 
 * Ported to RP2040 by adapting original Sangaboard code
 * 
 * Released under GPL v3, 2017
 */

#include <Arduino.h>
#include "StepperPico.h"
#include <assert.h>
#include <limits.h>

//debug serial prints
#define DEBUG_ON

#ifdef DEBUG_ON
  #define D(x)  Serial.println (x)
#else
  #define D(x)
#endif

#define BOARD_STRING "Pico OpenFlexure v1.0"
#define VER_STRING "Pico OpenFlexure Firmware v1.0.0"

#define EACH_MOTOR for(int i=0; i<n_motors; i++)

// The array below has 3 stepper objects, for X,Y,Z respectively
const int n_motors = 3;
long min_step_delay = 1000;
long ramp_time = 0;

// Stepper motor pin definitions for 28BYJ-48
const int motor0_pins[] = {2, 3, 4, 5};    // X-axis
const int motor1_pins[] = {6, 7, 8, 9};    // Y-axis  
const int motor2_pins[] = {10, 11, 12, 13}; // Z-axis

StepperPico* motors[n_motors];
signed long current_pos[n_motors];
long steps_remaining[n_motors];

bool test_mode = false;

// We'll use this input buffer for serial comms
const int INPUT_BUFFER_LENGTH = 64;
char input_buffer[INPUT_BUFFER_LENGTH];

int command_prefix(String command, const char ** prefixes, int n_prefixes){
  // Check if the command starts with any of the prefixes in the list.
  // returns the index if so, otherwise returns -1
  for(int i=0; i<n_prefixes; i++){
    if(command.startsWith(prefixes[i])){
      return i;
    }
  }
  return -1;
}

void setup() {
  // initialise serial port
  Serial.begin(115200);
  while (! Serial )
    delay(1);
    
  // Initialize stepper motors
  motors[0] = new StepperPico(motor0_pins);
  motors[1] = new StepperPico(motor1_pins);
  motors[2] = new StepperPico(motor2_pins);
  
  EACH_MOTOR{
    motors[i]->setSpeed(10); // as a default set to 10 rpm, though this is ignored...
    steps_remaining[i] = 0;
    current_pos[i] = 0; // reset on power cycle
  }

  Serial.println(F(VER_STRING));
}

void stepMotor(int motor, long dx){
  //make a single step of a single motor.
  current_pos[motor] += dx;
  motors[motor]->stepMotor(((current_pos[motor] % 8) + 8) % 8); //forgive the double-modulo; I need 0-7 even for -ve numbers
}

void releaseMotor(int motor){
  //release the stepper motor (de-energise the coils)
  motors[motor]->stepMotor(8);
}

void print_position(){
  EACH_MOTOR{
    if(i>0) Serial.print(" ");
    Serial.print(current_pos[i]);
  }
  Serial.println();
}

int move_axes(long displ[n_motors]){
   long displacement[n_motors];
  // move all the axes in a nice move
  // split displacements into magnitude and direction, and find max. travel
    int ret=0;
    long max_steps = 0;
    long dir[n_motors];
    EACH_MOTOR{
      dir[i] = displ[i] > 0 ? 1 : -1;
      displacement [i] = displ[i]*dir[i];
      if(displacement[i]>max_steps) max_steps=displacement[i];
    }
    // scale the step delays so the move goes in a straight line, with >=1 motor
    // running at max. speed.
    float step_delay[n_motors];
    EACH_MOTOR if(displacement[i]>0){
      step_delay[i] = float(max_steps)/float(displacement[i])*float(min_step_delay);
    }else{
      step_delay[i] = 9999999999;
    }
   
  // actually make the move
    bool finished = false;
    long distance_moved[n_motors];
    EACH_MOTOR distance_moved[i] = 0;
    unsigned long start = micros();
    float final_scaled_t = (float) max_steps * min_step_delay; //NB total time taken will be final_scaled_t + 2*ramp_time
    while(!finished){
      unsigned long now=micros();
      float elapsed_t;
      if(now<start) //overflow in micros() after ~70 min
        elapsed_t=(float) (now+(ULONG_MAX-start));
      else
        elapsed_t = (float) (now-start);

      float scaled_t; //scale time to allow for acceleration
      if(ramp_time > 0){
        // if a ramp time is specified, accelerate at a constant acceleration for the
        // ramp time, then move at constant (maximum) speed, then decelerate.  If the
        // move is shorter than 2*ramp_time, accelerate then decelerate.
        float remaining_t = final_scaled_t + ramp_time - elapsed_t;
        if(elapsed_t < ramp_time && remaining_t > elapsed_t){ //for the first ramp_time, gradually accelerate
          scaled_t = elapsed_t*elapsed_t/(2*ramp_time);
        }else if(remaining_t < ramp_time){
          scaled_t = final_scaled_t - remaining_t*remaining_t/(2*ramp_time);
        }else{
          scaled_t = elapsed_t - ramp_time/2;
        }
      }else{
        scaled_t = elapsed_t;
      }
      finished = true;
      EACH_MOTOR{
        if(distance_moved[i] < displacement[i]){
          finished = false; //only if all axes are done are we truly finished.

          // check if it's time to take another step and move if needed.
          if(scaled_t > ((float)distance_moved[i] + 0.5) * step_delay[i]){
            stepMotor(i, dir[i]);
            distance_moved[i]++;
          }
        }
      }
      //delayMicroseconds(2000);
    }
    return ret;
}

void loop() {
  // wait for a serial command and read it
  int received_bytes = Serial.readBytesUntil('\n',input_buffer,INPUT_BUFFER_LENGTH-1);
  if(received_bytes > 0){
    input_buffer[received_bytes] = '\0';
    String command = String(input_buffer);
//   Serial.println("Got command: <"+command+">");

    const char* single_axis_moves[3] = {"mrx ", "mry ", "mrz "};
    int axis = command_prefix(command, single_axis_moves, 3);
    if(axis >= 0){
      int preceding_space = command.indexOf(' ',0);
      if(preceding_space <= 0) Serial.println("Bad command.");
      long n_steps = command.substring(preceding_space+1).toInt();
      long displacement[n_motors];
      EACH_MOTOR displacement[i]=0;
      displacement[axis]=n_steps;
      move_axes(displacement);
      Serial.println("done");
      return;
    }

    if(command.startsWith("list_modules")){//list available modules
      Serial.println("--END--");
      return;
    }

    if(command.startsWith("move_rel ") or command.startsWith("mr ")){//relative move
      int preceding_space = -1;
      long displacement[n_motors];
      EACH_MOTOR{ //read three integers and store in steps_remaining
        preceding_space = command.indexOf(' ',preceding_space+1);
        if(preceding_space<0){
          Serial.println(F("Error: command is mr <int> <int> <int>"));
          break;
        }
        displacement[i] = command.substring(preceding_space+1).toInt();
      }
      move_axes(displacement);
      Serial.println("done.");
      return;
    }

    if(command.startsWith("release")){ //release steppers (de-energise coils)
      EACH_MOTOR releaseMotor(i);
      Serial.println("motors released");
      return;
    }
    if(command.startsWith("p?") or command.startsWith("position?")){
      print_position();
      return;
    }
    if(command.startsWith("ramp_time ")){
      int preceding_space = command.indexOf(' ',0);
      if(preceding_space <= 0){
        Serial.println("Bad command.");
        return;
      }
      ramp_time = command.substring(preceding_space+1).toInt();
      Serial.println("done.");
      return;
    }
    if(command.startsWith("ramp_time?")){
      Serial.print("ramp time ");
      Serial.println(ramp_time);
      return;
    }
    if(command.startsWith("min_step_delay ") || command.startsWith("dt ")){
      int preceding_space = command.indexOf(' ',0);
      if(preceding_space <= 0){
        Serial.println("Bad command.");
        return;
      }
      min_step_delay = command.substring(preceding_space+1).toInt();
      Serial.println("done.");
      return;
    }
    if(command.startsWith("min_step_delay?") || command.startsWith("dt?")){
      Serial.print("minimum step delay ");
      Serial.println(min_step_delay);
      return;
    }
    if(command.startsWith("zero")){
      EACH_MOTOR current_pos[i]=0;
      Serial.println("position reset to 0 0 0");
      return;
    }
    if(command.startsWith("test_mode")){
      if(command.equals("test_mode on")){
        test_mode=true;
        Serial.println("test_mode on");
      }else{
        test_mode=false;
        Serial.println("test_mode off");
      }
      return;
    }

    if(command.startsWith("version")){
      Serial.println(F(VER_STRING));
      return;
    }
    if(command.startsWith("board")){
      Serial.println(F(BOARD_STRING));
      return;
    }

    if(command.startsWith("help")){
      Serial.println("");
      Serial.print("Board: ");
      Serial.println(F(BOARD_STRING));
      Serial.println("");
      Serial.println(F(VER_STRING));
      
      Serial.println("");
      Serial.println(F("Commands (terminated by a newline character):"));
      Serial.println(F("mrx <d>                        - relative move in x"));
      Serial.println(F("mry <d>                        - relative move in y"));
      Serial.println(F("mrz <d>                        - relative move in z"));
      Serial.println(F("mr <d> <d> <d>                 - relative move in all 3 axes"));
      Serial.println(F("release                        - de-energise all motors"));
      Serial.println(F("p?                             - print position (3 space-separated integers"));
      Serial.println(F("ramp_time <d>                  - set the time taken to accelerate/decelerate in us"));
      Serial.println(F("min_step_delay <d>             - set the minimum time between steps in us."));
      Serial.println(F("dt <d>                         - set the minimum time between steps in us."));
      Serial.println(F("ramp_time?                     - get the time taken to accelerate/decelerate in us"));
      Serial.println(F("min_step_delay?                - get the minimum time between steps in us."));
      Serial.println(F("zero                           - set the current position to zero."));
      Serial.println(F("test_mode <s>                  - set test_mode <on> <off>"));
      Serial.println(F("version                        - get firmware version string"));
      Serial.println("");
      Serial.println("Input Key:");
      Serial.println(F("<d>                            - a decimal integer."));
      Serial.println("");
      Serial.println("--END--");
      return;
    }
    Serial.println(F("Type 'help' for a list of commands."));
  }else{
    delay(1);
    return;
  }
}
