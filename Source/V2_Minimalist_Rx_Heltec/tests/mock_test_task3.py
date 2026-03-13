import os, sys
radio_path = 'Source/V2_Minimalist_Rx_Heltec/Radio.ino'
if not os.path.exists(radio_path):
    print("FAIL: File not found")
    sys.exit(1)
with open(radio_path, 'r') as f:
    content = f.read()
    if 'hal_radio_switch_mode' not in content:
        print("FAIL: Radio not using HAL switch mode")
        sys.exit(1)
print("PASS")
