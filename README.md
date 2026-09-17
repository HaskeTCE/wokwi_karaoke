# wokwi_karaoke
Karaoke Timer Lock Activation System


# usage
Be sure to have Visual Studio Code with the Wokwi Simulator and PlatformIO extensions installed

https://marketplace.visualstudio.com/items?itemName=Wokwi.wokwi-vscode

https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide

Just open the wokwi_karaoke folder as a workspace and start simulating.

Don't forget to install the dependencies as listed in the platform.ini file.

# pin layout

| Component         | Pin Function                  | ESP32 GPIO                    | Notes                                                            |
|-------------------|-------------------------------|-------------------------------|------------------------------------------------------------------|
| LCD Operator      | SDA SCL                       | 21 22                         | I2C Display (PCF8574T module).                                   |
| LCD Booth         | SDA SCL                       | 18 19                         | I2C Display (PCF8574T module).                                   |
| Keypad            | R1, R2, R3, R4 C1, C2, C3, C4 | 13, 12, 14, 27 26, 25, 33, 32 | Standard 4x4 membrane keypad.                                    |
| Servo             | PWM Signal                    | 15                            | 0° = Locked, 90° = Unlocked                                      |
| Ultrasonic Sensor | TRIG ECHO                     | 5 23                          | HC-SR04. Checks if the door is physically closed before locking. |
| Red LED           | Anode (+)                     | 2                             | Requires current-limiting resistor (220Ω)                  |
| Green LED         | Anode (+)                     | 4                             | Requires current-limiting resistor (220Ω)                  |
| Buzzer            | Signal                        | 16                            | Active buzzer.                                                   |
| Booth Button      | Signal                        | 17                            | Uses INPUT_PULLUP (connect to GND when pressed).                 |
| Main Switch       | Signal                        | 34                            | Input-only pin. Uses external pull-down resistor.                |

[md table maker](https://www.tablesgenerator.com/markdown_tables)
