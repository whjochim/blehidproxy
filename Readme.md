# Warning: Proof-of-Concept only

This project is currently not ready for public use and is presented here as a proof of concept developed within the context of a bachelor's thesis.

The code provided is in its unfinished state, as it was left after the completion of the thesis.

It serves as a foundation for future development and further enhancements.

[Thesis](./thesis.pdf)

---

# BLEHIDPROXY

THe purpose of blehidproxy is to implement a generic Bluetooth Low Energy keyboard dongle.
While most commercial keyboards still operate on Bluetooth classic, Bluetooth Low Energy offers massively improved battery life and latency.
A notable project implementing open source BLE keyboard firmware is [ZMK](https://github.com/zmkfirmware/zmk).

Currently, the dongle only supports BLE Boot Protocol mode, limiting it to 6-key rollover functionality.

blehidproxy was written for Zephyr version 2.7.1 and tested with the NRF52840 SOC.