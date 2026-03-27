# OSCINO - Osciloscop digital interfațat USB cu intrare izolată galvanic
Proiectul nostru constă într-un osciloscop digital interfațat prin USB, capabil să achiziționeze, să proceseze și să afișeze semnale electrice în timp real pe PC.
## Specificații
- in: 5V DC USB-C;<br>
- out: >2 Mbit/s serial UART data;<br>
- Arduino Mega compatible pinout
- sampling rate: ~300 ksa/s;<br>
- AC/DC coupling;<br>
- Galvanically Isolated Frontend;<br>
- 0...230V AC/DC input range using different attenuation stages;<br>
- Attenuator for High Input Voltages;<br>
- UI for PC use;<br>
- timebase and synchronisation (trigger).<br>
## Elaborare etape
- [x] Alegere componente<br>
- [x] Schemă electrică<br>
- [x] Cablaj<br>
- [x] Prototip<br>
- [x] Programare uC<br>
- [x] Programare UI<br>
- [x] Testare<br>
- [x] Elaborare documentație<br>
- [x] Realizare prezentare PowerPoint<br>
## Alegerea unui microcontroller potrivit
- [ ] C8051F120 -> preț: $11.22<br>
- [ ] STM32H743ZIT6 -> preț: $15.23<br>
- [ ] MIMXRT1062DVL6B -> preț: $12.00<br>
- [x] ATMEGA 2560 -> preț: $10.56<br>
## Asamblarea PCB-ului la fabrică este mult prea scumpă pentru a finaliza cablajul (TBD asamblare manuală)
