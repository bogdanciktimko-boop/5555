# HLK-LD2410S (ESP32)

Додав приклад скетчу `esp32_ld2410s_settings.ino`, який підключається до датчика HLK-LD2410S через UART2 та виводить поточні налаштування в `Serial Monitor`.

## Що потрібно
- ESP32
- HLK-LD2410S
- Arduino IDE
- Бібліотека **ld2410** (автор: `ncmreynolds`)

## Піни за замовчуванням у скетчі
- `LD2410S TX -> ESP32 GPIO16 (RX2)`
- `LD2410S RX -> ESP32 GPIO17 (TX2)`
- `GND -> GND`

> Якщо у вас інші піни, змініть `RADAR_RX_PIN` і `RADAR_TX_PIN` у скетчі.

## Швидкий старт
1. Встановіть бібліотеку `ld2410` через Library Manager.
2. Відкрийте `esp32_ld2410s_settings.ino`.
3. Завантажте на ESP32.
4. Відкрийте Serial Monitor на `115200`.
5. Ви побачите поточні параметри (гейти, чутливість, timeout тощо).
