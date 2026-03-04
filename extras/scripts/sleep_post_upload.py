import time
Import("env")

def after_upload(source, target, env):
    board_config = env.BoardConfig()
    mcu = board_config.get("build.mcu", "")
    if "esp32s3" in mcu:
        print("Waiting for serial port to reappear after ESP32-S3 upload...")
        time.sleep(1)

env.AddPostAction("upload", after_upload)

