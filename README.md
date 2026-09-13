# wokwi_karaoke
Karaoke Timer Lock Activation System


# usage
Be sure to have Visual Studio Code with the Wokwi Simulator and PlatformIO extensions installed

https://marketplace.visualstudio.com/items?itemName=Wokwi.wokwi-vscode

https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide

Just open the wokwi_karaoke folder as a workspace and start simulating.

Don't forget to install the dependencies as listed in the platform.ini file.

# pin layout

| Component              | Pin Function     | ESP32 GPIO     | Notes                                               |
|------------------------|------------------|----------------|-----------------------------------------------------|
| LCD   Booth & Operator | RS               | 22             | Two Standard   Parallel LCDs                        |
|                        | D4               | 5              |                                                     |
|                        | D5               | 18             |                                                     |
|                        | D6               | 19             |                                                     |
|                        | D7               | 21             |                                                     |
| LCD Operator           | EN               | 23             | Operator   display                                  |
| LCD Booth              | EN               | 0              | Booth   display                                     |
| Keypad                 | R1, R2, R3, R4   | 13, 12, 14, 27 | 4x4 keypad                                          |
|                        | C1, C2, C3,   C4 | 26, 25, 33, 32 |                                                     |
| Servo                  | PWM Signal       | 15             | 0° = Locked, 90° = Unlocked                         |
| Red LED                | Anode (+)        | 2              | Requires   current-limiting resistors (220Ω)        |
| Green LED              | Anode (+)        | 4              |                                                     |
| Buzzer                 | Signal           | 16             |                                                     |
| Booth Button           | Signal           | 17             | Uses INPUT_PULLUP (connect to GND)                  |
| Main Switch            | Signal           | 34             | Input-only pin. Uses external pull-down   resistor. |

[md table maker](https://www.tablesgenerator.com/markdown_tables)
