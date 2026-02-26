static const char* WEB_CFG_AP_SSID = "BREmoteV2-TX-WebConfig";
static const char* WEB_CFG_AP_PASS = "12345678";
static WebServer webCfgServer(80);
static const uint8_t WEB_CFG_DBG_OFF = 0;
static const uint8_t WEB_CFG_DBG_SOME = 1;
static const uint8_t WEB_CFG_DBG_FULL = 2;
static bool web_cfg_ap_started = false;
static bool web_cfg_ap_had_client = false;
static bool web_cfg_tx_unlocked = false;
static uint8_t web_cfg_last_station_count = 0;
static uint32_t web_cfg_ap_started_at_ms = 0;
static String web_cfg_last_shutdown_reason = "";

static void webCfgStopService(const char* reason)
{
  if(!web_cfg_ap_started) return;
  web_cfg_last_shutdown_reason = reason ? String(reason) : String("unknown");
  webCfgServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  web_cfg_ap_started = false;
  web_cfg_service_enabled = false;
  Serial.print("Web config AP stopped. Reason: ");
  Serial.println(web_cfg_last_shutdown_reason);
}

static const char* webCfgMethodName(HTTPMethod method)
{
  switch(method)
  {
    case HTTP_GET: return "GET";
    case HTTP_POST: return "POST";
    case HTTP_PUT: return "PUT";
    case HTTP_PATCH: return "PATCH";
    case HTTP_DELETE: return "DELETE";
    case HTTP_OPTIONS: return "OPTIONS";
    default: return "OTHER";
  }
}

static bool webCfgDebugEnabledFor(bool importantOnly)
{
  if (web_cfg_debug_mode == WEB_CFG_DBG_OFF) return false;
  if (web_cfg_debug_mode == WEB_CFG_DBG_FULL) return true;
  return importantOnly;
}

String webCfgGetDebugModeName()
{
  if (web_cfg_debug_mode == WEB_CFG_DBG_FULL) return "full";
  if (web_cfg_debug_mode == WEB_CFG_DBG_SOME) return "some";
  return "off";
}

bool webCfgSetDebugMode(const String& modeName)
{
  String m = modeName;
  m.trim();
  m.toLowerCase();
  if (m == "off" || m == "0")
  {
    web_cfg_debug_mode = WEB_CFG_DBG_OFF;
    return true;
  }
  if (m == "some" || m == "1")
  {
    web_cfg_debug_mode = WEB_CFG_DBG_SOME;
    return true;
  }
  if (m == "full" || m == "2")
  {
    web_cfg_debug_mode = WEB_CFG_DBG_FULL;
    return true;
  }
  return false;
}

static void webCfgLogReq(const char* action, const String& detail, bool importantOnly = false)
{
  if (!webCfgDebugEnabledFor(importantOnly)) return;
  Serial.print("[WEB] ");
  Serial.print(webCfgMethodName(webCfgServer.method()));
  Serial.print(" ");
  Serial.print(webCfgServer.uri());
  Serial.print(" | ");
  Serial.print(action);
  if(detail.length() > 0)
  {
    Serial.print(" | ");
    Serial.print(detail);
  }
  Serial.println();
}

static void webCfgMarkOk()
{
  web_cfg_req_total++;
  web_cfg_req_ok++;
}

static void webCfgMarkErr(const String& err)
{
  web_cfg_req_total++;
  web_cfg_req_err++;
  web_cfg_last_err = err;
}

static void webCfgSendJson(int code, const String& payload)
{
  webCfgServer.send(code, "application/json", payload);
}

static void webCfgHandleRoot()
{
  webCfgLogReq("root", "", true);
  const char* webPath = "/index.html";
  if(!SPIFFS.exists(webPath))
  {
    webCfgMarkErr("ERR_UI_NOT_FOUND");
    webCfgSendJson(500, "{\"ok\":0,\"err\":\"ERR_UI_NOT_FOUND\"}");
    return;
  }

  File file = SPIFFS.open(webPath, FILE_READ);
  if(!file)
  {
    webCfgMarkErr("ERR_UI_OPEN");
    webCfgSendJson(500, "{\"ok\":0,\"err\":\"ERR_UI_OPEN\"}");
    return;
  }

  webCfgServer.streamFile(file, "text/html; charset=utf-8");
  file.close();
}
static void webCfgHandleState()
{
  webCfgLogReq("state", "");
  String json = "{\"ok\":1,\"state\":\"" + webCfgGetStateLine() + "\",\"last_err\":\"" + webCfgGetLastError() + "\"}";
  webCfgSendJson(200, json);
}

static void webCfgHandleGetAll()
{
  webCfgLogReq("config_get_all", "");
  String out;
  if(!cfgGetAllJson(out))
  {
    webCfgLogReq("config_get_all_err", "ERR_GET_ALL_FAILED");
    webCfgMarkErr("ERR_GET_ALL_FAILED");
    webCfgSendJson(500, "{\"ok\":0,\"err\":\"ERR_GET_ALL_FAILED\"}");
    return;
  }
  webCfgLogReq("config_get_all_ok", "");
  webCfgMarkOk();
  webCfgSendJson(200, "{\"ok\":1,\"data\":" + out + "}");
}

static void webCfgHandleGet()
{
  String key = webCfgServer.arg("key");
  webCfgLogReq("config_get", "key=" + key);
  String out;
  String err;
  if(!cfgGetValueByKey(key, out, err))
  {
    if(err.length() == 0) err = "ERR_GET_FAILED";
    webCfgLogReq("config_get_err", err);
    webCfgMarkErr(err);
    webCfgSendJson(400, "{\"ok\":0,\"err\":\"" + err + "\"}");
    return;
  }
  webCfgLogReq("config_get_ok", "key=" + key + ",value=" + out);
  webCfgMarkOk();
  webCfgSendJson(200, "{\"ok\":1,\"key\":\"" + key + "\",\"value\":\"" + out + "\"}");
}

static void webCfgHandleSet()
{
  String key = webCfgServer.arg("key");
  String value = webCfgServer.arg("value");
  webCfgLogReq("config_set", "key=" + key + ",value=" + value);
  String err;
  bool radioReinit = false;
  if(!cfgSetValueByKey(key, value, err, radioReinit))
  {
    if(err.length() == 0) err = "ERR_SET_FAILED";
    webCfgLogReq("config_set_err", err);
    webCfgMarkErr(err);
    webCfgSendJson(400, "{\"ok\":0,\"err\":\"" + err + "\"}");
    return;
  }

  web_cfg_pending_save = true;
  if(radioReinit) web_cfg_radio_reinit_required = true;

  webCfgLogReq("config_set_ok", radioReinit ? "radio_reinit_required=1" : "");
  webCfgMarkOk();
  String data = "{\"ok\":1,\"pending_save\":1";
  if(radioReinit) data += ",\"radio_reinit_required\":1";
  data += "}";
  webCfgSendJson(200, data);
}

static void webCfgHandleSetBatch()
{
  String payload = webCfgServer.arg("payload");
  String payloadInfo = "len=" + String(payload.length());
  if(payload.length() <= 120) payloadInfo += ",payload=" + payload;
  webCfgLogReq("config_set_batch", payloadInfo);
  String err;
  bool radioReinit = false;
  if(!cfgSetBatch(payload, err, radioReinit))
  {
    if(err.length() == 0) err = "ERR_SET_BATCH_FAILED";
    webCfgLogReq("config_set_batch_err", err);
    webCfgMarkErr(err);
    webCfgSendJson(400, "{\"ok\":0,\"err\":\"" + err + "\"}");
    return;
  }

  web_cfg_pending_save = true;
  if(radioReinit) web_cfg_radio_reinit_required = true;

  webCfgLogReq("config_set_batch_ok", radioReinit ? "radio_reinit_required=1" : "");
  webCfgMarkOk();
  String data = "{\"ok\":1,\"pending_save\":1";
  if(radioReinit) data += ",\"radio_reinit_required\":1";
  data += "}";
  webCfgSendJson(200, data);
}

static void webCfgHandleSave()
{
  webCfgLogReq("config_save", "", true);
  saveConfToSPIFFS(usrConf);
  web_cfg_pending_save = false;
  webCfgLogReq("config_save_ok", "", true);
  webCfgMarkOk();
  webCfgSendJson(200, "{\"ok\":1,\"saved\":1}");
}

static void webCfgHandleLoad()
{
  webCfgLogReq("config_load", "");
  if(!readConfFromSPIFFS(usrConf))
  {
    webCfgLogReq("config_load_err", "ERR_LOAD_FAILED");
    webCfgMarkErr("ERR_LOAD_FAILED");
    webCfgSendJson(500, "{\"ok\":0,\"err\":\"ERR_LOAD_FAILED\"}");
    return;
  }
  web_cfg_pending_save = false;
  webCfgLogReq("config_load_ok", "");
  webCfgMarkOk();
  webCfgSendJson(200, "{\"ok\":1,\"loaded\":1}");
}

static void webCfgHandleReboot()
{
  webCfgLogReq("device_reboot", "", true);
  webCfgMarkOk();
  webCfgSendJson(200, "{\"ok\":1,\"rebooting\":1}");
  delay(50);
  ESP.restart();
}

static void webCfgHandleNotFound()
{
  webCfgLogReq("not_found", "");
  webCfgMarkErr("ERR_NOT_FOUND");
  webCfgSendJson(404, "{\"ok\":0,\"err\":\"ERR_NOT_FOUND\"}");
}

void webCfgInit()
{
  if(web_cfg_service_enabled) return;

  WiFi.mode(WIFI_AP);
  if(!WiFi.softAP(WEB_CFG_AP_SSID, WEB_CFG_AP_PASS))
  {
    web_cfg_last_err = "ERR_AP_START";
    Serial.println("Web config AP start failed.");
    return;
  }

  Serial.print("Web config AP started. SSID: ");
  Serial.println(WEB_CFG_AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Web debug mode: ");
  Serial.println(webCfgGetDebugModeName());

  webCfgServer.on("/", HTTP_GET, webCfgHandleRoot);
  webCfgServer.on("/api/state", HTTP_GET, webCfgHandleState);
  webCfgServer.on("/api/config", HTTP_GET, webCfgHandleGetAll);
  webCfgServer.on("/api/get", HTTP_GET, webCfgHandleGet);
  webCfgServer.on("/api/set", HTTP_POST, webCfgHandleSet);
  webCfgServer.on("/api/set_batch", HTTP_POST, webCfgHandleSetBatch);
  webCfgServer.on("/api/save", HTTP_POST, webCfgHandleSave);
  webCfgServer.on("/api/load", HTTP_POST, webCfgHandleLoad);
  webCfgServer.on("/api/reboot", HTTP_POST, webCfgHandleReboot);
  webCfgServer.onNotFound(webCfgHandleNotFound);
  webCfgServer.begin();

  web_cfg_ap_started = true;
  web_cfg_ap_had_client = false;
  web_cfg_tx_unlocked = false;
  web_cfg_last_station_count = 0;
  web_cfg_ap_started_at_ms = millis();
  web_cfg_last_shutdown_reason = "";
  web_cfg_service_enabled = true;
}

void webCfgLoop()
{
  if(!web_cfg_ap_started) return;
  webCfgServer.handleClient();

  const uint8_t stationCount = WiFi.softAPgetStationNum();
  if(stationCount > 0)
  {
    web_cfg_ap_had_client = true;
  }
  if(stationCount != web_cfg_last_station_count)
  {
    Serial.print("Web config client count: ");
    Serial.println(stationCount);
    web_cfg_last_station_count = stationCount;
  }

  if(web_cfg_tx_unlocked)
  {
    webCfgStopService("tx_unlocked");
    return;
  }

  if(!web_cfg_ap_had_client && web_cfg_ap_startup_timeout_ms > 0)
  {
    const uint32_t elapsedMs = millis() - web_cfg_ap_started_at_ms;
    if(elapsedMs >= web_cfg_ap_startup_timeout_ms)
    {
      webCfgStopService("startup_no_client_timeout");
      return;
    }
  }
}

String webCfgGetStateLine()
{
  String line = "enabled=";
  line += web_cfg_service_enabled ? "1" : "0";
  line += ",pending_save=";
  line += web_cfg_pending_save ? "1" : "0";
  line += ",radio_reinit_required=";
  line += web_cfg_radio_reinit_required ? "1" : "0";
  line += ",req_total=" + String(web_cfg_req_total);
  line += ",req_ok=" + String(web_cfg_req_ok);
  line += ",req_err=" + String(web_cfg_req_err);
  if(web_cfg_ap_started)
  {
    line += ",ap_clients=" + String(WiFi.softAPgetStationNum());
    line += ",ap_had_client=" + String(web_cfg_ap_had_client ? 1 : 0);
    line += ",startup_timeout_ms=" + String(web_cfg_ap_startup_timeout_ms);
    line += ",ap_uptime_ms=" + String(millis() - web_cfg_ap_started_at_ms);
  }
  if(web_cfg_last_shutdown_reason.length() > 0)
  {
    line += ",last_shutdown=" + web_cfg_last_shutdown_reason;
  }
  return line;
}

String webCfgGetLastError()
{
  return web_cfg_last_err;
}

uint32_t webCfgGetStartupTimeoutMs()
{
  return web_cfg_ap_startup_timeout_ms;
}

bool webCfgSetStartupTimeoutMs(uint32_t timeoutMs)
{
  if(timeoutMs > 3600000UL) return false;
  web_cfg_ap_startup_timeout_ms = timeoutMs;
  return true;
}

void webCfgNotifyTxUnlocked()
{
  web_cfg_tx_unlocked = true;
}

void webCfgEnableService()
{
  web_cfg_tx_unlocked = false;
  webCfgInit();
}

void webCfgDisableService()
{
  web_cfg_tx_unlocked = true;
  if(web_cfg_ap_started)
  {
    webCfgStopService("serial_wifioff");
    return;
  }
  WiFi.mode(WIFI_OFF);
  web_cfg_service_enabled = false;
}
