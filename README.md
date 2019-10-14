# ubitx_v5
Firmware for the version 5 of the ubitx
Modified by K9SUL for the modified version.
    HW Changes:
    - Use of Adafruit i2c backpack for LCD, freed up I/O pins.
    - Added band up, band dn , menu, lock/exit, tuning step buttons.
    - Encoder wired to pin 2 and 3 for dual interrupt.
    SW Changes:
    - Fixed tuning rate with selectable tuning steps.
    - Last used frequency per band saved.
    - Keyer implementation removed (I use a SK or bug).
    - Split A/B removed (Not really useful. RIT can cover typical few kHz split QSOs).
    - Ham band only operation. Band limits are observed.
    - Removed dead code.
