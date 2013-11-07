/*
 *  uvbox.ino
 *  Timed relay controller for Atmega328.
 *  Created by Kilian Ciuffolo on 11/09/13.
 *  This software is released under the MIT license cited below.
 *
 *  Copyright (c) 2010 Kilian Ciuffolo, me@nailik.org. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the 'Software'), to deal in the Software without
 *  restriction, including without limitation the rights to use,
 *  copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following
 *  conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 */

#include <avr/eeprom.h>
#include <SerialLCD.h>
#include <utils.h>
#include <simple_button.h>
#include <simple_timer.h>
#include <oscillate.h>
#include "digits.h" //change this to digits_[N].h

#define UP_PIN 9
#define DOWN_PIN 7
#define SEL_PIN 10
#define STOP_PIN 12
#define START_PIN 11
#define BUZZ_PIN 8
#define RELAY_PIN 13

#define BTN_HOLD_TRESHOLD 700
#define BTN_HOLD_FREQUENCY 100
#define DIGITS_SPACING 8
#define DIGITS_X (128 - DIGITS_W * 4 - DIGITS_SPACING) * 0.5
#define DIGITS_Y 6

simple_button_t up_btn;
simple_button_t down_btn;
simple_button_t sel_btn;
simple_button_t stop_btn;
simple_button_t start_btn;

SerialLCD lcd(Serial);
int configured_seconds;
int remaining_seconds;
bool selection = 0;
bool running = false;
bool should_write_eeprom = false;
int timer_id;
int tick_count = 0;

void setup() {
  // Bootstrap display
  lcd.begin(115200, 3000);
  lcd.clearDisplay();

  // Setting up buttons
  simple_button_set(UP_PIN, &up_btn, HIGH, BTN_HOLD_TRESHOLD, BTN_HOLD_FREQUENCY);
  simple_button_set(DOWN_PIN, &down_btn, HIGH, BTN_HOLD_TRESHOLD, BTN_HOLD_FREQUENCY);
  simple_button_set(SEL_PIN, &sel_btn, HIGH, BTN_HOLD_TRESHOLD, BTN_HOLD_FREQUENCY);
  simple_button_set(STOP_PIN, &stop_btn, HIGH, BTN_HOLD_TRESHOLD, BTN_HOLD_FREQUENCY);
  simple_button_set(START_PIN, &start_btn, HIGH, BTN_HOLD_TRESHOLD, BTN_HOLD_FREQUENCY);

  // Hooking up callbacks to buttons
  up_btn.rising_edge_cb = up_btn.hold_cb = on_up_btn;
  down_btn.rising_edge_cb = down_btn.hold_cb = on_down_btn;
  sel_btn.click_cb = on_sel_btn_click;
  start_btn.click_cb = on_start_btn_click;
  stop_btn.click_cb = on_stop_btn_click;

  // Output (Buzzers and relay)
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZ_PIN, LOW);

  // Loading last configured timer from EEPROM
  configured_seconds = eeprom_read_word(0);
  // Normalize if eeprom has never been written
  if (configured_seconds > 5999) {
    configured_seconds = 0;
  }
  remaining_seconds = configured_seconds;

  update_display(configured_seconds);
  update_selection();
}

void setup_LCD() {
  lcd.reset();
  lcd.baudRate(6);
  lcd.backLight(20);
  lcd.debugLevel(0);
  lcd.reverseMode();
}

void start_timer() {
  running = true;
  digitalWrite(RELAY_PIN, HIGH);
  timer_id = set_interval(1000, timer_tick);
}

void timer_tick(int arg) {
  // since you can alter the timer while is running it is possible
  // that it is already 0 when it ticks and we don't want negative values
  remaining_seconds = max(0, remaining_seconds - 1);

  update_display(remaining_seconds);

  if (!remaining_seconds) {
    stop_timer();
    remaining_seconds = configured_seconds;
    update_display(remaining_seconds);
    oscillate(BUZZ_PIN, 100, HIGH, 5);
  }
}

void stop_timer() {
  running = false;
  digitalWrite(RELAY_PIN, LOW);
  clear_timer(timer_id);
}

void update_display(int seconds) {
  // declaring as static to avoid stack frag
  // still cleaner than global
  static char digits[4];
  static char digit_ba[122];

  seconds_to_digits(seconds, digits);

  load_digit_ba_P(digits[0], digit_ba);
  lcd.bitblt(DIGITS_X + DIGITS_W * 0, DIGITS_Y, digit_ba);

  load_digit_ba_P(digits[1], digit_ba);
  lcd.bitblt(DIGITS_X + DIGITS_W * 1, DIGITS_Y, digit_ba);

  load_digit_ba_P(digits[2], digit_ba);
  lcd.bitblt(DIGITS_X + DIGITS_W * 2 + DIGITS_SPACING, DIGITS_Y, digit_ba);

  load_digit_ba_P(digits[3], digit_ba);
  lcd.bitblt(DIGITS_X + DIGITS_W * 3 + DIGITS_SPACING, DIGITS_Y, digit_ba);
}

void update_selection() {
  lcd.filledBox(
    DIGITS_X,
    DIGITS_Y + DIGITS_H + 5,
    DIGITS_X + DIGITS_W * 2,
    DIGITS_Y + DIGITS_H + 8,
    255 * selection
  );
  lcd.filledBox(
    DIGITS_X + DIGITS_W * 2 + DIGITS_SPACING,
    DIGITS_Y + DIGITS_H + 5,
    DIGITS_X + DIGITS_W * 4 + DIGITS_SPACING,
    DIGITS_Y + DIGITS_H + 8,
    255 * !selection
  );
}

void on_up_btn(simple_button_t *button) {
  if (!running) {
    should_write_eeprom = true;
    configured_seconds += selection ? 60 : 1;
    remaining_seconds = configured_seconds;
  } else {
    remaining_seconds += selection ? 60 : 1;
  }
  update_display(remaining_seconds);
}

void on_down_btn(simple_button_t *button) {
  if (!running) {
    should_write_eeprom = true;
    configured_seconds -= selection ? 60 : 1;
    configured_seconds = max(0, configured_seconds);
    remaining_seconds = configured_seconds;
  } else {
    remaining_seconds -= selection ? 60 : 1;
    remaining_seconds = max(0, remaining_seconds);
  }
  update_display(remaining_seconds);
}

void on_sel_btn_click(simple_button_t *button) {
  selection ^= 1;
  update_selection();
}

void on_start_btn_click(simple_button_t *button) {
  if (!remaining_seconds) return;
  if (should_write_eeprom) {
    eeprom_write_word(0, configured_seconds);
    should_write_eeprom = false;
  }
  running ? stop_timer() : start_timer();
  oscillate(BUZZ_PIN, 100, HIGH, 3);
}

void on_stop_btn_click(simple_button_t *button) {
  if (running) {
    remaining_seconds = configured_seconds;
    stop_timer();
  } else {
    remaining_seconds = configured_seconds = 0;
  }
  update_display(remaining_seconds);
  oscillate(BUZZ_PIN, 100, HIGH, 3);
}

void buttons_read() {
  simple_button_read(&up_btn);
  simple_button_read(&down_btn);
  simple_button_read(&sel_btn);
  simple_button_read(&stop_btn);
  simple_button_read(&start_btn);
}

void loop() {
  buttons_read();
  update_timers();
  delay(20);
}
