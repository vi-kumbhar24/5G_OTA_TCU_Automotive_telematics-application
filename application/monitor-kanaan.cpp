/******************************************************************
 * File: monitor.cpp
 *
 * Main control program
 *
 ******************************************************************
 * Change log:
 * 2012/07/28 rwp
 *    Changed behavior for eth0 to keep connection up only with
 *    Ign/CAN
 *    Added KeepOn feature control
 * 2012/08/02 rwp
 *    Changed getting VIC version (now in vic.h)
 *    Changed loading of default VIC code to 1st VIC-*.dat file
 *    Changed flash VIC firmware after upgrade to use current VIC
 *    version
 *    Fixed to not attempt to get QuickScan ECUs in none were seen
 *    by VIC
 * 2012/08/08 rwp
 *    Moved network utility subs to network_utils.h
 * 2012/08/17 rwp
 *    Added clearing GPS when antenna is changed, VIC is reset or
 *    power is loss
 * 2012/09/12 rwp
 *    Added logging of LowBattery notification
 *    Added creating/deleting WokeOnCanFile (for ignoring queued
 *    car_control)
 *    Fixed AlarmNotification 
 * 2012/09/13 rwp
 *    Make DontConnectFile override and connect reason
 * 2012/09/14 rwp
 *    Added default position report interval (if FindMyCar enabled)
 *    to be 300
 *    Fixed alarm edge detection
 * 2012/09/17 rwp
 *    Kill save_dns_entries on_term
 *    Kill update_default_sites on_term
 * 2012/09/21 rwp
 *    Don't send PositionReport if location disabled
 * 2012/10/06 rwp
 *    Added alarm break in getCellResponse
 * 2012/10/09 rwp
 *    Added ValetAlert trunk_opened code
 * 2012/10/26 rwp
 *    Increased ppp startup_timeout to 60 seconds
 *    Moved creation of GSM files into main loop
 *    Exit connection backoff is only need connection for events
 * 2012/11/01 rwp
 *    Added keepOn function to determine if unit should be kept on
 * 2012/11/02 rwp
 *    Added edge detection for sending ValetAlert trunk_opened
 * 2012/11/06 rwp
 *    Added HomeLan framework
 * 2012/11/07 rwp
 *    Changes for single_os (for Dow)
 * 2012/11/14 rwp
 *    Changes for OBD-II unit (reports %fuel instead of liters)
 * 2012/11/15 rwp
 *    Changed to use features.h isEnabled for enabled_until
 * 2012/11/16 rwp
 *    Clear car_state.stopped_fuel_level when ignition turned off
 * 2012/11/23 rwp
 *    Made cellular_keepup active while keep_on or StayOn (not ignition)
 * 2012/12/03 rwp
 *    Change format of gsm parms file to all csv
 * 2012/12/07 rwp
 *    Changed the way that GSM config files are built
 * 2012/12/12 rwp
 *    Fixed wakeup on SMS, broke by 2012/11/23 change
 * 2012/12/13 rwp
 *    Change clearing of GPS to use cell "resetgps"
 * 2012/12/21 rwp
 *    Fixed sending tire pressure if ideal pressures unknown
 * 2013/01/10 rwp
 *   Added code to reflash the cell module after upgrade (not tested)
 * 2013/01/17 rwp
 *   Fixed sending provider name that contains spaces in hw_config 
 * 2013/01/21 rwp
 *   Added code to load VIC based off OBDII bus structure
 * 2013/01/30 rwp
 *   Added ability to tell monitor not to read and clear SMS (ford)
 * 2013/02/01 rwp
 *   Added periodic wlan1 scaning (dow)
 * 2013/02/04 rwp
 *   Added reporting of Dow logging status
 * 2013/02/12 rwp
 *   Added code to connect_wifi
 *   Added code to check result of wpa_supplicant
 * 2013/02/20 rwp
 *   Changed canConnect to use checkConnect (fix problem with
 *   initial cell activation to make connection)
 * 2013/02/22 rwp
 *   Added dup of filesystem if upgrade is working
 * 2013/02/25 rwp
 *   Fixed false detection of cell activation with invalid MDN
 * 2013/02/27 rwp
 *   Fixed sending stop to unitcomm
 *   Fixed dup of filesystem if upgrade is working
 * 2013/03/04 rwp
 *   Added alarm break in getCellResponse around send
 * 2013/03/06 rwp
 *   Change to report empty cell profile values
 * 2013/03/16 rwp
 *   Added occupacy/seatbelt notification
 * 2013/03/22 rwp
 *   Added log error if cell talker does not exist
 * 2013/03/25 rwp
 *   Fixed reporting ppp terminated with signal on connection close
 *   Added delay for seatbelt notifications
 *   Added config seatbelt delay feature
 * 2013/03/26 rwp
 *   Added restarting seatbelt check timer on start to move
 * 2013/03/27 rwp
 *   Added separate socket for vic notifications, to fix bug with
 *   getting notification during get/set RTC and getVicVersion
 * 2013/03/28 rwp
 *   Added qualification of not SNA for trunk opened check
 * 2013/04/01 rwp
 *   Added setting seat occupied if belt fastened
 *   Added resetting VicRtc if MD5 error returned from Vic
 * 2013/04/02 rwp
 *   Added reporting VIC errors to trumanager
 *   Set VicRtc before we send shutdown to Vic
 *   Kill vic_talker after we send shutdown to Vic
 * 2013/04/11 rwp
 *   Added new lowbat processes
 * 2013/04/19 rwp
 *   Fixed monitor not to save_dns_entries on startup
 * 2013/04/29 rwp
 *   Added changing bluetooth name for SMS blocking
 * 2013/05/07 rwp
 *   Added LTE changes
 * 2013/05/09 rwp
 *   Added code to reset wlan0 if nousage and users connected
 * 2013/05/13 rwp
 *   Closed cell talker socket before kill cell talker
 * 2013/05/24 rwp
 *   Fixed sending VIC Loader reset
 * 2013/05/27 rwp
 *   Keep on until verify_sys completes if state is booting
 * 2013/05/29 rwp
 *   Fixed sending Vic error hundreds of times
 * 2013/05/30 rwp
 *   Added power_off timer for Maserati
 * 2013/06/05 rwp
 *   Added /tmp/ign to simulate ignition switch
 * 2013/06/05 rwp
 *   Fixed getWan to not set cellinfo.connected
 * 2013/06/28 rwp
 *   AUT-40: Clear trunk_opened_sent after trunk is closed
 * 2013/07/07 rwp
 *   AUT-43: Changes for initial upgrade to perform untar and install
 * 2013/07/11 rwp
 *   AUT-47: Send SB capability if seat belt and occpancy detected
 * 2013/07/12 rwp
 *   AUT-46: Added Power Loss event
 * 2013/08/02 rwp
 *   AUT-56: Fix to send ValetAlert TrunkOpened once unless trunk is closed
 * 2013/08/08 rwp
 *   AUT-45: Extend up_wait time when unitcomm connects
 *           Clear up_wait when all events are sent
 *   Added ability to tcpdump wan connection
 * 2013/08/08 rwp
 *   AUT-52: Send ALARM capability if alarm system is seen as armed
 * 2013/08/08 rwp
 *   MYAUT-849: Fixed sending 2 CarState events with LowBattery
 * 2013/08/08 rwp
 *   AUT-54: Keep connection up if GPS report interval <= 5 min
 * 2013/08/08 rwp
 *   AUT-50: Keep connection up if fleet tracker is enabled
 * 2013/08/08 rwp
 *   AUT-59: Set obd2_unit base on VIC build not ObdIIFeature
 * 2013/08/08 rwp
 *   AUT-58: Move eventCarState call to set VIN before checkVicVersion
 * 2013/08/09 rwp
 *   HT-40: Added BatAdjust value (4th parameter in PwrCntrl feature)
 * 2013/08/14 rwp
 *   Added logging of tx/rx byte counters
 * 2013/08/23 rwp
 *   AUT-75: Fixed check for events all sent
 * 2013/08/29 rwp
 *   AUT-77: Send Check Engine status
 * 2013/08/31 rwp
 *   Add ability to send fake faults
 * 2013/09/04 rwp
 *   Qualify Check Engine status with engine on
 * 2013/09/04 rwp
 *   AUT-76: Determine if remote started and check for aborts
 * 2013/09/10 rwp
 *   AUT-83: Send Low Fuel indication
 * 2013/09/23 rwp
 *   Moved CAN testing from eventCarInfo to new checkCan routine
 * 2013/11/11 rwp
 *   AUT-90: Use same procedure to flash VIC version for incorrect build as upgrade
 * 2013/11/14 rwp
 *   AUT-79: Use hostname instead of MAC address for ESSID
 * 2013/11/15 rwp
 *   Added code to use CAN status bits for checkCan
 * 2013/11/15 rwp
 *   AUT-85: Check alarm occured status bit
 * 2013/11/18 rwp
 *   AUT-92: Check for RegFeature changing and run autonet_router
 * 2013/11/30 rwp
 *   AUT-93: Control wlan0 and hci0 on SmsBlockingFeature and NoWlanFeatures
 * 2013/12/14 rwp
 *   AUT-76: Handle VIC status notifications for remote start aborted
 * 2014/02/06 rwp
 *   AUT-110: Send cellular roaming status in Connection and Stats events
 * 2014/02/18 rwp
 *   AUT-100: Change to use FleetFeature enabled instead of Fleet feature
 * 2014/02/21 rwp
 *   Dont log empty profile string
 * 2014/02/21 rwp
 *   AUT-116: Always set VIC RTC when setting systime
 * 2014/03/24 rwp
 *   AUT-119: Place Uboot version and cell firmware version in hw config
 * 2014/03/26 rwp
 *   AUT-120: Changed to aggressively get VIN and check CAN failed
 * 2014/04/01 rwp
 *   Keep update_files_time over power cycles to reduce data usage
 * 2014/04/17 rwp
 *   AUT-71/HT-63: Changed to reset wlan0 on ARP usage
 * 2014/04/28 rwp
 *   AUT-132: Changed to not report the CAN off event if NoCan
 * 2014/05/06 rwp
 *   AUT-135: Changed to only report VIC errors once
 ******************************************************************
 */

#include <cstring>
#include <iostream>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <regex.h>
#include <unistd.h>
#include <vector>
#include "autonet_files.h"
#include "autonet_types.h"
#include "backtick.h"
#include "csv.h"
#include "split.h"
#include "splitFields.h"
#include "getToken.h"
#include "getParm.h"
#include "dateTime.h"
#include "logError.h"
#include "eventLog.h"
#include "fileExists.h"
#include "fileStats.h"
#include "writeStringToFile.h"
#include "updateFile.h"
#include "touchFile.h"
#include "getStringFromFile.h"
#include "pgrep.h"
#include "string_convert.h"
#include "string_printf.h"
#include "readFileArray.h"
#include "readAssoc.h"
#include "readFile.h"
#include "remote_start_reasons.h"
#include "grepFile.h"
#include "ssl_keys.h"
#include "str_system.h"
#include "my_features.h"
#include "getopts.h"
#include "getUptime.h"
#include "trim.h"
#include "vic.h"
#include "vta_causes.h"
#include "network_utils.h"
using namespace std;

void setEssid();
void createGsmFiles();
void getCellConfig();
void connect();
void connect_ppp();
void connect_lte();
void dial_lte();
bool checkWanUp();
string getInterfaceIpAddress(string dev);
string getDefaultRoute(string dev);
string swappedHexToIp(string str);
void killWan(string why);
void setResolver(string type);
void cleanUpgradeDir();
void checkUpgrading();
void getMonthlyUsage();
void upLoop();
void update_files();
void updatePrl(string prlfile);
void setNtpTime();
void setSysTime(struct tm *tm_in, int usec);
void updateMonthlyUsage();
void writeMonthlyUsage();
void waitingLoop();
bool connectWifi(string ap_str);
void ifdownWlan0();
void ifupWlan0();
string makeHexKey(string enc);
void writeWpaConf(string essid, string key);
void disconnectWifi();
void resetWlan0(string reason);
void wlan1Loop();
void monWait(int timeout);
void openVic();
void checkVicVersion();
void getVicVersion();
int getVicResponse(int reqid, int reqlen, Byte *buffer);
void processVicMessage(int len, Byte *buffer);
void decodeVicStatus(int len, Byte *buffer);
void handleVicStatus(int len, Byte *buffer);
void updateCarCapabilities(int capabilities);
void sendLowFuelEvent();
void sendLowBatteryEvent();
long int getOdometerX10();
int getFuelLevel();
int getSpeed();
void doIgnitionProcess();
bool ignition_on();
bool ignition_off();
bool getWan(bool init);
void getWlan0();
void getRateLimit();
void getStateInfo();
void saveTmpState();
void savePermState();
string getStateString();
void getCellInfo();
void getCellTime();
void getUserConnections();
int getWlan0Users();
void readGpsPosition();
void checkCarStatus();
void notifySeatbelts();
void reportRemoteAborted();
void reportAlarm();
void getVin();
void checkCan();
void getVicRtc(struct tm *tm);
void setVicRtc();
void setVicWatchdog();
void setVicQuickScanTimes();
void getVicStatus(bool getlast);
void check_for_faults();
void sendFaultsEvent(string faults_str);
string getFaults();
void getCellProfile();
bool activateCell();
string getCellResponse(const char *cmnd, int timeout);
string getCellResponse(const char *cmnd);
void killCellTalker();
bool turnOnCell();
void cellPulseOnKey();
void resetCell();
void resetCellHdw();
void checkFeaturesChange();
void getWifiEnable();
void getWifiMaxOnTime();
void getPositionReportInterval();
void getPowerControlSettings(bool initial);
void getCarKeySettings();
void getAntennaSettings();
void getKeepOnFeature();
void getValetFeature();
void getNoCanFeature();
void getSeatbeltsFeature();
void getHomeLanFeature();
void getFleetFeature();
void getSmsBlockingFeature();
bool vinToExtAnt(string vin);
void clearGpsReceiver();
//void checkGpsCleared();
void getDisableLocation();
bool checkLink();
bool canConnect();
void checkConnect();
void tellWhyCantConnect();
bool smsMessages();
void clearSmsMessages();
bool keepOn();
void checkKeepOn();
bool needConnection();
void checkNeedConnection();
void tellWhyNeedConnection();
string eventConnection();
string eventDeltaTime(string cellstate, bool updatetime);
string eventIp();
string eventPosition();
string eventGpsPosition();
string eventCellInfo();
string eventFailCount();
string eventHdwInfo();
string getFlashSize();
string kbToMeg(string str);
string kbToMeg(int kb);
string eventRateLimit();
string eventUptime();
string eventUsage(bool withspeed);
string eventWifiUsers();
string eventCarInfo(bool showign, bool getlast);
string eventLowFuel();
string eventDowStatus();
double getDoubleUptime();
string regrepFile(const char *file, const char *search);
void writeToFile(const char *file, string str);
void dial();
void catFile(const char *src, const char *dst);
void startUnitcomm();
void stopUnitcomm();
void sendStopToUnitcomm();
void restartUnitcomm();
void reboot(string cause);
void powerOff(string reason, int type, int timer);
void updateUpgradeInfo();
void completeUpgrade();
void waitForUpgradeToFinish();
void doUpgrade();
void updateVic(string new_vers);
void upgradeCellModule(string bootdir);
void dupFs();
void unmountAutonet();
void backupAutonet();
void buildToc(string autonet);
void writeLastEvent(string reason);
void on_child(int parm);
void on_alrm(int parm);
//void on_term(int parm);
void on_term(int sig, siginfo_t *info, void *data);
void on_usr1(int parm);
void enableWatchdog();
void disableWatchdog();
string getPppCloseCause();

#define RtcErrorFile "/tmp/rtc_failed"
#define FuelFile "/tmp/fuel"
#define OdomFile "/tmp/odom"
#define SpeedFile "/tmp/speed"
#define SeatsFile "/tmp/seats"
#define IgnFile "/tmp/ign"
#define FaultsFile "/tmp/faults"
bool single_os = false;
int app_id = 1;
Byte vic_notifications[6] = {0xF0,0xF1,0xF2,0xF4,0xF5,0xF8};
bool alrm_sig = false;
#define CommandTimeout 60
#define CommandHoldTime 300
//#define CarOffDelay 10
#define CarOffDelay 2
#define CellHoldTime 120
#define VicWatchdogTime 10
#define VicQuickScanTime 1440
#define VicQuickScanIgnDelay 5
#define VicQuickScanToolDisconnectTime 5
#define VICQuickScanConfigChangeTime 3
#define Wlan1ScanTime 30
#define DowStatusInterval 3600
bool dow_unit = false;

struct CellInfo {
    string type;
    string device;
    bool diag_available;
    bool connected;
    bool have_signal;
    bool dont_connect;
    bool no_config;
    int socket;
    string talker;
    string state;
    string substate;
    string prev_state;
    string connection_type;
    string signal;
    bool roaming;
    string model;
    bool gsm;
    bool lteppp;
    bool activation_required;
    bool activated;
    bool activation_in_progress;
    string provider;
    string mdn;
    string min;
    string esn;
    string prl;
    string imei;
    string smsc;
    string imsi;
    string iccid;
    string mcn;
    string sim;
    bool got_profile;
    bool firstreactivate;
    bool first_connection;
    bool testing_keepup;
    bool can_activate;
    bool events_to_send;
    unsigned long boot_delay;
    int fail_count;
    unsigned long last_report;
    unsigned long command_timeout;
    unsigned long support_hold;
    string position;
    int error_count;
    unsigned long fail_time;
    string why_need_connection;
    string why_cant_connect;
    string why_keepon;
    bool need_connection;
    bool can_connect;
    bool keepon;
    unsigned long getinfo_time;
    bool overtemp;
    int temp_fail_cnt;
    bool no_apn;

    CellInfo() {
        type = "ppp";
        device = "ppp0";
        diag_available = false;;
        connected = false;;
        have_signal = false;;
        dont_connect = false;
        no_config = true;
        socket = -1;
        talker = "";
        state = "down";
        substate = "";
        prev_state = "";
        connection_type = "";
        signal = "unknown";
        roaming = false;
        model = "";
        gsm = false;
        lteppp = false;
        activation_required = false;
        activated = false;
        activation_in_progress = false;
        provider = "";
        mdn = "";
        min = "";
        esn = "";
        prl = "";
        imei = "";
        smsc = "";
        imsi = "";
        iccid = "";
        mcn = "";
        sim = "";
        got_profile = false;
        firstreactivate = false;
        first_connection = true;
        testing_keepup = false;
        can_activate = true;
        events_to_send = false;
        boot_delay = 900;
        fail_count = 0;
        last_report = 0;
        command_timeout = 0;
        long support_hold = 0;
        position = "";
        error_count = 0;
        fail_time = 0;
        why_need_connection = "";
        why_cant_connect = "";
        why_keepon = "";
        need_connection = false;
        can_connect = false;
        keepon = false;
        getinfo_time = 0;
        overtemp = false;
        temp_fail_cnt = 0;
        no_apn = false;
    }
};
CellInfo cellinfo;

struct WanInfo {
    bool up;
    bool exists;
    pid_t pid;
    string ipAddress;
    bool pppd_exited;
    int startup_timeout;
    unsigned long rx_bytes;
    unsigned long rx_frames;
    unsigned long tx_bytes;
    unsigned long tx_frames;
    unsigned long last_rx_bytes;
    unsigned long last_tx_bytes;
    double lasttime;
    double rx_kBps;
    double tx_kBps;
    double max_rx_kBps;
    double max_tx_kBps;
    u_int64_t sum_rx_bytes;
    u_int64_t sum_tx_bytes;
    unsigned long monthly_usage;
    string usage_month;
    int ratelimit;
    bool reload_mods;
    bool reset_cell;
    bool reboot;

    WanInfo() {
        up = false;
        exists = false;;
        pid = 0;
        ipAddress = "";
        pppd_exited = false;;
        startup_timeout = 60;
        rx_bytes = 0;
        rx_frames = 0;
        tx_bytes = 0;
        tx_frames = 0;
        last_rx_bytes = 0;
        last_tx_bytes = 0;
        lasttime = 0.;
        rx_kBps = 0.;
        tx_kBps = 0.;
        max_rx_kBps = 0.;
        max_tx_kBps = 0.;
        sum_rx_bytes = 0;
        sum_tx_bytes = 0;
        monthly_usage = 0;
        usage_month = "";
        ratelimit = 0;
        reload_mods = false;
        reset_cell = false;
        reboot = false;
    }
};
WanInfo waninfo;

struct Wlan0Info {
    bool enabled;
    bool off;
    bool up;
    int users;
    int real_users;
    int fake_users;
    string state;
    unsigned long last_rx_frames;
    unsigned long last_tx_frames;
    unsigned long last_check;
    bool being_used;
    int max_on_time;
    unsigned long ign_off_time;

    Wlan0Info() {
        enabled = true;
        off = false;
        up = false;
        users = 0;
        real_users = 0;
        fake_users = 0;
        state = "down";
        last_rx_frames = 0;
        last_tx_frames = 0;
        last_check = 0;
        being_used = false;
        max_on_time = 0;
        ign_off_time = 0;
    }
};
Wlan0Info wlan0info;

struct Wlan1Info {
    bool up;
    string state;
    unsigned long scan_time;
    unsigned long next_scan_time;
    int scan_period;
    bool homelan_enabled;
    bool syncing;
    bool found_wap;
    bool connected;

    Wlan1Info() {
        up = false;
        state = "down";
        scan_time = 0;
        next_scan_time = 0;
        scan_period = 3600;
        homelan_enabled = false;
        syncing = false;
        found_wap = false;
        connected = false;
    }
};
Wlan1Info wlan1info;

struct Eth0Info {
    bool up;
    string state;

    Eth0Info() {
        up = false;
        state = "down";
    }
};
Eth0Info eth0info;

#define GET_POSITION_INTERVAL 60
#define POSITION_REPORT_INTERVAL 300
struct GpsInfo {
    bool present;
    string position;
    string datetime;
    int report_interval;
    int position_interval;
    unsigned long report_time;
    unsigned long position_time;
    off_t geofence_file_time;
    bool report_position;
    bool fleet_enabled;
//    unsigned int report_enabled_until;
//    bool clearing;

    GpsInfo() {
        present = false;
        position = "";
        datetime = "";
        report_interval = 0;
        position_interval = GET_POSITION_INTERVAL;
        report_time = 0;
        position_time = 0;
        geofence_file_time = 0;
        report_position = false;
        fleet_enabled = false;
//        report_enabled_until = 0;
//        clearing = false;
    }
};
GpsInfo gpsinfo;

class PowerSchedule {
  public:
    int ignition_off_time;
    int cycle_cell_time;

    PowerSchedule(int ig_off_time, int cc_time) {
        ignition_off_time = ig_off_time;
        cycle_cell_time = cc_time;
    }
};

class CarState {
  public:
    bool ignition;
    bool no_can;
    bool engine;
    bool moving;
    int check_engine;
    bool can_active;
    bool can_stopped;
    bool vic_reset;
    unsigned int ign_time;
    float batval;
    long last_check_time;
    float lowbatval;
    float critbatval;
    float batadjust;
    int lowbat_polltime;
    bool reboot_on_dead;
    bool remote_started;
    unsigned long remote_start_time;
    unsigned long remote_aborted_time;
    int remote_aborted_cause;
    string last_car_command;
    unsigned long command_hold;
    bool ignition_was_on;
    bool ignition_already_on;
    bool engine_was_on;
    time_t ignition_off_time;
    time_t ignition_on_time;
    string powered_off_state;
    time_t powered_off_time;
    unsigned int check_fault_time;
    int locks;
    int doors;
    int seats;
    int belts;
    int vic_status;
    int vic_errors;
    int vic_reported_errors;
    int can_status_bits;
    vector<PowerSchedule> power_schedule;
    int cell_hold_time;
    string vin;
    unsigned int stay_on_delay;
    unsigned long low_bat_time;
    bool low_battery;
    unsigned int ignition_change_time;
    unsigned int ignition_delay;
    string ecall_state;
    bool location_disabled;
    int capabilities;
    string capabilities_str;
    int alarm_state;
    unsigned int alarm_report_time;
    int alarm_report_state;
    int alarm_cause;
    bool get_quickscan;
    int quickscan_idx;
    int quickscan_ecus;
    int quickscan_cnt;
    string quickscan_str;
//    string vic_version;
//    string vic_version_short;
//    string vic_build;
//    string vic_requested_build;
    int stopped_fuel_level;
    int stopped_odometer;
    bool trunk_opened;
    bool valet_mode;
    bool trunk_opened_sent;
    bool keep_on;
    bool obd2_unit;
    bool seatbelts_enabled;
    int seatbelts_delay;
    unsigned int seatbelts_delay_time;
    unsigned int seatbelts_check_time;
    unsigned int seatbelts_stopped_time;
    int last_belts;
    int last_seats;
    int old_seats;
    unsigned int next_dow_status_time;
    bool bluetooth_flag;
    int low_fuel;
    int low_fuel_sent;
    unsigned int low_fuel_timer;
    int ignition_status;
    int key_status;
    bool sms_blocking;
    bool can_failed;

    CarState() {
        ignition = false;
        no_can = false;
        engine = false;
        moving = false;
        check_engine = 0;
        can_active = false;
        can_stopped = false;
        vic_reset = false;
        ign_time  = 0;
        batval = 0.;
        last_check_time = 0;
        lowbatval = 10.5;
        critbatval = 6.0;
        batadjust = 0.3;
	lowbat_polltime = 15;
        reboot_on_dead = true;
        remote_started = false;
        remote_start_time = 0;
        remote_aborted_time = 0;
        remote_aborted_cause = 0x3F; // SNA
        last_car_command = "";
        command_hold = 0;
        ignition_was_on = false;
        ignition_already_on = false;
        engine_was_on = false;
        powered_off_state = "";
        powered_off_time = 0;
        ignition_off_time = 0;
        ignition_on_time = 0;
        check_fault_time = 0;
        locks = 0;
        doors = 0;
        seats = 0;
        belts = 0;
        vic_status = 0;
        vic_errors = 0;
        vic_reported_errors = 0;
        can_status_bits = 0;
        cell_hold_time = CellHoldTime;
        vin = "";
        stay_on_delay = 0;
        low_bat_time = 0;
        low_battery = false;
        ignition_change_time = 0;
        ignition_delay = 0;
        ecall_state = "";
        location_disabled = false;
        capabilities_str = "none";
        alarm_state = 0x07; // SNA
        alarm_report_time = 0;
        alarm_report_state = 0;
        alarm_cause = -1; // Not set
        get_quickscan = false;
        quickscan_idx = 0;
        quickscan_ecus = 0;
        quickscan_cnt = 0;
        quickscan_str = "";
//        vic_version = "";
//        vic_version_short = "";
//        vic_build = "";
//        vic_requested_build = "";
        stopped_fuel_level = -1;
        trunk_opened = false;
        valet_mode = false;
        trunk_opened_sent = false;
        keep_on = false;
        obd2_unit = false;
        seatbelts_enabled = false;
        seatbelts_delay = 0;
        seatbelts_delay_time = 0;
        seatbelts_check_time = 0;
        seatbelts_stopped_time = 0;
        last_belts = 0;
        last_seats = 0;
        old_seats = 0;
        next_dow_status_time = 0;
        bluetooth_flag = false;
        low_fuel = 0;
        low_fuel_sent = 0;
        low_fuel_timer = 0;
        ignition_status = 7; //SNA
        key_status = 3; //SNA
        sms_blocking = false;
        can_failed = true;
    }

    void defaultValues() {
        lowbatval = 10.5;
        critbatval = 6.0;
	lowbat_polltime = 15;
        reboot_on_dead = true;
    }

    void setValues(string val_str) {
        Strings vals = split(val_str, "/");
        lowbatval = stringToDouble(vals[0]);
        critbatval = stringToDouble(vals[1]);
        if (vals.size() > 2) {
            lowbat_polltime = stringToInt(vals[2]);
            if (vals.size() > 3) {
                batadjust = stringToDouble(vals[3]);
            }
        }
    }

    void setSchedule(string sched_str) {
        power_schedule.clear();
        cell_hold_time = CellHoldTime;
        Strings strs = split(sched_str, ",");
        int steps = strs.size();
        int val1, val2;
        char ch;
        for (int i=0; i < steps; i++) {
//            if ((i == 0) and (sscanf(strs[i].c_str(), "%d%c", &val1, &ch) == 1)) {
//               cell_hold_time = val1;
//               continue;
//            }
            if (sscanf(strs[i].c_str(), "%d/%d%c", &val1, &val2, &ch) == 2) {
                int ig_off_time = val1;
                int cc_time = val2;
                power_schedule.push_back(PowerSchedule(val1, val2));
            }
            else {
                break;
            }
        }
        if (power_schedule.size() == 0) {
            power_schedule.push_back(PowerSchedule(0,0));
        }
    }

    int getPowerScheduleLength() {
        return power_schedule.size();
    }

    int getIgnitionOffTime(int idx) {
        return power_schedule[idx].ignition_off_time;
    }

    int getCycleCellTime(int idx) {
        return power_schedule[idx].cycle_cell_time;
    }

    void make_capabilities_string() {
        string cap_str = "";
        if (capabilities & 0x8000) {
            cap_str += ":HFM";
        }
        if (capabilities & 0x4000) {
            cap_str += ":REON";
        }
        if (capabilities & 0x2000) {
            cap_str += ":RSO";
        }
        if (capabilities & 0x1000) {
            cap_str += ":RTO";
        }
        if (capabilities & 0x0800) {
            cap_str += ":RDU";
        }
        if (capabilities & 0x0001) {
            cap_str += ":SB";
        }
        if (capabilities & 0x0002) {
            cap_str += ":ALARM";
        }
        if (cap_str == "") {
            cap_str = "none";
        }
        else {
            cap_str.erase(0,1);
        }
        capabilities_str = cap_str;
    }
};
CarState car_state;
VIC vic;
VIC vic_notifies;

#define COMM_NOTSTARTED 0
#define COMM_RUNNING 1
#define COMM_STOPPING 2
#define COMM_STOPPED 3
#define COMM_PPPDKILLED 4
#define COMM_FAILED 5
struct CommInfo {
    int state;
    pid_t pid;
    bool exited;
    time_t stop_time;

    CommInfo() {
        state = COMM_NOTSTARTED;
        pid = 0;
        exited = false;
        stop_time = 0;
    }
};
CommInfo comminfo;

typedef enum {Unknown, Bad, Close, Good} TimeState;

class TimeInfo {
  public:
    TimeState state;
    unsigned long ntp_timer;
    unsigned long celltime_timer;
    unsigned long rtc_timer;
    time_t last_celltime;
    bool celltime_available;

    TimeInfo() {
        state = Unknown;
        ntp_timer = 0;
        celltime_timer = 0;
        last_celltime = 0;
        rtc_timer = 0;
        celltime_available = false;
    }
};
TimeInfo timeinfo;

class UpgradeInfo {
  public:
    string sw_version;
    unsigned long time_of_upgrade;
    int boot_cnt;
    int connect_cnt;
    int gps_cnt;

    UpgradeInfo() {
        sw_version = "";
        time_of_upgrade = 0;
        boot_cnt = 0;
        connect_cnt = 0;
        gps_cnt = 0;
    }

    void read() {
        if (fileExists(UpgradeInfoFile)) {
            string upgrade_info = getStringFromFile(UpgradeInfoFile);
//            logError("upgrade_info:"+upgrade_info);
            Strings vals = split(upgrade_info, ",");
            if (vals.size() > 0) {
                sw_version = vals[0];
//                logError(sw_version);
            }
            if (vals.size() > 1) {
                time_of_upgrade = stringToULong(vals[1]);
//                logError(vals[1]);
            }
            if (vals.size() > 2) {
                boot_cnt = stringToInt(vals[2]);
//                logError(vals[2]);
            }
            if (vals.size() > 3) {
                connect_cnt = stringToInt(vals[3]);
//                logError(vals[3]);
            }
            if (vals.size() > 4) {
                gps_cnt = stringToInt(vals[4]);
//                logError(vals[4]);
            }
        }
    }

    void write() {
        string update_str = string_printf("%s,%ld,%d,%d,%d",
            sw_version.c_str(),time_of_upgrade,boot_cnt,connect_cnt,gps_cnt);
        updateFile(UpgradeInfoFile, update_str);
    }

    void remove() {
        unlink(UpgradeInfoFile);
    }

    bool check() {
        bool retval = false;
        string current_version = getStringFromFile(IssueFile);
        if (current_version == sw_version) {
            if ( (boot_cnt > 4) and
                 (connect_cnt > 4) and
                 (gps_cnt > 4) ) {
//                logError("check true");
                retval = true;
            }
        }
        return retval;
    }
};
    

unsigned long Uptime = 0;
time_t update_files_time = 0;
bool timevalid = true;
bool warmstart = false;
EventLog eventlog;
Features features;
string kernelId = "";
bool wifi_enabled = false;
bool registered = false;
//unsigned long wifi_enabled_until = 0;
int monitor_flags = 0;
#define IN_CELLWAIT 0x01
#define IN_VICWAIT 0x02
#define IN_UPDATEFILES 0x04
#define IN_BACKOFF 0x08
#define IN_MONWAIT 0x10
#define IN_ACTIVATEWAIT 0x20
#define IN_UPGRADEWAIT 0x40
#define IN_CONNECT 0x100
#define IN_UPLOOP 0x200
#define IN_WAITLOOP 0x400
#define IN_DUPFSWAIT 0x800
#define IN_CELLCONNECT 0x100
#define IN_CELLSEND 0x200
#define IN_CELLRECV 0x400
#define IN_MONITOR_LOCK 0x8000
bool experimental_lte = false;
int booting = 0;

int main(int argc, char **argp) {
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1);
    umask(S_IWGRP | S_IWOTH);
    signal(SIGALRM, on_alrm);
//    signal(SIGTERM, on_term);
//    signal(SIGINT, on_term);
//    signal(SIGQUIT, on_term);
//    signal(SIGHUP, on_term);

    struct sigaction sig_act;
    memset(&sig_act, 0, sizeof(struct sigaction));
    sig_act.sa_sigaction = on_term;
    sig_act.sa_flags |= SA_SIGINFO;
    sigemptyset(&sig_act.sa_mask);
    sigaction(SIGTERM, &sig_act, NULL);
    sigaction(SIGINT, &sig_act, NULL);
    sigaction(SIGQUIT, &sig_act, NULL);
    sigaction(SIGHUP, &sig_act, NULL);
    signal(SIGCHLD, on_child);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, on_usr1);
    GetOpts opts(argc, argp, "l:w");
    warmstart = opts.exists("w");
    if (opts.exists("l")) {
        setLogErrorFile(opts.get("l"));
    }
    logError("Monitor started");
    if (fileExists("/boot/experimental_lte")) {
        experimental_lte = true;
    }
    if (fileExists(MonitorFlagsFile)) {
        unlink(MonitorFlagsFile);
    }
    if (fileExists(WhosKillingMonitorFile)) {
        unlink(WhosKillingMonitorFile);
    }
    if (fileExists(UpdateFilesTimeFile)) {
        update_files_time = stringToLong(getStringFromFile(UpdateFilesTimeFile));
    }
    kernelId = getStringFromFile(KernelIdFile);
    if (kernelId.find_first_of("012") != string::npos) {
        string kernelstate = backtick("fw_printenv .state"+kernelId);
        if (kernelstate == "") {
            kernelstate = backtick("fw_printenv .state"+kernelId);
        }
        if (kernelstate.find("booting") != string::npos) {
            logError("Setting booting");
            booting = 1;
        }
    }
    getNoCanFeature();
    single_os = fileExists(SingleOsFile);
    cellinfo.state = "boot";
    string prevstate_file = TempStateFile;
    if (fileExists(prevstate_file)) {
        time_t prev_time = fileTime(prevstate_file);
        string prevstate_str = getStringFromFile(prevstate_file);
        Strings prevstate_info = split(prevstate_str, ":");
        string prev_uptime_str = prevstate_info[0];
        string deltat_str = "0";
        if (prevstate_info.size() > 1) {
            string prev_state = prevstate_info[1];
            deltat_str = prevstate_info[2];
            string tx_str = prevstate_info[3];
            string rx_str = prevstate_info[4];
            string fails_str = prevstate_info[5];
            string event = "Last event: monitor died";
            event += " " + prev_state + ":" + deltat_str;
            event += " Up:" + tx_str;
            event += " Down:" + rx_str;
            if (fails_str != "0") {
                event += " fails:" + fails_str;
            }
//            event += " uptime:" + prev_uptime_str;
            eventlog.append(prev_time, event);
        }
        unsigned long prev_uptime = stringToULong(prev_uptime_str);
        int delta_time = stringToInt(deltat_str);
        cellinfo.last_report = prev_uptime - delta_time;
        cellinfo.state = "down";
    }
    bool woke_on_can = true;
    if (warmstart) {
        cellinfo.state = "down";
    }
    generateKeys();
    Uptime = getUptime();
    if (!warmstart and (Uptime < 120)) {
        cleanUpgradeDir();
    }
    bool cell_bad = false;
    getCellConfig();
    if (!fileExists(CellTalkerPath)) {
        cell_bad = true;
    }
    else {
        getCellProfile();
        if (!fileExists(GpsTimeFile)) {
            getCellTime();
        }
        else {
            timeinfo.state = Good;
        }
    }
    openVic();
//    vic.setDebug(true);
    if (vic.getSocket() == -1) {
        logError("Could not open VIC socket...setting ignition on");
        car_state.ignition = true;
    }
    struct tm tm;
    getVicRtc(&tm);
    bool battery_connected = false;
    if (tm.tm_year > 2001) {
        logError("RTC valid");
    }
    else {
        battery_connected = true;
        rename(VinFile, OldVinFile);
        unlink(LastGpsAntStateFile);
        logError("WokeOnCanFile set - init loss of power");
        string event = "CarState PowerLoss";
        eventlog.append(event);
        woke_on_can = false;
        touchFile(WokeOnCanFile);
        logError("RTC invalid");
    }
    if (timeinfo.state == Unknown) {
        setSysTime(&tm, 0);
        if (tm.tm_year > 2001) {
            timeinfo.state = Close;
        }
        else {
            timeinfo.state = Bad;
        }
    }
    else {
        setVicRtc();
    }
//    upgradeCellModule("/boot/");
    getVicVersion();
    if (vic.build == "OBDII") {
        car_state.obd2_unit = true;
    }
    checkCan();
    getVin();
    checkVicVersion();
    getWifiEnable();
    getPositionReportInterval();
    getPowerControlSettings(true);
    getAntennaSettings();
    getCarKeySettings();
    getKeepOnFeature();
    getValetFeature();
    getSeatbeltsFeature();
    getHomeLanFeature();
    getFleetFeature();
    getSmsBlockingFeature();
    features.getFeature(RateLimits);
    getDisableLocation();
    string stayontime = features.getFeature(StayOn);
    if (stayontime != "") {
        logError("Got StayOn="+stayontime);
        car_state.stay_on_delay = stringToInt(stayontime) * 60;
    }
    getWifiMaxOnTime();
    wlan0info.ign_off_time = Uptime;
    if (experimental_lte) {
        string devs = backtick("ls /dev/ttyHS*");
        if (devs != "") {
            cellPulseOnKey();
        }
    }
    else {
        if (cell_bad) {
            // This should not occur under normal cases
            if (turnOnCell()) {
                getCellConfig();
                getCellProfile();
                if (!fileExists(GpsTimeFile)) {
                    getCellTime();
                }
                else {
                    timeinfo.state = Good;
                    setVicRtc();
                }
            }
        }
    }
    if (cellinfo.gsm) {
        if (cellinfo.mcn == "") {
            cellinfo.no_apn = true;
        }
        else {
            createGsmFiles();
        }
    }
    if (fileExists(CapabilitiesFile)) {
        car_state.capabilities = stringToInt(getStringFromFile(CapabilitiesFile));
        car_state.make_capabilities_string();
    }
    setVicWatchdog();
    setVicQuickScanTimes();
    getVicStatus(false);
    if (car_state.ignition) {
        logError("Ignition on");
        logError("WokeOnCanFile set - init engine on");
        touchFile(WokeOnCanFile);
        woke_on_can = false;
    }
    else {
        logError("Ignition off");
        touchFile(IgnitionOffFile);
    }
    if (car_state.can_active) {
        logError("CAN bus active");
        logError("WokeOnCanFile set - init CAN active");
        touchFile(WokeOnCanFile);
        woke_on_can = false;
    }
    else {
        logError("CAN bus inactive");
    }
    if (car_state.vic_reset) {
        logError("VIC was reset...Need to clear GPS");
        unlink(LastGpsAntStateFile);
    }
    if (car_state.batval < car_state.lowbatval) {
        sendLowBatteryEvent();
        cellinfo.events_to_send = true;
        car_state.low_battery = true;
        woke_on_can = false;
    }
//kanaan    setEssid();
//        if ( (strncmp(essid.c_str(), "autonet-", 8) == 0) or
//             ( (strncmp(essid.c_str(), "mopar-", 6) == 0) and
//               (essid != mopar_essid) ) ) {
//            logError("Setting ESSID to "+mopar_essid);
//            str_system("autonetconfig essid " + mopar_essid);
//        }
//    }
//    else {
//        Assoc wificonfig = readAssoc(WifiConfigFile, " ");
//        string essid = wificonfig["essid"];
//        essid = essid.substr(1,essid.size()-2);
//        if (strncmp(essid.c_str(), "mopar-", 6) == 0) {
//            string mac = backtick("ifconfig eth0 | grep HWaddr | sed 's/://g'");
//            mac = getParm(mac, "HWaddr ");
//            string last4 = mac.substr(mac.size()-4);
//            string autonet_essid = "autonet-" + last4;
//            logError("Setting ESSID to "+autonet_essid);
//            str_system("autonetconfig essid " + autonet_essid);
//        }
//    }
    getStateInfo();
    string bootdelay_str = features.getFeature(BootDelay);
    string brand = features.getFeature(VehicleBrand);
    if (brand == "Dow") {
        dow_unit = true;
        string scan_period = features.getFeature(Wlan1ScanPeriod);
        if (scan_period != "") {
            wlan1info.scan_period = stringToInt(scan_period);
        }
    }
    if (dow_unit) {
        wlan1info.next_scan_time = Uptime + wlan1info.scan_period;
    }
    else {
        wlan1info.next_scan_time = 0;
    }
    if (bootdelay_str != "") {
        cellinfo.boot_delay = stringToInt(bootdelay_str);
    }
    if (!(car_state.ignition or car_state.can_active) and !battery_connected) {
        logError("Setting boot_delay to 0");
        cellinfo.boot_delay = 0;
//        cellinfo.booting = false;
    }
    if (smsMessages()) {
        logError("SMS message");
        cellinfo.events_to_send = true;
        cellinfo.command_timeout = Uptime + CommandTimeout;
        clearSmsMessages();
        woke_on_can = false;
    }
    else {
        logError("No SMS message");
    }
    car_state.powered_off_state = "Normal";
    if (fileExists(PowerOffStateFile)) {
        string pos_str = getStringFromFile(PowerOffStateFile);
        string po_state = pos_str;
        Strings vals = split(pos_str, ":");
        unsigned long ign_off_time = 0;
        unsigned long powered_off_time = 0;
        if (vals.size() > 1) {
            powered_off_time = stringToULong(vals[1]);
            po_state = vals[0];
        }
        if (vals.size() > 2) {
            ign_off_time = stringToULong(vals[2]);
        }
        car_state.ignition_off_time = ign_off_time;
        car_state.powered_off_state = po_state;
        car_state.powered_off_time = powered_off_time;
        logError("PowerOffState: "+po_state+" "+toString(ign_off_time));
        if (po_state == "IgnOff") {
            if ((time(NULL)-powered_off_time) > ((CarOffDelay-1)*60)) {
                cellinfo.events_to_send = true;
            }
        }
        unlink(PowerOffStateFile);
    }
    else {
        if (!car_state.ignition) {
            if ( (timeinfo.state == Good) or (timeinfo.state == Close) ) {
               car_state.ignition_off_time = time(NULL) - car_state.ign_time*60;
            }
        }
    }
    if (fileExists(CellularTestingFile)) {
        cellinfo.can_activate = false;
        cellinfo.testing_keepup = true;
    }
    if (fileExists(FirstReactivateFile)) {
        cellinfo.firstreactivate = true;
    }
    if (cellinfo.can_activate) {
        if ( cellinfo.activation_required and
             cellinfo.got_profile and
             !cellinfo.activated ) {
            activateCell();
        }
        else if (cellinfo.firstreactivate) {
            activateCell();
        }
    }
    logError("Boot delay: "+toString(cellinfo.boot_delay));
    readGpsPosition();
    setResolver("init");
    enableWatchdog();
    if ((kernelId == "0") and !single_os) {
        string event = "NotifySupport Unit is in kernel 0";
        eventlog.append(event);
    }
    if (woke_on_can and !car_state.no_can) {
       car_state.can_active = true;
       logError("Woke on CAN... setting can_active");
    }
    getStateInfo();
    getWan(true);
    while (keepOn() or (needConnection() and canConnect())) {
        getCellInfo();
        while (canConnect() and needConnection()) {
            tellWhyNeedConnection();
            logError("Connecting");
//            system("ledcontrol 6");
//            if (cellinfo.gsm and (cellinfo.mcn == "")) {
//                getCellProfile();
//                if (cellinfo.mcn != "") {
//                    createGsmFiles();
//                }
//            }
            connect();
            if (cellinfo.connected) {
                if (fileExists("/autonet/admin/tcpdump")) {
                    string dumpfile = "/user/"+cellinfo.device+"-"+backtick("date +%Y%m%d%H%M%S")+".dmp";
                    str_system("tcpdump -i ppp0 -n -w "+dumpfile+" &");
                }
                setResolver(cellinfo.type);
//                system("ledcontrol 1");
                system("autonet_router");
                if (cellinfo.firstreactivate) {
                    cellinfo.firstreactivate = false;
                    unlink(FirstReactivateFile);
                }
                if (Uptime > timeinfo.ntp_timer) {
                    setNtpTime();
                }
                if (cellinfo.first_connection) {
                    getMonthlyUsage();
                }
                updateMonthlyUsage();
                system("ratelimit");
                getRateLimit();
                getCellProfile();
                logError("Connection established");
                string event = "Connection established";
                event += eventDeltaTime(cellinfo.prev_state, true);
                event += eventConnection();
                event += eventIp();
                event += eventPosition();
                event += eventCellInfo();
                event += eventHdwInfo();
                event += eventFailCount();
                event += eventRateLimit();
                event += eventUptime();
                event += eventDowStatus();
                eventlog.append(event);
//                logError(event);
                touchFile(CommNeededFile);
                if ((time(NULL)-fileTime(LastDefaultSitesUpdateFile)) > 3600*24*30) {
                    system("update_default_sites &");
                }
                if ( car_state.ignition and
                     ((kernelId != "0") or single_os) ) {
                    time_t now = time(NULL);
                    if ( (update_files_time == 0) or
                       ((now-update_files_time) > 86400) ) {
                        update_files();
                        update_files_time = now;
                        updateFile(UpdateFilesTimeFile, toString(now));
                    }
                    if (cellinfo.first_connection) {
                        checkUpgrading();
                    }
                }
                if (cellinfo.first_connection) {
                    cellinfo.first_connection = false;
                }
                system("backup_autonet");
                logError("Entering upLoop");
                upLoop();
                if (fileExists("/autonet/admin/tcpdump")) {
                    system("pkill tcpdump");
                }
                setResolver("nowan");
                system("autonet_router");
                unlink(CommNeededFile);
            }
        }
//        cout << "ignition:" << car_state.ignition << endl;
//        cout << "ignition_change_time:" << car_state.ignition_change_time << endl;
//        cout << "needConnection:" << needConnection() << endl;
//        tellWhyNeedConnection();
        if (car_state.low_battery) {
            break;
        }
//        if (ignition_off() and !car_state.can_active and !needConnection()) {
//        if ( !(car_state.ignition or
//              (car_state.ignition_change_time > 0)) and
//             !needConnection()) {
//            break;
//        }
        if (wlan1info.up) {
            logError("Entering wlan1Loop");
//            system("ledcontrol 13");
            wlan1Loop();
        }
        else {
            logError("Entering waitingLoop");
//            system("ledcontrol 13");
            waitingLoop();
        }
    }
    if (car_state.low_battery) {
        // shutdown in deep sleep
        logError("Power down - low bat");
        powerOff("LowBat", 0x02, 0);
    }
    else if (cellinfo.mdn == "") {
        // shutdown in deep sleep
        logError("Power down - no MDN");
        powerOff("NoMDN", 0x02, 0);
    }
    else if (car_state.ignition_was_on) {
        // shutdown w/restart in CarOff minutes
        logError("Power down - ignition turned off");
//        car_state.powered_off_time = time(NULL);
//        powerOff("IgnOff", 0x05, CarOffDelay);
        powerOff("Normal", 0x00, 0);
    }
//    else if (car_state.powered_off_state == "IgnOff") {
//        // Powered after ignition was turned off
//        time_t now = time(NULL);
//        if ((now-car_state.powered_off_time) > ((CarOffDelay-1)*60)) {
//            logError("Power down - after off poweroff delay");
//            powerOff("Normal", 0x00, 0);
//        }
//        else {
//            logError("Power down - before poweroff delay");
//            int to = 10 - (now - car_state.powered_off_time)/60;
//            powerOff("Normal", 0x05, to);
//        }
//    }
    // if low signal
    //     shutdown w/cell poll every 5 minutes
    else {
        // normal shutdown
        logError("Power down - normal");
        powerOff("Normal", 0x00, 0);
    }
}

void setEssid() {
    string hostname = getStringFromFile(ProcHostname);
    string last4 = hostname.substr(hostname.size()-4);
    if (car_state.vin != "") {
        string vin = car_state.vin;
        last4 = vin.substr(vin.size()-4);
    }
    Assoc wificonfig = readAssoc(WifiConfigFile, " ");
    string old_essid = wificonfig["essid"];
    old_essid = old_essid.substr(1,old_essid.size()-2);
    int pos = old_essid.find("-");
    if (pos != string::npos) {
        string old_brand = old_essid.substr(0, pos);
        string old_last = old_essid.substr(pos+1);
        if ( (old_last.size() == 4) and
             (old_last.find_first_not_of("0123456789ABCDEF") == string::npos)) {
            bool is_old_brand = false;
            string brand_essids = backtick("getbranding -l essid");
            Strings essids = split(brand_essids, "\n");
            for (int i=0; i < essids.size(); i++) {
                if (old_brand == essids[i]) {
                    is_old_brand = true;
                    break;
                }
            }
            if (is_old_brand) {
                string new_essid = backtick("getbranding essid") + "-" + last4;
                if (new_essid != old_essid) {
                    logError("Setting ESSID to "+new_essid);
                    str_system("autonetconfig essid " + new_essid);
                }
            }
        }
    }
}

void createGsmFiles() {
    Assoc gsminfo;
    if (fileExists(GsmConfFile)) {
        gsminfo = readAssoc(GsmConfFile, ":");
    }
    string mcn = cellinfo.mcn;
    if (gsminfo.find("apn") != gsminfo.end()) {
        system("update_gsm_info -c");
    }
    else {
        string res = backtick("get_gsm_parms -q "+mcn);
        if (res == "") {
            cellinfo.no_apn = true;
        }
        else {
            Strings vals = csvSplit(res, ",");
            string apn = vals[0];
            string user = vals[1];
            string passwd = vals[2];
            string provider = vals[3];
            string cmnd = "update_gsm_info";
            cmnd += " -p "+provider;
            cmnd += " -a "+apn;
            if (user != "") {
                cmnd += " -u "+user;
                if (passwd != "") {
                    cmnd += "/"+passwd;
                }
            }
            str_system(cmnd);
        }
    }
}

void getCellConfig() {
//    logError("getCellConfig");
    if (fileExists(HardwareConfFile)) {
//        logError("Reading cell config file");
        Assoc cellConfig = readAssoc(HardwareConfFile, " ");
        if (cellConfig.find("cell_isgsm") != cellConfig.end()) {
            cellinfo.gsm = true;
        }
        if (cellConfig.find("cell_islteppp") != cellConfig.end()) {
            cellinfo.lteppp = true;
        }
        if (cellConfig.find("cell_talker") != cellConfig.end()) {
            cellinfo.talker = cellConfig["cell_talker"];
        }
        if (cellConfig.find("cell_model") != cellConfig.end()) {
            cellinfo.model = cellConfig["cell_model"];
            cellinfo.no_config = false;
            logError("Got cell_model");
        }
        if (cellConfig.find("activation_required") != cellConfig.end()) {
            cellinfo.activation_required = true;
        }
        if (cellConfig.find("cell_islte") != cellConfig.end()) {
            cellinfo.type = "lte";
        }
        if (cellConfig.find("cell_device") != cellConfig.end()) {
            cellinfo.device = cellConfig["cell_device"];
        }
    }
}

void connect() {
    if (cellinfo.type == "ppp") {
        connect_ppp();
    }
    else if (cellinfo.type == "lte") {
        connect_lte();
    }
}

void connect_ppp() {
    monitor_flags |= IN_CONNECT;
    int num_backoffs = 7;
    int verizon_backoffs[7] = {0,0,0,60,120,480,900};
    int non_verizon_backoffs[7] = {0,0,0,60,60,60,180};
    int *backoffs = non_verizon_backoffs;
    if (cellinfo.provider == "verizon") {
        backoffs = verizon_backoffs;
    }
    int backoff_idx = 0;
    cellinfo.temp_fail_cnt = 0;
    unsigned long reactivation_time = Uptime + 900;
    while (!cellinfo.connected and canConnect() and needConnection()) {
        dial();
        bool first = true;
        unsigned long up_start = 0;
        unsigned long check_pppd_time = Uptime;
        unsigned long pppd_start_time = Uptime;
        while (!waninfo.pppd_exited and !cellinfo.connected) {
            monWait(1);
            needConnection();
            getStateInfo();
            if (getWan(false)) {
                if (first) {
                    first = false;
                    up_start = Uptime;
                }
                if (checkWanUp()) {
                    if (fileExists("/tmp/failconnect")) {
                        killWan("/tmp/failconnect");
//                        system("pkill pppd");
                    }
                    else {
                        cellinfo.connected = true;
                        cellinfo.prev_state = cellinfo.state;
                        cellinfo.state = "up";
                        cellinfo.temp_fail_cnt = 0;
                    }
                }
                else if ((up_start != 0) and
                         ((Uptime - up_start) > waninfo.startup_timeout))  {
                    killWan("no link");
                    up_start = 0;
                }
            }
            if ((Uptime-check_pppd_time) >= 60) {
                string pppd_pid = backtick("pgrep pppd");
                if (pppd_pid == "") {
                    waninfo.pppd_exited = true;
                    break;
                }
                else if ((Uptime-pppd_start_time) >= 300) {
                    logError("killing pppd...locked up");
                    string event = "Connection failed... cell modem locked up";
                    eventlog.append(event);
                    system("pkill -9 pppd");
                    sleep(1);
                    unlink(CellmodemLockFile);
                    resetCell();
                    waninfo.pppd_exited = true;
                }
                check_pppd_time = Uptime;
            }
        }
        getCellInfo();
        if (waninfo.pppd_exited) {
            string cause = getPppCloseCause();
            logError("Connection failed: "+cause);
            cellinfo.fail_count++;
            if (waninfo.reboot) {
                reboot("Connection "+cause);
            }
            if ( cellinfo.activation_required and
                 cellinfo.first_connection and
                 (Uptime > reactivation_time) ) {
                activateCell();
                reactivation_time = Uptime + 900;
            }
            cellinfo.temp_fail_cnt++;
            if (!needConnection()) {
                break;
            }
            int backoff = backoffs[backoff_idx];
            backoff_idx++;
            if (backoff_idx >= num_backoffs) {
                backoff_idx = num_backoffs - 1;
            }
            unsigned long wait_time = Uptime + backoff;
            if (wait_time > Uptime) {
                logError("In connect backoff");
                monitor_flags |= IN_BACKOFF;
                while ( canConnect() and needConnection() and
                        (wait_time > Uptime)) {
                    monWait(1);
                    getStateInfo();
                }
                monitor_flags &= ~IN_BACKOFF;
            }
        }
    }
    monitor_flags &= ~IN_CONNECT;
}

void connect_lte() {
    monitor_flags |= IN_CONNECT;
    while (!cellinfo.connected and canConnect() and needConnection()) {
        dial_lte();
        bool connection_failed = false;
        unsigned long check_time = Uptime;
        while (!connection_failed and !cellinfo.connected) {
            monWait(1);
            needConnection();
            getStateInfo();
            if (getWan(false)) {
                if (checkWanUp()) {
                    if (fileExists("/tmp/failconnect")) {
                        killWan("/tmp/failconnect");
                        connection_failed = true;
                    }
                    else {
                        cellinfo.connected = true;
                        cellinfo.prev_state = cellinfo.state;
                        cellinfo.state = "up";
                        cellinfo.temp_fail_cnt = 0;
                    }
                }
            }
            if ((Uptime-check_time) >= 60) {
                check_time = Uptime;
                connection_failed = true;
                break;
            }
        }
    }
    monitor_flags &= ~IN_CONNECT;
}

void dial_lte() {
    string resp;
//    resp = getCellResponse("AT%CMATT=1");
//    logError("Response to AT%CMATT: "+resp);
//    if (resp.find("OK") != string::npos) {
        resp = getCellResponse("connect",60);
        logError("Response to connect: "+resp);
        if (resp.find("OK") != string::npos) {
            sleep(1);
            str_system("ifconfig "+cellinfo.device+" up");
            sleep(1);
            str_system("udhcpc -i "+cellinfo.device);
            sleep(1);
            cellinfo.connected = false;
            waninfo.pppd_exited = false;
        }
//    }
}

bool checkWanUp() {
    bool retval = false;
    string ipaddr = getInterfaceIpAddress(cellinfo.device);
    if (ipaddr != "") {
        waninfo.ipAddress = ipaddr;
        if (getDefaultRoute(cellinfo.device) != "") {
            retval = true;
        }
    }
    return retval;
}

void killWan(string why) {
    logError("Killing wan connection: "+why);
    if (cellinfo.type == "ppp") {
        appendToFile(PppLogFile, "monitor: "+why);
        kill(waninfo.pid, SIGTERM);
    }
    else if (cellinfo.type == "lte") {
        system("route del default");
        system("cp /ro/etc/resolv.conf /rw/etc/resolv.conf");
        system("pkill udhcpc");
        string resp = getCellResponse("disconnect",10);
//        resp = getCellResponse("AT%CMATT=0");
        str_system("ifconfig "+cellinfo.device+" 0.0.0.0 down");
    }
}

void setResolver(string type) {
//    if ((type == "nowan") or fileExists(DisableFile)) {
    if (type == "init") {
        unlink(DnsmasqConf);
        symlink(DnsmasqLocalConf, DnsmasqConf);
        system("killall dnsmasq");
    }
    else if (type == "nowan") {
        unlink(DnsmasqConf);
        symlink(DnsmasqLocalConf, DnsmasqConf);
        system("save_dns_entries");
        system("killall dnsmasq");
    }
    else if (type == "ppp") {
        unlink(DnsmasqConf);
        symlink(DnsmasqRemoteConf, DnsmasqConf);
        system("killall dnsmasq");
        monWait(1);
    }
    else if (type == "lte") {
        unlink(DnsmasqConf);
        symlink(DnsmasqLteConf, DnsmasqConf);
        system("killall dnsmasq");
        monWait(1);
    }
}

void setNtpTime() {
    if (!timeinfo.celltime_available and !fileExists(GpsTimeFile)) {
        logError("Setting time via NTP");
        if (system("ntpdate 0.pool.ntp.org") == 0) {
            if (timeinfo.state != Good) {
                setVicRtc();
            }
            timeinfo.state = Good;
        }
    }
    timeinfo.ntp_timer = Uptime + 3600;
}

void setSysTime(struct tm *tm_in, int usec) {
    struct tm tm;
    tm.tm_year = tm_in->tm_year - 1900;
    tm.tm_mon = tm_in->tm_mon -1;
    tm.tm_mday = tm_in->tm_mday;
    tm.tm_hour = tm_in->tm_hour;
    tm.tm_min = tm_in->tm_min;
    tm.tm_sec = tm_in->tm_sec;
    struct timeval tv;
    tv.tv_sec = timegm(&tm);
    tv.tv_usec = usec;
    settimeofday(&tv, NULL);
}

void update_files() {
    string prlfile = "";
    if (cellinfo.provider == "verizon") {
        if (strncmp(cellinfo.mdn.c_str(), "415", 3) == 0) {
           prlfile = VerizonRetailPrlFile;
        }
        else {
           prlfile = VerizonWholesalePrlFile;
        }
    }
    else if (cellinfo.provider == "sprint") {
        prlfile = SprintPrlFile;
    }
    time_t prl_time = 0;
    if (prlfile != "") {
        prl_time = fileTime(prlfile);
    }
    logError("Updating files");
    string update_cmd = "updatefiles";
    if (prlfile != "") {
        update_cmd += " -p " + prlfile;
    }
    update_cmd += " &";
    str_system(update_cmd);
    monitor_flags |= IN_UPDATEFILES;
    while (true) {
        monWait(1);
        string pid = backtick("pgrep updatefiles");
        if (pid == "") break;
        getStateInfo();
        needConnection();
    }
    monitor_flags &= ~IN_UPDATEFILES;
    if ( (prlfile != "") and
         (fileTime(prlfile) > prl_time) ) {
        updatePrl(prlfile);
    }
    system("upgrade_media &");
}

void updatePrl(string prlfile) {
}

void cleanUpgradeDir() {
    if ( (kernelId == "1") or (kernelId == "2") or single_os ) {
        if (fileExists(UpgradeStateFile)) {
            string state = getStringFromFile(UpgradeStateFile);
//            if ( (state == "untarring") or (state == "upgrading") ) {
//                system("upgrade -c &");
//            }
//            else if (state != "fetching") {
//                str_system(string("rm -rf ") + UpgradeDir + "/*");
//            }
            if ( (state == "starting") or
                 (state == "upgraded") or
                 (state == "failed") ) {
                str_system(string("rm -rf ") + UpgradeDir + "/*");
            }
        }
    }
}

void checkUpgrading() {
    if ( (kernelId == "1") or (kernelId == "2") or single_os ) {
        if (fileExists(UpgradeStateFile)) {
            string state = getStringFromFile(UpgradeStateFile);
            if ( (state == "fetching") or
                 (state == "untarring") or
                 (state == "upgrading") ) {
                logError("Restarting upgrade");
                system("upgrade -c &");
            }
        }
    }
}

void getMonthlyUsage() {
    string month = getDateTime();
    month = month.substr(0,4) + month.substr(5,2);
    waninfo.usage_month = month;
    unsigned long monthly_usage = 0;
    string tmp_usage_file = "/tmp/" + month;
    string usage_file = UsageDir;
    usage_file += "/" + month;
    if (fileExists(tmp_usage_file)) {
        monthly_usage = stringToULong(getStringFromFile(tmp_usage_file));
    }
    else if (fileExists(usage_file)) {
        monthly_usage = stringToULong(getStringFromFile(usage_file));
    }
    monthly_usage += waninfo.monthly_usage;
    waninfo.monthly_usage = monthly_usage;
    if (!fileExists(tmp_usage_file)) {
        updateFile(tmp_usage_file, toString(monthly_usage));
    }
    if (!fileExists(usage_file)) {
        updateFile(usage_file, toString(monthly_usage));
    }
}

void upLoop() {
    monitor_flags |= IN_UPLOOP;
    bool linkfailed = false;
//    startUnitcomm();
    comminfo.state = COMM_RUNNING;
    string last_commstate = "";
    unsigned long up_wait = 0;
    if (cellinfo.events_to_send) {
        up_wait = Uptime + 60;
        cellinfo.events_to_send = false;
    }
    if (gpsinfo.report_position) {
        string pos_event = eventGpsPosition();
        if (pos_event != "") {
            string event = "PositionReport";
            event += pos_event;
            eventlog.append(event);
        }
        gpsinfo.report_position = false;
    }
    unsigned long check_pppd_time = Uptime;
    unsigned long stats_time = Uptime;
    getCellInfo();
    while (true) {
        monWait(1);
        getStateInfo();
        if (smsMessages()) {
            clearSmsMessages();
            logError("Restarting unitcomm to run any queued commands");
            restartUnitcomm();
            cellinfo.command_timeout = Uptime + CommandTimeout;
        }
//        if (!car_state.ignition and (cellinfo.boot_delay != 0)) {
//            up_wait = Uptime + 60;
//            cellinfo.boot_delay = 0;
//        }
        if (!getWan(false)) {
//            logError("Wan interface went away");
            break;
        }
        if (cellinfo.overtemp) {
            killWan("overtemp");
        }
        if ((cellinfo.type == "ppp") and (Uptime-check_pppd_time) >= 60) {
            string pppd_pid = backtick("pgrep pppd");
            if (pppd_pid == "") {
                break;
            }
            check_pppd_time = Uptime;
        }
        if ((waninfo.tx_bytes+waninfo.rx_bytes) > 0) {
            updateMonthlyUsage();
        }
        if (up_wait > 0) {
            string commstate = getStringFromFile(CommStateFile);
            if (last_commstate == "") {
                if (commstate == "Connected") {
                    up_wait = Uptime + 60;
                    if (Uptime < cellinfo.command_timeout) {
                        cellinfo.command_timeout = Uptime + 60;
                    }
                }
            }
            last_commstate = commstate;
//            if (commstate == "Connected") {
//                up_wait = 0;
//            }
//            else {
                if (cellinfo.events_to_send) {
                    up_wait = Uptime + 60;
                    cellinfo.events_to_send = false;
                 }
                 if ( (Uptime > up_wait) or
                      (!fileExists(EventCountFile)) ) {
                     up_wait = 0;
                 }
//            }
        }
        else {
            cellinfo.events_to_send = false;
        }
        if (Uptime > timeinfo.ntp_timer) {
           setNtpTime();
        }
        if (!(needConnection() or (up_wait > 0)) and
             (comminfo.state == COMM_RUNNING))  {
            string commstate = getStringFromFile(CommStateFile);
            if (commstate == "Connected") {
                string event = "Connection closed";
                event += eventDeltaTime(cellinfo.state, true);
                event += eventUsage(false);
                event += eventUptime();
                event += eventPosition();
                event += eventDowStatus();
                eventlog.append(event);
                logError("sending stop to unicomm");
                comminfo.state = COMM_STOPPING;
                comminfo.stop_time = time(NULL);
                sendStopToUnitcomm();
            }
            else {
                logError("Killing PPP...unitcomm connection was closed");
                killWan("connection closed");
//                stopUnitcomm();
                comminfo.state = COMM_PPPDKILLED;
            }
        }
        else if (comminfo.state == COMM_STOPPING) {
            string commstate = getStringFromFile(CommStateFile);
//            logError("commstate:"+commstate);
            if (commstate.find("Closed") != string::npos) {
                killWan("connection closed");
                comminfo.state = COMM_PPPDKILLED;
            }
            if ((time(NULL)-comminfo.stop_time) > 60) {
//                stopUnitcomm();
                logError("Killing PPP...unitcomm did not close");
                killWan("connection closed");
                comminfo.state = COMM_PPPDKILLED;
            }
        }
//        else if (comminfo.state == COMM_STOPPED) {
//            logError("Killing ppp after comm closed");
//            killWan("connection closed");
//            comminfo.state = COMM_PPPDKILLED;
//        }
        else {
            if ((Uptime - stats_time) >= 300) {
                bool check_link = false;
                if (waninfo.sum_rx_bytes == 0) {
                    check_link = true;
                }
                int old_ratelimit = waninfo.ratelimit;
                getRateLimit();
                if (waninfo.ratelimit != old_ratelimit) {
                    string ratelimit_event = "Ratelimit";
                    ratelimit_event += eventRateLimit();
                    ratelimit_event += eventUptime();
                    eventlog.append(ratelimit_event);
                }
                string event = "Stats";
                event += eventDeltaTime(cellinfo.state, true);
                event += eventUsage(true);
                event += eventWifiUsers();
                event += eventConnection();
                event += eventPosition();
                event += eventUptime();
                event += eventDowStatus();
                eventlog.append(event);
//                logError(event);
                stats_time = Uptime;
                if (check_link) {
                    logError("Nothing received...checking link");
                    if (!checkLink()) {
                        linkfailed = true;
                    }
                }
            }
            if ( (gpsinfo.report_interval != 0) and
                 ((Uptime-gpsinfo.report_time) >= gpsinfo.report_interval) ) {
//                if ( (gpsinfo.report_enabled_until != 0) and
//                     (time(NULL) > gpsinfo.report_enabled_until) ) {
//                    gpsinfo.report_interval = 0;
//                }
                if ( (gpsinfo.report_interval != 0) and
                     (gpsinfo.position != "") ) {
                    string pos_event = eventGpsPosition();
                    if (pos_event != "") {
                        string event = "PositionReport";
                        event += pos_event;
                        eventlog.append(event);
                    }
                }
            }
            if (linkfailed) {
//               stopUnitcomm();
                killWan("link failure");
            }
        }
    }
    getCellInfo();
//    stopUnitcomm();
    comminfo.state = COMM_STOPPED;
    string reason = getPppCloseCause();
    if (reason != "closed") {
        logError("Connection "+reason);
        string event = "Connection ";
        event += reason;
        event += eventDeltaTime(cellinfo.state, true);
        event += eventUsage(false);
        event += eventUptime();
        event += eventPosition();
        eventlog.append(event);
    }
    for (int l=0; l < 5; l++) {
        monWait(1);
        if (backtick("pgrep pppd") == "") {
            break;
        }
    }
    cellinfo.prev_state = cellinfo.state;
    cellinfo.state = "down";
    monitor_flags &= ~IN_UPLOOP;
}

void updateMonthlyUsage() {
    string tmp_usage_file = "/tmp/" + waninfo.usage_month;
    string month = getDateTime();
    month = month.substr(0,4) + month.substr(5,2);
    if (month != waninfo.usage_month) {
        unlink(tmp_usage_file.c_str());
        writeMonthlyUsage();
        waninfo.monthly_usage = 0;
        waninfo.usage_month = month;
        writeMonthlyUsage();
        tmp_usage_file = "/tmp/" + month;
    }
    updateFile(tmp_usage_file, toString(waninfo.monthly_usage));
}

void writeMonthlyUsage() {
    string usage_file = UsageDir;
    usage_file += "/" + waninfo.usage_month;
    updateFile(usage_file, toString(waninfo.monthly_usage));
}

void waitingLoop() {
    monitor_flags |= IN_WAITLOOP;
    if (needConnection()) {
        tellWhyCantConnect();
    }
    while (keepOn() and (!needConnection() or !canConnect())) {
        if (car_state.low_battery) {
            break;
        }
//        if (ignition_off() and !car_state.can_active and !needConnection()) {
//            break;
//        }
        monWait(1);
        getStateInfo();
        if ( (car_state.next_dow_status_time > 0) and
             (Uptime >= car_state.next_dow_status_time) ) {
            if (canConnect()) {
                cellinfo.events_to_send = true;
            }
            car_state.next_dow_status_time = Uptime + DowStatusInterval;
        }
        if (fileExists("/tmp/test_sync")) {
            wlan1info.scan_time = Uptime + Wlan1ScanTime;
            unlink("/tmp/test_sync");
        }
        if ( fileExists("/tmp/connect_wifi") ) {
            if ( (wlan1info.scan_time ==  0) and 
                 !wlan1info.connected ) {
                wlan1info.scan_time = Uptime + Wlan1ScanTime;
            }
        }
        else if ( (wlan1info.next_scan_time > 0) and
             (Uptime >= wlan1info.next_scan_time) ) {
            wlan1info.next_scan_time = Uptime + wlan1info.scan_period;
            wlan1info.scan_time = Uptime + Wlan1ScanTime;
        }
        if (wlan1info.scan_time > 0) {
            if (Uptime < wlan1info.scan_time) {
                if (!wlan1info.found_wap) {
                    updateFile(SyncStatusFile,"Scanning for WAP(s)");
                    wlan1info.found_wap = true;
                }
                string ap_str = backtick("config_aplist -1");
                if (ap_str != "") {
                    updateFile(SyncStatusFile,"Found WAP");
                    if (connectWifi(ap_str)) {
                        if (fileExists("/tmp/connect_wifi")) {
                            logError("Connected Wifi");
                            wlan1info.connected = 1;
                            wlan1info.scan_time = 0;
                        }
                        else {
                            updateFile(SyncStatusFile,"Starting homesync");
                            logError("Starting homesync");
                            system("homesync &");
                            wlan1info.syncing = true;
                            wlan1info.scan_time = 0;
                        }
                    }
                    else {
                        disconnectWifi();
                    }
                }
            }
            else {
                wlan1info.scan_time = 0;
                wlan1info.found_wap = false;
            }
        }
        if (wlan1info.connected and !fileExists("/tmp/connect_wifi")) {
            logError("Disconnecting Wifi");
            disconnectWifi();
            wlan1info.connected = false;
        }
        if (wlan1info.syncing) {
            string pid = backtick("ps ax | grep homesync | grep -v grep");
            if (pid == "") {
                logError("Disconnecting wifi");
                disconnectWifi();
                wlan1info.syncing = false;
                wlan1info.found_wap = false;
            }
        }
        if (smsMessages()) {
            clearSmsMessages();
            cellinfo.events_to_send = true;
            cellinfo.command_timeout = Uptime + CommandTimeout;
        }
        if ( (gpsinfo.report_interval != 0) and
//            !cellinfo.first_connection and
             ((Uptime-gpsinfo.report_time) >= gpsinfo.report_interval) ) {
//            if ( (gpsinfo.report_enabled_until != 0) and
//                 (time(NULL) > gpsinfo.report_enabled_until) ) {
//                gpsinfo.report_interval = 0;
//            }
            if ( (gpsinfo.report_interval != 0) and
                 (gpsinfo.position != "") ) {
                gpsinfo.report_position = true;
                cellinfo.events_to_send = true;
                if (!canConnect()) {
                    string pos_event = eventGpsPosition();
                    if (pos_event != "") {
                        string event = "PositionReport";
                        event += pos_event;
                        eventlog.append(event);
                    }
                }
                logError("Need to report postition");
            }
        }
    }
    monitor_flags &= ~IN_WAITLOOP;
//    tellWhyNeedConnection();
}

bool connectWifi(string ap_str) {
    bool retval = false;
    if (!fileExists("/boot/ar6102")) {
        retval = true;
        ifdownWlan0();
        sleep(1);
        logError("rmmod ar6000");
        if (system("rmmod ar6000") != 0) {
            logError("rmmod failed");
            retval = false;
        }
        if (retval) {
            sleep(1);
            logError("modprobe ar6103 devmode=sta,sta");
            if (system("modprobe ar6103 devmode=\"sta,sta\"") != 0) {
                logError("modprobe failed");
                retval = false;
            }
        }
        Strings vals = csvSplit(ap_str, " ");
        string wifi_config = csvUnquote(vals[0]);
        string ip_config = csvUnquote(vals[1]);
        string essid = vals[2];
        string iwconfig_cmd = "iwconfig wlan0 mode managed essid "+essid;
        string type = wifi_config;
        string key = "";
        if (wifi_config != "off") {
            int pos = wifi_config.find(":");
            type = wifi_config.substr(0, pos);
            key = csvQuote(wifi_config.substr(pos+1));
        }
        if (type == "wep") {
            key = makeHexKey(csvUnquote(key));
            iwconfig_cmd += " enc "+key;
        }
        if (retval) {
            sleep(1);
            logError(iwconfig_cmd);
            if (str_system(iwconfig_cmd) != 0) {
                updateFile(SyncStatusFile, "Wifi:iwconfig failed");
                logError("iwconfig failed");
                retval = false;
            }
        }
        sleep(1);
        if (retval and (type == "wpa")) {
            writeWpaConf(essid, key);
            logError("wpa_supplicant");
            str_system(string("wpa_supplicant -i wlan0 -c ")+WpaSupplicantFile+" >/var/log/wpa_supplicant.log 2>&1 &");
            bool auth = false;
            for (int i=0; i < 30; i++) {
                monWait(1);
                getStateInfo();
                string wpa_status = backtick("wpa_cli status");
                if (wpa_status.find("COMPLETED") != string::npos) {
                    auth = true;
                    break;
                }
            }
            if (!auth) {
                logError("WPA authentication failed");
                updateFile(SyncStatusFile, "Wifi:Authentication failed");
                retval = false;
            }
        }
        if (retval) {
            if (ip_config == "dhcp") {
                logError("ifconfig wlan0 up");
                system("ifconfig wlan0 up");
                sleep(1);
                logError("udhcpc -i wlan0");
                int dhcp_stat = system("udhcpc -i wlan0 -n");
                if (dhcp_stat != 0) {
                    updateFile(SyncStatusFile, "Wifi:Failed to obtain DHCP address");
                    logError("Failed to obtain DHCP address");
                    disconnectWifi();
                    retval = false;
                }
            }
            else {
                Strings ip_parms = csvSplit(ip_config, ":");
                string ip_addr = ip_parms[0];
                string netmask = ip_parms[1];
                string router = ip_parms[2];
                string dns_server = ip_parms[3];
                string ifconfig_cmd = "ifconfig wlan0 "+ip_addr+" netmask "+netmask+" up";
                string resolvconf_str = "";
                if (dns_server != "") {
                    resolvconf_str += "nameserver "+dns_server;
                }
                str_system(ifconfig_cmd);
                writeStringToFile("/rw/etc/resolv.conf", resolvconf_str);
                if (router != "") {
                    string route_cmd = "route add default gw "+router;
                    str_system(route_cmd);
                }
            }
        }
    }
    return retval;
}

void ifdownWlan0() {
    if (fileExists(HostapdConfFile)) {
        logError("pkill hostapd");
        system("pkill hostapd");
        sleep(1);
    }
    logError("ifdown wlan0");
    system("ifdown wlan0");
    wlan0info.off = true;
}

void ifupWlan0() {
    logError("ifup wlan0");
    system("ifup wlan0");
    if (fileExists(HostapdConfFile)) {
        str_system(string("hostapd -B ")+HostapdConfFile);
    }
    wlan0info.off = false;
}

string makeHexKey(string enc) {
    int enclen = enc.size();
    if ( (enclen == 5) or
         (enclen == 13) or
         (enclen == 16) ) {
        string enchex = "";
        for (int i=0; i < enclen; i++) {
            char hexbuf[3];
            unsigned char ch = enc.at(i);
            sprintf(hexbuf, "%02x", ch);
            enchex += hexbuf;
        }
        enc = enchex;
    }
    return enc;
}

void writeWpaConf(string essid, string key) {
    string conf = "ctrl_interface=DIR=/var/run/wpa_supplicant\n";
    conf += "network={\n";
    conf += "    ssid="+essid+"\n";
    conf += "    scan_ssid=1\n";
    conf += "    key_mgmt=WPA-PSK\n";
    conf += "    pairwise=CCMP TKIP\n";
    conf += "    psk="+key+"\n";
    conf += "}";
    writeStringToFile(WpaSupplicantFile, conf);
}

void disconnectWifi() {
    system("route del default");
    system("cp /ro/etc/resolv.conf /rw/etc/resolv.conf");
    system("pkill udhcpc");
    sleep(1);
    system("pkill wpa_supplicant");
    sleep(1);
    if (system("rmmod ar6000") != 0) {
        logError("rmmod failed");
    }
    sleep(1);
    if (system("modprobe ar6103 devmode=\"ap,sta\"") != 0) {
        logError("modprobe failed");
    }
    sleep(1);
    ifupWlan0();
}

void resetWlan0(string reason) {
    logError("Resetting wlan0: "+reason);
    string event = "NotifyEngineering wlan0 faiure ("+reason+")... resetting";
    eventlog.append(event);
    ifdownWlan0();
    sleep(1);
    if (system("rmmod ar6000") != 0) {
        logError("rmmod failed");
        reboot("rmmod failed");
    }
    string logger = backtick("ls /etc/init.d/*logging");
    str_system(logger+" stop");
    unlink("/var/log/messages");
    unlink(Wlan0AssocFile);
    str_system(logger+" start");
    system("pkill wifi_counter");
    sleep(1);
    string modprobe_cmd = "modprobe ar6103 devmode=\"ap,sta\"";
    if (fileExists("/boot/ar6102")) {
        modprobe_cmd = "modprobe ar6102";
    }
    if (str_system(modprobe_cmd) != 0) {
        logError("modprobe failed");
    }
    sleep(1);
    if (wlan0info.enabled) {
        ifupWlan0();
    }
}

void wlan1Loop() {
}

void monWait(int timeout) {
    monitor_flags |= IN_MONWAIT;
    openVic();
    time_t end_t = time(NULL) + timeout;
    while (timeout > 0) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        if (vic.checkForFrame()) {
            Byte buffer[64];
            int len = vic.getResp(buffer, 0);
            if (len > 0) {
                processVicMessage(len, buffer);
            }
        }
        if (vic_notifies.checkForFrame()) {
            Byte buffer[64];
            int len = vic_notifies.getResp(buffer, 0);
            if (len > 0) {
                processVicMessage(len, buffer);
            }
        }
        int vic_socket = vic.getSocket();
        if (vic_socket >= 0) {
            FD_SET(vic_socket, &rd);
            nfds = max(nfds, vic_socket);
        }
        int notifies_socket = vic_notifies.getSocket();
        if (notifies_socket >= 0) {
            FD_SET(notifies_socket, &rd);
            nfds = max(nfds, notifies_socket);
        }
        struct timeval tv;
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        int r = select(nfds+1, &rd, &wr, &er, &tv);
        if ((r == -1) && (errno == EINTR)) {
            timeout = end_t - time(NULL) + 1;
            continue;
        }
        if (r < 0) {
            perror("select()");
            exit(1);
        }
        if (r > 0) {
            if ((vic_socket >= 0) and (FD_ISSET(vic_socket, &rd))) {
                Byte buffer[64];
                int len = vic.getResp(buffer, 0);
                if (len > 0) {
                    processVicMessage(len, buffer);
                }
            }
            if ((notifies_socket >= 0) and (FD_ISSET(notifies_socket, &rd))) {
                Byte buffer[64];
                int len = vic_notifies.getResp(buffer, 0);
                if (len > 0) {
                    processVicMessage(len, buffer);
                }
            }
        }
        if (r == 0) {
            break;
        }
        timeout = end_t - time(NULL) + 1;
    }
    monitor_flags &= ~IN_MONWAIT;
}

void openVic() {
    if (vic.getSocket() < 0) {
        vic.openSocket();
    }
    if (vic_notifies.getSocket() < 0) {
        vic_notifies.openSocket(sizeof(vic_notifications), vic_notifications);
    }
}

void checkVicVersion() {
    if (vic.build != vic.requested_build) {
        string binfile = backtick("ls /boot/VIC-"+vic.requested_build+"-*.dat | tail -1");
        if (binfile != "") {
            updateVic(binfile);
        }
    }
    else if (vic.build == "OBDII") {
        if (car_state.vin != "") {
            string make = backtick("vin_to_make "+car_state.vin);
            if ( (make == "Dodge") or (make == "Chrysler") or
                 (make == "Jeep") or (make == "Ram") ) {
                vic.getBusStructure(app_id);
                if (vic.bus1active) {
                    string requested_build = "CHY11";
                    if (vic.bus1type == _29bit) {
                        requested_build = "CHY29";
                    }
                    string binfile = backtick("ls /boot/VIC-"+requested_build+"-*.dat | tail -1");
                    if (binfile != "") {
                        updateVic(binfile);
                    }
                }
            }
        }
    }
}

void getVicVersion() {
    vic.getVersion(app_id);
//    car_state.vic_version = vic.long_version;
//    car_state.vic_version_short = vic.version;
//    car_state.vic_build = vic.build;
//    car_state.vic_requested_build = vic.requested_build;
//    logError("VIC version: "+car_state.vic_build+"-"+car_state.vic_version_short);
    logError("VIC version: "+vic.build+"-"+vic.version_str);
}

int getVicResponse(int reqid, int reqlen, Byte *buffer) {
    monitor_flags |= IN_VICWAIT;
    int resplen = 0;
    openVic();
    if (vic.getSocket() != -1) {
        if (reqlen == 0) {
            vic.sendRequest(app_id, reqid);
        }
        else {
            vic.sendRequest(app_id, reqid, reqlen, buffer);
        }
        unsigned long end_time = Uptime + 15;
        while (true) {
            int len = vic.getResp(buffer, 1);
            if (len > 0) {
                if (buffer[1] == reqid) {
                    resplen = len;
//                    logError(string_printf("got %d bytes in response to 0x%02X command", len, reqid));
                    break;
                }
                else {
                    processVicMessage(len, buffer);
                }
            }
            else if (len == -1) {
                // socket error
                break;
            }
            else if (len < -1) {
                // process vic error message
                processVicMessage(-len, buffer);
            }
            Uptime = getUptime();
            if (Uptime > end_time) {
                break;
            }
        }
    }
    monitor_flags &= ~IN_VICWAIT;
    return resplen;
}

void processVicMessage(int len, Byte *buffer) {
    Byte appid = buffer[0];
    Byte type = buffer[1];
//    logError(string_printf("Got %d bytes for response 0x%02X",len,type));
    if (type == 0x20) {
        handleVicStatus(len, buffer);
    }
    else if (type == 0xFF) {
        // Error returned
        int code = buffer[3];
        logError("Vic returned error: "+toString(code));
        if (code == 0x01) {
            // Authentication error - Rtc clock error
            logError("Vic authentication error...setting RTC");
            setVicRtc();
            if (!fileExists(RtcErrorFile)) {
                string event = "NotifySupport RTC failed";
                eventlog.append(event);
                touchFile(RtcErrorFile);
            }
        }
    }
    else if (type == 0xF0) {
        // Emergency Notification
        logError("Got emergency notification");
        car_state.ecall_state = "starting";
    }
    else if (type == 0xF1) {
	// Ignition Change Notification
        bool ign = buffer[2];
        logError("Got ignition change notification:"+toString(ign));
        if (car_state.ignition != ign) {
            car_state.ignition = ign;
            doIgnitionProcess();
        }
    }
    else if (type == 0xF2) {
        // Low Battery Notification
        logError("Got LowBattery Notification");
//        if (!car_state.low_battery) {
//            sendLowBatteryEvent();
//            car_state.low_battery = true;
//        }
    }
    else if (type == 0xF4) {
        logError("Got QuickScan Notification");
        car_state.get_quickscan = true;
        car_state.quickscan_ecus = buffer[2];
        int year = buffer[3] + 2000;
        int mon = buffer[4];
        int day = buffer[5];
        int hour = buffer[6];
        int min = buffer[7];
        int sec = buffer[8];
        car_state.quickscan_idx = 0;
        car_state.quickscan_cnt = 0;
        car_state.quickscan_str = string_printf(" QSTime:%04d/%02d/%02dT%02d:%02d:%02d",
                             year, mon, day, hour, min, sec);
        if (car_state.quickscan_ecus == 0) {
            car_state.get_quickscan = false;
        }
    }
    else if (type == 0xF5) {
        car_state.check_fault_time = Uptime;
    }
    else if (type == 0xF8) {
        int status_type = (buffer[2] << 8) | buffer[3];
        if (status_type == 1) {
            int remote_aborted_cause = buffer[4];
            logError("Remote start aborted notification: "+string_printf("%02X", remote_aborted_cause));
            if (car_state.remote_started) {
                car_state.remote_aborted_cause = remote_aborted_cause;
                car_state.remote_aborted_time = Uptime;
            }
        }
        else if (status_type == 2) {
           int vta_cause = buffer[4];
           logError("VTA cause: "+string_printf("%02X", vta_cause));
           if (car_state.alarm_cause == -1) {
               car_state.alarm_report_time = Uptime + 10;
           }
           else {
               car_state.alarm_report_time = Uptime;
           }
           car_state.alarm_cause = vta_cause;
        }
        else {
            logError("Unknown VIC status notification: "+toString(status_type));
        }
    }
}

void decodeVicStatus(int len, Byte *buffer) {
    Byte type = buffer[1];
    Byte status = buffer[2];
    bool last_engine = car_state.engine;
    car_state.locks = buffer[3];
    car_state.doors = buffer[4];
    car_state.ign_time = ((int)buffer[5] << 8) | buffer[6];
    car_state.batval = buffer[7] / 10. + car_state.batadjust;
    car_state.vic_status = buffer[8];
    car_state.ignition = status & 0x01;
    car_state.can_active = status & 0x80;
    car_state.engine = status & 0x02;
    car_state.moving = status & 0x04;
    int last_alarm = car_state.alarm_state;
    if (car_state.engine or (type == 0x22)) {
        car_state.check_engine = ((status & 0x10) >> 4) | 0x02;
    }
    if (!car_state.can_active) {
        if (fileExists("/tmp/moving")) {
            car_state.moving = (bool)stringToInt(getStringFromFile("/tmp/moving"));
        }
        if (fileExists("/tmp/engine")) {
            car_state.engine = (bool)stringToInt(getStringFromFile("/tmp/engine"));
        }
    }
    if (fileExists(FaultsFile)) {
        string faults = getStringFromFile(FaultsFile);
        if (faults == "") {
            faults = "none";
        }
        sendFaultsEvent(faults);
        unlink(FaultsFile);
    }
    if (fileExists("/tmp/chkeng")) {
        car_state.check_engine = stringToInt(getStringFromFile("/tmp/chkeng")) | 0x02;
    }
    if (last_engine != car_state.engine) {
        if (car_state.engine) {
            car_state.engine_was_on = true;
            if (fileExists(LowFuelFile)) {
                car_state.low_fuel_timer = Uptime;
            }
        }
    }
    car_state.vic_errors = (car_state.vic_status & 0x0F) | car_state.vic_reported_errors;
    if (car_state.vic_errors == 0x00) {
        if (!fileExists(VicStatusFile)) {
             updateFile(VicStatusFile, "Passed");
        }
    }
    else {
        string vic_status = "";
        if (car_state.vic_errors & 0x01) {
            vic_status += ",RtcFail";
        }
        if (car_state.vic_errors & 0x02) {
            vic_status += ",CellPowerFail";
        }
        if (car_state.vic_errors & 0x04) {
            vic_status += ",VicTestFail";
        }
        if (car_state.vic_errors & 0x08) {
            vic_status += ",CanBusFail";
        }
        if (vic_status != "") {
            vic_status.erase(0, 1);
        }
        if (car_state.vic_errors != car_state.vic_reported_errors) {
            string event = "NotifySupport Vic error "+vic_status;
            eventlog.append(event);
            logError("Vic error: "+vic_status);
            car_state.vic_reported_errors = car_state.vic_errors;
            updateFile(VicStatusFile, vic_status);
        }
    }
    if (vic.protocol_version >= 0x0229) {
        car_state.can_status_bits = (car_state.vic_status & 0x60) >> 5;
    }
    car_state.vic_reset = (car_state.vic_status & 0x80);
    car_state.alarm_state = 0x07; //SNA
    if (car_state.doors != 0xFF) {
        if (car_state.doors & 0x10) {
            car_state.trunk_opened = true;
        }
        else {
            car_state.trunk_opened = false;
        }
    }
    if (len > 9) {
        int more_state = (buffer[9] << 8) | buffer[10];
//        if (car_state.vic_version > "000.027.000") {
        if (vic.protocol_version >= 0x0217) {
            car_state.alarm_state = (more_state & 0xE000) >> 13;
            if ( (vic.protocol_version >= 0x0229)  and
                 (more_state & 0x0020) ) {
                if ( ( (car_state.alarm_state < 0x03) or
                       (car_state.alarm_state == 0x07) ) and
                     ( (last_alarm < 0x03) or
                       (last_alarm == 0x07) ) ) {
                    car_state.alarm_state = 0x04;
                }
            }
        }
        else {
            if (buffer[9] & 0x80) {
                car_state.alarm_state = 0x04; // Alarm on
            }
            else {
                car_state.alarm_state = 0x08; // Alarm off
            }
        }
        if (more_state & 0x1000) {
            car_state.trunk_opened = true;
            if (!car_state.trunk_opened_sent) {
                logError("VIC reported trunk was opened");
            }
        }
        if (vic.protocol_version >= 0x0224) {
            car_state.ignition_status = (more_state & 0x0E00) >> 9;
            if (car_state.engine) {
                int lf = (more_state & 0x0100) >> 8;
                if (lf) {
                    car_state.low_fuel = lf | 0x02;
                    car_state.low_fuel_timer = 0;
                }
                else {
                    if ( (car_state.low_fuel_timer == 0) or
                         ((Uptime-car_state.low_fuel_timer) > 10) ) {
                        car_state.low_fuel = lf | 0x02;
                        car_state.low_fuel_timer = 0;
                    }
                }
            }
            car_state.key_status = (more_state & 0x00C0) >> 6;
        }
        int capabilities = (buffer[11] << 8) | buffer[12];
        updateCarCapabilities(capabilities);
    }
}

void handleVicStatus(int len, Byte *buffer) {
    bool last_ign = car_state.ignition;
    bool last_can = car_state.can_active;
    bool last_moving = car_state.moving;
    bool last_engine = car_state.engine;
    int last_alarm = car_state.alarm_state;
    int last_low_fuel = car_state.low_fuel;
    decodeVicStatus(len, buffer);
    if (car_state.can_active and car_state.ignition) {
        if (!fileExists(CurrentVinFile) or (getStringFromFile(CurrentVinFile) == "")) {
            getVin();
//kanaan            setEssid();
        }
        if (car_state.can_failed) {
            checkCan();
        }
    }
    if (car_state.batval < car_state.lowbatval) {
        if (car_state.low_bat_time == 0) {
            car_state.low_bat_time = Uptime;
        }
        else if ((Uptime-car_state.low_bat_time) >= 120) {
            if (!car_state.low_battery) {
                sendLowBatteryEvent();
                car_state.low_battery = true;
            }
            // shutdown in deep sleep
            //logError("Power down - low bat");
            //powerOff("LowBat", 0x02, 0);
        }
    }
    else if (car_state.low_bat_time != 0) {
        car_state.low_bat_time = 0;
    }
    if (fileExists(IgnFile)) {
        car_state.ignition = stringToInt(getStringFromFile(IgnFile));
    }
    if (car_state.remote_started) {
        if (car_state.moving) {
            logError("Remote start ended: moving");
            car_state.remote_started = false;
            car_state.remote_start_time = 0;
            unlink(RemoteStartedFile);
        }
//        else if ( (car_state.ignition_status != 0x07) and
//                  (car_state.ignition_status != 0x01) ) {
//            logError("Remote start ended: ignition");
//            car_state.remote_started = false;
//            car_state.remote_start_time = 0;
//            unlink(RemoteStartedFile);
//        }
        else if ( car_state.engine and
                  (car_state.key_status == 2) ) {
            logError("Remote start ended: ignition");
            car_state.remote_started = false;
            car_state.remote_start_time = 0;
            unlink(RemoteStartedFile);
        }
    }
    if (last_engine != car_state.engine) {
        if ( !car_state.engine and car_state.remote_started) {
            car_state.remote_aborted_time = Uptime + 10;
            if ((Uptime-car_state.remote_start_time) > 890) {
               car_state.remote_aborted_cause = 0x01; // Normal Timeout
            }
            else {
                car_state.remote_aborted_cause = 0x3F; // Unknown (SNA)
                int fuel = getFuelLevel();
                if (car_state.last_car_command == "engine off") {
                    car_state.remote_aborted_cause = 0x100; // We turned it off
                }
                else if (car_state.capabilities & 0x4000) {
                    car_state.remote_aborted_cause = 0x1C; // RKE Off Message
                }
                //else if (fuel < 5) {
                else if (car_state.low_fuel == 0x03) {
                    car_state.remote_aborted_cause = 0x26; // LowFuel
                }
                else if (car_state.check_engine == 0x03) {
                    car_state.remote_aborted_cause = 0x17; // MIL on
                }
//                bool report_it = true;
//                if (report_it) {
//                    string event = "NotifyUser";
//                    event += " type:remote_start_aborted";
//                    event += " reason:"+cause;
//                    eventlog.append(event);
//                    if (canConnect()) {
//                        cellinfo.events_to_send = true;
//                    }
//                    logError("Remote stop aborted: "+cause);
//                }
            }
        }
    }
    if (last_ign != car_state.ignition) {
        doIgnitionProcess();
    }
    if (last_can != car_state.can_active) {
        if (car_state.can_active) {
            logError("CAN turned on");
            if (!fileExists(CarControlCommandFile) and (Uptime >= car_state.command_hold)) {
                logError("WokeOnCanFile set - CAN change");
                touchFile(WokeOnCanFile);
            }
        }
        else {
            logError("Can turned off");
            car_state.can_stopped = true;
            logError("WokeOnCanFile removed");
            unlink(WokeOnCanFile);
        }
    }
//    if (car_state.remote_started and !car_state.engine) {
//        car_state.remote_started = false;
//        unlink(RemoteStartedFile);
//    }
    if (car_state.low_fuel != last_low_fuel) {
        logError("Low fuel indicator changed: "+toString(car_state.low_fuel));
    }
//        if (car_state.low_fuel & 0x02) {
        if (car_state.low_fuel != car_state.low_fuel_sent) {
            if (car_state.ignition_change_time == 0) {
                sendLowFuelEvent();
            }
        }
//        }
    if (car_state.trunk_opened) {
        if (car_state.valet_mode and !car_state.trunk_opened_sent) {
            string event = "ValetAlert";
            event += " TrunkOpened";
            event += eventGpsPosition();
            eventlog.append(event);
            if (canConnect()) {
                cellinfo.events_to_send = true;
                logError("Need to report ValetAlert TrunkOpened");
            }
            car_state.trunk_opened_sent = true;
        }
    }
    else {
        if (car_state.trunk_opened_sent) {
            car_state.trunk_opened_sent = false;
        }
    }
    if (last_moving != car_state.moving) {
        if (car_state.moving) {
            int fuel = getFuelLevel();
            long int odom_x10 = getOdometerX10();
            if ( (fuel != -1) and (car_state.stopped_fuel_level != -1) and
                 (odom_x10 != -1) ) {
                int fuel_diff = fuel - car_state.stopped_fuel_level;
                if (fuel_diff > 5) {
                    string event = "CarState";
                    event += " Odom:" + string_printf("%.1f", odom_x10/10.);
                    if (car_state.obd2_unit) {
                        event += " Fuel%:" + toString(fuel);
                        event += " AddedFuel%:" + toString(fuel_diff);
                    }
                    else {
                        event += " Fuel:" + toString(fuel);
                        event += " AddedFuel:" + toString(fuel_diff);
                    }
                    eventlog.append(event);
                    logError("AddedFuel");
                    if (canConnect()) {
                        cellinfo.events_to_send = true;
                        logError("Need to report AddedFuel");
                    }
                }
            }
            if (car_state.seatbelts_check_time > 0) {
                car_state.seatbelts_check_time = Uptime + car_state.seatbelts_stopped_time;
                car_state.seatbelts_stopped_time = 0;
            }
        }
        else {
            // We just stopped
            // Get odometer and fuel level
            car_state.stopped_fuel_level = getFuelLevel();
            car_state.stopped_odometer = getOdometerX10();
            car_state.old_seats = 0;
            if (car_state.seatbelts_check_time > 0) {
                car_state.seatbelts_stopped_time = Uptime - car_state.seatbelts_check_time;
            }
        }
    }
    if (last_alarm != car_state.alarm_state) {
//        logError("Alarm change: "+toString(last_alarm)+"->"+toString(car_state.alarm_state));
        if ( (car_state.alarm_state >= 0x03) and
             (car_state.alarm_state <= 0x06) ) {
            logError("Alarm detected: "+toString(car_state.alarm_state));
            car_state.alarm_report_state = car_state.alarm_state;
            if (car_state.alarm_cause == -1) {
                car_state.alarm_report_time = Uptime + 30;
                car_state.alarm_cause = 0x00; //Unknown
            }
            else {
                car_state.alarm_report_time = Uptime;
            }
        }
        if (car_state.alarm_state == 0x02) {
            updateCarCapabilities(0x0002);
        }
    }
}

void updateCarCapabilities(int capabilities) {
    int old_cap = car_state.capabilities;
    int new_cap = old_cap | capabilities;
    if (old_cap != new_cap) {
        car_state.capabilities = new_cap;
        car_state.make_capabilities_string();
        updateFile(CapabilitiesFile, toString(new_cap));
        backupAutonet();
    }
}

void sendLowFuelEvent() {
    string event = "CarState";
    event += eventLowFuel();
    eventlog.append(event);
    if (canConnect()) {
        cellinfo.events_to_send = true;
        logError("Need to report Low Fuel");
    }
}

void sendLowBatteryEvent() {
    string event = "CarState";
    string batval = string_printf("%.1f", car_state.batval);
    event += " Bat:" + batval;
    eventlog.append(event);
    if (canConnect()) {
        cellinfo.events_to_send = true;
        logError("Need to report Low Battery");
    }
}

long int getOdometerX10() {
    Byte buffer[64];
    long int odom = -1;
    int len = getVicResponse(0x28, 0, buffer);
    if (len == 5) {
        odom = (buffer[2]<<16) +
                   (buffer[3]<<8) +
                   buffer[4];
        if (odom == 0xFFFFFF) {
            odom = -1;
        }
    }
    if (fileExists(OdomFile)) {
        odom = stringToInt(getStringFromFile(OdomFile));
    }
    return odom;
}

int getFuelLevel() {
    Byte buffer[64];
    int fuel = -1;
    int len = getVicResponse(0x25, 0, buffer);
    if (len == 3) {
        fuel = buffer[2];
        if (fuel == 0xFF) {
            fuel = -1;
        }
    }
    if (fileExists(FuelFile)) {
        fuel = stringToInt(getStringFromFile(FuelFile));
    }
    return fuel;
}

int getSpeed() {
    Byte buffer[64];
    int speed = -1;
    int len = getVicResponse(0x27, 0, buffer);
    if (len == 4) {
        speed = (buffer[2] << 8) | buffer[3];
        if (speed == 0xFFFF) {
            speed = -1;
        }
        else {
            speed = speed/128. + .5;
        }
    }
    if (fileExists(SpeedFile)) {
        speed = stringToInt(getStringFromFile(SpeedFile));
    }
    return speed;
}

void doIgnitionProcess() {
    car_state.ignition_change_time = Uptime;
    if (car_state.ignition) {
        logError("Ignition turned on");
        car_state.ignition_delay = 5;
        car_state.ignition_off_time = 0;
        car_state.ignition_on_time = time(NULL);
        car_state.ignition_was_on = true;
        car_state.engine_was_on = false;
//        if (car_state.vic_version < "000.027.000") {
        if (vic.protocol_version <= 0x0216) {
            car_state.check_fault_time = Uptime + 10;
        }
//        unlink(RemoteStartedFile);
        wlan0info.ign_off_time = 0;
        unlink(IgnitionOffFile);
    }
    else {
        logError("Ignition turned off");
        car_state.ignition_delay = 2;
        car_state.ignition_off_time = time(NULL);
        if ( !car_state.engine_was_on and
             (car_state.ignition_on_time != 0) and
             ((car_state.ignition_off_time - car_state.ignition_on_time) < 90) ) {
            string event = "OnOff";
            eventlog.append(event);
        }
        car_state.ignition_on_time = 0;
//        car_state.check_fault_time = Uptime;
        system("pkill -HUP gps_mon");
        wlan0info.ign_off_time = Uptime;
        touchFile(IgnitionOffFile);
        car_state.stopped_fuel_level = -1;
        car_state.seats = 0;
        car_state.last_seats = 0;
        car_state.belts = 0;
        car_state.last_belts = 0;
        car_state.seatbelts_delay_time = 0;
        car_state.seatbelts_check_time = 0;
        car_state.seatbelts_stopped_time = 0;
    }
}

bool ignition_on() {
    return car_state.ignition and (car_state.ignition_change_time == 0);
}

bool ignition_off() {
    return !car_state.ignition and (car_state.ignition_change_time == 0);
}

bool canConnect() {
     return cellinfo.can_connect;
}

void checkConnect() {
    bool can_connect = true;
    string reason = "";
    if (cellinfo.signal == "0") {
        reason += " NoSignal";
        can_connect = false;
    }
    if (cellinfo.overtemp) {
        reason += " Overtemp";
        can_connect = false;
    }
    if (wlan1info.syncing) {
        reason += " Wlan1Syncing";
        can_connect = false;
    }
    else if ( (wlan1info.scan_time > 0) and
         (Uptime < wlan1info.scan_time) ) {
        reason += " Wlan1Scanning";
        can_connect = false;
    }
    if (cellinfo.activation_required) {
        if (cellinfo.activation_in_progress) {
            reason += " ActivatingCell";
            can_connect = false;
        }
        else if (!cellinfo.activated) {
            reason += " NotActivated";
            can_connect = false;
        }
    }
    if ((cellinfo.type == "ppp") and !fileExists(CellModemDevice)) {
        reason += " NoCellDevice";
        can_connect = false;
    }
    if (cellinfo.no_config) {
        reason += " NoCellConfig";
        can_connect = false;
    }
    if (cellinfo.no_apn) {
        string apn = grepFile(GsmConfFile, "apn:");
        if (apn == "") {
            reason += " NoApn";
            can_connect = false;
        }
    }
    if (fileExists(DontConnectFile)) {
        cellinfo.dont_connect = true;
        reason += " DontConnectFile";
        can_connect = false;
    }
    else {
        cellinfo.dont_connect = false;
    }
    if ((cellinfo.sim != "") and (cellinfo.sim != "Ready")) {
       reason += " Sim:"+cellinfo.sim;
       can_connect = false;
    }
    if (reason != "") {
        reason.erase(0,1);
    }
    cellinfo.why_cant_connect = reason;
    updateFile(CantConnectReasonFile, reason);
    cellinfo.can_connect = can_connect;
}

void tellWhyCantConnect() {
    string reason = "";
    if (canConnect()) {
        reason = "No reason not to connect";
    }
    else {
        reason = "Can't connect because:"+cellinfo.why_cant_connect;
    }
    logError(reason);
}

bool smsMessages() {
    bool retval = false;
    if (!fileExists(NoSmsCommandsFile)) {
        string resp = getCellResponse("checksms");
        if (resp == "Yes") {
            resp = getCellResponse("listsms");
            logError("SMS:"+resp);
            retval = true;
        }
    }
    return retval;
}

void clearSmsMessages() {
    string resp = getCellResponse("clearsms");
}

bool keepOn() {
    return cellinfo.keepon;
}

void checkKeepOn() {
    bool keepon = false;
    string why = "";
    if (car_state.low_battery) {
       keepon = false;
    }
    else {
        if (car_state.can_active) {
            why += " CanActive";
            keepon = true;
        }
        if (!ignition_off()) {
            why += " Ignition";
            keepon = true;
        }
        if (fileExists(KeepOnFile) or car_state.keep_on) {
            why += " KeepOn";
            keepon = true;
        }
        if (car_state.ecall_state != "") {
            why += " Ecall";
            keepon = true;
        }
        if ( (car_state.ignition_off_time > 0) and
             (car_state.stay_on_delay > 0) and
             ((time(NULL)-car_state.ignition_off_time) < car_state.stay_on_delay) ) {
            why += " StayOn";
            keepon = true;
        }
        if ( (wlan1info.scan_time > 0) and
             (Uptime < wlan1info.scan_time) ) {
            why += " Wlan1Scanning";
            keepon = true;
        }
        if (wlan1info.syncing) {
            why += " HomeSync";
            keepon = true;
        }
        if (fileExists(DupingFilesystemFile)) {
            why += " DupingFilesystem";
            keepon = true;
        }
        if (booting != 0) {
            string verify_pid = pgrep("verify_sys");
            if (booting == 1) {
                if (verify_pid != "") {
                    logError("verify_sys running");
                    booting = 2;
                }
            }
            else {
                if (verify_pid == "") {
                    logError("verify_sys completed");
                    booting = 0;
                }
            }
            if (booting != 0) {
                why += " Verifying";
                keepon = true;
            }
        }
    }
    if (why != "") {
        why.erase(0,1);
    }
    updateFile(KeepOnReasonFile, why);
    cellinfo.why_keepon = why;
    cellinfo.keepon = keepon;
}

bool needConnection() {
    return cellinfo.need_connection;
}

void checkNeedConnection() {
    bool needconnection = false;
    string why = "";
    if (wlan1info.up) {
        needconnection = false;
    }
    else if (cellinfo.dont_connect) {
        needconnection = false;
    }
    else if (car_state.low_battery and !cellinfo.events_to_send) {
        needconnection = false;
    }
    else {
        if (wlan0info.up) {
            bool wlan0 = true;
            if ( (wlan0info.ign_off_time > 0) and
                 (wlan0info.max_on_time > 0) and
                 ((Uptime-wlan0info.ign_off_time) > wlan0info.max_on_time) ) {
                wlan0 = false;
            }
            if (wlan0) {
                why += " wlan0";
                needconnection = true;
            }
        }
//        if (eth0info.up) {
//            why += " eth0";
//            needconnection = true;
//        }
        if (ignition_on() or car_state.can_active or (cellinfo.temp_fail_cnt < 3)) {
            if (cellinfo.events_to_send) {
                why += " events_to_send";
                needconnection = true;
            }
            if (fileExists(UpgradeStateFile) and
                (getStringFromFile(UpgradeStateFile) == "fetching")) {
                if (backtick("pgrep upgrade") != "") {
                    why += " upgrading";
                    needconnection = true;
                }
            }
        }
//        if (fileExists(KeepOnFile) or car_state.keep_on) {
//            why += " KeepOn";
//            needconnection = true;
//        }
//        if (car_state.ecall_state != "") {
//            why += " Ecall";
//            needconnection = true;
//        }
//        if ( (car_state.ignition_off_time > 0) and
//             (car_state.stay_on_delay > 0) and
//             ((time(NULL)-car_state.ignition_off_time) < car_state.stay_on_delay) ) {
//            why += " StayOn";
//            needconnection = true;
//        }
        if (car_state.remote_started) {
            why += " remote_started";
            needconnection = true;
        }
        if (car_state.alarm_report_time > 0) {
            why += " report_alarm";
            needconnection = true;
        }
        if (Uptime < car_state.command_hold) {
            why += " car_command_hold";
            needconnection = true;
        }
        if (Uptime < cellinfo.command_timeout) {
            why += " waiting_for_command";
            needconnection = true;
        }
        if (Uptime < cellinfo.support_hold) {
            why += " support_hold";
            needconnection = true;
        }
        if (fileExists(CellularKeepupFile) and keepOn()) {
            why += " cellular_keepup";
            needconnection = true;
        }
//        if (car_state.ignition or (car_state.ignition_change_time > 0)) {
        if (!ignition_off() or car_state.can_active) {
            if (eth0info.up) {
                why += " eth0";
                needconnection = true;
            }
            if (cellinfo.firstreactivate) {
                why += " firstreactivate";
                needconnection = true;
            }
            if (cellinfo.testing_keepup) {
                why += " testing_keepup";
                needconnection = true;
            }
//            if (fileExists(CellularKeepupFile)) {
//                why += " cellular_keepup";
//                needconnection = true;
//            }
            if (Uptime < cellinfo.boot_delay) {
                why += " boot_delay";
                needconnection = true;
            }
            if ( (gpsinfo.report_interval != 0) and
                 (gpsinfo.report_interval <= 300) ) {
                why += " report_interval";
                needconnection = true;
            }
            if (gpsinfo.fleet_enabled) {
                why += " fleet";
                needconnection = true;
            }
        }
    }
    if (why != "") {
        why.erase(0,1);
    }
    cellinfo.why_need_connection = why;
    updateFile(ConnectionReasonFile, why);
    cellinfo.need_connection =  needconnection;
}

void tellWhyNeedConnection() {
    if (wlan1info.up) {
        logError("We don't need a connection, wlan1");
    }
    else if (!needConnection()) {
        logError("We don't need a connection");
    }
    else {
        string why = "Need connection because:"+cellinfo.why_need_connection;
        logError(why);
    }
}

string eventConnection() {
    string str = "";
//    if (cellinfo.diag_available) {
        str += " Conn:" + cellinfo.connection_type;
        if (cellinfo.roaming) {
            str += " roaming";
        }
        str += " Sig:" + cellinfo.signal;
//    }
    return str;
}

string eventDeltaTime(string cellstate, bool updatetime) {
    string str = "";
    string state = "D";
    unsigned long deltatime = Uptime - cellinfo.last_report;
    if (updatetime) {
        cellinfo.last_report = Uptime;
    }
    if (cellstate == "boot") {
        state = "B";
    }
    else if (cellstate == "up") {
        state = "U";
    }
    str += " " + state + ":" + toString(deltatime);
    return str;
}

string eventIp() {
    string str = "";
    str += " IP:" + waninfo.ipAddress;
    return str;
}

string eventPosition() {
    string str = "";
    if (!car_state.location_disabled) {
        if (gpsinfo.position != "") {
            str += " gps:";
            str += gpsinfo.position;
        }
//        if (fileExists(GpsPositionFile)) {
//            readGpsPosition();
//            if (gpsinfo.position != "") {
//                str += " gps:";
//                str += gpsinfo.position;
//            }
//            gpsinfo.report_time = Uptime;
//        }
//        else {
//            getCellPosition();
//            if (cellinfo.position != "") {
//                str += " cellpos:";
//                str += cellinfo.position;
//            }
//        }
    }
    gpsinfo.report_time = Uptime;
    return str;
}

string eventGpsPosition() {
    string str = "";
    if (!car_state.location_disabled) {
        if (fileExists(GpsPositionFile)) {
            readGpsPosition();
        }
        if (gpsinfo.position != "") {
            str += " gps:";
            str += gpsinfo.position;
        }
    }
    gpsinfo.report_time = Uptime;
    return str;
}

string eventCellInfo() {
    string str = "";
    Assoc oldinfo = readAssoc(PermCellInfoFile, " ");
    if (cellinfo.got_profile) {
        if (cellinfo.mdn != oldinfo["MDN"]) {
            str += " MDN:" + cellinfo.mdn;
        }
        if (cellinfo.min != oldinfo["MIN"]) {
            str += " MIN:" + cellinfo.min;
        }
        if (cellinfo.esn != oldinfo["ESN"]) {
            str += " ESN:" + cellinfo.esn;
        }
        if (cellinfo.prl != oldinfo["PRL"]) {
            str += " PRL:" + cellinfo.prl;
        }
        if (cellinfo.imei != oldinfo["IMEI"]) {
            str += " IMEI:" + cellinfo.imei;
        }
        if (cellinfo.smsc != oldinfo["SMSC"]) {
            str += " SMSC:" + cellinfo.smsc;
        }
        if (cellinfo.imsi != oldinfo["IMSI"]) {
            str += " IMSI:" + cellinfo.imsi;
        }
        if (cellinfo.iccid != oldinfo["ICCID"]) {
            str += " ICCID:" + cellinfo.iccid;
        }
        if (str != "") {
            str_system(string("cp -f ") + TempCellInfoFile + " " + PermCellInfoFile);
        }
    }
    else {
        cellinfo.mdn = oldinfo["MDN"];
    }
    return str;
}

string eventFailCount() {
    string str = "";
    if (cellinfo.fail_count > 0) {
        str = " Fails:" + toString(cellinfo.fail_count);
        cellinfo.fail_count = 0;
    }
    return str;
}

string eventHdwInfo() {
    string str = "";
    if (cellinfo.first_connection) {
        str += " HDW";
        string cpu = grepFile(ProcCpuinfo, "Hardware");
        if (cpu != "") {
            cpu = cpu.substr(cpu.find(": ")+2);
            int pos;
            while ((pos=cpu.find(" ")) != string::npos) {
                cpu.replace(pos, 1, "_");
            }
            str += ":";
            str += cpu;
        }
        Assoc hdw = readAssoc(HardwareConfFile, " ");
        string wifi = "AR6103";
        if (fileExists(Ar6102File)) {
            wifi = "AR6102";
        }
        str += ":wifi=" + wifi;
        string model = "";
        if (cellinfo.model != "") {
            model = cellinfo.model;
        }
        else if (hdw["cell_model"] != "") {
            model = hdw["cell_model"];
        }
        else if (hdw["cell_type"] != "") {
            model = hdw["cell_type"];
        }
        if (model != "") {
            str += ":cell=" + model;
            if (cellinfo.provider != "") {
                str += "(" + cellinfo.provider + ")";
            }
        }
        string model_info = getCellResponse("getmodel");
        if (model_info != "ERROR") {
            Strings vals = split(model_info, ",");
            if (vals.size() > 2) {
                string cell_version = vals[2];
                str += ":cellvers=" + cell_version;
            }
        }
        string flashsize = getFlashSize();
        if (flashsize != "") {
            str += ":flash=" + flashsize;
        }
        string memsize = grepFile(ProcMeminfo, "MemTotal");
        if (memsize != "") {
            memsize = getToken(memsize, 1);
            memsize = kbToMeg(memsize);
            str += ":mem=" + memsize;
        }
        if (!single_os) {
            str += ":kernel=" + kernelId;
        }
        string uboot_vers = backtick("nanddump -l 0x80000 /dev/mtd0 | strings | grep 'U-Boot ' | grep -v BUG | sed 's/.*(//' | sed 's/ - .*$//' | sed 's/ /_/g'");
        str += ":uboot="+uboot_vers;
        str += ":vic="+vic.build+"-"+vic.version_str;
        string brand = backtick("getbranding");
        str += ":brand=" + brand;
        string hdw_fail = "0";
        if (fileExists(HardwareFailureFile)) {
            hdw_fail = getStringFromFile(HardwareFailureFile);
        }
        str += ":hdfail="+hdw_fail;
    }
    return str;
}

string getFlashSize() {
    string flashsize = regrepFile(ProcPartitions, "hda$");
    if (flashsize != "") {
        flashsize = getToken(flashsize, 2);
        flashsize = kbToMeg(flashsize);
    }
    else {
        Strings lines = readFileArray(ProcPartitions);
        int mtdtotal = 0;
        for (int i=0; i < lines.size(); i++) {
            string ln = lines[i];
            if (ln.find("mtdblock") != string::npos) {
                string blocks = getToken(ln, 2);
                int kb = stringToInt(blocks);
                mtdtotal += kb;
            }
        }
        if (mtdtotal > 0) {
            flashsize = kbToMeg(mtdtotal);
        }
    }
    return flashsize;
}

string kbToMeg(string kbstr) {
    int kb = stringToInt(kbstr);
    return kbToMeg(kb);
}

string kbToMeg(int kb) {
    int mb = kb / 1024;
    int size = 1;
    while (mb > size) {
        size <<= 1;
    }
    string units = "M";
    if (size >= 1024) {
        size = size / 1024;
        units = "G";
    }
    string outstr = toString(size) + units;
    return outstr;
}

string eventRateLimit() {
    string str = "";
    str += " RateLimit:";
    str += toString(waninfo.ratelimit);
    str += " MonthlyUsage:";
    str += toString(waninfo.monthly_usage);
    return str;
}

string eventUptime() {
    string str = "";
    str += " Uptime:" + toString(Uptime);
    return str;
}

string eventUsage(bool withspeed) {
    string str = "";
    char speedbuf[10];
    str += " Up:";
    str += toString(waninfo.sum_tx_bytes);
    if (withspeed) {
        str += "(";
        sprintf(speedbuf, "%.1f", waninfo.max_tx_kBps);
        str += speedbuf;
        str += ")";
    }
    str += " Down:";
    str += toString(waninfo.sum_rx_bytes);
    if (withspeed) {
        str += "(";
        sprintf(speedbuf, "%.1f", waninfo.max_rx_kBps);
        str += speedbuf;
        str += ")";
    }
    waninfo.max_tx_kBps = waninfo.max_rx_kBps = 0.;
    waninfo.sum_tx_bytes = waninfo.sum_rx_bytes = 0;
    return str;
}

string eventWifiUsers() {
    string str = "";
    str = " wifi:" + toString(wlan0info.users);
    return str;
}

string eventCarInfo(bool showign, bool getlast) {
    string str = "";
    bool pass = false;
    Byte buffer[64];
    int len;
    if (showign) {
        //IGN
        string ign = "off";
        if (car_state.ignition) {
            ign = "on";
        }
        str += " Ign:"+ign;
    }
    string vin = car_state.vin;
    if (vin == "") {
        vin = "none";
    }
    str += " VIN:"+vin;
    //Fuel level = xx (liters)
    int fuel = getFuelLevel();
    if (fuel != -1) {
        if (car_state.obd2_unit) {
            str += " Fuel%:" + toString(fuel);
        }
        else {
            str += " Fuel:" + toString(fuel);
        }
        pass = true;
    }
    //Oil life = xx (percent)
    len = getVicResponse(0x24, 0, buffer);
    if (len == 3) {
        if (buffer[2] != 0xFF) {
            str += " Oil:" + toString(buffer[2]);
            pass = true;
        }
    }
    //Odometer = xxxxxx (km)
    long int odom_x10 = getOdometerX10();
    car_state.stopped_odometer = odom_x10;
    if (odom_x10 != -1) {
        str += " Odom:" + string_printf("%.1f", odom_x10/10.);
        pass = true;
    }
    //Tire pressures = xx/xx/xx/xx/xx(xx/xx) (kpa)
    len = getVicResponse(0x23, 0, buffer);
    if (len == 9) {
        if ( (buffer[4] != 0xFF) and
             (buffer[5] != 0xFF) and
             (buffer[6] != 0xFF) and
             (buffer[7] != 0xFF) ) {
            str += " Tires:";
            str += toString(buffer[4]) + "/";
            str += toString(buffer[5]) + "/";
            str += toString(buffer[6]) + "/";
            str += toString(buffer[7]);
            if (buffer[8] != 0xFF) {
                str += "/" + toString(buffer[8]);
            }
            if ( (buffer[2] != 0xFF) and
                 (buffer[3] != 0xFF) ) {
                str += "(" + toString(buffer[2]) + "/" +
                             toString(buffer[3]) + ")";
            }
            pass = true;
        }
    }
    if (vic.protocol_version >= 0x0217) {
        int len = getVicResponse(0x30, 0, buffer);
        if (len > 0) {
            int seats = buffer[2];
            int belts = buffer[3];
            if ((seats != 0xFF) and (belts != 0xFF)) {
                updateCarCapabilities(0x0001);
            }
        }
    }
    // Normal car status
    int type = 0x20;
    if (getlast) {
        type = 0x22;
    }
    len = getVicResponse(type, 0, buffer);
//    if (len == 8) {
    if (len > 5) {
//        decodeVicStatus(len, buffer);
        handleVicStatus(len, buffer);
        if (car_state.locks != 0xFF) {
            //Locks = xx (hex)
            str += " Locks:" + toString(car_state.locks);
//            pass = true;
        }
        if (car_state.doors != 0xFF) {
            //Doors = xx (hex)
            str += " Doors:" + toString(car_state.doors);
//            pass = true;
        }
        string batval = string_printf("%.1f", car_state.batval);
        str += " Bat:" + batval;
    }
//    if (!car_state.ignition) {
        str += " IgnOff:" + toString(car_state.ignition_off_time);
//    }
    string eng = "off";
    if (car_state.engine) {
        eng = "on";
    }
    str += " Eng:" + eng;
    if (car_state.alarm_state != 0x07) {
        str += " Alarm:" + toString(car_state.alarm_state);
    }
    if (showign) {
        if  (car_state.check_engine & 0x02) {
            string chkeng = "0";
            if (car_state.check_engine & 0x01) {
               chkeng = "1";
            }
            str += " ChkEng:" + chkeng;
        }
        if (car_state.low_fuel & 0x02) {
            if (!car_state.ignition) {
                if  (car_state.low_fuel & 0x01) {
                    touchFile(LowFuelFile);
                }
                else {
                    unlink(LowFuelFile);
                }
            }
            str += eventLowFuel();
        }
        if (!car_state.ignition) {
            string faults = getFaults();
            if (faults != "") {
            }
        }
    }
    string passfail = "Failed";
    if (pass) {
        passfail = "Passed";
    }
    updateFile(CanStatusFile, passfail);
    logError("CarInfo:"+str);
    return str;
}

string eventLowFuel() {
    string str = "";
    if (car_state.low_fuel & 0x02) {
        string lfs = "0";
        if (car_state.low_fuel & 0x01) {
            lfs = "1";
        }
        str += " LowFuel:" + lfs;
        car_state.low_fuel_sent = car_state.low_fuel;
    }
    return str;
}

string eventDowStatus() {
    string str = "";
    if (dow_unit) {
        str = " dow";
        str += ":can=";
        if (car_state.can_active) {
            str += "active";
        }
        else {
            str += "nonactive";
        }
        str += ":lastlog="+toString(fileTime(DowLastLogFile));
        if (fileExists(SyncStatusFile)) {
            string sync_state = getStringFromFile(SyncStatusFile);
            int pos;
            while ((pos=sync_state.find(" ")) != string::npos) {
                sync_state.replace(pos, 1, "_");
            }
            str += ":sync="+sync_state;
            str += ":lastsync="+toString(fileTime(SyncStatusFile));
        }
        car_state.next_dow_status_time = Uptime + DowStatusInterval;
    }
    return str;
}

bool getWan(bool init) {
    unsigned long rx_bytes = 0;
    unsigned long rx_frames = 0;
    unsigned long tx_bytes = 0;
    unsigned long tx_frames = 0;
    waninfo.exists = false;
    bool got_counts = false;
    if (cellinfo.type == "lte") {
        string resp = getCellResponse("isconnected");
        if ((resp != "no") and (resp != "")) {
            waninfo.exists = true;
//            cellinfo.connected = true;
        }
        else {
            cellinfo.connected = false;
        }
        resp = getCellResponse("getusage");
        if (resp.find("rxbytes") != string::npos) {
            rx_bytes = stringToULong(getParm(resp,"rxbytes:",","));
            tx_bytes = stringToULong(getParm(resp,"txbytes:",","));
            got_counts = true;
        }
    }
    else {
        string dev_line = grepFile(ProcDevFile, cellinfo.device);
        if (dev_line != "") {
            waninfo.exists = true;
            Strings vals = split(dev_line, " :");
            rx_bytes = stringToULong(vals[1]);
            rx_frames = stringToULong(vals[2]);
            tx_bytes = stringToULong(vals[9]);
            tx_frames = stringToULong(vals[10]);
            got_counts = true;
        }
        else {
            cellinfo.connected = false;
        }
    }
    if (got_counts) {
        if (init) {
            waninfo.rx_bytes = 0;
            waninfo.tx_bytes = 0;
            waninfo.sum_rx_bytes = 0;
            waninfo.sum_tx_bytes = 0;
        }
        else {
            unsigned long rx_delta = 0;
            if (rx_bytes < waninfo.last_rx_bytes) {
                logError("wrapped rx_bytes new:"+toString(rx_bytes)+" last:"+toString(waninfo.last_rx_bytes));
                rx_delta = 0xFFFFFFFF - waninfo.last_rx_bytes + rx_bytes + 1;
            }
            else {
                rx_delta = rx_bytes - waninfo.last_rx_bytes;
            }
            waninfo.rx_bytes = rx_delta;
            waninfo.sum_rx_bytes += rx_delta;
            unsigned long tx_delta = 0;
            if (tx_bytes < waninfo.last_tx_bytes) {
                logError("wrapped tx_bytes new:"+toString(tx_bytes)+" last:"+toString(waninfo.last_tx_bytes));
                tx_delta = 0xFFFFFFFF - waninfo.last_tx_bytes + tx_bytes + 1;
            }
            else {
                tx_delta = tx_bytes - waninfo.last_tx_bytes;
            }
            waninfo.tx_bytes = tx_delta;
            waninfo.sum_tx_bytes += tx_delta;
            double now = getDoubleUptime();
            double deltatime = now - waninfo.lasttime;
            waninfo.lasttime = now;
            waninfo.tx_kBps = tx_delta / deltatime / 1000.;
            waninfo.rx_kBps = rx_delta / deltatime / 1000.;
            waninfo.max_tx_kBps = max(waninfo.tx_kBps, waninfo.max_tx_kBps);
            waninfo.max_rx_kBps = max(waninfo.rx_kBps, waninfo.max_rx_kBps);
            waninfo.monthly_usage += tx_delta + rx_delta;
        }
        waninfo.last_rx_bytes = rx_bytes;
        waninfo.last_tx_bytes = tx_bytes;
    }
    return waninfo.exists;
}

void getWlan0() {
    string dev_line = grepFile(ProcDevFile, "wlan0");
    if (dev_line != "") {
        Strings vals = split(dev_line, " :");
        unsigned long rx_bytes = stringToULong(vals[1]);
        unsigned long rx_frames = stringToULong(vals[2]);
        unsigned long tx_bytes = stringToULong(vals[9]);
        unsigned long tx_frames = stringToULong(vals[10]);
//        if (rx_frames != wlan0info.last_rx_frames)  {
//            wlan0info.being_used = true;
//        }
        wlan0info.last_rx_frames = rx_frames;
        wlan0info.last_tx_frames = tx_frames;
    }
    string wlan0_arps = grepFile(ProcArpFile, "wlan0");
    if (wlan0_arps != "") {
        wlan0info.being_used = true;
    }
    string iwlist_error = backtick("iwlist wlan0 channel 2>&1 >/dev/null");
    if (iwlist_error != "") {
//kanaan        resetWlan0("iwlist:"+trim(iwlist_error));
    }
}

void getRateLimit() {
    int ratelimit = 0;
    if (fileExists(RateLimitFile)) {
        ratelimit = stringToInt(getStringFromFile(RateLimitFile));
    }
    waninfo.ratelimit = ratelimit;
}

void getStateInfo() {
    Uptime = getUptime();
    if (cellinfo.no_config) {
        getCellConfig();
    }
    if (!cellinfo.got_profile and (cellinfo.talker != "")) {
        getCellProfile();
    }
    checkFeaturesChange();
    if (!wlan0info.off) {
//kanaan        getWlan0();
    }
    getUserConnections();
    if ((Uptime - cellinfo.getinfo_time) >= 10) {
        getCellInfo();
        cellinfo.getinfo_time = Uptime;
    }
    string statestr = "";
    statestr += getDateTime();
    statestr += " ";
    statestr += cellinfo.state;
    statestr += " ";
    statestr += cellinfo.connection_type;
    statestr += " sig:";
    statestr += cellinfo.signal;
    if (cellinfo.roaming) {
        statestr += " roaming";
    }
    updateFile(CellularStateFile, statestr);
    if (fileExists(SupportHoldTimeFile)) {
        string timestr = getStringFromFile(SupportHoldTimeFile);
        cellinfo.support_hold = Uptime + stringToInt(timestr);
        unlink(SupportHoldTimeFile);
    }
    if (fileExists(EcallFile)) {
        car_state.ecall_state = getStringFromFile(EcallFile);    
    }
    else if ( (car_state.ecall_state != "") and
              (car_state.ecall_state != "starting") ) {
        car_state.ecall_state = "";
        cellinfo.events_to_send = true;
        logError("Need to report ecall stopped");
    }
    if (fileExists(GpsTimeFile)) {
        timeinfo.ntp_timer = Uptime + 3600;
        timeinfo.last_celltime = 0;
        timeinfo.celltime_timer = Uptime + 60;
        if ((timeinfo.state == Close) or (timeinfo.state == Bad)) {
            setVicRtc();
        }
        timeinfo.state = Good;
    }
    else if ((timeinfo.state != Good) or (Uptime >= timeinfo.celltime_timer)) {
        getCellTime();
    }
    if (Uptime > timeinfo.rtc_timer) {
        setVicRtc();
    }
//    if (gpsinfo.clearing) {
//        checkGpsCleared();
//    }
    if ((Uptime-gpsinfo.position_time) > gpsinfo.position_interval) {
        readGpsPosition();
        gpsinfo.position_time = Uptime;
    }
    if (fileExists(CarControlCommandFile)) {
        string carcommand = getStringFromFile(CarControlCommandFile);
        car_state.last_car_command = carcommand;
//        if (carcommand == "engine on") {
//            car_state.remote_started = true;
//        }
        logError("Got car command: keeping online for additional 5 minutes");
        car_state.command_hold = Uptime + CommandHoldTime;
        unlink(CarControlCommandFile);
        unlink(WokeOnCanFile);
    }
    if (car_state.remote_started) {
        if (!fileExists(RemoteStartedFile)) {
            logError("Remote start stopped");
            car_state.remote_started = false;
            car_state.remote_start_time = 0;
        }
    }
    else {
        if (fileExists(RemoteStartedFile)) {
            car_state.remote_started = true;
            car_state.remote_start_time = Uptime;
            logError("Remote started");
        }
    }
    if ( (car_state.ignition_change_time != 0) and
         ((Uptime-car_state.ignition_change_time) >= car_state.ignition_delay) ) {
        logError("Adding CarState event");
        string car_event = "CarState";
        car_event += eventCarInfo(true,false);
        car_event += eventGpsPosition();
        eventlog.append(car_event);
        if (canConnect()) {
            cellinfo.events_to_send = true;
            logError("Need to report Ignition change");
        }
        if (!car_state.ignition and wlan1info.homelan_enabled) {
            if (fileExists(ApListFile)) {
                wlan1info.scan_time = Uptime + Wlan1ScanTime + 120;
            }
        }
        car_state.ignition_change_time = 0;
    }
    if ( (car_state.check_fault_time > 0) and
         (Uptime > car_state.check_fault_time) ) {
        check_for_faults();
    }
    if (car_state.can_stopped) {
        getVicStatus(true);
        string car_event = "CarState";
        car_event += eventCarInfo(false,true);
        car_event += " Seats:0/0";
        car_event += " Cap:"+car_state.capabilities_str;
        car_event += eventGpsPosition();
        eventlog.append(car_event);
        if (canConnect()) {
            cellinfo.events_to_send = true;
            logError("Need to report CAN off");
        }
        car_state.can_stopped = false;
    }
    if (car_state.remote_aborted_time != 0) {
        if (Uptime > car_state.remote_aborted_time) {
            reportRemoteAborted();
            car_state.remote_started = false;
            car_state.remote_start_time = 0;
            car_state.remote_aborted_time = 0;
            unlink(RemoteStartedFile);
        }
    }
    if (car_state.alarm_report_time != 0) {
       if (Uptime > car_state.alarm_report_time) {
           reportAlarm();
           car_state.alarm_report_time = 0;
       }
    }
//    if ( wifi_enabled and 
//         (wifi_enabled_until != 0) and
//         (time(NULL) > wifi_enabled_until) ) {
//        system("autonet_router");
//    }
//    getVicStatus(false);
    checkKeepOn();
    checkNeedConnection();
    checkConnect();
    checkCarStatus();
    if (fileExists("/tmp/monitor_lock")) {
        logError("Locking up monitor");
        unlink("/tmp/monitor_lock");
        monitor_flags |= IN_MONITOR_LOCK;
        sleep(300);
        logError("Continuing");
        monitor_flags &= ~IN_MONITOR_LOCK;
    }
    saveTmpState();
    if (fileExists("/tmp/reboot_it")) {
        unlink("/tmp/reboot_it");
        reboot("/tmp/reboot_it");
    }
}

void saveTmpState() {
    string state_str = getStateString();
    updateFile(TempStateFile, state_str);
}

void savePermState() {
    string state_str = getStateString();
    updateFile(PermStateFile, state_str);
}

string getStateString() {
    string state_str = "";
    state_str += toString(Uptime);
    string cellst = "";
    if (cellinfo.state == "boot") {
        cellst = "B";
    }
    else if (cellinfo.state == "down") {
        cellst = "D";
    }
    else if (cellinfo.state == "up") {
        cellst = "U";
    }
    state_str += ":" + cellst;
    state_str += ":" + toString(Uptime-cellinfo.last_report);
    state_str += ":" + toString(waninfo.sum_tx_bytes);
    state_str += ":" + toString(waninfo.sum_rx_bytes);
    state_str += ":" + toString(cellinfo.fail_count);
    return state_str;
}

void getCellInfo() {
    string connection_type = "unknown";
    string signal = "unknown";
    string roaming = "";
    if (cellinfo.activation_in_progress) {
        string pid = backtick("pgrep cell_shell");
        if (pid == "") {
            string resp = readFile("/tmp/activate.out");
            if (resp.find("OK") != string::npos) {
                cellinfo.activation_required = false;
                cellinfo.activated = true;
                logError("Activation successful");
            }
            else {
                logError("Activation failed");
            }
            cellinfo.activation_in_progress = false;
        }
    }
    string netinfo = getCellResponse("getnetinfo");
//    cerr << "getnetinfo:" << netinfo << endl;
    if (netinfo != "") {
        connection_type = getParm(netinfo, "type:", ",");
        roaming = getParm(netinfo, "roam:", ",");
        if (roaming == "1") {
            roaming == "roaming";
        }
        signal = getParm(netinfo, "signal:", ",");
        if (signal == "") {
            if (connection_type == "1xRTT") {
                signal = getParm(netinfo, "1xrttsig:", ",");
//                cerr << "1xrttsig:" << signal << endl;
            }
            else {
                signal = getParm(netinfo, "evdosig:", ",");
//                cerr << "evdosig:" << signal << endl;
            }
        }
    }
    cellinfo.overtemp = false;
    string tempinfo = getCellResponse("gettemp:1");
    if (tempinfo.find("overtemp") != string::npos) {
        cellinfo.overtemp = true;
    }
    cellinfo.connection_type = connection_type;
    cellinfo.signal = signal;
    if (roaming == "roaming") {
        cellinfo.roaming = true;
    }
    else {
        cellinfo.roaming = false;
    }
//    cerr << "cellinfo.signal:" << cellinfo.signal << endl;
}

void getCellTime() {
    if (!fileExists(BadCellTimeFile)) {
        timeinfo.celltime_available = false;
        string celltime = getCellResponse("gettime");
        int year, mon, mday, hour, min;
        float f_sec;
        int n = sscanf(celltime.c_str(), "%d/%d/%d %d:%d:%f",
                       &year, &mon, &mday, &hour, &min, &f_sec);
        if ((n == 6) and (year > 2000)) {
            int sec = (int)f_sec;
            int usec = (int)((f_sec-sec)*1000+.5);
            struct tm tm;
            tm.tm_year = year - 1900;
            tm.tm_mon = mon - 1;
            tm.tm_mday = mday;
            tm.tm_hour = hour;
            tm.tm_min = min;
            tm.tm_sec = sec;
            time_t t = timegm(&tm);
            if ( (timeinfo.last_celltime == 0) or
                 ( (t > timeinfo.last_celltime) and
                   ((t-timeinfo.last_celltime) < 600)
               ) ) {
                time_t now = time(NULL);
                if (abs(now-t) > 2) {
                    logError("Setting system time from celltime");
                    struct timeval tv;
                    tv.tv_sec = timegm(&tm);
                    tv.tv_usec = usec;
                    settimeofday(&tv, NULL);
                }
                timeinfo.last_celltime = t;
                timeinfo.ntp_timer = Uptime + 3600;
//                if ((timeinfo.state == Bad) or (timeinfo.state == Close)) {
                    setVicRtc();
//                }
                timeinfo.state = Good;
                timeinfo.celltime_available = true;
            }
        }
    }
    timeinfo.celltime_timer = Uptime + 60;
}

void getCellPosition() {
    string position = getCellResponse("getposition");
//    cerr << "position:" << position << endl;
    if (position != "") {
        cellinfo.position = position;
    }
    else {
        cellinfo.position = "";
    }
}

void getUserConnections() {
    wlan0info.up = false;
    wlan0info.state = "down";
    wlan0info.users = 0;
    wlan1info.up = false;
    wlan1info.state = "down";
    eth0info.up = false;
    eth0info.state = "NoLink";

    string eth_resp = backtick("ethtool eth0");
    if (eth_resp.find("Link detected: yes") != string::npos) {
        string speed = getParm(eth_resp, "Speed: ", " \n");
        string duplex = getParm(eth_resp, "Duplex: ", " \n");
        eth0info.state = speed + "-" + duplex;
        eth0info.up = true;
    }
//    string mii_resp = backtick("mii-tool eth0");
//    if (mii_resp.find("link ok") != string::npos) {
//        Strings eth0words = split(mii_resp, " ");
//        eth0info.state = eth0words[2];
//        eth0info.up = true;
//    }
    if (fileExists(Eth0DownFile)) {
        eth0info.up = false;
        eth0info.state = "f-down";
    }
    if (!wlan0info.off) {
        int last_real_users = wlan0info.real_users;
        int wlan0_users = 0;
        wlan0_users = getWlan0Users();
        wlan0info.real_users = wlan0_users;
        int wlan0_fake_users = -1;
        if (fileExists(Wlan0UsersFile)) {
            wlan0_fake_users = stringToInt(getStringFromFile(Wlan0UsersFile));
        }
        if (wlan0_fake_users >= 0) {
            wlan0_users = wlan0_fake_users;
        }
        wlan0info.users = wlan0_users;
        if (wlan0_users > 0) {
            wlan0info.up = true;
            wlan0info.state = "up";
        }
        if ( (last_real_users == 0) and (wlan0info.real_users > 0) ) {
            wlan0info.last_check = Uptime;
            wlan0info.being_used = false;
        }
        if (wlan0info.real_users == 0) {
            wlan0info.being_used = false;
            wlan0info.last_check = 0;
        }
        if ( (wlan0info.last_check > 0) and
             ((Uptime - wlan0info.last_check) > 3600) ) {
            if (!wlan0info.being_used) {
//kanaan                resetWlan0("no data");
            }
            wlan0info.being_used = false;
            wlan0info.last_check = Uptime;
        }
    }
    string statestr = "";
    statestr += "wifi:";
    statestr += wlan0info.state;
    statestr += " users:";
    statestr += toString(wlan0info.users);
    statestr += " eth0:";
    statestr += eth0info.state;
    statestr += " wlan1:";
    statestr += wlan1info.state;
    updateFile(WifiStateFile, statestr);
}

int getWlan0Users() {
    int users = 0;
//    if (fileExists(Wlan0AssocFile)) {
//        Strings lines = readFileArray(Wlan0AssocFile);
//        for (int i=0; i < lines.size(); i++) {
//            if (lines[i].find("acaddr") != string::npos) {
//                users++;
//            }
//        }
//    }
    if (fileExists(StationListFile)) {
        Strings lines = readFileArray(StationListFile);
        users = lines.size();
    }
    return users;
}

void readGpsPosition() {
    if (fileExists(GpsPresentFile) and !car_state.location_disabled) {
        if (fileExists(GpsPositionFile)) {
            gpsinfo.position = getStringFromFile(GpsPositionFile);
        }
    }
}

void checkCarStatus() {
    Byte buffer[64];
//    if ((car_state.last_check_time == 0) or ((Uptime-car_state.last_check_time) > 10)) {
    int check_period = 5;
    if (car_state.remote_started) {
        check_period = 2;
    }
    if ((car_state.last_check_time == 0) or ((Uptime-car_state.last_check_time) > check_period)) {
        getVicStatus(false);
        if ( car_state.seatbelts_enabled and
             car_state.moving and
             (vic.protocol_version >= 0x0217) ) {
            int len = getVicResponse(0x30, 0, buffer);
            if (len > 0) {
                int seats = buffer[2];
                int belts = buffer[3];
                if ((seats != 0xFF) and (belts != 0xFF)) {
                    int prev_seats = car_state.seats;
                    int prev_belts = car_state.belts;
                    car_state.seats = seats;
                    car_state.belts = belts;
                    if (!car_state.can_active and fileExists(SeatsFile)) {
                        string seats_str = getStringFromFile(SeatsFile);
                        int seats = 0;
                        sscanf(seats_str.c_str(), "%x", &seats);
                        car_state.seats = seats & 0x0F;
                        car_state.belts = (seats>>4) & 0x0F;
                    }
                    car_state.seats |= car_state.old_seats | car_state.belts;
                    car_state.old_seats = car_state.seats;
                    if ( (car_state.seats != prev_seats) or
                         (car_state.belts != prev_belts) ) {
                       logError("Seatbelts changed: "+toString(car_state.seats)+"/"+toString(car_state.belts));
                    }
                    if ( (car_state.last_seats == 0) and
                         (car_state.seatbelts_delay_time == 0) ) {
//                        logError("Starting seatbelt delay");
                        car_state.seatbelts_delay_time = Uptime + car_state.seatbelts_delay;
                    }
                    else if (car_state.seatbelts_delay_time > 0) {
                        if ( (Uptime >= car_state.seatbelts_delay_time) or
                             (car_state.seats == car_state.belts) ) {
//                            logError("Sending initial seats status: "+toString(car_state.seats)+"/"+toString(car_state.belts));
                            notifySeatbelts();
                            car_state.seatbelts_delay_time = 0;
                        }
                    }
                    else if (car_state.seatbelts_check_time > 0) {
                        if (Uptime >= car_state.seatbelts_check_time) {
                            if ( (car_state.last_seats != car_state.seats) or
                                 (car_state.last_belts != car_state.belts) ) {
//                                logError("Sending changed seats status: "+toString(car_state.seats)+"/"+toString(car_state.belts));
                                notifySeatbelts();
                            }
                            car_state.seatbelts_check_time = 0;
                        }
                    }
                    else if ( (car_state.last_seats != car_state.seats) or
                              (car_state.last_belts != car_state.belts) ) {
                        if (car_state.seats == car_state.belts) {
//                            logError("Sending belted seats status: "+toString(car_state.seats)+"/"+toString(car_state.belts));
                            notifySeatbelts();
                            car_state.seatbelts_check_time = 0;
                        }
                        else {
//                            logError("Starting seatbelt change delay");
                            car_state.seatbelts_check_time = Uptime + car_state.seatbelts_delay;
                        }
                    }
                }
            }
        }
//        openVic();
//        if (vic.getSocket() != -1) {
//            vic.sendRequest(app_id, 0x20);
//            car_state.last_check_time = Uptime;
//        }
        if (car_state.sms_blocking) {
            int bt_kph = getSpeed();
            if (bt_kph > 254) { bt_kph = 254;
            }
            else if (bt_kph == -1) {
                bt_kph = 0;
            }
            if ((!car_state.engine) and (bt_kph == 0)) {
                bt_kph = 0xFF;
            }
            char ef = 'E';
            string ef_trailer = "Z0000 E";
            if (car_state.bluetooth_flag) {
               ef = 'F';
               ef_trailer = "00000 F";
            }
            car_state.bluetooth_flag = !car_state.bluetooth_flag;
            string bt_name = string_printf("cellcon%c%02X001%s",ef,bt_kph,ef_trailer.c_str());
            str_system("hciconfig hci0 name '"+bt_name+"'");
//            logError("Setting BT name: "+bt_name);
        }
        car_state.last_check_time = Uptime;
    }
    else if (car_state.get_quickscan) {
        buffer[0] = car_state.quickscan_idx;
        int len = getVicResponse(0x2E, 1, buffer);
        if (len > 3) {
            len -= 3;
            string data = string_printf(" ECU:%d:", car_state.quickscan_idx);
            for (int i=0; i < len; i++) {
                 data += string_printf("%02X", buffer[i+3]);
            }
            car_state.quickscan_str += data;
            car_state.quickscan_cnt++;
            car_state.quickscan_idx++;
            if ( (car_state.quickscan_idx >= car_state.quickscan_ecus) or
                 (car_state.quickscan_cnt > 100)) {
                string event = "CarState";
                event += car_state.quickscan_str;
                logError("Quickscan event: "+event);
                eventlog.append(event);
                car_state.quickscan_str = "";
            }
            if (car_state.quickscan_idx >= car_state.quickscan_ecus) {
                car_state.get_quickscan = false;
            }
        }
    }
}

void notifySeatbelts() {
    string event = "CarState";
    event += " Seats:"+toString(car_state.seats)+"/"+toString(car_state.belts);
    event += eventGpsPosition();
    eventlog.append(event);
    if (canConnect()) {
        cellinfo.events_to_send = true;
    }
    car_state.last_seats = car_state.seats;
    car_state.last_belts = car_state.belts;
}

void reportRemoteAborted() {
    bool report_it = true;
    string reason = "Unknown";
    int cause = car_state.remote_aborted_cause;
    if (cause == 0x3F) {
        reason = "Unknown";
    }
    else if (cause >= 0x100) {
        int a_cause = cause - 0x100;
        if (a_cause < (sizeof(AutonetRemoteAbortReasons)/sizeof(string))) {
            reason = AutonetRemoteAbortReasons[a_cause];
        }
    }
    else {
        if (cause == 0xFF) {
            reason = "Not Programmed";
        }
        else if (cause < (sizeof(RemoteAbortReasons)/sizeof(string))) {
             reason = RemoteAbortReasons[cause];
        }
    }
    logError("Remote start aborted reason: "+reason);
    if ((cause == 0x01) or (cause == 0x1C) or (cause == 0x100)) {
        report_it = false;
    }
    if (report_it) {
        int pos;
        while ((pos=reason.find(" ")) != string::npos) {
            reason.replace(pos,1,"_");
        }
        string event = "NotifyUser";
        event += " type:remote_start_aborted";
        event += " reason:"+reason;
        eventlog.append(event);
        if (canConnect()) {
            cellinfo.events_to_send = true;
        }
    }
}

void reportAlarm() {
    string reason = "Unknown";
    int cause = car_state.alarm_cause;
    if (cause == 0xFF) {
        reason = "Not Programmed";
    }
    else if (cause < (sizeof(VTA_causes)/sizeof(string))) {
         reason = VTA_causes[cause];
    }
    int pos;
    while ((pos=reason.find(" ")) != string::npos) {
        reason.replace(pos,1,"_");
    }
    string event = "NotifyAlarm";
    event += " Alarm:" + toString(car_state.alarm_report_state);
    event += " reason:" + reason;
    event += eventGpsPosition();
    eventlog.append(event);
    if (canConnect()) {
        cellinfo.events_to_send = true;
        logError("Need to report NotifyAlarm");
    }
    car_state.alarm_cause = -1;
}

void getVin() {
    Byte buffer[64];
    int len;
    string vin = "";
    string old_vin = "";
    if (fileExists(VinFile)) {
        old_vin = getStringFromFile(VinFile);
    }
    len = getVicResponse(0x26, 0, buffer);
    if (len > 3) {
        int vinlen = buffer[2];
        if (len == (vinlen+3)) {
            vin.assign((char *)(&buffer[3]), vinlen);
            if (vin == "ABCDEF1234GHIJK") {
                string hostname = getStringFromFile(ProcHostname);
                vin = "VIN" + hostname;
            }
            else if (vin == "AUT0NETDUMMY00000") {
                vin = "";
            }
            if (old_vin != vin) {
                updateFile(VinFile, vin);
                if (fileExists(OldVinFile)) {
                    string prev_vin = getStringFromFile(OldVinFile);
                    if (vin != prev_vin) {
                        unlink(CapabilitiesFile);
                        car_state.capabilities = 0;
                        car_state.capabilities_str = "none";
                    }
                    unlink(OldVinFile);
                }
            }
        }
    }
    updateFile(CurrentVinFile, vin);
    if (vin == "") {
        vin = old_vin;
    }
    car_state.vin = vin;
}

void checkCan() {
    bool pass = false;
    Byte buffer[64];
    int len;
    if (car_state.no_can) {
        pass = true;
    }
    else {
        if (vic.protocol_version > 0x0229) {
            if (car_state.can_status_bits == 3) {
                pass = true;
            }
        }
        else {
            // Check VIN
            len = getVicResponse(0x26, 0, buffer);
            if (len > 3) {
                int vinlen = buffer[2];
                if (len == (vinlen+3)) {
                    string vin;
                    vin.assign((char *)(&buffer[3]), vinlen);
                    if (vin == "ABCDEF1234GHIJK") {
                    }
                    else if (vin == "AUT0NETDUMMY00000") {
                        pass = true;
                    }
                    else {
                        pass = true;
                    }
                }
            }
            // Check fuel level
            int fuel = getFuelLevel();
            if (fuel != -1) {
                pass = true;
            }
            // Check oil life
            len = getVicResponse(0x24, 0, buffer);
            if (len == 3) {
                if (buffer[2] != 0xFF) {
                    pass = true;
                }
            }
            // Check odometer
            long int odom_x10 = getOdometerX10();
            car_state.stopped_odometer = odom_x10;
            if (odom_x10 != -1) {
                pass = true;
            }
            // Check tire pressure
            len = getVicResponse(0x23, 0, buffer);
            if (len == 9) {
                if ( (buffer[4] != 0xFF) and
                     (buffer[5] != 0xFF) and
                     (buffer[6] != 0xFF) and
                     (buffer[7] != 0xFF) ) {
                    pass = true;
                }
            }
        }
    }
    car_state.can_failed = !pass;
    string passfail = "Failed";
    if (pass) {
        passfail = "Passed";
    }
    updateFile(CanStatusFile, passfail);
    logError("CAN status: "+passfail);
}

void getVicRtc(struct tm *tm) {
    openVic();
    Byte buffer[64];
    tm->tm_year = 2000;
    tm->tm_mon = 1;
    tm->tm_mday = 1;
    tm->tm_hour = 0;
    tm->tm_min = 0;
    tm->tm_sec = 0;
    vic.getRtc(app_id, tm);
}

void setVicRtc() {
    struct tm tm;
    getVicRtc(&tm);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    time_t vic_time = timegm(&tm);
    if (abs(vic_time - time(NULL)) > 2) {
        logError("Setting VIC RTC");
        vic.setRtc(app_id);
        sleep(1);
    }
    timeinfo.rtc_timer = Uptime + 3600;
}

void setVicWatchdog() {
    Byte buffer[16];
    int watchdog = VicWatchdogTime;
    string watchdog_s = features.getFeature(VicWatchdog);
    if (watchdog_s != "") {
        watchdog = stringToInt(watchdog_s);
    }
    buffer[0] = watchdog;
    getVicResponse(0x07, 1, buffer);
}

void setVicQuickScanTimes() {
    Byte buffer[16];
    buffer[0] = VicQuickScanTime >> 8;
    buffer[1] = VicQuickScanTime & 0xFF;
    buffer[2] = VicQuickScanIgnDelay >> 8;
    buffer[3] = VicQuickScanIgnDelay & 0xFF;
    buffer[4] = VicQuickScanToolDisconnectTime >> 8;
    buffer[5] = VicQuickScanToolDisconnectTime & 0xFF;
    buffer[6] = VICQuickScanConfigChangeTime >> 8;
    buffer[7] = VICQuickScanConfigChangeTime & 0xFF;
    getVicResponse(0x0C, 8, buffer);
}

void getVicStatus(bool getlast) {
    Byte buffer[64];
    int type = 0x20;
    if (getlast) {
        type = 0x22;
    }
    int len = getVicResponse(type, 0, buffer);
    if (len > 0) {
        handleVicStatus(len, buffer);
    }
    else {
        updateFile(VicStatusFile, "Failed");
    }
}

void check_for_faults() {
    string faults_str = getFaults();
    if (faults_str != "") {
        sendFaultsEvent(faults_str);
    }
    car_state.check_fault_time = 0;
}

void sendFaultsEvent(string faults_str) {
    string event = "CarState";
    if (car_state.vin != "") {
        event += " VIN:" + car_state.vin;
    }
    if (car_state.check_engine & 0x02) {
        string chkeng = "0";
        if (car_state.check_engine & 0x01) {
           chkeng = "1";
        }
        event += " ChkEng:" + chkeng;
    }
    event += " Faults:" + faults_str;
    eventlog.append(event);
    logError("Sending: fault event: "+event);
    if (canConnect()) {
        cellinfo.events_to_send = true;
        logError("Need to report vehicle faults");
    }
}

string getFaults() {
    Byte buffer[128];
    string faults_str = "";
    int idx = 0;
    bool got_faults = false;
    logError("Checking for fault codes");
    while (true) {
        buffer[0] = 0x00;
        buffer[1] = idx;
        int len = getVicResponse(0x29, 2, buffer);
        Byte *p = buffer + 4;
        if ( (len < 5) or (*p == 0xFF) ) {
//        int len = getVicResponse(0x29, 1, buffer);
//        Byte *p = buffer + 3;
//        if ( (len < 4) or (*p == 0xFF) ) {
            got_faults = false;
            break;
        }
        got_faults = true;
        int num_faults = *(p++);
        if (num_faults == 0) {
            break;
        }
        for (int i=0; i < num_faults; i++) {
            if (faults_str != "") {
                faults_str += ",";
            }
            unsigned long fault = 0;
            for (int j=0; j < 3; j++) {
                fault = (fault<<8) | *p;
                len--;
                p++;
            }
            int status = 00;
//            if (car_state.vic_version > "000.027.000") {
            if (vic.protocol_version >= 0x0217 ) {
                status = *p;
                len--;
                p++;
            }
            int l = 3;
            if ((status&0x0F) == 0x00) {
                l = 2;
                fault >>= 8;
            }
            if (l == 2) {
                faults_str += string_printf("%04X", fault);
            }
            else {
                faults_str += string_printf("%06X", fault);
            }
//            if (car_state.vic_version > "000.027.000") {
            if (vic.protocol_version >= 0x0217) {
                faults_str += string_printf(".%02X", status);
            }
        }
        if (num_faults < 10) {
            break;
        }
        idx += 10;
    }
    if (got_faults) {
        if (faults_str == "") {
            faults_str = "none";
        }
    }
    return faults_str;
}

void getCellProfile() {
    string profile = getCellResponse("getprofile");
    if (profile != "") {
        logError("profile:"+profile);
        cellinfo.mdn = getParm(profile, "mdn:", ",");
        cellinfo.min = getParm(profile, "min:", ",");
        cellinfo.esn = getParm(profile, "esn:", ",");
        cellinfo.imei = getParm(profile, "imei:", ",");
        cellinfo.imsi = getParm(profile, "imsi:", ",");
        cellinfo.iccid = getParm(profile, "iccid:", ",");
        cellinfo.prl = getParm(profile, "prl:", ",");
        cellinfo.sim = getParm(profile, "sim:",",");
        string provider = getParm(profile, "provider:", ",");
        int pos = 0;
        while ((pos=provider.find(" ")) != string::npos) {
            provider.replace(pos,1,"_");
        }
        cellinfo.provider = provider;
        cellinfo.mcn = getParm(profile, "mcn:", ",");
        cellinfo.got_profile = true;
        if (cellinfo.mdn != "") {
            if (cellinfo.mdn.substr(0,3) == "000") {
                cellinfo.mdn = "";
            }
            else {
                cellinfo.activated = true;
            }
        }
        if (cellinfo.min != "") {
            if (cellinfo.min.substr(0,3) == "000") {
                cellinfo.min = "";
            }
        }
    }
    else {
        cellinfo.got_profile = false;
    }
    string outstr = "";
    if (cellinfo.esn != "") {
        outstr += "ESN " + cellinfo.esn + "\n";
    }
    if (cellinfo.mdn != "") {
        outstr += "MDN " + cellinfo.mdn + "\n";
    }
    if (cellinfo.min != "") {
        outstr += "MIN " + cellinfo.min + "\n";
    }
    if (cellinfo.imei != "") {
        outstr += "IMEI " + cellinfo.imei + "\n";
    }
    if (cellinfo.imsi != "") {
        outstr += "IMSI " + cellinfo.imsi + "\n";
    }
    if (cellinfo.iccid != "") {
        outstr += "ICCID " + cellinfo.iccid + "\n";
    }
    if (cellinfo.prl != "") {
        outstr += "PRL " + cellinfo.prl + "\n";
    }
    writeToFile(TempCellInfoFile, outstr);
}

bool activateCell() {
    logError("Attempting to activate cell");
    monitor_flags |= IN_ACTIVATEWAIT;
    bool success = false;
    cellinfo.activation_in_progress = true;
    system("cell_shell activate >/tmp/activate.out &");
//    while (true) {
//        sleep(1);
//        getStateInfo();
//        string pid = backtick("pgrep cell_shell");
//        if (pid == "") {
//            break;
//        }
//    }
//    string resp = readFile("/tmp/activate.out");
//    if (resp.find("OK") != string::npos) {
//        success = true;
//        cellinfo.activation_required = false;
//    }
//    cellinfo.activation_in_progress = false;
    monitor_flags &= ~IN_ACTIVATEWAIT;
    return success;
}

string getCellResponse(const char *cmnd) {
    return getCellResponse(cmnd, 5);
}

string getCellResponse(const char *cmnd, int timeout) {
    string resp = "";
    if (fileExists(NoCellCommandsFile)) {
//        if (cellinfo.socket >= 0) {
//            close(cellinfo.socket);
//            cellinfo.socket = -1;
//        }
        return resp;
    }
    if (cellinfo.talker == "" or cellinfo.no_config) {
        return resp;
    }
    monitor_flags |= IN_CELLWAIT;
    string cmndstr = cmnd;
    char buff[256];
    if (cellinfo.socket < 0) {
        if (fileExists(CellTalkerPath)) {
            logError("Attempting to connect to cell_talker");
            int sock = -1;
            if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
                logError("Could not open cell talker socket");
            }
            else {
                struct sockaddr_un a;
                memset(&a, 0, sizeof(a));
                a.sun_family = AF_UNIX;
                strcpy(a.sun_path, CellTalkerPath);
                alarm(5);
                monitor_flags |= IN_CELLCONNECT;
                if (connect(sock, (struct sockaddr *)&a, sizeof(a)) < 0) {
                    logError("Could not connect to cell talker socket");
                    close(sock);
                    sock = -1;
                }
                else {
                    logError("Connected to cell_talker");
                    cellinfo.fail_time = 0;
                }
                monitor_flags &= ~IN_CELLCONNECT;
                alarm(0);
                if (alrm_sig) {
                    alrm_sig = false;
                    string talker = cellinfo.talker;
                    if (talker != "") {
                        eventlog.append("CellTalker connect blocked...killing celltalker");
                        logError("CellTalker connect blocked...killing "+talker);
                        killCellTalker();
                        return resp;
                    }
                }
            }
            cellinfo.socket = sock;
        }
        else {
            logError("Cell talker socket does not exist");
        }
        if ((cellinfo.socket < 0) and !fileExists(CellModemDevice)) {
            if (cellinfo.fail_time != 0) {
                if ((Uptime-cellinfo.fail_time) > 10) {
                    logError("Cell module failed...resetting");
                    eventlog.append("CellModule failed...reseting");
                    cellinfo.fail_time = 0;
                    resetCell();
                }
            }
        }
    }
    if (cellinfo.socket >= 0) {
        alarm(2);
        monitor_flags |= IN_CELLSEND;
        int sent = send(cellinfo.socket, cmndstr.c_str(), cmndstr.size(), 0);
        alarm(0);
        monitor_flags &= ~IN_CELLSEND;
        if (alrm_sig) {
            alrm_sig = false;
            string talker = cellinfo.talker;
            if (talker != "") {
                eventlog.append("CellTalker send blocked...killing celltalker");
                logError("CellTalker socket blocked...killing "+talker);
                killCellTalker();
                return resp;
            }
        }
        if (sent == 0) {
            logError("Cell socket closed");
            close(cellinfo.socket);
            cellinfo.socket = -1;
            cellinfo.fail_time = Uptime;
        }
        else {
            bool done = false;
            while (!done) {
                alarm(timeout);
                monitor_flags |= IN_CELLRECV;
                int nread = recv(cellinfo.socket, buff, 255, 0);
                alarm(0);
                monitor_flags &= ~IN_CELLRECV;
                if (nread < 0) {
                    if (errno == EINTR) {
                        if (alrm_sig) {
                            alrm_sig = false;
                            string talker = cellinfo.talker;
                            if (talker != "") {
                                eventlog.append("CellTalker socket blocked...killing celltalker");
                                logError("CellTalker recv blocked...killing "+talker);
                                killCellTalker();
                                break;
                            }
                        }
                    }
                }
                else if (nread == 0) {
                    logError("Nothing read from cell_socket");
                    close(cellinfo.socket);
                    cellinfo.socket = -1;
                    cellinfo.fail_time = Uptime;
                    resp = "";
                    done = true;
                }
                else {
                    buff[nread] = '\0';
                    if (buff[nread-1] == '\n') {
                        nread--;
                        buff[nread] = '\0';
                        done = true;
                    }
                    resp += buff;
                }
            }
        }
    }
    if (resp == "ERROR") {
        logError("Got error from cell talker");
        cellinfo.error_count++;
        if (cellinfo.error_count > 5) {
            logError("Got error from cell talker... closing socket");
            eventlog.append("CellModule lockup...resetting");
            close(cellinfo.socket);
            cellinfo.socket = -1;
            cellinfo.fail_time = Uptime;
            resetCell();
        }
        resp = "";
    }
    else {
        cellinfo.error_count = 0;
    }
    monitor_flags &= ~IN_CELLWAIT;
    return resp;
}

void killCellTalker() {
    alarm(2);
    close(cellinfo.socket);
    alarm(0);
    cellinfo.socket = -1;
    str_system("pkill "+cellinfo.talker);
    sleep(1);
}

bool turnOnCell() {
    bool retval = true;
    if (!fileExists(CellTalkerPath)) {
        cellPulseOnKey();
        sleep(3);
        if (!fileExists(CellTalkerPath)) {
            cellPulseOnKey();
            sleep(3);
            if (!fileExists(CellTalkerPath)) {
                retval = false;
            }
        }
    }
    return retval;
}

void cellPulseOnKey() {
    Byte buffer[64];
    buffer[0] = 2;
    int len = getVicResponse(0x05, 1, buffer);
}

void resetCell() {
    cellPulseOnKey();
    sleep(3);
    cellPulseOnKey();
}

void resetCellHdw() {
    Byte buffer[64];
    buffer[0] = 1;
    int len = getVicResponse(0x05, 1, buffer);
    sleep(1);
    cellPulseOnKey();
}

void checkFeaturesChange() {
    if (features.checkFeatures()) {
        if ( features.featureChanged(PosRptInt) or
             features.featureChanged(FindMyCarFeature) ) {
            getPositionReportInterval();
        }
        if ( features.featureChanged(PwrCntrl) or
             features.featureChanged(PwrSched) ) {
            getPowerControlSettings(false);
        }
        if ( features.featureChanged(AntennaSettings) ) {
            getAntennaSettings();
        }
        if ( features.featureChanged(RateLimits)) {
            system("ratelimit");
        }
        if ( features.featureChanged(VicWatchdog)) {
            setVicWatchdog();
        }
        if ( features.featureChanged(WifiFeature) or
             features.featureChanged(RegFeature) ) {
            getWifiEnable();
        }
        if ( features.featureChanged(DisableLocationFeature) ) {
            getDisableLocation();
        }
        if ( features.featureChanged(KeepOnFeature) ) {
            getKeepOnFeature();
        }
        if ( features.featureChanged(ValetFeature) ) {
            getValetFeature();
        }
        if ( features.featureChanged(HomeLanFeature) ) {
            getHomeLanFeature();
        }
        if ( features.featureChanged(FleetFeature) ) {
            getFleetFeature();
        }
        if ( features.featureChanged(SeatbeltsFeature) ) {
            getSeatbeltsFeature();
        }
        if ( features.featureChanged(WifiMaxOnTimeFeature) ) {
            getWifiMaxOnTime();
        }
        if ( features.featureChanged(SmsBlockingFeature) or
             features.featureChanged(NoWlanFeature) ) {
            getSmsBlockingFeature();
        }
    }
}

void getWifiEnable() {
//    string enable_str = features.getFeature(WifiFeature);
    bool was_enabled = wifi_enabled;
    bool was_registered = registered;
    wifi_enabled = features.isEnabled(WifiFeature);
    registered = false;
    if (features.getFeature(RegFeature) == "yes") {
        registered = true;
    }
//    wifi_enabled = false;
//    wifi_enabled_until = 0;
//    if (enable_str.find("enab") != string::npos) {
//        wifi_enabled = true;
//        size_t pos = enable_str.find(":");
//        if (pos != string::npos) {
//            wifi_enabled_until = stringToULong(enable_str.substr(pos+1));
//        }
//    }
    if ( (wifi_enabled != was_enabled) or (registered != was_registered) ) {
        system("autonet_router");
    }
}

void getWifiMaxOnTime() {
    wlan0info.max_on_time = 0;
    if (features.exists(WifiMaxOnTimeFeature)) {
        string maxstr = features.getData(WifiMaxOnTimeFeature);
        if (maxstr != "") {
            wlan0info.max_on_time = stringToInt(maxstr) * 60;
        }
    }
}

void getPositionReportInterval() {
    string intval_str = features.getFeature(PosRptInt);
    bool enabled = features.isEnabled(FindMyCarFeature);
//    string enable_str = features.getFeature(FindMyCarFeature);
//    bool enabled = false;
//    gpsinfo.report_enabled_until = 0;
//    if (enable_str.find("enab") != string::npos) {
//        enabled = true;
//        size_t pos = enable_str.find(":");
//        if (pos != string::npos) {
//            gpsinfo.report_enabled_until = stringToULong(enable_str.substr(pos+1));
//        }
//    }
    if (enabled) {
        if (intval_str != "") {
            gpsinfo.report_interval = stringToInt(intval_str);
        }
        else {
            gpsinfo.report_interval = 300;
        }
    }
    else {
        gpsinfo.report_interval = 0;
    }
}

void getPowerControlSettings(bool initial) {
    string settings = features.getFeature(PwrCntrl);
    if (settings != "") {
        car_state.setValues(settings);
    }
    else {
        car_state.defaultValues();
    }
    string sched_str = features.getFeature(PwrSched);
    if (sched_str == "") {
        sched_str = "0/0";
        string event = "NotifySupport No PwrSched";
        eventlog.append(event);
    }
    car_state.setSchedule(sched_str);
    car_state.cell_hold_time = CellHoldTime;
    string hold_str = features.getFeature(CellHoldFeature);
    if (hold_str != "") {
        car_state.cell_hold_time = stringToInt(hold_str);
    }
    int len = car_state.getPowerScheduleLength();
    Byte msg[64];
    int state0_time = car_state.getIgnitionOffTime(0);
    msg[0] = car_state.cell_hold_time;
    msg[1] = (Byte)(state0_time >> 8);
    msg[2] = (Byte)(state0_time & 0xFF);
    msg[3] = (Byte)(len - 1);
    int msg_len = 4;
    Byte *p = &msg[4];
    for (int i=1; i < len; i++) {
        int state_time = car_state.getIgnitionOffTime(i);
        *p++ = (Byte)(state_time >> 8);
        *p++ = (Byte)(state_time & 0xFF);
        int cc_time = car_state.getCycleCellTime(i);
        *p++ = (Byte)(cc_time);
        msg_len += 3;
    }
    getVicResponse(0x03, msg_len, msg);
    msg_len = 2;
    msg[0] = (int)((car_state.lowbatval-car_state.batadjust)*10+.5);
    msg[1] = (int)((car_state.critbatval-car_state.batadjust)*10+.5);
    if (vic.protocol_version >= 0x0222) {
        msg[2] = car_state.lowbat_polltime;
        msg_len = 3;
    }
    getVicResponse(0x0B, msg_len, msg);
}

void getCarKeySettings() {
}

void getAntennaSettings() {
    Byte buffer[16];
    string settings = features.getFeature(AntennaSettings);
    int val = 0;
    if (settings != "") {
        if (settings == "internal") {
            val = 0;
        }
        else if (settings == "external") {
            val = 3;
        }
        else if (settings == "gps_external") {
            val = 2;
        }
        else if (settings == "cell_external") {
            val = 1;
        }
    }
    else {
        if (car_state.vin != "") {
            if (vinToExtAnt(car_state.vin)) {
                val = 3;
            }
        }
    }
    int gps_ext = (val >> 1) & 0x01;
    int last_gps_ext = -1;
    if (fileExists(LastGpsAntStateFile)) {
        last_gps_ext = 0;
        if (getStringFromFile(LastGpsAntStateFile) == "1") {
            last_gps_ext = 1;
        }
    }
    buffer[0] = val;
    getVicResponse(0x09, 1, buffer);
    if (last_gps_ext != gps_ext) {
        sleep(1);
        clearGpsReceiver();
        updateFile(LastGpsAntStateFile, toString(gps_ext));
    }
}

void getKeepOnFeature() {
    car_state.keep_on = false;
    if (features.getFeature(KeepOnFeature) == "true") {
        car_state.keep_on = true;
    }
}

void getValetFeature() {
    car_state.valet_mode = false;
    car_state.trunk_opened_sent = false;
    if (features.isEnabled(ValetFeature)) {
        car_state.valet_mode = true;
    }
}

void getNoCanFeature() {
    car_state.no_can = false;
    if (features.isEnabled(NoCan)) {
        car_state.no_can = true;
    }
}

void getSeatbeltsFeature() {
    car_state.seatbelts_enabled = false;
    if (features.isEnabled(SeatbeltsFeature)) {
        car_state.seatbelts_enabled = true;
        string val = features.getData(SeatbeltsFeature);
        if (val == "") {
            car_state.seatbelts_delay = 30;
        }
        else {
            car_state.seatbelts_delay = stringToInt(val);
        }
    }
}

void getHomeLanFeature() {
    wlan1info.homelan_enabled = false;
    if (features.isEnabled(HomeLanFeature)) {
        wlan1info.homelan_enabled = true;
    }
}

void getFleetFeature() {
    gpsinfo.fleet_enabled = false;
    if (features.isEnabled(FleetFeature)) {
        gpsinfo.fleet_enabled = true;
    }
}

void getSmsBlockingFeature() {
     bool last_wlan = wlan0info.enabled;
     bool last_sms = car_state.sms_blocking;
     car_state.sms_blocking = false;
     if (features.isEnabled(SmsBlockingFeature)) {
         car_state.sms_blocking = true;
     }
     wlan0info.enabled = true;
     if (features.exists(NoWlanFeature)) {
         wlan0info.enabled = false;
     }
     if (wlan0info.enabled != last_wlan) {
         if (wlan0info.enabled) {
             ifupWlan0();
         }
         else {
             ifdownWlan0();
         }
     }
     if (car_state.sms_blocking != last_sms) {
         if (car_state.sms_blocking) {
             system("hciconfig hci0 piscan >/dev/null 2>&1");
         }
         else {
             system("hciconfig hci0 noscan >/dev/null 2>&1");
         }
     }
}

bool vinToExtAnt(string vin) {
    bool retval = false;
    if (fileExists(VinToExtAntFile)) {
        Strings vin_patterns = readFileArray(VinToExtAntFile);
        for (int i=0; i < vin_patterns.size(); i++) {
            regex_t rgx;
            regcomp(&rgx, vin_patterns[i].c_str(), 0);
            if (regexec(&rgx, vin.c_str(), 0, NULL, 0) == 0) {
                retval = true;
                break;
            }
            regfree(&rgx);
        }
    }
    return retval;
}

void clearGpsReceiver() {
    if (cellinfo.type == "ppp") {
        logError("Clearing GPS receiver");
        string resp = getCellResponse("resetgps");
    }
}

//void checkGpsCleared() {
//    string resp = getCellResponse("AT_OGPSCLEAR=\"ALL\",1");
//    if (resp == "OK") {
//        logError("GPS receiver cleared");
//        (void)getCellResponse("AT_OGPS=2");
//        gpsinfo.clearing = false;
//    }
//}

void getDisableLocation() {
    string disableloc = features.getFeature(DisableLocationFeature);
    car_state.location_disabled = false;
    if (disableloc != "") {
        car_state.location_disabled = true;
    }
}

bool checkLink() {
    bool ok = false;
    int retval = system("checklink");
    if (retval == 0) {
        ok = true;
    }
    return ok;
}

double getDoubleUptime() {
    string upline = getStringFromFile(ProcUptime);
    double upval = 0;
    if (upline != "") {
        int pos = upline.find(" ");
        upline = upline.substr(0, pos);
        upval = stringToDouble(upline);
    }
    return upval;
}

string regrepFile(const char *file, const char *search) {
    string outline = "";
    string cmnd = "grep \"";
    cmnd += search;
    cmnd += "\" ";
    cmnd += file;
    outline = backtick(cmnd);
    return outline;
}

void writeToFile(const char *file, string str) {
     ofstream fout(file);
     if (fout.is_open()) {
         fout << str;
         fout.close();
     }
}

void dial() {
    string pppd_pid = backtick("pgrep pppd");
    if (pppd_pid != "") {
        logError("Killing pppd");
        system("pkill -9 pppd");
        sleep(1);
    }
    if (fileExists(CellmodemLockFile)) {
        logError("Removing cellmodem lockfile");
        unlink(CellmodemLockFile);
    }
    logError("Dialing");
    cellinfo.connected = false;
    waninfo.pppd_exited = false;
    waninfo.last_rx_bytes = 0;
    waninfo.last_tx_bytes = 0;
    pid_t pid = fork();
    if (pid) {
        waninfo.pid = pid;
    }
    else {
        catFile(PppLogFile, PppAllLogFile);
        string datetime = getDateTime();
        writeStringToFile(PppLogFile, datetime+" pppd started");
        if (cellinfo.gsm) {
            execl("/usr/sbin/pppd", "/usr/sbin/pppd", "call", "GSM", NULL);
        }
        else if (cellinfo.lteppp) {
            execl("/usr/sbin/pppd", "/usr/sbin/pppd", "call", "LTE", NULL);
        }
        else {
            execl("/usr/sbin/pppd", "/usr/sbin/pppd", "call", "EVDO", NULL);
        }
        exit(1);
    }
}

void catFile(const char *src, const char *dst) {
    str_system(string("cat ") + src + " >>" + dst + " 2>/dev/null");
}

/*
void startUnitcomm() {
    pid_t pid = fork();
    if (pid) {
        comminfo.state = COMM_RUNNING;
        comminfo.pid = pid;
    }
    else {
        int type = O_CREAT | O_WRONLY | O_TRUNC;
        int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        int fd = open("/var/log/comm.log", type, mode);
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            execl("/usr/local/bin/unitcomm", "/usr/local/bin/unitcomm", NULL);
        }
        exit(1);
    }
}
*/

void stopUnitcomm() {
    if (comminfo.pid != 0) {
//        kill(comminfo.pid, SIGTERM);
        system("pkill unitcomm");
    }
    comminfo.state = COMM_STOPPED;
}

void sendStopToUnitcomm() {
//    kill(comminfo.pid, SIGHUP);
    system("pkill -HUP unitcomm");
    comminfo.state = COMM_STOPPING;
}

void restartUnitcomm() {
//    kill(comminfo.pid, SIGUSR1);
    system("pkill -USR1 unitcomm");
}

void reboot(string cause) {
    logError(cause+"...rebooting system");
//    if (comminfo.pid != 0) {
//       kill(comminfo.pid, SIGTERM);
//    }
    if (waninfo.pid != 0) {
        kill(waninfo.pid, SIGTERM);
    }
    completeUpgrade();
    string event = cause + "...rebooting";
    event += eventDeltaTime(cellinfo.state, 1);
    event += eventUsage(false);
    event += eventFailCount();
    event += eventUptime();
    eventlog.append(event);
    updateFile(TempStateFile, toString(Uptime));
    writeMonthlyUsage();
    sleep(2);
    system("pkill -USR1 respawner");
    sleep(1);
    system("pkill event_logger; pkill gps_mon");
    system("cp /tmp/loggedin_macs /autonet/loggedin_macs");
//    backupAutonet();
    if (fileExists(PowercontrollerDevice)) {
        system("powercontroller shutdown 5 10");
    }
    else {
        system("real_reboot");
    }
    exit(0);
}

void powerOff(string reason, int type, int timer) {
//    if (comminfo.pid != 0) {
//       kill(comminfo.pid, SIGTERM);
//    }
    if (waninfo.pid != 0) {
        kill(waninfo.pid, SIGTERM);
    }
    updateUpgradeInfo();
    string power_off_state = reason + ":" +
                             toString(car_state.powered_off_time) + ":" +
                             toString(car_state.ignition_off_time);
    writeStringToFile(PowerOffStateFile, power_off_state);
    writeMonthlyUsage();
    completeUpgrade();
    UpgradeInfo upgrade_info;
    upgrade_info.read();
    if (upgrade_info.check()) {
        upgrade_info.remove();
        dupFs();
    }
    setVicRtc();
    disableWatchdog();
    system("pkill -USR1 respawner");
    sleep(1);
    system("pkill event_logger; pkill gps_mon");
    getCellResponse("AT_OGPS=0");
    logError("Sending shutdown command:"+toString(type)+"/"+toString(timer));
    unmountAutonet();
    sleep(2);
    Byte buffer[16];
    buffer[0] = 10;
    buffer[1] = type;
    buffer[2] = timer;
    getVicResponse(0x04, 3, buffer);
    // Kill vic_talker so if we don't shutdown vic will kick us
    system("pkill vic_talker");
    unlink(TempStateFile);
    exit(0);
}

void updateUpgradeInfo() {
    UpgradeInfo upgrade_info;
    upgrade_info.read();
    string current_version = getStringFromFile(IssueFile);
    if (current_version == upgrade_info.sw_version) {
        // Increment boot_cnt if we successfully booted
        upgrade_info.boot_cnt++;
        // Increment connect_cnt if we made a connection this time
        if (!cellinfo.first_connection) {
            upgrade_info.connect_cnt++;
        }
        // Increment gps_cnt if we made got GPS fix this time
        if (gpsinfo.position != "") {
            upgrade_info.gps_cnt++;
        }
        upgrade_info.write();
    }
}

void completeUpgrade() {
    if (fileExists(UpgradeStateFile)) {
        string upgrading_state = getStringFromFile(UpgradeStateFile);
        logError("Upgrade state1: "+upgrading_state);
        if ( (upgrading_state == "pending") or
             (upgrading_state == "untarring") or
             (upgrading_state == "upgrading") ) {
            string upgrade_pid = backtick("pgrep upgrade");
            if (upgrade_pid == "") {
                string upgrade_cmd = "upgrade";
                if (upgrading_state == "pending") {
                    upgrade_cmd += " -p";
                    logError("Performing upgrade");
                }
                else {
                    upgrade_cmd += " -c";
                    logError("Continuing upgrade");
                }
                if (single_os) {
                    upgrade_cmd += " -i";
                }
                upgrade_cmd += " &";
                str_system(upgrade_cmd);
            }
            waitForUpgradeToFinish();
        }
        upgrading_state = getStringFromFile(UpgradeStateFile);
        logError("Upgrade state2: "+upgrading_state);
        if (upgrading_state == "upgraded") {
            doUpgrade();
        }
    }
}

void waitForUpgradeToFinish() {
    monitor_flags |= IN_UPGRADEWAIT;
    logError("Waiting for upgrade to complete");
    string upgrading_state = "";
    while (true) {
        upgrading_state = "";
        if (fileExists(UpgradeStateFile)) {
            upgrading_state = getStringFromFile(UpgradeStateFile);
        }
//        if ( (upgrading_state != "untarring") and (upgrading_state != "upgrading") ) {
        if (upgrading_state == "upgraded") {
            logError("Upgrade completed");
            break;
        }
        string upgrade_pid = backtick("pgrep upgrade");
        if (upgrade_pid == "") {
            logError("No upgrade process");
            break;
        }
        sleep(1);
        getStateInfo();
    }
    monitor_flags &= ~IN_UPGRADEWAIT;
}

void doUpgrade() {
    logError("Do post-upgrade process");
    string version = "";
    string boot_dir = "";
    if (single_os) {
        boot_dir = "/boot/";
        version = getStringFromFile(IssueFile);
    }
    else {
        string opposite = "2";
        if (kernelId == "2") {
            opposite = "1";
        }
        logError("Setting .kernel to "+opposite);
        if (str_system("fw_setenv .kernel "+opposite+" >/dev/null 2>&1") != 0) {
            str_system("fw_setenv .kernel "+opposite+" >/dev/null 2>&1");
        }
        boot_dir = "/mnt/boot/";
        system("flash_mount -o");
        version = getStringFromFile(string("/mnt")+IssueFile);
        if (fileExists("/mnt/boot/bootscript.img")) {
            if (system("cmp -s /boot/bootscript.img /mnt/boot/bootscript.img") != 0) {
                logError("Updating bootscript");
                system("flash_erase /dev/mtd0 0x60000 1");
                system("nandwrite -p -s 0x60000 /dev/mtd0 /mnt/boot/bootscript.img");
            }
        }
    }
    UpgradeInfo upgrade_info;
    upgrade_info.sw_version = version;
    upgrade_info.time_of_upgrade = time(NULL);
    upgrade_info.write();
    upgradeCellModule(boot_dir);
    string vic_vers = "VIC-"+vic.build+"-"+vic.version_str+".dat";
    if (!fileExists(boot_dir+vic_vers)) {
        string vic_base = "VIC-"+vic.build+"-";
        string new_vers = backtick("ls "+boot_dir+vic_base+"* 2>/dev/null");
        if (new_vers == "") {
            logError("No new VIC version");
            string event = "NotifyEngineering Missing VIC firmare "+vic_base;
            eventlog.append(event);
        }
        else {
            updateVic(new_vers);
        }
    }
    if (!single_os) {
        system("flash_mount -u");
    }
}

void updateVic(string new_vers) {
    logError("Updating VIC to "+new_vers);
    writeMonthlyUsage();
    disableWatchdog();
    vic.closeSocket();
    int exit_val = str_system("vic_loader "+new_vers);
    sleep(1);
    vic.openSocket();
    enableWatchdog();
    if (exit_val == 0) {
        sleep(2);
        system("pkill -USR1 respawner");
        sleep(1);
        system("pkill event_logger; pkill gps_mon");
        sleep(2);
        logError("Resetting VIC after loading VIC firmware");
        getCellResponse("AT_OGPS=0");
        unmountAutonet();
        sleep(2);
        // Resetting the VIC bootloader will reset the MX35
        Byte buffer[16];
        buffer[0] = app_id;
        buffer[1] = 0x06;
        buffer[2] = 0x64;
        vic.sendMesgWithoutSig(3, buffer);
        int len = vic.getResp(buffer, 5);
        exit(0);
    }
}

void upgradeCellModule(string bootdir) {
    int pos;
    string oid = getCellResponse("AT_OID");
    string fwv = getParm(oid, "FWV: ", ";");
    Strings vals = split(fwv, " ");
    fwv = vals[0];
    string build_type = vals[1];
//    string oimgact = getCellResponse("AT_OIMGACT?");
//    string cust_build = getParm(oimgact, "BuildID: ECN", ";");
    string ocid = getCellResponse("AT_OCID");
    string cust_build = getParm(ocid, "_OCID: ", ";");
    pos = cust_build.find_first_of("0123456789");
    if (pos != string::npos) {
        cust_build = cust_build.substr(pos);
    }
    pos = cust_build.find_first_not_of("0123456789");
    if (pos != string::npos) {
        cust_build = cust_build.substr(0, pos);
    }
    cout << "Current FW:" << fwv << endl;
    cout << "Current Build:" << build_type << endl;
    cout << "Current CUST:" << cust_build << endl;
    string loader_file = bootdir+"loader"+build_type+".cfg";
    cout << "Loader file:" << loader_file << endl;
    if (fileExists(loader_file)) {
        Assoc config = readAssoc(loader_file, "=");
        string new_fwv = config["fw_zip"];
        pos = new_fwv.find_first_of("0123456789");
        if (pos != string::npos) {
            new_fwv = new_fwv.substr(pos);
        }
        pos = new_fwv.find_first_not_of("0123456789.");
        if (pos != string::npos) {
            new_fwv = new_fwv.substr(0,pos);
        }
        cout << "New FW:" << new_fwv << endl;
        string new_cust_build = config["cust_zip"];
        pos = new_cust_build.find_first_of("0123456789");
        if (pos != string::npos) {
            new_cust_build = new_cust_build.substr(pos);
        }
        pos = new_cust_build.find_first_not_of("0123456789");
        if (pos != string::npos) {
            new_cust_build = new_cust_build.substr(0,pos);
        }
        cout << "New CUST:" << new_cust_build << endl;
        string new_build_type = config["type"];
        cout << "New Build:" << new_build_type << endl;
        if (new_build_type == build_type) {
            if ( (new_cust_build != cust_build) or
                 (new_fwv != fwv) ) {
                logError("Upgrading cell firmware");
                cout << "Need to upgrade cell module" << endl;
                chdir("/tmp");
                str_system("ln -s "+bootdir+"FW FW");
                str_system("ln -s "+bootdir+"CUST CUST");
                str_system("ln -s "+loader_file+" loader.cfg");
                disableWatchdog();
                system("FWLoader");
                enableWatchdog();
            }
        }
    }
}

void dupFs() {
    if ((kernelId == "1") or (kernelId == "2")) {
        string opposite = "2";
        if (kernelId == "2") {
            opposite = "1";
        }
        string opposite_state = backtick("fw_printenv .state"+opposite);
        if (opposite_state == "") {
            opposite_state = backtick("fw_printenv .state"+opposite);
        }
//        logError("opposite_state:"+opposite_state);
        if (opposite_state.find("verified") != string::npos) {
            logError("Duplicating upgraded filesystem");
            system("dup_fs &");
            monitor_flags |= IN_DUPFSWAIT;
            while (true) {
                sleep(1);
                getStateInfo();
                string pid = backtick("ps ax | grep dup_fs | grep -v grep");
                if (pid == "") {
                    break;
                }
            }
            monitor_flags &= ~IN_DUPFSWAIT;
        }
    }
}

void unmountAutonet() {
    system("/etc/init.d/mount_autonet stop");
}

void backupAutonet() {
    system("backup_autonet");
//    string autonet = backtick("mount | grep '/autonet '");
//    string autonet_alt = backtick("mount | grep '/autonet.alt '");
//    cout << "mount autonet: " << autonet << endl;
//    cout << "mount autonet.alt: " << autonet_alt << endl;
//    if (autonet != "") {
//        sync();
//        buildToc("/autonet");
//        sync();
//        if (autonet_alt != "") {
//            system("rm -rf /autonet.alt/*");
//            system("cp -a /autonet/* /autonet.alt/");
//            sync();
//        }
//    }
}

void buildToc(string autonet) {
    str_system("verify_fs -f " + autonet + "/etc/verify_autonet.conf " + autonet + " >" + autonet + "/verify_autonet.chk");
}
    
void writeLastEvent(string reason) {
    string event = "Last event:";
    if (reason != "") {
        event += " " + reason;
    }
    event += eventDeltaTime(cellinfo.state, 1);
    event += eventUsage(false);
    event += eventFailCount();
    event += eventUptime();
    eventlog.append(event);
    updateFile(TempStateFile, toString(Uptime));
}

void on_child(int parm) {
    int status = 0;
    if (waninfo.pid != 0) {
        pid_t child = waitpid(waninfo.pid,&status,WNOHANG);
        if (child == waninfo.pid) {
            waninfo.pid = 0;
            waninfo.pppd_exited = true;
            logError("pppd exited");
        }
    }
//    if (comminfo.pid != 0) {
//        pid_t child = waitpid(comminfo.pid,&status,WNOHANG);
//        if (child == comminfo.pid) {
//            comminfo.pid = 0;
//            if (comminfo.state == COMM_STOPPING) {
//                comminfo.state = COMM_STOPPED;
//            }
//            else {
//                comminfo.state = COMM_FAILED;
//            }
//        }
//    }
    signal(SIGCHLD, on_child);
}

void on_alrm(int sig) {
    alrm_sig = true;
    signal(SIGALRM, on_alrm);
}

//void on_term(int sig) {
void on_term(int sig, siginfo_t *info, void *data) {
//    if (comminfo.pid != 0) {
//       kill(comminfo.pid, SIGTERM);
//    }
    killWan("Got term signal");
//    if (waninfo.pid != 0) {
//        kill(waninfo.pid, SIGTERM);
//    }
    system("pkill save_dns_entries");
    system("pkill update_default_sites");
    string reason = "";
    bool rebooting = false;
    if (fileExists(RebootReasonFile)) {
        rebooting = true;
        reason = getStringFromFile(RebootReasonFile);
        if (reason != "") {
            int pos;
            while ((pos=reason.find(" ")) != string::npos) {
                reason.replace(pos, 1, "_");
            }
        }
        else {
            reason = "No_reason_given";
        }
        reason = "rebooting:" + reason;
        system("cp /tmp/loggedin_macs /autonet/loggedin_macs");
    }
    else {
        string sigstr = toString(sig);
        switch (sig) {
          case SIGTERM:
             sigstr = "TERM";
             break;
          case SIGINT:
             sigstr = "INT";
             break;
          case SIGHUP:
             sigstr = "HUP";
             break;
          case SIGQUIT:
             sigstr = "QUIT";
             break;
        }
        string monflags_str = "";
        if (monitor_flags & IN_CONNECT) {
            monflags_str += " CONNECT";
        }
        if (monitor_flags & IN_UPLOOP) {
            monflags_str += " UPLOOP";
        }
        if (monitor_flags & IN_WAITLOOP) {
            monflags_str += " WAITLOOP";
        }
        if (monitor_flags & IN_CELLWAIT) {
            monflags_str += " CELLWAIT";
        }
        if (monitor_flags & IN_VICWAIT) {
            monflags_str += " VICWAIT";
        }
        if (monitor_flags & IN_UPDATEFILES) {
            monflags_str += " UPDATEFILES";
        }
        if (monitor_flags & IN_BACKOFF) {
            monflags_str += " BACKOFF";
        }
        if (monitor_flags & IN_MONWAIT) {
            monflags_str += " MONWAIT";
        }
        if (monitor_flags & IN_ACTIVATEWAIT) {
            monflags_str += " ACTIVATEWAIT";
        }
        if (monitor_flags & IN_UPGRADEWAIT) {
            monflags_str += " UPGRADEWAIT";
        }
        if (monitor_flags & IN_DUPFSWAIT) {
            monflags_str += " DUPFSWAIT";
        }
        if (monitor_flags & IN_CELLCONNECT) {
            monflags_str += " CELLCONNECT";
        }
        if (monitor_flags & IN_CELLSEND) {
            monflags_str += " CELLSEND";
        }
        if (monitor_flags & IN_CELLRECV) {
            monflags_str += " CELLRECV";
        }
        if (monitor_flags & IN_MONITOR_LOCK) {
            monflags_str += " MONITOR_LOCK";
        }
        updateFile(MonitorFlagsFile, monflags_str);
        reason = "ExitingOnSignal:" + sigstr + monflags_str;
    }
    if (rebooting) {
        logError("Killing process:reboot");
    }
    else if (fileExists(WhosKillingMonitorFile)) {
        string who = getStringFromFile(WhosKillingMonitorFile);
        logError("Killing process:"+who);
        unlink(WhosKillingMonitorFile);
    }
    else if (info == NULL) {
        logError("No process info");
    }
    else {
        int pid = info->si_pid;
        string pid_str = toString(pid);
        logError("Signalling PID:"+pid_str);
        string procdir = "/proc/"+pid_str;
        if (fileExists(procdir)) {
            string comm = getStringFromFile(procdir+"/comm");
            string cmdline = getStringFromFile(procdir+"/cmdline");
            string ppid = grepFile(procdir+"/status","PPid");
            logError("Signalling Command:"+comm);
            logError("Signalling CmdLine:"+cmdline);
            logError(ppid);
        }
        else {
            string ps = backtick("ps ax");
            logError("ps:"+ps);
            logError("Process dir does not exist");
        }
    }
    logError(reason);
    writeLastEvent(reason);
    writeMonthlyUsage();
//    system("pkill respawner");
//    sleep(1);
//    backupAutonet();
    disableWatchdog();
//    system("ledcontrol 2");
//    if (sig == SIGTERM) {
//        disableVicWatchdog();
//    }
    exit(0);
}

void on_usr1(int parm) {
    if (!needConnection()) {
        cellinfo.events_to_send = true;
    }
}

void enableWatchdog() {
    updateFile(LastTimeFile, "Started");
}

void disableWatchdog() {
    unlink(LastTimeFile);
}

string getPppCloseCause() {
    string cause = "";
    bool reload_mods = false;
    bool reset_cell = false;
    bool established = false;
    bool reboot = false;
    string ppplog = readFile(PppLogFile);
    if (ppplog.find("Serial connection established") != string::npos) {
        established = true;
    }
    if (ppplog.find("tcgetattr:") != string::npos) {
        cause = "Critical failure";
        reboot = true;
    }
    else if (ppplog.find("OK") == string::npos) {
        cause = "chat failed";
        reset_cell = true;
    }
    else if (ppplog.find("linkcheckd: link failure") != string::npos) {
        cause = "link failed (linkcheckd)";
    }
    else if (ppplog.find("linkcheckd: no link") != string::npos) {
        cause = "not established (linkcheckd)";
    }
    else if (ppplog.find("monitor: no link") != string::npos) {
        cause = "not established (monitor)";
    }
    else if (ppplog.find("monitor: link failure") != string::npos) {
        cause = "link failed (monitor)";
    }
    else if (ppplog.find("monitor: connection closed") != string::npos) {
        cause = "closed";
    }
    else if (ppplog.find("ecall:killed") != string::npos) {
        cause = "closed";
    }
    else if (ppplog.find("monitor: overtemp") != string::npos) {
        cause = "Overtemp";
    }
    else if (ppplog.find("Terminating on signal") != string::npos) {
        cause = "terminated on signal";
    }
    else if (ppplog.find("Hangup") != string::npos) {
        cause = "hungup";
        reload_mods = true;
    }
    else if (ppplog.find("cellmodem is locked") != string::npos) {
        cause = "cellmodem locked";
        reload_mods = true;
    }
    else if (ppplog.find("LCP terminated by peer") != string::npos) {
        cause = "terminated by carrier";
    }
    else if (ppplog.find("Modem hangup") != string::npos) {
        cause = "failed modem hangup";
    }
    else if (ppplog.find("NO CARRIER") != string::npos) {
        cause = "failed no carrier";
    }
    else if (ppplog.find("PAP authentication failed") != string::npos) {
        cause = "failed PPP authentication";
    }
    else {
        cause = "failed unknown reason";
        reload_mods = 1;
    }
    waninfo.reload_mods = reload_mods;
    waninfo.reset_cell = reset_cell;
    waninfo.reboot = reboot;
    return cause;
}
