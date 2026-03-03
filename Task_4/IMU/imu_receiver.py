import serial

COM_PORT  = "/dev/ttyUSB0"
BAUD_RATE = 115200
XOR_KEY   = 0x5A

def xor_decrypt(data):
    return "".join(chr(b ^ XOR_KEY) for b in data)

def verify_checksum(plain_str, received_cs):
    try:
        expected = int(received_cs, 16)
        actual = 0
        for ch in plain_str:
            actual ^= ord(ch)
        return actual == expected
    except ValueError:
        return False

def parse_values(plain_str):
    values = {}
    for part in plain_str.split(","):
        key, val = part.split(":")
        values[key.strip()] = float(val.strip())
    return values

print(f"Opening {COM_PORT} at {BAUD_RATE} baud...")
print("Waiting for IMU data (Ctrl+C to stop)\n")
print(f"{'AX(g)':>8} {'AY(g)':>8} {'AZ(g)':>8} | {'GX':>9} {'GY':>9} {'GZ':>9}  STATUS")
print("-" * 72)

ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=2)
good = 0
bad  = 0

try:
    while True:
        raw_line = ser.readline()
        if not raw_line:
            continue
        try:
            raw_line = raw_line.strip()
            if not raw_line.startswith(b'$'):
                raise ValueError("No start marker")
            raw_line = raw_line[1:]
            cs_marker = b",CS:"
            cs_pos = raw_line.rfind(cs_marker)
            if cs_pos == -1:
                raise ValueError("No CS field")
            encrypted_data = raw_line[:cs_pos]
            received_cs = raw_line[cs_pos + len(cs_marker):].decode("ascii").strip()
            plain_str = xor_decrypt(encrypted_data)
            if not verify_checksum(plain_str, received_cs):
                raise ValueError(f"Checksum FAIL")
            v = parse_values(plain_str)
            good += 1
            print(f"{v['AX']:>+8.2f} {v['AY']:>+8.2f} {v['AZ']:>+8.2f} | "
                  f"{v['GX']:>+9.1f} {v['GY']:>+9.1f} {v['GZ']:>+9.1f}  "
                  f"OK [{good} good / {bad} bad]")
        except Exception as e:
            bad += 1
            print(f"  SKIP: {e} [{good} good / {bad} bad]")
except KeyboardInterrupt:
    print(f"\nStopped. {good} good, {bad} bad.")
    ser.close()
