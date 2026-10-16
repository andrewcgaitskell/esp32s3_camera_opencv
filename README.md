# cam_test (LilyGO T-Camera S3, ESP-IDF 5.4.1)

1. Activate IDF:  source ~/.espressif/tools/activate_idf_v5.4.1.sh
2. Edit main/secrets.h with your Wi-Fi details
3. idf.py set-target esp32s3
4. idf.py build
5. idf.py -p /dev/cu.usbmodem* flash monitor
6. Open the printed http://<ip>/ in a browser on the same network

If flashing fails, hold BOOT while plugging in USB.
Pins/flash size are in main/main.c and sdkconfig.defaults - verify against your board revision.
