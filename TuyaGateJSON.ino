/*
example curl.exe --% -X POST http://IP ADDRESS OF THE GATE/control -H "Content-Type: application/json" -d "{\"ip\":\"IP ADDRESS OF THE SWITCH\",\"id\":\"ID OF THE SWITCH\",\"key\":\"TUYA SWITCH LOCAL KEY\",\"dps\":{\"1\":true}}" true to switch on, false to switch off   
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <bearssl/bearssl_block.h>
#include <ArduinoJson.h>
#include <time.h>

// --- Ваши настройки Wi-Fi ---
const char* ssid     = "YOUR SSID";
const char* password = "WIFI PASSWORD";

ESP8266WebServer server(80);

// --- Функция расчета CRC32 пакета Tuya ---
uint32_t get_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return ~crc;
}

// --- Шифрование AES-128-ECB средствами BearSSL ---
int encrypt_tuya_payload(const uint8_t *input, size_t input_len, uint8_t *output, const char *key) {
    br_aes_ct_cbcenc_keys ctx;
    br_aes_ct_cbcenc_init(&ctx, key, 16);

    size_t padded_len = ((input_len / 16) + 1) * 16;
    uint8_t padding_val = padded_len - input_len;

    for (size_t i = 0; i < padded_len; i++) {
        output[i] = (i < input_len) ? input[i] : padding_val;
    }

    for (size_t i = 0; i < padded_len; i += 16) {
        uint8_t iv[16] = {0}; // Секрет режима ECB - нулевой вектор IV
        br_aes_ct_cbcenc_run(&ctx, iv, output + i, 16);
    }
    return padded_len;
}

// --- Отправка бинарного пакета в устройство Tuya ---
bool sendTuyaCommand(const char* ip, const char* id, const char* key, const String& dpsString) {
    WiFiClient client;
    client.setTimeout(2000); 

    if (!client.connect(ip, 6668)) {
        Serial.println("Ошибка TCP соединения с IP: " + String(ip));
        return false;
    }

    // Время для защиты от повторов (Tuya Anti-replay)
    String timestamp = String((uint32_t)time(nullptr));
    if(timestamp.toInt() < 1000) timestamp = "1717960000";

    // Формируем внутренний JSON для самой лампы
    String jsonStr = "{\"devId\":\"" + String(id) + "\",\"uid\":\"" + String(id) + "\",\"t\":\"" + timestamp + "\",\"dps\":" + dpsString + "}";
    Serial.println("Внутренний JSON: " + jsonStr);

    uint8_t encryptedData[512];
    int enc_len = encrypt_tuya_payload((const uint8_t*)jsonStr.c_str(), jsonStr.length(), encryptedData, key);

    uint8_t payload[600];
    int payload_len = 15 + enc_len;
    memcpy(payload, "3.3", 3);
    memset(payload + 3, 0, 12);
    memcpy(payload + 15, encryptedData, enc_len);

    uint8_t packet[650];
    int packet_len = 16 + payload_len + 8;
    memset(packet, 0, packet_len);
    
    // Заголовок Tuya
    packet[2] = 0x55; packet[3] = 0xAA;
    packet[7] = 0x01; // sequence ID
    packet[11] = 0x07; // command = 7
    
    uint32_t len_field = payload_len + 8;
    packet[12] = (len_field >> 24) & 0xFF; packet[13] = (len_field >> 16) & 0xFF;
    packet[14] = (len_field >> 8) & 0xFF;  packet[15] = len_field & 0xFF;
    
    memcpy(packet + 16, payload, payload_len);
    
    uint32_t crc = get_crc32(packet, 16 + payload_len);
    int crc_pos = 16 + payload_len;
    packet[crc_pos]   = (crc >> 24) & 0xFF; packet[crc_pos+1] = (crc >> 16) & 0xFF;
    packet[crc_pos+2] = (crc >> 8) & 0xFF;  packet[crc_pos+3] = crc & 0xFF;
    
    packet[crc_pos+6] = 0xAA; packet[crc_pos+7] = 0x55; // Хвост

    client.write(packet, packet_len);
    delay(20);
    client.stop();
    return true; 
}

// --- Обработка входящего POST-запроса ---
void handleControl() {
    if (server.method() != HTTP_POST) {
        server.send(405, "application/json", "{\"error\":\"Use POST method.\"}");
        return;
    }

    String body = server.arg("plain");
    
    // Парсим присланный JSON
    StaticJsonDocument<1024> doc; // Для тех, у кого ArduinoJson v6
    // Если у вас ArduinoJson v7+, замените строчку выше на: JsonDocument doc;
    
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        server.send(400, "text/plain", "bad json");
        return;
    }

    const char* ip  = doc["ip"];
    const char* id  = doc["id"];
    const char* key = doc["key"];
    
    if (!ip || !id || !key || !doc.containsKey("dps")) {
        server.send(400, "application/json", "{\"error\":\"Missing fields: ip, id, key, dps\"}");
        return;
    }

    // Вытаскиваем объект "dps" и конвертируем его в строку
    JsonObject dpsObj = doc["dps"];
    String dpsString;
    serializeJson(dpsObj, dpsString); 

    // Отправляем команду
    if (sendTuyaCommand(ip, id, key, dpsString)) {
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server.send(500, "application/json", "{\"error\":\"offline\"}");
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nМега-Шлюз запущен! IP: " + WiFi.localIP().toString());

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    // Вы просили URL /control
    server.on("/control", handleControl);
    server.begin();
}

void loop() {
    server.handleClient();
}
