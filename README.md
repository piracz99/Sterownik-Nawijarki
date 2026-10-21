# Sterownik nawijarki Arduino Nano

Program steruje bipolarnym silnikiem krokowym przez moduł L298N. Potencjometr reguluje zadaną prędkość, wyświetlacz LCD 16x2 pokazuje licznik zwojów i RPM, a przycisk zeruje licznik.

## Film

[Zobacz film na YouTube](https://www.youtube.com/watch?v=0M0IyVGj4Qw)

## Połączenia

### L298N i silnik

| Arduino Nano | L298N |
|---|---|
| D2 | IN1 |
| D3 | IN2 |
| D4 | IN3 |
| D5 | IN4 |
| GND | GND |

Podłącz dwie cewki silnika do wyjść mostka: jedną do OUT1–OUT2, drugą do OUT3–OUT4. Upewnij się, że przewody każdej cewki są prawidłowo dobrane. Zasilanie silnika podłącz do zacisków zasilania L298N. Wejścia ENA i ENB muszą być włączone, np. za pomocą zworek modułu.

### Potencjometr

| Potencjometr | Arduino Nano |
|---|---|
| Jeden skrajny pin | 5V |
| Drugi skrajny pin | GND |
| Środkowy pin (suwak) | A0 |

Odczyt A0 jest skalowany dla maksymalnego napięcia suwaka około 3,3 V przy domyślnym, 5-woltowym odniesieniu ADC Nano.

### LCD 16x2 z adapterem I²C PCF8574T

| Adapter LCD | Arduino Nano |
|---|---|
| SDA | A4 (SDA) |
| SCL | A5 (SCL) |
| VCC | 5V |
| GND | GND |

Adres I²C w programie jest ustawiony na `0x27`.

### Przycisk zerowania

Podłącz przycisk chwilowy między **D6** a **GND**. Program korzysta z wewnętrznego rezystora podciągającego, więc zewnętrzny rezystor nie jest potrzebny.

Masa Arduino, L298N, potencjometru i LCD musi być wspólna.

## Obsługa

- Obrót potencjometru reguluje zadaną prędkość w zakresie do 400 RPM; po ustawieniu minimum silnik jest wyłączany.
- Pierwszy wiersz LCD wyświetla licznik zwojów w formacie pięciocyfrowym.
- Drugi wiersz pokazuje zadane RPM/min i animowaną kreskę.
- Krótkie naciśnięcie przycisku D6 zeruje licznik.

Licznik i prędkość są obliczane programowo, bez czujnika położenia. Wartość `STEPS_PER_REVOLUTION` jest ustawiona na 48 zgodnie z kalibracją układu. Jeśli licznik nie zgadza się z rzeczywistymi obrotami, należy ponownie skalibrować tę wartość. Odczyt RPM jest wartością zadaną, a nie pomiarem rzeczywistej prędkości wału.

## Kompilacja i wgrywanie

W Arduino IDE wybierz płytkę **Arduino Nano** i właściwy port szeregowy. Alternatywnie, mając zainstalowany Arduino CLI:

```powershell
arduino-cli compile --fqbn arduino:avr:nano .
arduino-cli upload -p COM20 --fqbn arduino:avr:nano .
```

## Uwagi dotyczące zasilania

Nie zasilaj silnika z pinu 5V Arduino. L298N nie ma regulacji ograniczenia prądu uzwojeń i może się mocno nagrzewać; zapewnij odpowiednie chłodzenie oraz zasilacz i silnik dobrane do siebie. Przerwij pracę, jeśli silnik lub sterownik nadmiernie się grzeje. Nie podłączaj ani nie odłączaj silnika przy włączonym zasilaniu.# Sterownik-Nawijarki
