import os, sys
hal_path = 'Source/V2_Minimalist_Rx_Heltec/HAL_Heltec_CT62.h'
if not os.path.exists(hal_path):
    print("FAIL: File not found")
    sys.exit(1)
with open(hal_path, 'r') as f:
    content = f.read()
    if 'void hal_init()' not in content or 'void hal_esc_write(uint16_t value)' not in content or 'hal_get_vesc_uart' not in content:
        print("FAIL: HAL interfaces missing")
        sys.exit(1)
print("PASS")
