Development Notes
=================


Project Version
---------------

From the [OTA Example](https://github.com/espressif/esp-idf/tree/b015061/examples/system/ota) documentation:


1. If `CONFIG_APP_PROJECT_VER_FROM_CONFIG` option is set, the value of `CONFIG_APP_PROJECT_VER` will be used.
2. Else, if `PROJECT_VER` variable set in project Cmake/Makefile file, its value will be used.
3. Else, if the `$PROJECT_PATH/version.txt` exists, its contents will be used as `PROJECT_VER`.
4. Else, if the project is located inside a Git repository, the output of git describe will be used.
5. Otherwise, `PROJECT_VER` will be `"1"`.
