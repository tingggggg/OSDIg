import os
import sys
import serial
import time

program = open(sys.argv[2], "rb")
payload = program.read()
payloadsize = len(payload).to_bytes(4, byteorder="big")

ser = serial.Serial(sys.argv[1], 115200, timeout=3)

ser.reset_input_buffer()
ser.reset_output_buffer()

ser.write(payloadsize)
c = ser.read(4)
print("payloadsize : ", payloadsize)
print("c : ", c)

if payloadsize != c:
    raise Exception("payloadsize received incorrect!")

print("total bytes to send: ", len(payload))
index = 0
step = 1000
for i in range(0, len(payload), step):
    print(f"{i} : {len(payload)}")
    n = ser.write(payload[i:i+step])

    c = ser.read(n)

    if payload[i:i+n] != c:
        raise Exception("payload received incorrect!")
    
    index += n

c = ser.read()
if c == b"#":
    print("total received bytes:", index)

ser.write(b"D")

ser.close()
program.close()
