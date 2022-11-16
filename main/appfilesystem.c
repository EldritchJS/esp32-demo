#include <string.h>
#include <esp_vfs.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <esp_spiffs.h>
#include "appdefs.h"
#include "appfilesystem.h"

static const char *TAG = "appfilesystem";

#define PIN_NUM_MISO CONFIG_SD_FS_GPIO_MISO
#define PIN_NUM_MOSI CONFIG_SD_FS_GPIO_MOSI //GPIO_NUM_7
#define PIN_NUM_CLK  CONFIG_SD_FS_GPIO_CLK //GPIO_NUM_6
#define PIN_NUM_CS   CONFIG_SD_FS_GPIO_CS //GPIO_NUM_10

void remove_file(char *filename)
{
  remove(filename);
}

void list_dir(char *path)
{	
  DIR *dir = NULL;
  struct dirent *ent;
  char type;
  char size[9];
  char tpath[255];
  char tbuffer[80];
  struct stat sb;
  struct tm *tm_info;
  char *lpath = NULL;
  int statok;
  int nfiles = 0;
  uint64_t total = 0;

  dir = opendir(path);
  
  ESP_LOGI(TAG, "T  Size      Date/Time         Name");
  ESP_LOGI(TAG, "-----------------------------------");

  while ((ent = readdir(dir)) != NULL) {
    sprintf(tpath, path);
    if (path[strlen(path)-1] != '/') strcat(tpath,"/");
    strcat(tpath,ent->d_name);
    tbuffer[0] = '\0';

    // Get file stat
    statok = stat(tpath, &sb);

    if (statok == 0) {
      tm_info = localtime(&sb.st_mtime);
      strftime(tbuffer, 80, "%d/%m/%Y %R", tm_info);
    }
    else sprintf(tbuffer, "                ");

    if (ent->d_type == DT_REG) {
      type = 'f';
      nfiles++;
      if (statok) strcpy(size, "       ?");
      else {
        total += sb.st_size;
        /*Addback*/ //if (sb.st_size < (1024*1024)) sprintf(size,"%8d", (int)sb.st_size);
        //else if ((sb.st_size/1024) < (1024*1024)) sprintf(size,"%6dKB", (int)(sb.st_size / 1024));
          //else sprintf(size,"%6dMB", (int)(sb.st_size / (1024 * 1024)));
      }
    }
    else {
      type = 'd';
      strcpy(size, "       -");
    }

    printf("%c  %s  %s  %s\r\n",
      type,
      size,
      tbuffer,
      ent->d_name
      );
  }
  if (total) {
    printf("-----------------------------------\n");
    if (total < (1024*1024)) printf("   %8d", (int)total);
    else if ((total/1024) < (1024*1024)) printf("   %6dKB", (int)(total / 1024));
    else printf("   %6dMB", (int)(total / (1024 * 1024)));
    printf(" in %d file(s)\n", nfiles);
  }
  printf("-----------------------------------\n");
  closedir(dir);
  free(lpath);
}

void init_internal_fs(void)
{
  ESP_LOGI(TAG, "Initializing internal SPIFFS");

  esp_vfs_spiffs_conf_t conf = {
    .base_path = CONFIG_FLASH_FS_MOUNT_POINT,
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = false
  };

  // Use settings defined above to initialize and mount SPIFFS filesystem.
  // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_spiffs_register(&conf); 

  if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
          ESP_LOGE(TAG, "Failed to mount or format internal SPIFFS");
      } else if (ret == ESP_ERR_NOT_FOUND) {
          ESP_LOGE(TAG, "Failed to find internal SPIFFS partition");
      } else {
          ESP_LOGE(TAG, "Failed to initialize internal SPIFFS (%s)", esp_err_to_name(ret));
      }
      return;
  }
}

void init_spi_sd_fs(void)
{
  // Use settings defined above to initialize SD card and mount FAT filesystem.
  // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
  // Please check its source code and implement error recovery when developing
  // production applications.
  ESP_LOGI(TAG, "Using SPI peripheral");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  spi_bus_config_t bus_cfg = {
      .mosi_io_num = PIN_NUM_MOSI,
      .miso_io_num = PIN_NUM_MISO,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4000,
  };

  esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize bus.");
      return;
  }
  
}
void init_external_sd_fs(void)
{
  esp_err_t ret;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };
  sdmmc_card_t *card;
  const char mount_point[] = CONFIG_SD_FS_MOUNT_POINT;
  ESP_LOGI(TAG, "Initializing SD card");
#if 0
  // Use settings defined above to initialize SD card and mount FAT filesystem.
  // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
  // Please check its source code and implement error recovery when developing
  // production applications.
  ESP_LOGI(TAG, "Using SPI peripheral");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  spi_bus_config_t bus_cfg = {
      .mosi_io_num = PIN_NUM_MOSI,
      .miso_io_num = PIN_NUM_MISO,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4000,
  };

  ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize bus.");
      return;
  }
#endif
  // This initializes the slot without card detect (CD) and write protect (WP) signals.
  // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = PIN_NUM_CS;
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  slot_config.host_id = host.slot;

  ESP_LOGI(TAG, "Mounting filesystem");
  ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
          ESP_LOGE(TAG, "Failed to mount filesystem.");
      } else {
          ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                   "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
      }
      return;
  }
  ESP_LOGI(TAG, "Filesystem mounted");

  // Card has been initialized, print its properties
  sdmmc_card_print_info(stdout, card);
}

