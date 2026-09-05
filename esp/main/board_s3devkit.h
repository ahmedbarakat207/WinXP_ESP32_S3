/* Generic ESP32-S3 devkit (e.g. DevKitC-1 N16R8) bring-up for XP.
 *
 * Headless (no LCD): the VGA frame buffer is kept but not displayed, boot
 * progress goes over serial logs. SD card is an external SPI module: set
 * SD_SPI_* below to your wiring. Input via USB HID (`enable_usb = 1`);
 * do not enable WiFi at the same time (not enough RAM).
 *
 * swap_size = 0 in the ini auto-sizes the resident window from the actual
 * PSRAM size (works for 8MB and 16MB modules).
 */
#define BUILD_ESP32

#define IRAM_ATTR_CPU_EXEC1 IRAM_ATTR

#define BPP 16
#define FULL_UPDATE
#define USE_LCD_HEADLESS
/* VGA canvas (not displayed headless, but the guest renders into it). */
#define LCD_WIDTH  640
#define LCD_HEIGHT 480

/* SD card: external SPI module. Adjust to your wiring. */
#define SD_SPI_MOSI  11
#define SD_SPI_MISO  13
#define SD_SPI_SCK   12
#define SD_SPI_CS    10
#define SD_SPI_FREQ_KHZ 20000

/* No I2S audio on the devkit build (saves RAM for the swap window). */
