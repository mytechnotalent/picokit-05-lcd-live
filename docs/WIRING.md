# Wiring

## Pin map

| Peripheral | Pico 2 pin | Notes |
| --- | --- | --- |
| DHT11 data | GP4 | 3.3 V data line, 10 k pull-up optional |
| 1602 LCD SDA | GP2 | I2C1 SDA |
| 1602 LCD SCL | GP3 | I2C1 SCL |
| RYLR998 TX | GP8 | UART1 TX to module RX |
| RYLR998 RX | GP9 | UART1 RX from module TX |
| VS1838B OUT | GP5 | NEC infrared input |
| SG90 signal | GP14 | 50 Hz PWM |
| Red LED | GP16 | through a 220 or 330 ohm resistor |
| Yellow LED | GP17 | through a 220 or 330 ohm resistor |
| Green LED | GP18 | through a 220 or 330 ohm resistor |
| Button | GP15 | to GND, internal pull-up, active low |
| Onboard LED | GP25 | heartbeat |
| Debug Probe | SWCLK / SWDIO / GND | SWD |
| Debug Probe UART | GP0 / GP1 | UART0 console |

## Power

- LCD backpack VCC to 5 V (VBUS).
- SG90 VCC to 5 V (VBUS) with the 1000 uF capacitor across VCC and GND at the
  servo connector.
- RYLR998 VCC to 3.3 V.
- All grounds common.

## Breadboard

1. Seat the Pico 2 across the center channel.
2. Wire the three LEDs with their resistors to GP16, GP17, and GP18, cathodes
   to the ground rail.
3. Wire the button between GP15 and the ground rail.
4. Wire the DHT11 data to GP4 with VCC to 3.3 V and GND to the ground rail.
5. Wire the LCD SDA to GP2 and SCL to GP3, VCC to 5 V.
6. Wire the servo signal to GP14, VCC to 5 V through the capacitor, GND common.
7. Wire the VS1838B OUT to GP5, VCC to 3.3 V, GND common.
8. Wire the RYLR998 TX to GP9 and RX to GP8, VCC to 3.3 V, GND common.
9. Connect the Debug Probe to SWCLK, SWDIO, and GND, and its UART to GP0/GP1.

## Gateway

- Connect the second RYLR998 to a USB serial adapter and the computer.
- On macOS the adapter appears as `/dev/cu.usbserial-A50285BI`.
