#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"

void configurarCamera();

long num = 0;
bool espReiniciado;

// Definir o pino que acorda o ESP32
#define WAKEUP_PIN 2 
#include "esp_sleep.h"

camera_fb_t *fb = NULL;

// Camera model
#define CAMERA_MODEL_AI_THINKER

#if defined(CAMERA_MODEL_AI_THINKER)
#include "camera_pins.h"
#endif

void configurarPWMFlash()
{
  // Canal 1, frequência de 5 kHz, resolução de 8 bits
  ledcSetup(LEDC_CHANNEL_1, 5000, 8); 
  ledcAttachPin(4, LEDC_CHANNEL_1);
  ledcWrite(LEDC_CHANNEL_1, 0);
}

void tirarFoto()
{
  ledcWrite(LEDC_CHANNEL_1, 90); 
  delay(500);

  // Captura 3 imagens e mantém apenas a última
  for (int i = 0; i < 4; i++)
  {
    fb = esp_camera_fb_get();
    delay(50);
    if (!fb)
    {
      Serial.printf("Falha na captura da câmera %d\n", i + 1);
      ledcWrite(LEDC_CHANNEL_1, 0); 
      return;
    }

    // Libera o buffer da imagem antiga, exceto na última iteração
    if (i < 3)
    {
      esp_camera_fb_return(fb);
      fb = NULL;
    }
  }

  if (!fb)
  {
    Serial.println("Falha ao capturar a imagem");
    ledcWrite(LEDC_CHANNEL_1, 0);
    return;
  }

  Serial.println("Imagem capturada");
  Serial.println("Tamanho da imagem: " + String(fb->len));

  delay(20);

  // Desliga o flash
  ledcWrite(LEDC_CHANNEL_1, 0);
}

void configurarCamera()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10; 
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }
  else
  {
    Serial.println("Camera inicializada com sucesso");
  }

  // Configuração manual da câmera
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor != nullptr)
  {
    Serial.println("Configurações otimizadas aplicadas com sucesso");
  }
  else
  {
    Serial.println("Falha ao obter sensor da câmera");
    return;
  }
}

// Setup inicial do ESP32-CAM
void setup()
{
  Serial.begin(115200);

  configurarCamera();

  configurarPWMFlash();

  delay(100);

  // Configura o pino com pull-up interno
  pinMode(WAKEUP_PIN, INPUT_PULLUP); 

  // Configurar o ESP32 para acordar no WAKEUP_PIN quando o sinal mudar para LOW
  esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKEUP_PIN, LOW);
  delay(100);
}

void loop() {
  // Captura uma foto
  tirarFoto(); 

  if (fb) {
    //Enviar marcador de início (1 byte único e distinto)
    Serial.write(0xA5); // Marcador de início

    //Enviar o tamanho da imagem (4 bytes)
    uint32_t tamanho = fb->len;
    Serial.write((uint8_t *)&tamanho, sizeof(tamanho));

    //Enviar os dados da imagem
    Serial.write(fb->buf, fb->len);

    // Liberta o framebuffer
    esp_camera_fb_return(fb);
    fb = NULL;

    Serial.printf("Imagem enviada: %u bytes\n", tamanho);
  } else {
    Serial.println("Erro ao capturar a imagem.");
  }

  delay(2000); 
  esp_deep_sleep_start(); 
}