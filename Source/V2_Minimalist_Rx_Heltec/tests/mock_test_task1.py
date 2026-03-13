import os, sys
header_path = 'Source/V2_Minimalist_Rx_Heltec/BREmote_V2_Rx_Heltec.h'
if not os.path.exists(header_path):
    print("FAIL: File not found")
    sys.exit(1)
with open(header_path, 'r') as f:
    content = f.read()
    if 'struct confStruct' not in content or 'dummy_delete_me' in content:
        print("FAIL: confStruct not correctly defined or legacy fields present")
        sys.exit(1)
print("PASS")
