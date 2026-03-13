import os, sys
cfg_path = 'Source/V2_Minimalist_Rx_Heltec/ConfigService.ino'
if not os.path.exists(cfg_path):
    print("FAIL: File not found")
    sys.exit(1)
with open(cfg_path, 'r') as f:
    content = f.read()
    if 'aw.digitalWrite' in content or 'steering_type' in content or 'ubat_cal' in content:
        print("FAIL: Legacy hardware calls or struct fields remain in ConfigService")
        sys.exit(1)
print("PASS")
