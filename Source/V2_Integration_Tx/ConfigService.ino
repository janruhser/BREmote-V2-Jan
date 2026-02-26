#include <stddef.h>

enum CfgFieldType
{
  CFG_U16,
  CFG_I16,
  CFG_FLOAT,
  CFG_ADDR3
};

struct CfgFieldSpec
{
  const char* key;
  CfgFieldType type;
  size_t offset;
  bool writable;
  bool radioReinitRequired;
  bool hasRange;
  float minValue;
  float maxValue;
  uint8_t floatPrecision;
  bool mustEqualSwVersion;
};

static const CfgFieldSpec kCfgFields[] = {
  {"radio_preset", CFG_U16, offsetof(confStruct, radio_preset), true, true, true, 1.0f, 3.0f, 0, false},
  {"rf_power", CFG_I16, offsetof(confStruct, rf_power), true, true, true, -9.0f, 22.0f, 0, false},
  {"max_gears", CFG_U16, offsetof(confStruct, max_gears), true, false, true, 1.0f, 10.0f, 0, false},
  {"startgear", CFG_U16, offsetof(confStruct, startgear), true, false, true, 0.0f, 9.0f, 0, false},
  {"no_lock", CFG_U16, offsetof(confStruct, no_lock), true, false, true, 0.0f, 1.0f, 0, false},
  {"no_gear", CFG_U16, offsetof(confStruct, no_gear), true, false, true, 0.0f, 1.0f, 0, false},
  {"steer_enabled", CFG_U16, offsetof(confStruct, steer_enabled), true, false, true, 0.0f, 1.0f, 0, false},
  {"thr_expo", CFG_U16, offsetof(confStruct, thr_expo), true, false, true, 0.0f, 100.0f, 0, false},
  {"tog_deadzone", CFG_U16, offsetof(confStruct, tog_deadzone), true, false, true, 100.0f, 3000.0f, 0, false},
  {"tog_diff", CFG_U16, offsetof(confStruct, tog_diff), true, false, true, 1.0f, 200.0f, 0, false},
  {"tog_block_time", CFG_U16, offsetof(confStruct, tog_block_time), true, false, true, 0.0f, 5000.0f, 0, false},
  {"menu_timeout", CFG_U16, offsetof(confStruct, menu_timeout), true, false, true, 0.0f, 1000.0f, 0, false},
  {"version", CFG_U16, offsetof(confStruct, version), true, false, true, (float)SW_VERSION, (float)SW_VERSION, 0, true},
  {"cal_ok", CFG_U16, offsetof(confStruct, cal_ok), true, false, true, 0.0f, 1.0f, 0, false},
  {"cal_offset", CFG_U16, offsetof(confStruct, cal_offset), true, false, true, 0.0f, 65535.0f, 0, false},
  {"thr_idle", CFG_U16, offsetof(confStruct, thr_idle), true, false, true, 0.0f, 65535.0f, 0, false},
  {"thr_pull", CFG_U16, offsetof(confStruct, thr_pull), true, false, true, 0.0f, 65535.0f, 0, false},
  {"tog_left", CFG_U16, offsetof(confStruct, tog_left), true, false, true, 0.0f, 65535.0f, 0, false},
  {"tog_mid", CFG_U16, offsetof(confStruct, tog_mid), true, false, true, 0.0f, 65535.0f, 0, false},
  {"tog_right", CFG_U16, offsetof(confStruct, tog_right), true, false, true, 0.0f, 65535.0f, 0, false},
  {"trig_unlock_timeout", CFG_U16, offsetof(confStruct, trig_unlock_timeout), true, false, true, 0.0f, 65535.0f, 0, false},
  {"lock_waittime", CFG_U16, offsetof(confStruct, lock_waittime), true, false, true, 0.0f, 65535.0f, 0, false},
  {"gear_change_waittime", CFG_U16, offsetof(confStruct, gear_change_waittime), true, false, true, 0.0f, 65535.0f, 0, false},
  {"gear_display_time", CFG_U16, offsetof(confStruct, gear_display_time), true, false, true, 0.0f, 65535.0f, 0, false},
  {"err_delete_time", CFG_U16, offsetof(confStruct, err_delete_time), true, false, true, 0.0f, 65535.0f, 0, false},
  {"thr_expo1", CFG_U16, offsetof(confStruct, thr_expo1), true, false, true, 0.0f, 65535.0f, 0, false},
  {"steer_expo", CFG_U16, offsetof(confStruct, steer_expo), true, false, true, 0.0f, 65535.0f, 0, false},
  {"steer_expo1", CFG_U16, offsetof(confStruct, steer_expo1), true, false, true, 0.0f, 65535.0f, 0, false},
  {"ubat_cal", CFG_FLOAT, offsetof(confStruct, ubat_cal), true, false, true, 0.000001f, 1.0f, 9, false},
  {"gps_en", CFG_U16, offsetof(confStruct, gps_en), true, false, true, 0.0f, 1.0f, 0, false},
  {"followme_mode", CFG_U16, offsetof(confStruct, followme_mode), true, false, true, 0.0f, 3.0f, 0, false},
  {"kalman_en", CFG_U16, offsetof(confStruct, kalman_en), true, false, true, 0.0f, 1.0f, 0, false},
  {"speed_src", CFG_U16, offsetof(confStruct, speed_src), true, false, true, 0.0f, 3.0f, 0, false},
  {"tx_gps_stale_timeout_ms", CFG_U16, offsetof(confStruct, tx_gps_stale_timeout_ms), true, false, true, 0.0f, 65535.0f, 0, false},
  {"paired", CFG_U16, offsetof(confStruct, paired), true, false, true, 0.0f, 1.0f, 0, false},
  {"own_address", CFG_ADDR3, offsetof(confStruct, own_address), true, false, false, 0.0f, 0.0f, 0, false},
  {"dest_address", CFG_ADDR3, offsetof(confStruct, dest_address), true, false, false, 0.0f, 0.0f, 0, false}
};

static const size_t kCfgFieldCount = sizeof(kCfgFields) / sizeof(kCfgFields[0]);

static bool cfgParseIntStrict(const String& text, long &out)
{
  String s = text;
  s.trim();
  if (s.length() == 0) return false;

  char *endPtr = NULL;
  long value = strtol(s.c_str(), &endPtr, 10);
  if (endPtr == s.c_str() || *endPtr != '\0') return false;

  out = value;
  return true;
}

static bool cfgParseFloatStrict(const String& text, float &out)
{
  String s = text;
  s.trim();
  if (s.length() == 0) return false;

  char *endPtr = NULL;
  float value = strtof(s.c_str(), &endPtr);
  if (endPtr == s.c_str() || *endPtr != '\0') return false;
  if (isnan(value) || isinf(value)) return false;

  out = value;
  return true;
}

static bool cfgParseUInt16Strict(const String& text, uint16_t &out)
{
  long v = 0;
  if (!cfgParseIntStrict(text, v)) return false;
  if (v < 0 || v > 65535) return false;
  out = (uint16_t)v;
  return true;
}

static bool cfgParseByteToken(const String& tokenIn, uint8_t &out, bool forceHex)
{
  String token = tokenIn;
  token.trim();
  if (token.length() == 0) return false;

  bool hexLike = false;
  for (int i = 0; i < token.length(); i++)
  {
    const char c = token[i];
    if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
    {
      hexLike = true;
      break;
    }
  }

  int base = 10;
  if (forceHex || token.startsWith("0x") || token.startsWith("0X") || hexLike) base = 16;

  char *endPtr = NULL;
  long value = strtol(token.c_str(), &endPtr, base);
  if (endPtr == token.c_str() || *endPtr != '\0') return false;
  if (value < 0 || value > 255) return false;

  out = (uint8_t)value;
  return true;
}

static bool cfgParseAddress3(const String& textIn, uint8_t out[3])
{
  String text = textIn;
  text.trim();
  if (text.length() == 0) return false;
  const bool forceHex = (text.indexOf(':') >= 0 || text.indexOf('-') >= 0);

  text.replace(':', ',');
  text.replace('-', ',');
  text.replace(';', ',');

  int start = 0;
  int idx = 0;
  while (start <= text.length())
  {
    int sep = text.indexOf(',', start);
    if (sep < 0) sep = text.length();
    String token = text.substring(start, sep);
    token.trim();

    if (token.length() > 0)
    {
      if (idx >= 3) return false;
      if (!cfgParseByteToken(token, out[idx], forceHex)) return false;
      idx++;
    }

    if (sep >= text.length()) break;
    start = sep + 1;
  }

  return idx == 3;
}

static String cfgFormatAddress3(const uint8_t addr[3])
{
  char buf[9];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X", addr[0], addr[1], addr[2]);
  return String(buf);
}

static String cfgNormalizeKey(const String& keyIn)
{
  String key = keyIn;
  key.trim();
  key.toLowerCase();
  return key;
}

static const CfgFieldSpec* cfgFindFieldByKey(const String& keyIn)
{
  const String key = cfgNormalizeKey(keyIn);
  for (size_t i = 0; i < kCfgFieldCount; i++)
  {
    if (key == kCfgFields[i].key) return &kCfgFields[i];
  }
  return NULL;
}

static bool cfgApplyFieldValue(confStruct &target, const CfgFieldSpec& spec, const String& valueIn, String &err)
{
  if (!spec.writable)
  {
    err = "ERR_READ_ONLY:" + String(spec.key);
    return false;
  }

  uint8_t* base = reinterpret_cast<uint8_t*>(&target);
  uint8_t* ptr = base + spec.offset;

  if (spec.type == CFG_ADDR3)
  {
    uint8_t parsed[3];
    if (!cfgParseAddress3(valueIn, parsed))
    {
      err = "ERR_BAD_VALUE:" + String(spec.key);
      return false;
    }
    uint8_t* addr = reinterpret_cast<uint8_t*>(ptr);
    addr[0] = parsed[0];
    addr[1] = parsed[1];
    addr[2] = parsed[2];
    return true;
  }

  if (spec.type == CFG_FLOAT)
  {
    float fv = 0.0f;
    if (!cfgParseFloatStrict(valueIn, fv))
    {
      err = "ERR_BAD_VALUE:" + String(spec.key);
      return false;
    }
    if (spec.hasRange && (fv < spec.minValue || fv > spec.maxValue))
    {
      err = "ERR_RANGE:" + String(spec.key);
      return false;
    }
    *reinterpret_cast<float*>(ptr) = fv;
    return true;
  }

  if (spec.type == CFG_U16)
  {
    uint16_t u16 = 0;
    if (!cfgParseUInt16Strict(valueIn, u16))
    {
      err = "ERR_BAD_VALUE:" + String(spec.key);
      return false;
    }

    if (spec.mustEqualSwVersion && u16 != SW_VERSION)
    {
      err = "ERR_RANGE:" + String(spec.key);
      return false;
    }
    if (spec.hasRange && ((float)u16 < spec.minValue || (float)u16 > spec.maxValue))
    {
      err = "ERR_RANGE:" + String(spec.key);
      return false;
    }

    *reinterpret_cast<uint16_t*>(ptr) = u16;
    return true;
  }

  if (spec.type == CFG_I16)
  {
    long iv = 0;
    if (!cfgParseIntStrict(valueIn, iv))
    {
      err = "ERR_BAD_VALUE:" + String(spec.key);
      return false;
    }

    if (iv < -32768 || iv > 32767)
    {
      err = "ERR_RANGE:" + String(spec.key);
      return false;
    }
    if (spec.hasRange && ((float)iv < spec.minValue || (float)iv > spec.maxValue))
    {
      err = "ERR_RANGE:" + String(spec.key);
      return false;
    }
    *reinterpret_cast<int16_t*>(ptr) = (int16_t)iv;
    return true;
  }

  err = "ERR_BAD_VALUE:" + String(spec.key);
  return false;
}

static bool cfgReadFieldValue(const confStruct &source, const CfgFieldSpec& spec, String &outValue)
{
  const uint8_t* base = reinterpret_cast<const uint8_t*>(&source);
  const uint8_t* ptr = base + spec.offset;

  if (spec.type == CFG_U16)
  {
    outValue = String(*reinterpret_cast<const uint16_t*>(ptr));
    return true;
  }
  if (spec.type == CFG_I16)
  {
    outValue = String(*reinterpret_cast<const int16_t*>(ptr));
    return true;
  }
  if (spec.type == CFG_FLOAT)
  {
    outValue = String(
      (double)(*reinterpret_cast<const float*>(ptr)),
      (unsigned int)spec.floatPrecision
    );
    return true;
  }
  if (spec.type == CFG_ADDR3)
  {
    outValue = cfgFormatAddress3(reinterpret_cast<const uint8_t*>(ptr));
    return true;
  }
  return false;
}

static bool cfgAppendFieldJson(const confStruct &source, const CfgFieldSpec& spec, String &out)
{
  const uint8_t* base = reinterpret_cast<const uint8_t*>(&source);
  const uint8_t* ptr = base + spec.offset;

  out += "\"";
  out += spec.key;
  out += "\":";

  if (spec.type == CFG_U16)
  {
    out += String(*reinterpret_cast<const uint16_t*>(ptr));
    return true;
  }
  if (spec.type == CFG_I16)
  {
    out += String(*reinterpret_cast<const int16_t*>(ptr));
    return true;
  }
  if (spec.type == CFG_FLOAT)
  {
    out += String(
      (double)(*reinterpret_cast<const float*>(ptr)),
      (unsigned int)spec.floatPrecision
    );
    return true;
  }
  if (spec.type == CFG_ADDR3)
  {
    const uint8_t* addr = reinterpret_cast<const uint8_t*>(ptr);
    out += "\"";
    out += cfgFormatAddress3(addr);
    out += "\"";
    return true;
  }
  return false;
}

static bool cfgValidateCrossField(confStruct &candidate, String &err)
{
  if (candidate.max_gears < 1 || candidate.max_gears > 10)
  {
    err = "ERR_RANGE:max_gears";
    return false;
  }
  if (candidate.startgear >= candidate.max_gears)
  {
    candidate.startgear = candidate.max_gears - 1;
  }
  return true;
}

bool cfgGetValueByKey(const String& keyIn, String &outValue, String &err)
{
  const CfgFieldSpec* spec = cfgFindFieldByKey(keyIn);
  if (spec == NULL)
  {
    err = "ERR_UNKNOWN_KEY:" + cfgNormalizeKey(keyIn);
    return false;
  }

  if (!cfgReadFieldValue(usrConf, *spec, outValue))
  {
    err = "ERR_BAD_VALUE:" + String(spec->key);
    return false;
  }

  return true;
}

bool cfgGetAllJson(String &out)
{
  out = "{";
  for (size_t i = 0; i < kCfgFieldCount; i++)
  {
    if (!cfgAppendFieldJson(usrConf, kCfgFields[i], out)) return false;
    if (i + 1 < kCfgFieldCount) out += ",";
  }
  out += "}";
  return true;
}

bool cfgSetValueByKey(const String& key, const String& value, String &err, bool &radioReinitRequired)
{
  const CfgFieldSpec* spec = cfgFindFieldByKey(key);
  if (spec == NULL)
  {
    err = "ERR_UNKNOWN_KEY:" + cfgNormalizeKey(key);
    return false;
  }

  confStruct staged = usrConf;
  if (!cfgApplyFieldValue(staged, *spec, value, err)) return false;
  if (!cfgValidateCrossField(staged, err)) return false;

  radioReinitRequired = spec->radioReinitRequired;
  usrConf = staged;
  return true;
}

bool cfgSetBatch(const String& payload, String &err, bool &radioReinitRequired)
{
  confStruct staged = usrConf;
  radioReinitRequired = false;

  int start = 0;
  while (start < payload.length())
  {
    int end = payload.indexOf(';', start);
    if (end < 0) end = payload.length();

    String token = payload.substring(start, end);
    token.trim();

    if (token.length() > 0)
    {
      const int eq = token.indexOf('=');
      if (eq <= 0)
      {
        err = "ERR_BAD_VALUE:batch_token";
        return false;
      }

      const String key = token.substring(0, eq);
      const String value = token.substring(eq + 1);

      const CfgFieldSpec* spec = cfgFindFieldByKey(key);
      if (spec == NULL)
      {
        err = "ERR_UNKNOWN_KEY:" + cfgNormalizeKey(key);
        return false;
      }

      if (!cfgApplyFieldValue(staged, *spec, value, err)) return false;
      radioReinitRequired = radioReinitRequired || spec->radioReinitRequired;
    }

    start = end + 1;
  }

  if (!cfgValidateCrossField(staged, err)) return false;
  usrConf = staged;
  return true;
}
