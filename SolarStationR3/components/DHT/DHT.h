#define DHT11 11
#define DHT22 22

bool dht_read(uint8_t type, uint8_t pin, float *tempC, float *humidity);
