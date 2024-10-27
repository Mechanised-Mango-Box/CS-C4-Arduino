// 7355608
#include <Tone.h>
#include <Keypad.h>
#include "DFRobot_RGBLCD1602.h"

// #region BUZZER
/// Thanks To: https://blog.woutergritter.me/2020/07/21/how-i-got-the-csgo-bomb-beep-pattern/

// seconds per beep = 1/bps
// next beep = spb(current)
const int BEEP_TONE = NOTE_C8;
const float BEEP_LEN = 125;
const float ARMED_LEN = 40000;

double bps(double t) {
  t = constrain(t, 0, 1);
  return 1.049 * exp(0.244 * t + 1.764 * (t * t));
}

float armed_time = -1;
float explode_time = -1;
float beep_start = -1;

// D12 = LED+
// D13 = beeper+
// GND = -
Tone bomb_tone;

void stop_buzzer() {
  bomb_tone.stop();
}

void arm() {
  stop_buzzer();
  armed_time = millis();
  explode_time = armed_time + ARMED_LEN;
  beep_start = armed_time;
}


//true if explode
bool armed_tick() {
  if (millis() <= explode_time) {  // before boom
    if ((beep_start <= millis()) && (millis() <= beep_start + BEEP_LEN)) {
      //       bomb_tone.play(NOTE_A4);
    } else {
      stop_buzzer();
    }

    float progress = (beep_start - armed_time) / ARMED_LEN;
    float time_till_next_beep = 1000 / bps(progress);
    // once the current beep is done append time to OLD START
    if (beep_start + time_till_next_beep <= millis()) {
      beep_start = beep_start + time_till_next_beep;
      bomb_tone.play(BEEP_TONE);
    }

  } else {  //boom
    return true;
  }
  return false;
}

// void setup() {
//   Serial.begin(9600);
//   arm();
// }

// void loop() {
//   if (armed_tick()) {
//     // maintaint beep rate till explode?
//     Serial.println("BOOM");
//     exit(0);
//   }
// }
//#endregion

// #region KEYPAD
const int ROW_NUM = 4;     //four rows
const int COLUMN_NUM = 3;  //three columns

char keys[ROW_NUM][COLUMN_NUM] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};

byte pin_rows[ROW_NUM] = { 3, 8, 7, 5 };    //connect to the row pinouts of the keypad
byte pin_column[COLUMN_NUM] = { 4, 2, 6 };  //connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

/*
   -------
  | 1 2 3 | 2
  | 4 5 6 | 7
  | 7 8 9 | 6
  | * 0 # | 4
   -------
    3 1 5

    PINS:
    1234567

*/
// void loop() {
//   char key = keypad.getKey();

//   if (key) {
//     Serial.println(key);
//   }
// }
// #endregion

// #region LCD
/*
https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557
*/
DFRobot_RGBLCD1602 lcd(/*RGBAddr*/ 0x6B, /*lcdCols*/ 16, /*lcdRows*/ 2);  // 16 characters and 2 lines of show
const char BLANK_STR[] = "*******";
const int PWM_MIN = 0;
const int PWM_MAX = 255;
const int PWM_SLEEP = 25;

float timer_start = -1;
float timer_end = -1;

void set_timer(float time) {
  timer_start = millis();
  timer_end = timer_start + time;  // show for 5s
}

bool is_timer_done() {
  return millis() >= timer_end;
}

/*
PORTS:
A5 =blue = SCL https://docs.arduino.cc/learn/communication/wire/
A4 = green  = SDA
3v3 or 5v
GND
*/
/// Data
const int CODE_LEN = 7;
const char BLANK_CHAR = '?';
const char CODE[] = { '7', '3', '5', '5', '6', '0', '8' };
char entered_code[] = { BLANK_CHAR, BLANK_CHAR, BLANK_CHAR, BLANK_CHAR, BLANK_CHAR, BLANK_CHAR, BLANK_CHAR };
int code_cursor = 0;
const int CURSOR_OFFSET = 8;
void reset_code() {
  for (int i = 0; i < CODE_LEN; i++) {
    entered_code[i] = BLANK_CHAR;
  }
  code_cursor = 0;
}

bool is_blank() {
  bool is_blank = true;
  for (int i = 0; i < CODE_LEN; i++) {
    is_blank = is_blank && (BLANK_CHAR == entered_code[i]);
  }
  return is_blank;
}

void display_code() {
  /// fill backwards until a non-'*' is seen
  // code = 12345**
  // print => **
  /// then do from front
  // print => **12345
  for (int i = (CODE_LEN - 1); i >= 0; i--) {
    if (entered_code[i] == BLANK_CHAR) {
      lcd.setCursor(CURSOR_OFFSET + (CODE_LEN - 1) - i, 0);
      lcd.print('*');
    } else {
      for (int j = 0; j < i + 1; j++) {
        lcd.setCursor(CURSOR_OFFSET + (CODE_LEN - 1) - i + j, 0);
        lcd.print(entered_code[j]);
      }
      return;
    }
  }
}

// 0 = normal
// 1 = wrong code
// 2 = plant
int enter_code(char c) {
  bomb_tone.play(BEEP_TONE);
  set_timer(BEEP_LEN);

  if (c == '*') {  //backspace
    if (code_cursor == 0) {
      return 0;
    }

    code_cursor--;
    entered_code[code_cursor] = BLANK_CHAR;
    display_code();
    return 0;
  } else if (c == '#') {  //enter
    // validate
    bool is_valid = true;
    for (int i = 0; i < CODE_LEN; i++) {
      is_valid = is_valid && (CODE[i] == entered_code[i]);
    }

    reset_code();
    lcd.clear();
    if (is_valid) {
      Serial.println("CORRECT KEY");
      return 2;
    } else {
      reset_code();
      return 1;
    }
  } else {  // number
    if (code_cursor == CODE_LEN) { return 0; }
    entered_code[code_cursor] = c;
    code_cursor++;
    display_code();
    Serial.print("Cursor at: ");
    Serial.println(code_cursor);
  }
  return 0;
}
// #endregion



/// States:
/*
SLEEP:
> The numpad will be blank initially
> brightness 0
> everything off

INPUTTING:
1) Any input sets text as 7x'*' (*******) + LCD full bright
2) while i < len(code)
  a) if new_num == code[i]
    i) i++
    ii) beep
    iii) update text - shift all numbers left, replacing '*'s
  b) else
    i) show all *******
    ii) beep-long
    iii) goto SLEEP
3) goto PLANTED

PLANTED:
> beep and LED increasing speeds
> at certain threashold goto BOOM

BOOM:
> show text "EXPLODE! :D"
> 5s wait
> goto SLEEP
*/
int current_state = -1;
const int STATE_WRONG = -1;
void GOTO_WRONG() {
  Serial.println("WRONG CODE");

  lcd.setPWM(lcd.REG_ONLY, PWM_MAX);
  lcd.clear();
  lcd.setCursor(1, 1);
  lcd.print("Wrong Code! T^T");

  reset_code();
  current_state = STATE_WRONG;
}

const int STATE_SLEEP = 1;
void GOTO_SLEEP() {
  Serial.println("SLEEP");

  lcd.setPWM(lcd.REG_ONLY, PWM_SLEEP);
  lcd.clear();

  current_state = STATE_SLEEP;
}

const int STATE_INPUT = 2;
void GOTO_INPUT() {
  Serial.println("INPUT");

  lcd.clear();
  lcd.setPWM(lcd.REG_ONLY, PWM_MAX);
  display_code();

  current_state = STATE_INPUT;
}

const int STATE_PLANTED = 3;
void GOTO_PLANTED() {
  Serial.println("PLANTED");
  arm();
  lcd.setCursor(CURSOR_OFFSET, 0);
  lcd.print(BLANK_STR);
  current_state = STATE_PLANTED;
}

const int STATE_BOOM = 4;
void GOTO_BOOM() {
  Serial.println("BOOM");

  set_timer(5000);  // show for 5s

  lcd.clear();
  lcd.setPWM(lcd.REG_ONLY, PWM_MAX);
  lcd.setCursor(3, 1);
  lcd.print("<<<Boom!>>>");
  lcd.display();

  current_state = STATE_BOOM;
}

void setup() {
  Serial.begin(9600);
  Serial.println("!");
  Serial.println("Setting Up...");

  Serial.println("Init LCD...");
  lcd.init();
  lcd.clear();
  lcd.display();

  Serial.println("Init Keypad...");
  bomb_tone.begin(13);
  set_timer(BEEP_LEN);
  bomb_tone.play(BEEP_TONE);

  GOTO_SLEEP();
}


void goto_after_key(int flag) {
  switch (flag) {
    case 0:
      {
        return;
      }
    case 1:
      {
        GOTO_WRONG();
        return;
      }
    case 2:
      {
        GOTO_PLANTED();
        return;
      }
  }
}
char key = '\0';
String buffer = "";

void loop() {
  // // CHEAT MODE: STATE SWAP
  // if (Serial.available()) {
  //   switch (Serial.parseInt()) {
  //     case STATE_SLEEP:
  //       GOTO_SLEEP();
  //       break;
  //     case STATE_INPUT:
  //       GOTO_INPUT();
  //       break;
  //     case STATE_PLANTED:
  //       GOTO_PLANTED();
  //       break;
  //     case STATE_BOOM:
  //       GOTO_BOOM();
  //       break;
  //     default:
  //       // Serial.println(Serial.parseInt());
  //       break;
  //   }
  // }

  switch (current_state) {
    case STATE_WRONG:  //same as sleep just different setup
    case STATE_SLEEP:
      {
        if (is_timer_done()) {
          stop_buzzer();
        }

        // if has entered key go to input mode
        if (!is_blank()) {
          GOTO_INPUT();
          break;
        }

        // CHEAT MODE: VIRTUAL KEYPAD
        if (Serial.available()) {
          buffer = Serial.readString();
          buffer.trim();
          key = buffer[0];

          Serial.print("Key Pressed: ");
          Serial.println(key);


          goto_after_key(enter_code(key));
          break;
        }


        char key = keypad.getKey();

        if (key) {
          Serial.print("Key Pressed: ");
          Serial.println(key);

          goto_after_key(enter_code(key));
        }
        break;
      }
    case STATE_INPUT:
      {
        if (is_timer_done()) {
          stop_buzzer();
        }

        // CHEAT MODE: VIRTUAL KEYPAD
        if (Serial.available()) {
          buffer = Serial.readString();
          buffer.trim();
          key = buffer[0];

          Serial.print("Key Pressed: ");
          Serial.println(key);

          goto_after_key(enter_code(key));
          break;
        }


        char key = keypad.getKey();

        if (key) {
          Serial.print("Key Pressed: ");
          Serial.println(key);

          goto_after_key(enter_code(key));
        }

        break;
      }
    case STATE_PLANTED:
      {

        if (armed_tick()) {
          GOTO_BOOM();
        }
        break;
      }
    case STATE_BOOM:
      {
        if (is_timer_done()) {
          stop_buzzer();
          GOTO_SLEEP();
        }

        break;
      }
    default:
      {
        Serial.println("ERROR");
        return;
      }
  }
}
