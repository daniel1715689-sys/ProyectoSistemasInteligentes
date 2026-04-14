#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <driver/i2s.h>
#include <math.h>

// Configuracion de pines para la pantalla
#define TFT_CS   8
#define TFT_DC   10
#define TFT_RST  9

#define TFT_SCLK 12
#define TFT_MOSI 11

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// GPIOs del microfono
#define I2S_WS  17
#define I2S_SD  15
#define I2S_SCK 18

#define I2S_PORT     I2S_NUM_0
#define SAMPLE_RATE  16000
#define BUFFER_SIZE  1024

int16_t buffer[BUFFER_SIZE];

// MAX98357A Amplificador de audio
#define SPEAKER_BCLK  5
#define SPEAKER_LRC   6
#define SPEAKER_DIN   7
#define SPEAKER_PORT  I2S_NUM_1
#define SPEAKER_RATE  16000

// Definicion de niveles del juego y secciones de pantalla
#define MAX_SEQ 5

int secuencia[MAX_SEQ];
int nivel = 1;

int w = 120;
int h = 160;

// Setup del altavoz
void setupAltavoz() {
  i2s_config_t spk_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SPEAKER_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  i2s_pin_config_t spk_pins = {
    .bck_io_num   = SPEAKER_BCLK,
    .ws_io_num    = SPEAKER_LRC,
    .data_out_num = SPEAKER_DIN,
    .data_in_num  = -1
  };

  i2s_driver_install(SPEAKER_PORT, &spk_config, 0, NULL);
  i2s_set_pin(SPEAKER_PORT, &spk_pins);
}

// Funcion reproducir tono
void reproducirTono(int frecuencia, int duracionMs) {
  int totalMuestras = (SPEAKER_RATE * duracionMs) / 1000;
  int16_t muestra[256];
  int escritas = 0;
  float amplitud = 8000.0f;

  while (escritas < totalMuestras) {
    int bloque = min(256, totalMuestras - escritas);

    for (int i = 0; i < bloque; i++) {
      float t = (float)(escritas + i) / SPEAKER_RATE;
      muestra[i] = (int16_t)(amplitud * sinf(2.0f * PI * frecuencia * t));
    }

    size_t bytesEscritos;
    i2s_write(SPEAKER_PORT, muestra, bloque * sizeof(int16_t), &bytesEscritos, portMAX_DELAY);

    escritas += bloque;
  }
}

// Tonos para cada color
void tonoColor(int color) {
  int frecuencias[] = {262, 294, 330, 392};
  reproducirTono(frecuencias[color], 300);
}

// Colorear 4 secciones
void dibujarBase() {
  tft.fillRect(0, 0, w, h, ST77XX_GREEN);
  tft.fillRect(w, 0, w, h, ST77XX_RED);
  tft.fillRect(0, h, w, h, ST77XX_YELLOW);
  tft.fillRect(w, h, w, h, ST77XX_BLUE);
}

// Mostrar colores de la secuencia
void iluminar(int color) {
  uint16_t c;

  if (color == 0) c = ST77XX_GREEN;
  if (color == 1) c = ST77XX_RED;
  if (color == 2) c = ST77XX_YELLOW;
  if (color == 3) c = ST77XX_BLUE;

  tft.fillScreen(c);
  tonoColor(color);
  delay(350);

  dibujarBase();
  delay(150);
}

// Lectura de voz
int leerVoz() {
  while (true) {
    size_t bytesRead;
    i2s_read(I2S_PORT, buffer, sizeof(buffer), &bytesRead, 1);
    Serial.write((uint8_t*)buffer, bytesRead);

    if (Serial.available()) {
      char comando = Serial.read();

      if (comando == '0') return 0;
      if (comando == '1') return 1;
      if (comando == '2') return 2;
      if (comando == '3') return 3;
    }
  }
}

// Mostrar secuencia
void mostrarSecuencia() {
  for (int i = 0; i < nivel; i++) {
    iluminar(secuencia[i]);
  }
}

// Turno del jugador
bool turnoJugador() {
  for (int i = 0; i < nivel; i++) {
    int voz = leerVoz();

    iluminar(voz);

    if (voz != secuencia[i]) return false;

    delay(200);
  }

  return true;
}

// Pantalla perder
void mostrarPerder() {
  tft.setRotation(1);

  for (int i = 0; i < 6; i++) {
    tft.fillScreen(ST77XX_RED);

    tft.setTextSize(5);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(30, 100);
    tft.print("PERDISTE");

    delay(300);

    tft.fillScreen(ST77XX_BLACK);
    delay(200);
  }

  reproducirTono(400, 300);
  delay(100);
  reproducirTono(300, 300);
  delay(100);
  reproducirTono(200, 500);

  tft.setRotation(0);
  dibujarBase();
}

// Configuracion de la pantalla y numeros aleatorios
void setup() {
  Serial.begin(2000000);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.init(240, 320);
  tft.setRotation(0);

 uint32_t seed = esp_random();
randomSeed(seed);

  dibujarBase();

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);

  setupAltavoz();
}

// Loop colores ganar
void loopColores() {
  int j = 0;

  uint16_t colores[] = {
    ST77XX_GREEN,
    ST77XX_RED,
    ST77XX_BLUE,
    ST77XX_YELLOW,
    ST77XX_CYAN,
    ST77XX_MAGENTA
  };

  while (j < 12) {
    for (int i = 0; i < 6; i++) {
      tft.fillScreen(ST77XX_BLACK);

      tft.setTextSize(6);
      tft.setTextColor(colores[i]);
      tft.setCursor(20, 100);
      tft.print("GANASTE!");

      delay(250);
      j++;
    }
  }
}

// Pantalla ganar
void mostrarGanar() {
  tft.setRotation(1);
  loopColores();

  for (int i = 0; i < 3; i++) {
    reproducirTono(523, 200);
    delay(200);
    reproducirTono(659, 200);
    delay(200);
    reproducirTono(784, 200);
    delay(300);
  }
}

// Loop principal
void loop() {
  for (int i = 0; i < MAX_SEQ; i++) {
    secuencia[i] = random(0, 4);
  }

  nivel = 1;

  while (true) {
    mostrarSecuencia();

    if (!turnoJugador()) {
      delay(1000);
      mostrarPerder();
      delay(1000);
      break;
    }

    if (nivel == MAX_SEQ) {
      delay(1000);
      mostrarGanar();
      delay(1000);
      tft.setRotation(0);
      break;
    }

    nivel++;
    delay(800);
  }
}