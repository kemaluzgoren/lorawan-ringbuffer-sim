# LoRaWAN Ring Buffer Simulation

This repository demonstrates a multi-threaded simulation of LoRaWAN-style message handling using a ring buffer in C. It aims to illustrate how circular buffers (ring buffers) can be used to decouple producer-consumer logic in embedded systems.

## Key Features

- Simulates OTAA join requests from random DevEUIs
- Validates and registers devices in a simple device database
- Uplink messages are only accepted from registered devices
- All messages are buffered using the [lwrb v1.2.0](https://github.com/MaJerle/lwrb) ring buffer library
- Multi-threaded design using POSIX threads

## Build & Run

```bash
make
./lorawan_sim
```

## Dependencies 

- POSIX-compliant environment (Linux/macOS)
- GCC 
- pthread support
- [lwrb v1.2.0](https://github.com/MaJerle/lwrb)

