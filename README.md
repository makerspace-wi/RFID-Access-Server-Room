# RFID-Access-Server-Room
Local regulations required, that the access to the Server Room has to be protected. From the former location we still had an electromechanical door lock and a RFID-Reader Unit with WIEGAND protocol.<br>
So we just build a small Controller for the door lock with an Arduino Nano, connected via USB to the Raspberry Pi 3+  and modified the Software from the Entry Door Control.
<br>
So if a member would like to enter the Server room he/she needs to place the personal RFID-Chip to the reader. If the individual UID does match with one of the UID's stored on the server - the door will be unlocked.<br>

[Ansteuerung DORMA SVP 2277/2278](https://sautter.com/blog/ansteuerung-svp-2xxx-motorschloss/)
