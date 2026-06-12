# Fritzing Fan Sketch Spec (ESP32 + 2N2222 + 5V Fan)

Use this as an exact build sheet in Fritzing Breadboard view.

## Target Circuit

- ESP32 GPIO controls a 5V, 0.2A fan through a 2N2222 low-side switch.
- Includes base resistor, base pulldown, and flyback diode.

## Parts To Place (Fritzing)

- ESP32 board part
  - Use your existing custom ESP32 part from the current project if available.
  - If not available, use a generic ESP32 DevKit part and map to equivalent pin.
- NPN transistor (2N2222 / PN2222)
- Resistor x2
  - Rb = 330 ohm
  - Rpd = 100k ohm
- Diode (1N400x family)
- DC motor (for fan symbol) or 2-pin fan symbol
- 5V supply symbol (or 2-pin power connector labeled +5V/GND)

## Nets (Exact Connections)

- NET_GPIODRIVE:
  - ESP32 GPIOx -> Rb(330) -> Q1 Base (B)
- NET_BASE_PULLDOWN:
  - Q1 Base (B) -> Rpd(100k) -> GND
- NET_SWITCH_NODE:
  - Q1 Collector (C) -> Fan Negative (-) -> D1 Anode (A)
- NET_5V:
  - +5V -> Fan Positive (+) -> D1 Cathode (K, striped end)
- NET_GND:
  - Q1 Emitter (E) -> GND
  - ESP32 GND -> same common GND
  - 5V supply GND -> same common GND

## Pin/Polarity Labels (must appear in drawing)

Add text labels near parts in Fritzing:

- Q1 pins: B, C, E
- D1 polarity: A at Fan-, K (striped) at +5V/Fan+
- Common ground note: "ESP32 GND and 5V supply GND must be tied"

## Layout Recommendation (Breadboard View)

- Place ESP32 on left side.
- Place Q1 near fan negative path on right side.
- Place fan at top-right.
- Place diode directly across fan terminals (short visual distance):
  - D1 K toward +5V side
  - D1 A toward transistor collector/fan- side
- Place Rb between GPIO and Q1 base.
- Place Rpd from base to ground rail.

## Suggested Wire Colors

- Red: +5V
- Black: GND
- Yellow: GPIO control line
- Blue: switched fan negative path

## Electrical Check Before Power-Up

- No direct short between +5V and GND
- Diode orientation is correct (K to +5V, A to Fan-)
- Transistor pinout matches your exact TO-92 part datasheet
- Fan current <= transistor capability with startup margin considered

## Export Targets

After drawing in Fritzing:

- Save project as: Hardware/Fan/Fan-Control-Wiring.fzz
- Export image (PNG) as: Hardware/Fan/Fan-Control-Wiring.png

