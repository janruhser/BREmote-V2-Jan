import os, sys
vesc_path = 'Source/V2_Minimalist_Rx_Heltec/VESC.ino'
if not os.path.exists(vesc_path):
    print("FAIL: File not found")
    sys.exit(1)
with open(vesc_path, 'r') as f:
    if 'hal_get_vesc_uart' not in f.read():
        print("FAIL: Not using HAL UART")
        sys.exit(1)
print("PASS")
