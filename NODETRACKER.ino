    // =========================================================
    // HYBRID PET TRACKER: P2P ↔ LoRaWAN AUTO SWITCH
    // RAK11720 – FINAL VERSION - Khớp với SUCA Board & Web
    // =========================================================

    // -------------------- P2P CONFIG -------------------------
    double myFreq = 923000000;
    uint16_t sf = 10;
    uint16_t bw = 0;
    uint16_t cr = 1;
    uint16_t preamble = 8;
    uint16_t txPower = 22;

    // -------------------- LoRaWAN CONFIG ---------------------
    #define ABP_PERIOD   5000
    #define ABP_BAND     9

    // ĐỔI CÁC KEY NÀY THEO CHIRPSTACK CỦA BẠN
    #define ABP_DEVADDR  {0x01, 0x55, 0xC7, 0xF1}
    #define ABP_APPSKEY  {0x80, 0xDB, 0x9B, 0xA0, 0x48, 0x50, 0xB5, 0x88, 0x29, 0xD7, 0x9F, 0xF0, 0xDB, 0xD4, 0x12, 0x9C}
    #define ABP_NWKSKEY  {0x58, 0x48, 0x78, 0xCC, 0x16, 0x68, 0xED, 0x56, 0xB9, 0xB8, 0x00, 0x04, 0xE4, 0xFF, 0x9C, 0x56}

    // -------------------- PET INFO -------------------------
    // ⚠️ QUAN TRỌNG: Phải KHỚP với SUCA Board
    const char* PET_NAME = "Mèo Miu";       // PHẢI GIỐNG với SUCA
    const char* DEVICE_ID = "node-001";     // PHẢI GIỐNG với SUCA

    uint32_t lastLorawanUplink = 0;
    uint32_t lastP2PScan = 0;

    volatile bool p2p_found_home = false;
    unsigned long lastSuccessfulRX = 0;  // ✅ Biến global

    // =========================================================
    // P2P CALLBACK
    // =========================================================
    void recv_cb(rui_lora_p2p_recv_t data)
    {
        if (data.BufferSize == 0) return;

        uint8_t cmd = data.Buffer[0];

        Serial.printf("\n[P2P RX] cmd = 0x%02X  RSSI=%d SNR=%d\n",
                    cmd, data.Rssi, data.Snr);

        lastSuccessfulRX = millis();

        // ✅ THÊM CASE MỚI
        if (cmd == 0x32) {
            Serial.println("⚠️ SUCA: PET NEAR HOME (Warning Zone)");
            p2p_found_home = true;  // Vẫn ở chế độ P2P
            // KHÔNG reboot, KHÔNG chuyển LoRaWAN
        }

        if (cmd == 0x31) {
            Serial.println("🚪 SUCA: PET HAS LEFT HOME → switch to LoRaWAN!");
            api.lorawan.nwm.set();
            delay(200);
            api.system.reboot();
        }

        if (cmd == 0x30) {
            Serial.println("🏠 SUCA: PET IS HOME");
            p2p_found_home = true;
        }
    }

    // =========================================================
    // LoRaWAN CALLBACK
    // =========================================================
    void lorawan_recv(SERVICE_LORA_RECEIVE_T * data)
    {
        Serial.println("[LoRaWAN DOWNLINK]");

        // Nếu server gửi downlink 0x30 → pet đã về nhà
        if (data->BufferSize == 1 && data->Buffer[0] == 0x30)
        {
            Serial.println("🏠 DOWNLINK: PET RETURNED HOME → switching to P2P!");
            api.lora.nwm.set();  // Switch to P2P mode
            delay(200);
            api.system.reboot();
        }
    }

    void lorawan_send(int32_t status)
    {
        Serial.println(status == RAK_LORAMAC_STATUS_OK ? "✓ TX OK" : "✗ TX FAIL");
    }


    // =========================================================
    // MODE 0 = P2P MODE (pet at home)
    // =========================================================
    void loop_p2p()
    {
        static bool rx_on = false;

        static unsigned long lastAutoSwitchCheck = 0;

        if (!rx_on)
        {
            Serial.println("[P2P] RX ON FOREVER - Listening for SUCA");
            api.lora.precv(65533);
            rx_on = true;
            lastSuccessfulRX = millis();
        }

        // ✅ AUTO-SWITCH: Nếu 30 giây không nhận được command từ SUCA
        if (millis() - lastAutoSwitchCheck > 30000) {
            lastAutoSwitchCheck = millis();
            
            unsigned long timeSinceRX = millis() - lastSuccessfulRX;
            if (timeSinceRX > 45000) {  // 45 giây không nhận command
                Serial.println("\n⚠️⚠️⚠️ AUTO SWITCH WARNING ⚠️⚠️⚠️");
                Serial.printf("No command from SUCA for %lu ms\n", timeSinceRX);
                Serial.println("→ Assuming pet left home - switching to LoRaWAN\n");
                
                api.lorawan.nwm.set();
                delay(200);
                api.system.reboot();
            }
        }

        // TX pet name every 2s để SUCA nhận diện
        static uint32_t lastTX = 0;
        if (millis() - lastTX > 2000)
        {
            // Gửi tên pet để SUCA biết đây là pet nào
            // SUCA sẽ parse tên này và gửi lên API
            uint8_t msg[32];
            memset(msg, 0, sizeof(msg));
            strncpy((char*)msg, PET_NAME, sizeof(msg) - 1);
            
            api.lora.psend(strlen(PET_NAME), msg);
            Serial.printf("[P2P] TX: %s\n", PET_NAME);
            lastTX = millis();
        }

        // NOTE: do not sleep → must always listen for SUCA
    }


    // =========================================================
    // MODE 1 = LoRaWAN MODE (pet far away)
    // =========================================================
    void loop_lorawan()
    {
        uint32_t now = millis();

        // Uplink data với device_id
        if (now - lastLorawanUplink > ABP_PERIOD)
        {
            // Payload: Gửi DEVICE_ID
            // ChirpStack sẽ decode và forward lên LPWAN Mapper API
            // Web sẽ match device_id này với pet đã thêm
            uint8_t payload[32];
            memset(payload, 0, sizeof(payload));
            
            // Format: "DEVICE_ID" hoặc "PET_NAME"
            // Web sẽ tự nhận diện qua deviceName từ ChirpStack
            strncpy((char*)payload, DEVICE_ID, sizeof(payload) - 1);
            
            Serial.printf("[LoRaWAN] Sending: %s\n", DEVICE_ID);
            api.lorawan.send(strlen(DEVICE_ID), payload, 2, true, 1);
            
            lastLorawanUplink = now;
        }

        // Every 30 sec → scan P2P để check xem đã về gần nhà chưa
        if (now - lastP2PScan > 30000)
        {
            Serial.println("\n[SCAN] Searching for SUCA (Home)...");
            lastP2PScan = now;

            api.lora.nwm.set();  // switch to P2P mode
            delay(150);
            api.system.reboot();
        }

        api.system.sleep.all(500);
    }


    // =========================================================
    // SETUP
    // =========================================================
    void setup()
    {
        Serial.begin(115200);
        delay(1500);

        Serial.println("\n========================================");
        Serial.println("  RAK11720 - PET TRACKER NODE");
        Serial.printf("  Pet Name: %s\n", PET_NAME);
        Serial.printf("  Device ID: %s\n", DEVICE_ID);
        Serial.println("  Target: LPWAN Mapper API");
        Serial.println("========================================\n");

        uint8_t mode = api.lora.nwm.get();
        Serial.printf("BOOT MODE = %d (%s)\n\n", mode, mode == 0 ? "P2P" : "LoRaWAN");

        // ----------------- P2P MODE -----------------
        if (mode == 0)
        {
            Serial.println("✓ Starting in P2P MODE (Pet at Home)");
            Serial.println("  → Will communicate with SUCA Board");
            Serial.println("  → SUCA will send data to LPWAN Mapper API\n");

            api.lora.pfreq.set(myFreq);
            api.lora.psf.set(sf);
            api.lora.pbw.set(bw);
            api.lora.pcr.set(cr);
            api.lora.ppl.set(preamble);
            api.lora.ptp.set(txPower);

            api.lora.registerPRecvCallback(recv_cb);
            return;
        }

        // ----------------- LoRaWAN MODE -----------------
        if (mode == 1)
        {
            Serial.println("✓ Starting in LoRaWAN ABP MODE (Pet Away)");
            Serial.println("  → Will send to LoRaWAN Gateway");
            Serial.println("  → ChirpStack → LPWAN Mapper API\n");

            uint8_t devaddr[4] = ABP_DEVADDR;
            uint8_t appskey[16] = ABP_APPSKEY;
            uint8_t nwkskey[16] = ABP_NWKSKEY;

            api.lorawan.njm.set(RAK_LORA_ABP);
            api.lorawan.daddr.set(devaddr, 4);
            api.lorawan.appskey.set(appskey, 16);
            api.lorawan.nwkskey.set(nwkskey, 16);
            api.lorawan.band.set(ABP_BAND);

            api.lorawan.deviceClass.set(RAK_LORA_CLASS_A);
            api.lorawan.adr.set(true);
            api.lorawan.cfm.set(1);

            api.lorawan.registerRecvCallback(lorawan_recv);
            api.lorawan.registerSendCallback(lorawan_send);
            return;
        }
    }


    // =========================================================
    // MAIN LOOP
    // =========================================================
    void loop()
    {
        uint8_t mode = api.lora.nwm.get();

        if (mode == 0) loop_p2p();        // pet is at home
        else           loop_lorawan();    // pet is outside
    }