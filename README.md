# RFID-Access-Server-Room

Steuerung des RFID-Zugangs zum Serverraum. Der Reader prüft nichts selbst —
er publiziert Scans per MQTT an IP-Symcon, das die Zugriffslogik (Mitglieder-
Prüfung, Türöffnung, Beeps) steuert und die Tür-Logik und das Einweisungs-
Modul bedient.

## Hardware

- **WIZnet W5500-EVB-Pico** (RP2040), Wiegand-Reader, Buzzer, LED, Türschloss-Ausgang
- Statische Netzwerkkonfiguration im Sketch:
  - Reader: `192.168.100.11`
  - MQTT-Broker: `192.168.100.251:1885`
- Türschloss: [DORMA SVP 2277/2278](https://sautter.com/blog/ansteuerung-svp-2xxx-motorschloss/)

## Funktionsweise

1. Chip an den Reader → Wiegand-Daten → Sketch publiziert `serverroom/card`
   (`<uid>;<wiegandTyp>`, z. B. `3705828210;34`)
2. IP-Symcon prüft UID/Mitgliedschaft und antwortet über die Topics
   `serverroom/unlock` (Tür entriegeln) und `serverroom/beep` (Piepser)
3. Türkontakt-Status läuft über `serverroom/door`, Lebenszeichen über
   `serverroom/status/online` (retained, LWT `0`)

## Firmware-Besonderheiten

- **1,5-s-Cooldown:** Wiederholungs-Reads innerhalb des Fensters werden
  verworfen statt gepuffert (keine verzögerten Events; Dedup macht Symcon)
- **50-ms-Polling** des Wiegand-Bus (niedrige Latenz)
- **Non-blocking MQTT-Reconnect** — der Task-Scheduler läuft auch bei
  Broker-Ausfall weiter
- **Buzzer ist aktiv-HIGH** (Ton bei HIGH) — `digitalWriteBeep()` invertiert
  entsprechend; Beep-Nachrichten ≤ 2 s nach (Re)Connect werden ignoriert
  (Schutz vor retained-Echo vom Broker)

## Flashen

⚠️ **Toolchain-Lock:** Nur mit den dokumentierten Versionen bauen/flashen
(Core `rp2040:rp2040` 6.4.0, ArduinoOTA 1.1.1, Ethernet 2.0.2, Wiegand 1.0.20,
TaskScheduler 4.1.1, PubSubClient 2.8) — abweichende Versionen → Boot-Layout-
Mismatch → Dauerton/boot-losem Reader.

- **OTA:** `POST http://192.168.100.11:65280/sketch` mit Header
  `Authorization: Basic YXJkdWlubzo=` (= `arduino:` + leeres Passwort) und dem
  `.bin` als Body
- **USB:** BOOTSEL gedrückt halten beim Einstecken → Laufwerk „RPI-RP2" →
  `.uf2` hineinkopieren

Die ausführliche Prozedur (inkl. curl-Einzeiler, Gotchas, Befunde):
[`docs/README-rfid-serverroom.md` im Mitgliederbereich-Repo](https://github.com/makerspace-wi/Mitgliederbereich/blob/main/docs/README-rfid-serverroom.md)
