/**
 * @file       TinyGsmClientNano.h
 * @author     Volodymyr Shymanskyy, Reid Forrest
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Apr 2021
 */

#ifndef SRC_TINYGSMCLIENTSKYWIRENANO_H_
#define SRC_TINYGSMCLIENTSKYWIRENANO_H_
// #pragma message("TinyGSM:  TinyGsmClientSkywireNano")

#define TINY_GSM_DEBUG Serial

#define TINY_GSM_MUX_COUNT 12
#define TINY_GSM_BUFFER_READ_AND_CHECK_SIZE
#define TINY_GSM_SKYWIRE_NANO_READ_TIMEOUT 1

#include "TinyGsmBattery.tpp"
#include "TinyGsmCalling.tpp"
#include "TinyGsmGPRS.tpp"
#include "TinyGsmGPS.tpp"
#include "TinyGsmModem.tpp"
#include "TinyGsmSMS.tpp"
#include "TinyGsmTCP.tpp"
#include "TinyGsmTemperature.tpp"
#include "TinyGsmTime.tpp"

#define GSM_NL "\r\n"
static const char GSM_OK[] TINY_GSM_PROGMEM    = "OK" GSM_NL;
static const char GSM_ERROR[] TINY_GSM_PROGMEM = "ERROR" GSM_NL;
#if defined       TINY_GSM_DEBUG
static const char GSM_CME_ERROR[] TINY_GSM_PROGMEM = GSM_NL "+CME ERROR:";
static const char GSM_CMS_ERROR[] TINY_GSM_PROGMEM = GSM_NL "+CMS ERROR:";
#endif

enum RegStatus {
  REG_NO_RESULT    = -1,
  REG_UNREGISTERED = 0,
  REG_SEARCHING    = 2,
  REG_DENIED       = 3,
  REG_OK_HOME      = 1,
  REG_OK_ROAMING   = 5,
  REG_UNKNOWN      = 4,
};

class TinyGsmSkywireNano : public TinyGsmModem<TinyGsmSkywireNano>,
                    public TinyGsmGPRS<TinyGsmSkywireNano>,
                    public TinyGsmTCP<TinyGsmSkywireNano, TINY_GSM_MUX_COUNT>,
                    public TinyGsmCalling<TinyGsmSkywireNano>,
                    public TinyGsmSMS<TinyGsmSkywireNano>,
                    public TinyGsmTime<TinyGsmSkywireNano>,
                    public TinyGsmGPS<TinyGsmSkywireNano>,
                    public TinyGsmBattery<TinyGsmSkywireNano>,
                    public TinyGsmTemperature<TinyGsmSkywireNano> {
  friend class TinyGsmModem<TinyGsmSkywireNano>;
  friend class TinyGsmGPRS<TinyGsmSkywireNano>;
  friend class TinyGsmTCP<TinyGsmSkywireNano, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmCalling<TinyGsmSkywireNano>;
  friend class TinyGsmSMS<TinyGsmSkywireNano>;
  friend class TinyGsmTime<TinyGsmSkywireNano>;
  friend class TinyGsmGPS<TinyGsmSkywireNano>;
  friend class TinyGsmBattery<TinyGsmSkywireNano>;
  friend class TinyGsmTemperature<TinyGsmSkywireNano>;

  /*
   * Inner Client
   */
 public:
  class GsmClientSkywireNano : public GsmClient {
    friend class TinyGsmSkywireNano;

   public:
    GsmClientSkywireNano() {}

    explicit GsmClientSkywireNano(TinyGsmSkywireNano& modem, uint8_t mux = 0) {
      init(&modem, mux);
    }

    bool init(TinyGsmSkywireNano* modem, uint8_t mux = 0) {
      this->at       = modem;
      sock_available = 0;
      prev_check     = 0;
      sock_connected = false;
      got_data       = false;

      if (mux < TINY_GSM_MUX_COUNT) {
        this->mux = mux;
      } else {
        this->mux = (mux % TINY_GSM_MUX_COUNT);
      }
      at->sockets[this->mux] = this;

      return true;
    }

   public:
    virtual int connect(const char* host, uint16_t port, int timeout_s) {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, false, timeout_s);
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES

    void stop(uint32_t maxWaitMs) {
      uint32_t startMillis = millis();
      dumpModemBuffer(maxWaitMs); 
      at->sendAT(GF("#XSOCKET="), mux, GF(",0"));
      sock_connected = false;
      at->waitResponse((maxWaitMs - (millis() - startMillis)));
    }
    void stop() override {
      stop(15000L);
    }

    void stopSsl(uint32_t maxWaitMs) {
      uint32_t startMillis = millis();
      dumpModemBuffer(maxWaitMs); 
      at->sendAT(GF("#XTLSSOCKET="), mux, GF(",0"));
      sock_connected = false;
      at->waitResponse((maxWaitMs - (millis() - startMillis)));
    }
    //void stopSsl() override {
    //  stopSsl(15000L);
    //}

    /*
     * Extended API
     */

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;
  };

  /*
   * Inner Secure Client
   */

  public:
  class GsmClientSecureSkywireNano : public GsmClientSkywireNano
  {
  public:
    GsmClientSecureSkywireNano() {}

    explicit GsmClientSecureSkywireNano(TinyGsmSkywireNano& modem, uint8_t mux = 0)
     : GsmClientSkywireNano(modem, mux) {}


  public:
    int connect(const char* host, uint16_t port, int timeout_s) override {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, true, timeout_s);
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES
  };
  

  /*
   * Constructor
   */
 public:
  explicit TinyGsmSkywireNano(Stream& stream) : stream(stream) {
    memset(sockets, 0, sizeof(sockets));
  }

  /*
   * Basic functions
   */
 protected:
  bool initImpl(const char* pin = NULL) {
    DBG(GF("### TinyGSM Version:"), TINYGSM_VERSION);
    DBG(GF("### TinyGSM Compiled Module:  TinyGsmClientSkywireNano"));

    if (!testAT()) { return false; }

    sendAT(GF("E0"));  // Echo Off
    if (waitResponse() != 1) { return false; }

#ifdef TINY_GSM_DEBUG
    sendAT(GF("+CMEE=2"));  // turn on verbose error codes
#else
    sendAT(GF("+CMEE=0"));  // turn off error codes
#endif
    waitResponse();

    DBG(GF("### Modem:"), getModemName());

    // Disable time and time zone URC's
    sendAT(GF("%XTIME=0"));
    if (waitResponse(10000L) != 1) { return false; }

    SimStatus ret = getSimStatus();
    // if the sim isn't ready and a pin has been provided, try to unlock the sim
    if (ret != SIM_READY && pin != NULL && strlen(pin) > 0) {
      simUnlock(pin);
      return (getSimStatus() == SIM_READY);
    } else {
      // if the sim is ready, or it's locked but no pin has been provided,
      // return true
      return (ret == SIM_READY || ret == SIM_LOCKED);
    }
  }

  /*
   * Power functions
   */
 protected:
  bool restartImpl(const char* pin = NULL) {
    if (!testAT()) { return false; }
    if (!setPhoneFunctionality(1, true)) { return false; }
    waitResponse(10000L, GF("READY"));
    return init(pin);
  }

  bool powerOffImpl() {
    sendAT(GF("#SHUTDOWN"));
    waitResponse(300);  // returns OK first
    return waitResponse(300, GF("+SHUTDOWN")) == 1;
  }

// The command sets the power saving mode. Sets activity timer and PSM period 
// after NAS signaling connection release. Configured values are stored in the
// nonvolatile memory when the device is powered off with +CFUN=0.  
bool sleepEnableImpl(bool enable = true) {
    sendAT(GF("+CPSMS="), enable);
    return waitResponse() == 1;
  }

// On the Skywire Nano (nRF916) the reset argument is not used
  bool setPhoneFunctionalityImpl(uint8_t fun, bool reset = false) {
    sendAT(GF("+CFUN="), fun);
    return waitResponse(10000L, GF("OK")) == 1;
  }

  /*
   * Generic network functions
   */
 public:
  RegStatus getRegistrationStatus() {
    // Check first for EPS registration
    RegStatus epsStatus = (RegStatus)getRegistrationStatusXREG("CEREG");
    return epsStatus;
  }

 protected:
  bool isNetworkConnectedImpl() {
    RegStatus s = getRegistrationStatus();
    return (s == REG_OK_HOME || s == REG_OK_ROAMING);
  }

  /*
   * GPRS functions
   */
 protected:
 // The Skywire Nano (nRF9160) does not use username and password so they are ignored here
  bool gprsConnectImpl(const char* apn, const char* user = NULL,
                       const char* pwd = NULL) {
    /*
    gprsDisconnect();
    
    // Configure the TCPIP Context
    sendAT(GF("+CGDCONT=1,\"IP\",\""), apn, GF("\""));
    if (waitResponse() != 1) { return false; }
    

    // Activate GPRS/CSD Context
    sendAT(GF("+CGACT=1"));
    if (waitResponse(150000L) != 1) { return false; }

    // Attach to Packet Domain service - is this necessary?
    sendAT(GF("+CGATT=1"));
    if (waitResponse(60000L) != 1) { return false; }
    */
    return true;
  }

  bool gprsDisconnectImpl() {
    /*
    sendAT(GF("+CGACT=0,1"));  // Deactivate the bearer context
    if (waitResponse(40000L) != 1) { return false; }
    */
    return true;
  }

  /*
   * SIM card functions
   */
 protected:
  String getSimCCIDImpl() {
    sendAT(GF("#ICCID"));
    if (waitResponse(GF(GSM_NL "#ICCID:")) != 1) { return ""; }
    String res = stream.readStringUntil('\n');
    waitResponse();
    res.trim();
    return res;
  }

  /*
   * Phone Call functions
   */
 protected:
  // Can follow all of the phone call functions from the template

  /*
   * Messaging functions
   */
 protected:
  // Follows all messaging functions per template

  /*
   * GSM Location functions
   */
 protected:

  /*
   * GPS/GNSS/GLONASS location functions
   */
 protected:
  // NOTE: GPS does not yet seem to be implemented in Skywire Nano firmware.
  
  // enable GPS
  bool enableGPSImpl() {
    sendAT(GF("#GPS=1"));
    if (waitResponse() != 1) { return false; }
    return true;
  }

  bool disableGPSImpl() {
    sendAT(GF("#GPS=0"));
    if (waitResponse() != 1) { return false; }
    return true;
  }

  // get the RAW GPS output
  String getGPSrawImpl() {
    // sendAT(GF("+QGPSLOC=2"));
    // if (waitResponse(10000L, GF(GSM_NL "+QGPSLOC:")) != 1) { return ""; }
    // String res = stream.readStringUntil('\n');
    // waitResponse();
    // res.trim();
    // return res;
    return "";
  }

  // get GPS informations
  bool getGPSImpl(float* lat, float* lon, float* speed = 0, float* alt = 0,
                  int* vsat = 0, int* usat = 0, float* accuracy = 0,
                  int* year = 0, int* month = 0, int* day = 0, int* hour = 0,
                  int* minute = 0, int* second = 0) {
    // sendAT(GF("+QGPSLOC=2"));
    // if (waitResponse(10000L, GF(GSM_NL "+QGPSLOC:")) != 1) {
      // NOTE:  Will return an error if the position isn't fixed
    return false;
    

    // // init variables
    // float ilat         = 0;
    // float ilon         = 0;
    // float ispeed       = 0;
    // float ialt         = 0;
    // int   iusat        = 0;
    // float iaccuracy    = 0;
    // int   iyear        = 0;
    // int   imonth       = 0;
    // int   iday         = 0;
    // int   ihour        = 0;
    // int   imin         = 0;
    // float secondWithSS = 0;

    // // UTC date & Time
    // ihour        = streamGetIntLength(2);      // Two digit hour
    // imin         = streamGetIntLength(2);      // Two digit minute
    // secondWithSS = streamGetFloatBefore(',');  // 6 digit second with subseconds

    // ilat      = streamGetFloatBefore(',');  // Latitude
    // ilon      = streamGetFloatBefore(',');  // Longitude
    // iaccuracy = streamGetFloatBefore(',');  // Horizontal precision
    // ialt      = streamGetFloatBefore(',');  // Altitude from sea level
    // streamSkipUntil(',');                   // GNSS positioning mode
    // streamSkipUntil(',');  // Course Over Ground based on true north
    // streamSkipUntil(',');  // Speed Over Ground in Km/h
    // ispeed = streamGetFloatBefore(',');  // Speed Over Ground in knots

    // iday   = streamGetIntLength(2);    // Two digit day
    // imonth = streamGetIntLength(2);    // Two digit month
    // iyear  = streamGetIntBefore(',');  // Two digit year

    // iusat = streamGetIntBefore(',');  // Number of satellites,
    // streamSkipUntil('\n');  // The error code of the operation. If it is not
    //                         // 0, it is the type of error.

    // // Set pointers
    // if (lat != NULL) *lat = ilat;
    // if (lon != NULL) *lon = ilon;
    // if (speed != NULL) *speed = ispeed;
    // if (alt != NULL) *alt = ialt;
    // if (vsat != NULL) *vsat = 0;
    // if (usat != NULL) *usat = iusat;
    // if (accuracy != NULL) *accuracy = iaccuracy;
    // if (iyear < 2000) iyear += 2000;
    // if (year != NULL) *year = iyear;
    // if (month != NULL) *month = imonth;
    // if (day != NULL) *day = iday;
    // if (hour != NULL) *hour = ihour;
    // if (minute != NULL) *minute = imin;
    // if (second != NULL) *second = static_cast<int>(secondWithSS);

    // waitResponse();  // Final OK
    // return true;
  }

  /*
   * Time functions
   */
 protected:
  String getGSMDateTimeImpl(TinyGSMDateTimeFormat format) {
    sendAT(GF("+CCLK"));
    if (waitResponse(2000L, GF("+CCLK: \"")) != 1) { return ""; }

    String res;

    switch (format) {
      case DATE_FULL: res = stream.readStringUntil('"'); break;
      case DATE_TIME:
        streamSkipUntil(',');
        res = stream.readStringUntil('"');
        break;
      case DATE_DATE: res = stream.readStringUntil(','); break;
    }
    waitResponse();  // Ends with OK
    return res;
  }

  // The Skywire Nano returns UTC time instead of local time as other modules do in
  // response to CCLK. It does return a timezone offset however, expressed as the
  // number of 15 minute increments ahead or behind UTC.
  bool getNetworkTimeImpl(int* year, int* month, int* day, int* hour,
                          int* minute, int* second, float* timezone) {
    sendAT(GF("+CCLK"));
    if (waitResponse(2000L, GF("+CCLK: \"")) != 1) { return false; }

    int iyear     = 0;
    int imonth    = 0;
    int iday      = 0;
    int ihour     = 0;
    int imin      = 0;
    int isec      = 0;
    int itimezone = 0;

    // Date & Time
    iyear       = streamGetIntBefore('/');
    imonth      = streamGetIntBefore('/');
    iday        = streamGetIntBefore(',');
    ihour       = streamGetIntBefore(':');
    imin        = streamGetIntBefore(':');
    isec        = streamGetIntLength(2);
    char tzSign = stream.read();
    itimezone   = streamGetIntBefore(',');
    if (tzSign == '-') { itimezone = itimezone * -1; }
    streamSkipUntil('\n');  // DST flag

    // Set pointers
    if (iyear < 2000) iyear += 2000;
    if (year != NULL) *year = iyear;
    if (month != NULL) *month = imonth;
    if (day != NULL) *day = iday;
    if (hour != NULL) *hour = ihour;
    if (minute != NULL) *minute = imin;
    if (second != NULL) *second = isec;
    if (timezone != NULL) *timezone = static_cast<float>(itimezone) / 4.0;

    // Final OK
    waitResponse();  // Ends with OK
    return true;
  }

  /*
   * Battery functions
   */
 protected:
  // Can follow CBC as in the template

  /*
   * Temperature functions
   */
 protected:
  // get temperature in degree celsius
  uint16_t getTemperatureImpl() {
    sendAT(GF("%XTEMP?"));
    if (waitResponse(GF(GSM_NL "%XTEMP:")) != 1) { return 0; }
    // return temperature in C
    int res = streamGetIntBefore('\n');
    waitResponse();
    return res;
  }

  /*
   * Client related functions
   */
 protected:
  
  // if modemConnect() is called without stag1, stag2, and stag3 then
  // we assume a TLS connection is not being requested
//  bool modemConnect(const char* host, uint16_t port, uint8_t mux,
//                    bool ssl = false, int timeout_s = 150) {
//    return modemConnect(host, port, mux, false, timeout_s, 0, 0, 0);
//  }

  // stag1 == index of CA certificate stored in the Nano
  // stag2 == index of Device Certificate stored in the Nano
  // stag3 == index of Private Key stored in the Nano
  bool modemConnect(const char* host, uint16_t port, uint8_t mux,
                    bool ssl = false, int timeout_s = 150,
                    int stag1 = 0, int stag2 = 0, int stag3 = 0) {
    if (ssl) { 
      DBG("SSL enabled"); 
      if (!stag1 || !stag2 || !stag3) { 
        DBG("SSL requested but no certificates given");
        return false; 
      }
      this->ssl = true;
    }

    uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;

    if (ssl) {
      // <action>(0-1)
      // "<IP_address>/<domain_name>",
      // <remote_port>, <access_mode>(0-2; 0=buffer)
      sendAT(GF("#XTLSSOCKET="), mux, GF(",1,\""), host, GF("\","), 
            GF(","), stag1, GF(","), stag2, GF(","), stag3);
    }
    else {
      sendAT(GF("#XSOCKET="), mux, GF(",1,1"));
    }
    // int res=waitResponse();
    // if (res != 1) {
    //   DBG(GF("modemConnect failed to create socket: "), res);
    //   return false;
    // }

    if (ssl) {
      if (waitResponse(timeout_ms, GF(GSM_NL "#XTLSSOCKET:")) == 0) { return false; }
    }
    else {
      DBG("modemConnect 2");
      if (waitResponse() != 1) { 
        DBG("modemconnect error 1");
        return false;
      }
      //if (waitResponse(timeout_ms, GF(GSM_NL "#XSOCKET:")) != 1) { return false; }
      String res = stream.readStringUntil('\n');
      DBG("modemConnect 3: ", res);
      DBG("host:'", GF(host), "'");

      sendAT(GF("#XTCPCONN="), mux, GF(",\""), host, GF("\","), port);
      //if (waitResponse(timeout_ms, GF(GSM_NL "#XTCPCONN:")) != 1) { 
        if (waitResponse() != 1) {
          String res = stream.readStringUntil('\n');
          DBG("modemConnect err 4");
          return false; 
      } else {
        DBG("returning true");
        return 1;
      }
    }

    if (streamGetIntBefore(',') != mux) { return false; }
    // Read status
    return (0 == streamGetIntBefore('\n'));
  }

  // Maximum transmit size is 576 bytes regardless of what is specified in len
  int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
      DBG("modemSend len: ", len);
      sendAT(GF("#XTCPSEND="), mux, ',', (uint16_t)len);
      // > prompt only appears if length is not given
      if (waitResponse(GF(">")) != 1) { return 0; }
      stream.write(reinterpret_cast<const uint8_t*>(buff), len);
      stream.flush();
      if (waitResponse(GF(GSM_NL "OK")) != 1) { return 0; }
      // TODO(?): Wait for ACK? AT+QISEND=id,0
      return len;
  }

  size_t modemRead(size_t size, uint8_t mux) {
/*    if (!sockets[mux]) return 0;
      sendAT(GF("+QIRD="), mux, ',', (uint16_t)size);
      if (waitResponse(GF("+QIRD:")) != 1) { return 0; }
      int16_t len = streamGetIntBefore('\n');

      for (int i = 0; i < len; i++) { moveCharFromStreamToFifo(mux); }
      waitResponse();
      // DBG("### READ:", len, "from", mux);
      sockets[mux]->sock_available = modemGetAvailable(mux);
      return len;
    */
    if (!sockets[mux]) return 0;
    sendAT(GF("#XTCPRECV="), mux, ',', (uint16_t)size, ',', TINY_GSM_SKYWIRE_NANO_READ_TIMEOUT);
    uint16_t len = waitResponse(GF("#XTCPRECV:"));
    if (len == 0) { return 0; }
    len = streamGetIntBefore(',');

    for (int i = 0; i < len; i++) { moveCharFromStreamToFifo(mux); }
    waitResponse();
    DBG("### READ:", len, "from", mux);
    sockets[mux]->sock_available = modemGetAvailable(mux);
    return len;  
  }

  size_t modemGetAvailable(uint8_t mux) {
    if (!sockets[mux]) return 0;
    sendAT(GF("#XTCPRECV="), mux, GF(",0"));
    size_t result = 0;
    if (waitResponse(GF("#XTCPRECV:")) == 1) {
      streamSkipUntil(',');  // Skip total received
      streamSkipUntil(',');  // Skip have read
      result = streamGetIntBefore('\n');
      if (result) { DBG("### DATA AVAILABLE:", result, "on", mux); }
      waitResponse();
    }
    if (!result) { sockets[mux]->sock_connected = modemGetConnected(mux); }
    return result;
  }

  bool modemGetConnected(uint8_t mux) {
    sendAT(GF("#XTCPCONN="), mux);
      return waitResponse(GF("#XTCPCONN:")) != 1;
  }

  /*
   * Utilities
   */
 public:
  // TODO(vshymanskyy): Optimize this!
  int8_t waitResponse(uint32_t timeout_ms, String& data,
                      GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    /*String r1s(r1); r1s.trim();
    String r2s(r2); r2s.trim();
    String r3s(r3); r3s.trim();
    String r4s(r4); r4s.trim();
    String r5s(r5); r5s.trim();
    DBG("### ..:", r1s, ",", r2s, ",", r3s, ",", r4s, ",", r5s); */
    data.reserve(64);
    uint8_t  index       = 0;
    uint32_t startMillis = millis();
    do {
      TINY_GSM_YIELD();
      while (stream.available() > 0) {
        TINY_GSM_YIELD();
        int8_t a = stream.read();
        if (a <= 0) continue;  // Skip 0x00 bytes, just in case
        data += static_cast<char>(a);
        if (r1 && data.endsWith(r1)) {
          index = 1;
          goto finish;
        } else if (r2 && data.endsWith(r2)) {
          index = 2;
          goto finish;
        } else if (r3 && data.endsWith(r3)) {
#if defined TINY_GSM_DEBUG
          if (r3 == GFP(GSM_CME_ERROR)) {
            streamSkipUntil('\n');  // Read out the error
          }
#endif
          index = 3;
          goto finish;
        } else if (r4 && data.endsWith(r4)) {
          index = 4;
          goto finish;
        } else if (r5 && data.endsWith(r5)) {
          index = 5;
          goto finish;
        } else if (data.endsWith(GF(GSM_NL "#URC:"))) {
          streamSkipUntil('\"');
          String urc = stream.readStringUntil('\"');
          streamSkipUntil(',');
          if (urc == "recv") {
            int8_t mux = streamGetIntBefore('\n');
            DBG("### URC RECV:", mux);
            if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
              sockets[mux]->got_data = true;
            }
          } else if (urc == "closed") {
            int8_t mux = streamGetIntBefore('\n');
            DBG("### URC CLOSE:", mux);
            if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
              sockets[mux]->sock_connected = false;
            }
          } else {
            streamSkipUntil('\n');
          }
          data = "";
        }
      }
    } while (millis() - startMillis < timeout_ms);
  finish:
    if (!index) {
      data.trim();
      if (data.length()) { DBG("### Unhandled:", data); }
      data = "";
    }
    data.replace(GSM_NL, "/");
    DBG('<', index, '>', data);
    return index;
  }

  int8_t waitResponse(uint32_t timeout_ms, GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    String data;
    return waitResponse(timeout_ms, data, r1, r2, r3, r4, r5);
  }

  int8_t waitResponse(GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    #ifdef TINY_GSM_DEBUG
      int8_t response;
      response = waitResponse(1000, r1, r2, r3, r4, r5);
      return response;
    #else
      return waitResponse(1000, r1, r2, r3, r4, r5);
  #endif
  }

 public:
  Stream& stream;

 protected:
  GsmClientSkywireNano* sockets[TINY_GSM_MUX_COUNT];
  const char*           gsmNL = GSM_NL;
  bool                  ssl = false;
};

#endif  // SRC_TINYGSMCLIENTSKYWIRENANO_H_
