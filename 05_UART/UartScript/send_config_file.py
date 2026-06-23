import serial
import time

ser = serial.Serial("COM13", 115200, timeout=1)

with open("config.txt", "r") as f:
    for line in f:
        ser.write(line.encode())
        ser.write(b"\r\n")
        time.sleep(0.01)

ser.close()
