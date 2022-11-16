#pragma once

void init_internal_fs(void);
void init_spi_sd_fs(void);
void init_external_sd_fs(void);
void list_dir(char *path);
void remove_file(char *filename);
