# win-ble-cpp

> PoC for BLE on Windows using WinRT APIs

A proof-of-concept code for controlling a [Bluetooth Low Energy Light Bulb](https://medium.com/@urish/reverse-engineering-a-bluetooth-lightbulb-56580fcb7546) using Windows RT APIs.

This project was created in hope to push forward the implementation of [Web Bluetooth](https://medium.com/@urish/start-building-with-web-bluetooth-and-progressive-web-apps-6534835959a6) for Windows 10.

License: MIT.

## What does it do?

1. Scans for BLE devices with a `0xffe5` Primary Service
2. Connects to any matching device
3. Writes to Characterstic number `0xffe9` and changes the color of the bulb every second (alternates between red and yellow)

Note: This only works on Windows 10 Creators Update. Prior Windows 10 versions did not support BLE connections with unpaired devices.
