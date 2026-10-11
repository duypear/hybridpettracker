# Hybrid Pet Tracker

**A hybrid LoRa P2P and LoRaWAN system for pet proximity monitoring.**

Hybrid Pet Tracker is an embedded IoT project designed to monitor a pet's proximity to a predefined home area. It combines direct LoRa Point-to-Point (P2P) communication with LoRaWAN connectivity and a web dashboard.

The system consists of a RAK11720 pet node, an ESP32-C3-based SUCA gateway, a backend API, and a web interface.

> **Project status:** Prototype. The complete end-to-end communication flow and hardware behavior require validation on the deployed system.

## Features

- Hybrid LoRa P2P and LoRaWAN communication.
- RSSI-based pet proximity classification.
- Three intended states: `HOME`, `NEAR`, and `AWAY`.
- Periodic pet identification messages over P2P.
- LoRaWAN uplink transmission using ABP activation.
- Downlink handling for switching communication modes.
- Gateway-to-backend communication over Wi-Fi and HTTP.
- Web dashboard for viewing pet and system information.

**Note:** RSSI estimates signal strength and proximity; it does not provide the pet's live GPS coordinates.

## System Architecture

```text
                 Pet Node
                  RAK11720
                      |
             +--------+--------+
             |                 |
          LoRa P2P          LoRaWAN
             |                 |
             v                 v
       SUCA Gateway       LoRaWAN Gateway
        ESP32-C3                 |
             |               ChirpStack
           Wi-Fi                  |
             |                   v
             v              Backend Integration
       LPWAN Mapper API
             |
        Web Dashboard
```

The P2P path allows the pet node to communicate with the home gateway. The gateway evaluates received signal strength and reports pet status to the configured API.

When the node operates in LoRaWAN mode, it sends uplinks through a LoRaWAN gateway. ChirpStack and the backend are intended to process and forward this data.

The complete LoRaWAN-to-dashboard path must be verified against the deployed network and backend configuration.

## Hardware

| Component | Role |
|---|---|
| RAK11720 | Pet node with P2P and LoRaWAN firmware |
| ESP32-C3 | SUCA gateway controller |
| LoRa P2P radio module | Direct communication between the node and gateway |
| LoRaWAN gateway | Radio connectivity for remote reporting |

The exact radio module model, antenna configuration, and final wiring should be documented from the actual hardware setup.

## Technologies

- Embedded C/C++ and Arduino-style firmware
- RAK11720 firmware APIs
- ESP32-C3
- LoRa P2P
- LoRaWAN ABP
- ChirpStack
- Wi-Fi and HTTP API integration
- HTML, CSS, and JavaScript for the web interface

## Operating Modes

### 1. P2P Mode — Home Proximity

The pet node periodically broadcasts its configured pet name over P2P. The SUCA gateway receives packets, evaluates RSSI measurements, classifies proximity, and reports status to the API.

The node recognizes the following command bytes:

| Command | Intended meaning |
|---|---|
| `0x30` | `HOME` |
| `0x32` | `NEAR` |
| `0x31` | `AWAY` |

The node can switch to LoRaWAN when it receives the `AWAY` command or when its P2P command timeout is exceeded.

### 2. LoRaWAN Mode — Remote Reporting

The node configures an ABP session and sends its configured device ID as an uplink payload.

A one-byte downlink containing `0x30` is intended to trigger a return to P2P mode. The current firmware also periodically attempts to switch back to P2P to search for the home gateway.

Mode switching, uplink scheduling, downlink delivery, and recovery behavior must be tested on the target hardware and network.

## Web Dashboard

The repository includes `index.html`, which provides the web interface.

Documented project intent includes displaying pet status and system information. The exact dashboard features, map behavior, and backend dependencies should be confirmed by running the current web application.

The configured home coordinates must not be interpreted as the pet's live GPS location.

## Repository Contents

```text
hybridpettracker/
├── ANCHORNODE.ino
├── NODETRACKER.ino
├── index.html
└── README.md
```

- `ANCHORNODE.ino` — RAK11720 pet-node firmware.
- `NODETRACKER.ino` — ESP32-C3 SUCA gateway firmware.
- `index.html` — web dashboard.
- `README.md` — project documentation.

## Configuration

Before deployment, review the following settings in the firmware:

- P2P frequency and radio parameters.
- LoRaWAN regional band and ABP session configuration.
- Pet name and device ID.
- Gateway Wi-Fi settings and API endpoint.
- RSSI thresholds and timeout values.
- Home coordinates and backend payload format.

The pet name and device ID must match the identifiers expected by the gateway and backend.

## Build and Run

### Firmware

1. Install the development environment and board support package compatible with the RAK11720.
2. Install the ESP32 board support package for the SUCA gateway.
3. Configure radio parameters, device identifiers, network settings, and API configuration.
4. Build and flash each firmware to its corresponding board.
5. Open the serial monitor and inspect initialization, packet reception, RSSI, mode transitions, and transmission results.

Exact build commands and tested software versions should be added after a clean build has been verified.

### Web Dashboard

1. Open `index.html` in a browser for an initial inspection.
2. Check whether the page depends on an API endpoint or other external services.
3. Configure the correct backend endpoint and authentication settings if required.
4. Verify the dashboard using actual API responses.

Opening the HTML file directly may not be sufficient if the dashboard requires a backend API.

## Known Limitations and Verification Tasks

- **RSSI accuracy:** Signal strength varies with obstacles, antenna orientation, interference, and the environment.
- **Mode switching:** Reboot-based switching must be tested to avoid unproductive transitions between P2P and LoRaWAN.
- **LoRaWAN integration:** Confirm regional settings, ABP credentials, payload decoding, and downlink delivery.
- **Backend integration:** Verify that P2P and LoRaWAN reports are associated with the same pet record.
- **Offline status:** Confirm timeout and heartbeat behavior against the backend implementation.
- **Web functionality:** Validate API calls, map behavior, and displayed status using the deployed backend.
- **Hardware documentation:** Add verified wiring diagrams, photographs, and measured test results.

## Security

- Use demo credentials in public examples.
- Do not publish production API tokens, Wi-Fi passwords, or active LoRaWAN session keys.
- Replace any real credentials previously exposed in a public repository.
- Validate received radio messages and backend inputs.

## Future Improvements

- Document and version the radio packet protocol.
- Add hysteresis to reduce RSSI-based status oscillation.
- Improve mode-switching recovery and error handling.
- Add reproducible hardware and backend integration tests.
- Include verified architecture diagrams, hardware photographs, and demonstration screenshots.
- Document measured communication range and RSSI thresholds.

## License

No license has been selected for this repository yet. Add a `LICENSE` file once the project's distribution terms have been decided.
