/*
void morseError(int morse) {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("morse");
  Serial.println(morse);
  switch(morse) {
    case 0: // - - - - -
       morse_dash(); morse_dash(); morse_dash(); morse_dash(); morse_dash();
       break;
    case 1: // . - - - -
       morse_dot(); morse_dash(); morse_dash(); morse_dash(); morse_dash();
       break;
    case 2: // . . - - -
       morse_dot(); morse_dot(); morse_dash(); morse_dash(); morse_dash();
       break;
    case 3: // . . . - -
       morse_dot(); morse_dot(); morse_dot(); morse_dash(); morse_dash();
       break;
    case 4: // . . . . -
       morse_dot(); morse_dot(); morse_dot(); morse_dot(); morse_dash();
       break;
    case 5: // . . . . .
       morse_dot(); morse_dot(); morse_dot(); morse_dot(); morse_dot();
       break;
    case 6: // - - - - .
       morse_dash(); morse_dash(); morse_dash(); morse_dash(); morse_dot();
       break;
    case 7: // - - - . .
       morse_dash(); morse_dash(); morse_dash(); morse_dot(); morse_dot();
       break;
    case 8: // - - . . .
       morse_dash(); morse_dash(); morse_dot(); morse_dot(); morse_dot();
       break;
    case 9: // - . . . .
       morse_dash(); morse_dot(); morse_dot(); morse_dot(); morse_dot();
       break;
  }

  delay(3000);
}

void morse_dash() {
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
}

void morse_dot() {
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
}*/
