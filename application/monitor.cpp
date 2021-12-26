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
 * 2014/05/22 rwp
 *   Corrections for LTE connect timing and bug fixes
 * 2014/05/28 rwp
 *   Change to only set ntptime every hour on reconnect
 * 2014/06/05 rwp
 *   AUT-144: Delay backupAutonet for 60 seconds after connect
 * 2014/06/20 rwp
 *   Added setting cell to low power before shutting down
 *   Changed turning off GPS to generic celltalker gpsoff
 *   Changed updateCellularModule to invoke different updaters
 * 2014/06/25 rwp
 *   Kanaan changes
 * 2014/08/01 rwp
 *   Save and restore last cell model, in case the module does not come up
 * 2014/08/12 rwp
 *   AK-3: Set piscan when setting the bluetooth name
 * 2014/08/21 rwp
 *   Add 15 second delay after 2nd on/off pulse in resetCell (Telit)
 *   Added cellinfo.error_flags to stop multiple reports
 * 2014/08/26 rwp
 *   AUT-152: Reset low_fuel indicator when ignition is turned off
 *   Change order of initial time checking for detecting power-loss
 * 2014/08/27 rwp
 *   AK-20: Changed timeout in getVicResponse to 5 seconds
 * 2014/09/07 rwp
 *   AUT-84: Dont kill waninfo.pid if equal 0
 * 2014/09/09 rwp
 *   AK-28: Set initial batval=12.5 and dont shutdown if vic_failed
 * 2014/09/23 rwp
 *   Fix reactivation to check for can_activate (verizon testing)
 *   AUT-149: Use GPS speed for PCSA if no CAN
 * 2014/10/06 rwp
 *   Modified Verizon backoff algorithm for Telit module
 *   Dont try to query Telit module while connecting
 * 2014/10/08 rwp
 *   Set waninfo.pid=0 in killWan
 * 2014/10/28 rwp
 *   Reset cell if chat fails...no response to AT commands
 * 2014/10/30 rwp
 *   AK-44: Set default WifiMaxOnTime to 1 hour
 * 2014/10/31 rwp
 *   AK-41: Fixed command line error for setting BT name
 * 2014/10/31 rwp
 *   AK-40: Fixed to allow resetCell to work if pppd locked
 * 2014/11/03 rwp
 *   AK-44: Use iw command to determing attached wifi stations
 * 2014/11/05 rwp
 *   Allow for disabling setting cell lowpower
 * 2014/11/06 rwp
 *   Cleanup pppd zombies, when killing pppd
 *   Allow for turning off USB VBUS to Telit
 * 2014/11/07 rwp
 *   AK-45: Save upgrade version to UpgradedVersionFile
 *   AK-50: Perform progressive cell reset strategy
 * 2014/11/11 rwp
 *   Added reporting FuelUsed
 * 2014/11/25 rwp
 *   Dont save stopped fuel level if ignition is off
 * 2014/11/26 rwp
 *   Check if external antenna is not connected
 * 2014/12/06 rwp
 *   Added code for Short Term wifi usage
 * 2015/01/08 rwp
 *   AK-63: Make updating associated_sta atomic
 * 2015/01/17 rwp
 *   AUT-115: Shutdown if ShutdownFile exists
 *   AK-84: Don't shutdown while duping filesystem
 * 2015/01/20 rwp
 *   Ak-72: Keep connection up longer if SMS wakeup
 * 2015/02/03 rwp
 *   AK-80: Get shortterm_starttime initially if ShortTermStarttimeFile exists
 * 2015/02/06 rwp
 *   AK-86: Run autonet_router if WhitelistFile is auto updated
 * 2015/02/12 rwp
 *   AK-90: Keep connection up for 5 minutes after remote start ends
 *   AK-88: Report remote ends with normal timeout
 * 2015/02/19 rwp
 *   AK-97: Change default WifiMaxOnTime to 30 minutes
 *   Report loss of power after time can be established
 * 2015/03/17 rwp
 *   AK-84: Wait for verify_sys to complete if kernel_fail
 * 2015/03/27 rwp
 *   AK-105: Auto-detect NoCan and touch/remove NoCanFile
 * 2015/03/31 rwp
 *   Added provision state control
 *   Dont keep online for wlan0 if wifi is not enabled
 * 2015/04/14 rwp
 *   Store batval in BatteryValFile
 * 2015/04/23 rwp
 *   Added calling omadm_client when needed
 * 2015/05/26 rwp
 *   Changed monthy_usage calculation to int64_t
 * 2015/06/12 rwp
 *   Fixed keeping connetion longer if SMS wakeup
 * 2015/06/18 rwp
 *    Changed vin_to_make to "vin_to_model -k"
 * 2015/06/22 rwp
 *    Change to leave ECU partnumber in hex if can't convert to ascii
 *    Save faults to FaultsFile
 * 2015/07/15 rwp
 *    Create certificate when time is established
 * 2015/07/31 rwp
 *    Added OtaFeature conditions
 * 2015/08/06 rwp
 *    AK-122: Added control of sshd
 * 2015/08/14 rwp
 *    AK-124: Dont try to activate LTE modules (no activatiion_required)
 * 2015/11/06 rwp
 *    Run omadm_client where RunOmadmClient file exists
 * 2015/12/01 rwp
 *    Qualify calls to startOmaDm with ota_info.enabled
 * 2015/12/09 rwp
 *    Returned value from getFuelUsed
 * 2018/04/30 rwp
 *    Modifed to work with Vehicle Abstraction instead of VIC
 *    Don't kill dnsmasq for now
 * 2018/06/05 rwp
 *    Fixed send faults = "none"
 * 2018/06/06 rwp
 *    Fixed tire_pressures="///"
 *    Log seatbelt events
 * 2018/08/31 rwp
 *    Changed config from local_features to ConfigManager
 * 2019/01/07 rwp
 *    Changed to report machine and distro server
 * 2019/03/07 rwp
 *    Modified to support EXO4 board
 * 2019/03/08
 *
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
#include <sys/time.h>
#include <vector>
#include "autonet_files.h"
#include "autonet_types.h"
#include "backtick.h"
#include "beginsWith.h"
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
#include "readconfig.h"
#include "readBuildVars.h"
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
#include "va_helper.h"
#include "vta_causes.h"
#include "network_utils.h"
using namespace std;

void setVicWakeup(time_t wakeup_time);
void setEssid();
void createGsmFiles();
void getCellConfig();
void getProvisionState();
void connect();
void connect_ppp();
void connect_lte();
bool dial_lte();
//void checkAntenna();
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
//void checkVicVersion();
//void getVicVersion();
int getVicResponse(int reqid, int reqlen, Byte *buffer);
void processVaNotify(string message);
void decodeVaStatus(string status);
void handleVaStatus(string status);
void updateCarCapabilities(int capabilities);
void sendLowFuelEvent();
void sendLowBatteryEvent();
double getFuelUsed();
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
void setWifiDefaults();
void readGpsPosition();
int readGpsSpeed();
void createCertificate();
void checkOtaStatus();
void checkCarStatus();
void notifySeatbelts();
void reportRemoteAborted();
void reportAlarm();
void getVin();
void checkCan();
void getVicRtc(struct tm *tm);
//void setVicRtc();
//void setVicWatchdog();
//void setVicQuickScanTimes();
void getVaStatus(bool getlast);
void check_for_faults();
void sendFaultsEvent(string faults_str);
string getFaults();
void getCellProfile();
bool activateCell();
string getCellResponse(const char *cmnd, int timeout);
string getCellResponse(const char *cmnd);
void killCellTalker();
bool isOptionModule();
bool isTelitModule();
bool turnOnCell();
void cellPulseOnKey();
void turnOffOnCell();
void resetCellHdw();
void resetCell(string reason, int state);
void checkFeaturesChange();
void getWifiEnable();
void getWifiMaxOnTime();
void getPositionReportInterval();
void getPowerControlSettings(bool initial);
void getCarKeySettings();
void getAntennaSettings();
//void setAntenna(int val);
void getValetFeature();
void getNoCanFeature();
void getSeatbeltsFeature();
void getHomeLanFeature();
void getFleetFeature();
void getSmsBlockingFeature();
void getProvisionedConfig();
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
void waitForVerifyToComplete();
void updateUpgradeInfo();
void completeUpgrade();
void waitForUpgradeToFinish();
void doUpgrade();
//void updateVic(string new_vers);
void upgradeCellModule(string bootdir);
void upgradeOptionModule(string bootdir);
void upgradeTelitModule(string bootdir);
string readUbootVariable(string var);
void setUbootVariable(string var, string val);
string getOppositeState();
void dupFs();
void stopGps();
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
void startOmaDm();
void getFakeEcus();

#define RtcErrorFile "/tmp/rtc_failed"
#define FuelFile "/tmp/fuel"
#define OdomFile "/tmp/odom"
#define SpeedFile "/tmp/speed"
#define SeatsFile "/tmp/seats"
#define IgnFile "/tmp/ign"
#define FakeFaultsFile "/tmp/faults"
#define FakeEcusFile "/tmp/fake_ecus"
bool single_os = false;
//#define va_event_list "crash,ignition,battery,ecu_scan,dtc_complete,vic_status"
#define va_event_list "ignition"
bool vicEnabled = false;
bool alrm_sig = false;
#define CommandTimeout1 120
#define CommandTimeout2 60
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
string last_vaquery_resp = "";
Config config;

struct CellInfo {
    string type;
    string device;
    bool diag_available;
    bool connected;
    bool have_signal;
    bool dont_connect;
    bool provisioned;
    bool activate;
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
    int error_flags;
    bool no_commands;
    int reset_state;
    bool needs_dhcp;

    CellInfo() {
        type = "ppp";
        device = "ppp0";
        diag_available = false;;
        connected = false;;
        have_signal = false;;
        dont_connect = false;
        provisioned = true;
        activate = false;
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
        support_hold = 0;
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
        error_flags = 0;
        no_commands = false;
        reset_state = 0;
	needs_dhcp = false;
    }
};
CellInfo cellinfo;

struct WanInfo {
    bool up;
    bool exists;
    pid_t pid;
    pid_t child_pid;
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
    int64_t sum_rx_bytes;
    int64_t sum_tx_bytes;
    int64_t monthly_usage;
    int64_t shortterm_usage;
    string usage_month;
    string shortterm_starttime;
    int ratelimit;
    bool reload_mods;
    bool reset_cell;
    bool reboot;

    WanInfo() {
        up = false;
        exists = false;;
        pid = 0;
        child_pid = 0;
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
        shortterm_usage = 0;
        usage_month = "";
        shortterm_starttime = "";
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
        max_on_time = 1800;
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

struct OtaInfo {
    bool enabled;
    bool start_omadm;
    bool omadm_running;
    bool flashing;
    bool downloading;
    unsigned long start_timer;
    unsigned long ota_timer;

    OtaInfo() {
        enabled = false;
        start_omadm = false;
        omadm_running = false;
        flashing = false;
        downloading = false;
        start_timer = 0;
        ota_timer = 0;
    }
};
OtaInfo ota_info;

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
    float speed;
    float odometer;
    int check_engine;
    bool can_active;
    bool can_a;
    bool can_b;
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
    vector<PowerSchedule> power_schedule;
    int cell_hold_time;
    string vin;
    unsigned int stay_on_delay;
    unsigned long low_bat_time;
    bool low_battery;
    unsigned int ignition_change_time;
    unsigned int ignition_delay;
    string oil_life;
    string ecall_state;
    bool location_disabled;
    int capabilities;
    string capabilities_str;
    string alarm_state;
    int alarm_val;
    unsigned int alarm_report_time;
    int alarm_report_state;
    string alarm_cause;
    string faults;
    bool get_quickscan;
    int quickscan_idx;
    int quickscan_ecus;
    int quickscan_cnt;
    string quickscan_str;
//    string vic_version;
//    string vic_version_short;
//    string vic_build;
//    string vic_requested_build;
    string tire_pressures;
    int fuel_level;
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
    double fuel_used;

    CarState() {
        ignition = false;
        no_can = false;
        engine = false;
        moving = false;
        speed = -1;
        odometer = -1;
        check_engine = 0;
        can_active = false;
        can_a = false;
        can_b = false;
        can_stopped = false;
        vic_reset = false;
        ign_time  = 0;
        batval = 12.5;
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
        cell_hold_time = CellHoldTime;
        vin = "";
        stay_on_delay = 0;
        low_bat_time = 0;
        low_battery = false;
        ignition_change_time = 0;
        ignition_delay = 0;
        oil_life = "";
        ecall_state = "";
        location_disabled = false;
        capabilities_str = "none";
        alarm_state = "unknown"; // SNA
        alarm_val = 0x07; // SNA
        alarm_report_time = 0;
        alarm_report_state = 0;
        alarm_cause = ""; // Not set
        faults = "none";
        get_quickscan = false;
        quickscan_idx = 0;
        quickscan_ecus = 0;
        quickscan_cnt = 0;
        quickscan_str = "";
//        vic_version = "";
//        vic_version_short = "";
//        vic_build = "";
//        vic_requested_build = "";
        tire_pressures = "";
        fuel_level = -1;
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
        fuel_used = 0.0;
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
VaHelper va;
VaHelper va_notifies;

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
        string current_version = getStringFromFile(SwVersionFile);
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
#define IN_VERIFYWAIT 0x80
#define IN_CONNECT 0x100
#define IN_UPLOOP 0x200
#define IN_WAITLOOP 0x400
#define IN_DUPFSWAIT 0x800
#define IN_CELLCONNECT 0x100
#define IN_CELLSEND 0x200
#define IN_CELLRECV 0x400
#define IN_MONITOR_LOCK 0x8000
bool experimental_lte = false;
int wait_on_verify = 0;
unsigned int backup_autonet_time = 0;
int antenna_setting = 0;
bool tried_internal_antenna = false;
string board_name = "unknown";

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
    char config_key[] = "monitor.*";
    board_name = readUbootVariable("board_name");
    if (board_name == "") {
        board_name = "unknown";
    }
    config.readConfigManager(config_key);
    car_state.keep_on = config.isTrue("monitor.KeepOn");
    wlan1info.homelan_enabled = config.isTrue("monitor.HomeLan");
    wlan0info.enabled = config.isTrue("monitor.NoWlan");
    ota_info.enabled = config.isTrue("monitor.OTA");
    getAntennaSettings();
    getProvisionedConfig();
    string stayontime = config.getString("monitor.StayOn");
    if (stayontime != "") {
        logError("Got StayOn="+stayontime);
        car_state.stay_on_delay = stringToInt(stayontime) * 60;
    }
    getWifiMaxOnTime();
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
    if (fileExists(LastCellModel)) {
        cellinfo.model = getStringFromFile(LastCellModel);
    }
    kernelId = getStringFromFile(KernelIdFile);
/*rcg
    if (kernelId.find_first_of("012") != string::npos) {
        string kernelstate = readUbootVariable(string(".state")+kernelId);
        string state = readUbootVariable(".state");
        string oppstate = getOppositeState();
        if ( (kernelstate == "booting")  or
             (state == "kernel_fail") or
             (oppstate == "bad") ){
            logError("Setting wait_on_verify");
            wait_on_verify = 1;
        }
    }
*/

//    getNoCanFeature();
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
        unlink(NoCanFile);
        logError("WokeOnCanFile set - init loss of power");
        woke_on_can = false;
        touchFile(WokeOnCanFile);
        logError("RTC invalid");
    }
    if (!cell_bad) {
        //if (!fileExists(GpsTimeFile)) {
            getCellTime();
        //}
    }
    car_state.no_can = fileExists(NoCanFile);
    if (fileExists(GpsTimeFile)) {
        createCertificate();
        timeinfo.state = Good;
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
//    else {
//        setVicRtc();
//    }
    va.createSocket("mon", false);
    va_notifies.createSocket("mon", true);
    string resp = va.sendQuery("ignition",5,1);
    if (resp == "") {
        logError("VA not responding");
    }
    else {
        logError("Talking to VA");
    }
    va_notifies.vah_send("register:faults_scan_complete,register:ecu_scan_complete");
//    upgradeCellModule("/boot/");
//    getVicVersion();
//    if (vic.build == "OBDII") {
//        car_state.obd2_unit = true;
//    }
    //checkCan();
    getVin();
    getProvisionState();
    getWifiEnable();
    getPositionReportInterval();
    getPowerControlSettings(true);
    getCarKeySettings();
    getValetFeature();
    getSeatbeltsFeature();
    getFleetFeature();
    getSmsBlockingFeature();
    features.getFeature(RateLimits);
    getDisableLocation();
    getProvisionState();
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
                //if (!fileExists(GpsTimeFile)) {
                    getCellTime();
                //}
                //else {
                if (fileExists(GpsTimeFile)) {
                    createCertificate();
                    timeinfo.state = Good;
//                    setVicRtc();
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
//    setVicWatchdog();
//    setVicQuickScanTimes();
//    if (vic.protocol_version >= 0x0233) {
        // Reset Wakeup time
//        setVicWakeup(0);
//    }
    getVaStatus(false);
    if (battery_connected) {
        string event = "CarState PowerLoss";
        eventlog.append(event);
/*rcg
        if (!fileExists(SshControlFile) and
            !fileExists(KernelTestingFile)) {
            string timestr = toString(time(NULL) + 120);
            updateFile(SshControlFile, timestr);
            int sysval = system("/etc/init.d/S50sshd start");
        }
*/
    }
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
    setEssid();
//        if ( (strncmp(essid.c_str(), "autonet-", 8) == 0) or
//             ( (strncmp(essid.c_str(), "mopar-", 6) == 0) and
//               (essid != mopar_essid) ) ) {
//            logError("Setting ESSID to "+mopar_essid);
//            int sysval = str_system("autonetconfig essid " + mopar_essid);
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
//            int sysval = str_system("autonetconfig essid " + autonet_essid);
//        }
//    }
    getStateInfo();
    string bootdelay_str = config.getString("monitor.BootDelay");
    string brand = features.getFeature(VehicleBrand);
    if (brand == "Dow") {
        dow_unit = true;
        string scan_period = config.getString("monitor.Wlan1ScanPeriod");
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
        cellinfo.command_timeout = Uptime + CommandTimeout1;
        clearSmsMessages();
        woke_on_can = false;
        if (ota_info.enabled) {
            ota_info.start_omadm = true;
        }
    }
    else {
        logError("No SMS message");
    }
    if (ota_info.enabled and fileExists(EcusChangeFile)) {
        ota_info.start_omadm = true;
        logError(string(EcusChangeFile)+" exists...need to start omadm_client");
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
        else if (po_state == "CellLocked") {
            logError("Powered up after cell locked");
            string event = "CellLocked...Power cycle";
            eventlog.append(event);
            woke_on_can = false;
            cellinfo.events_to_send = true;
        }
        else if (strncmp(po_state.c_str(),"Shutdown-",9) == 0) {
            int holdtime = stringToInt(po_state.substr(9));
            cellinfo.support_hold = Uptime + holdtime*60;
            woke_on_can = false;
        }
        else if (po_state == "OtaWakeup") {
            if (system("ota_schedules -i") == 0) {
                logError("Powered up on OTA schedule");
                ota_info.start_timer = Uptime + 300;
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
    if (cellinfo.can_activate and cellinfo.activation_required) {
        bool try_activate = false;
        if ( cellinfo.got_profile and
             !cellinfo.activated ) {
            try_activate = true;
        }
        else if (!cellinfo.provisioned) {
            if (cellinfo.activate) {
                try_activate = true;
            }
        }
        else if (cellinfo.firstreactivate) {
            try_activate = true;
        }
        if (try_activate) {
//             if ( (antenna_setting == 3) and
//                  !tried_internal_antenna ) {
//                logError("checking antenna");
//                checkAntenna();
//            }
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
//            int sysval = system("ledcontrol 6");
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
                    int sysval = str_system("tcpdump -i ppp0 -n -w "+dumpfile+" &");
                }
                setResolver(cellinfo.type);
//                int sysval = system("ledcontrol 1");
                int sysval = system("autonet_router");
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
                //int sysval = system("ratelimit");
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
                    int sysval = system("update_default_sites &");
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
                    time_t last_omadm = fileTime(LastOmaDmTimeFile);
                    if ( ota_info.enabled and
                         (ota_info.start_omadm or
                         ((now - last_omadm) > 86400) or
                         (fileExists(RunOmadmClientFile))) ) {
                        startOmaDm();
                    }
                    if (cellinfo.first_connection) {
                        checkUpgrading();
                    }
                }
                if (ota_info.start_timer > 0) {
                    int sysval = system("ota_downloadMgr &");
                    sysval = system("ota_installMgr &");
                    ota_info.ota_timer = Uptime + 60;
                }
                if (cellinfo.first_connection) {
                    cellinfo.first_connection = false;
                }
//                int sysval = system("backup_autonet");
                if (backup_autonet_time == 0) {
                    backup_autonet_time = Uptime + 60;
                }
                logError("Entering upLoop");
                upLoop();
                if (fileExists("/autonet/admin/tcpdump")) {
                    int sysval = system("pkill tcpdump");
                }
                setResolver("nowan");
                //int sysval = system("autonet_router");
                unlink(CommNeededFile);
            }
        }
//        cout << "ignition:" << car_state.ignition << endl;
//        cout << "ignition_change_time:" << car_state.ignition_change_time << endl;
//        cout << "needConnection:" << needConnection() << endl;
//        tellWhyNeedConnection();
        if (car_state.low_battery and !ota_info.flashing) {
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
//            int sysval = system("ledcontrol 13");
            wlan1Loop();
        }
        else {
            logError("Entering waitingLoop");
//            int sysval = system("ledcontrol 13");
            waitingLoop();
        }
    }
    if (fileExists(ShutdownFile)) {
        string val_str = getStringFromFile(ShutdownFile);
        Strings vals = split(val_str, ",");
        int mode = 0x00; // Normal mode
        int timer = 0;
        int waketime = 0;
        if (vals.size() > 0) {
            mode = stringToInt(vals[0]);
            if (vals.size() > 1) {
                timer = stringToInt(vals[1]);
                if (vals.size() > 2) {
                    waketime = stringToInt(vals[2]);
                }
            }
        }
        logError("Shutdown requested: "+val_str);
        powerOff("Shutdown-"+toString(waketime), mode, timer);
    }
    else if (car_state.low_battery) {
        // shutdown in deep sleep
        logError("Power down - low bat");
        powerOff("LowBat", 0x02, 0);
    }
    else if (backtick("ota_schedules") != "0") {
        time_t wakeup_time = stringToLong(backtick("ota_schedules"));
//        if (vic.protocol_version >= 0x0233) {
//            setVicWakeup(wakeup_time);
//            logError("Power down - OTA Schedule, wakeup at "+getDateTime(wakeup_time));
//            powerOff("OtaWakeup", 0x00, 0);
//        }
//        else {
//            int delta = (wakeup_time - time(NULL))/60 + 1;
//            if (delta <= 0) {
//                delta = 1;
//            }
//            if (delta > 255) {
//                delta = 255;
//            }
//            logError("Power down - OTA Schedule, wakeup in "+toString(delta)+" minutes");
//            powerOff("OtaWakeup", 0x05, delta);
//        }
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

void setVicWakeup(time_t wakeup_time) {
//    Byte buffer[64];
//    struct tm *ptm;
//    if (wakeup_time > 0) {
//        ptm = gmtime(&wakeup_time);
//        buffer[0] = (ptm->tm_year+1900) % 100;
//        buffer[1] = ptm->tm_mon+1;
//        buffer[2] = ptm->tm_mday;
//        buffer[3] = ptm->tm_hour;
//        buffer[4] = ptm->tm_min;
//        buffer[5] = ptm->tm_sec;
//    }
//    else {
//        for (int i=0; i < 6; i++) {
//            buffer[i] = 0;
//        }
//    }
//    getVicResponse(0x0E, 6, buffer);
}

void setEssid() {
    string hostname = getStringFromFile(UnitIdFile);
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
                    int sysval = str_system("autonetconfig essid " + new_essid);
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
        int sysval = system("update_gsm_info -c");
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
            int sysval = str_system(cmnd);
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
            string model = cellConfig["cell_model"];
            if (cellinfo.model != model) {
                updateFile(LastCellModel, model);
            }
            cellinfo.model = model;
            cellinfo.no_config = false;
            logError("Got cell_model");
        }
        if (cellConfig.find("activation_required") != cellConfig.end()) {
            cellinfo.activation_required = true;
        }
        if (cellConfig.find("cell_TYPE") != cellConfig.end()) {
            cellinfo.type = cellConfig["cell_TYPE"];
        } else if (cellConfig.find("cell_islte") != cellConfig.end()) {
            cellinfo.type = "lte";
        }
        if (cellConfig.find("cell_needsdhcp") != cellConfig.end()) {
            cellinfo.needs_dhcp = true;
        }
        if (cellConfig.find("cell_device") != cellConfig.end()) {
            cellinfo.device = cellConfig["cell_device"];
        }
    }
}

void getProvisionState() {
    Strings vals; //split(backtick("eeprom -a 30 -n 5"), " ");
    cellinfo.provisioned = true;
    if ((vals.size() > 0) && (vals[0] == "02")) {
        cellinfo.provisioned = false;
        string hextime = vals[1]+vals[2]+vals[3]+vals[4];
        time_t activate_time = 0;
        cellinfo.activate = false;
        if (hextime != "ffffffff") {
            sscanf(hextime.c_str(), "%lX", &activate_time);
            if ( (time(NULL) - activate_time) < (3*24*3600) ) {
                cellinfo.activate = true;
            }
        }
    }
}

void connect() {
    if (checkWanUp()) {
        cellinfo.connected = true;
        cellinfo.prev_state = cellinfo.state;
        cellinfo.state = "up";
        cellinfo.temp_fail_cnt = 0;
        cellinfo.reset_state = 0;

    } else if (cellinfo.type == "ppp") {
        connect_ppp();
    }
    else if (cellinfo.type == "lte") {
        connect_lte();
    }
}

void connect_ppp() {
    monitor_flags |= IN_CONNECT;
    int reset_cnt = 0;
    int num_backoffs = 7;
    int verizon_backoffs[7] = {0,0,0,60,120,480,900};
    int non_verizon_backoffs[7] = {0,0,0,60,60,60,180};
//    int telit_verizon_backoffs[7] = {0,0,0,60,60,60,60};
    int telit_verizon_backoffs[7] = {0,0,0,0,0,0,0};
    int *backoffs = non_verizon_backoffs;
    if (cellinfo.provider == "verizon") {
        backoffs = verizon_backoffs;
        if (isTelitModule()) {
            backoffs = telit_verizon_backoffs;
        }
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
        if (isTelitModule() and !fileExists(NoCellCommandsFile)) {
            cellinfo.no_commands = true;
            touchFile(NoCellCommandsFile);
        }
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
//                        int sysval = system("pkill pppd");
                    }
                    else {
                        cellinfo.connected = true;
                        cellinfo.prev_state = cellinfo.state;
                        cellinfo.state = "up";
                        cellinfo.temp_fail_cnt = 0;
                        cellinfo.reset_state = 0;
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
                    int sysval = system("pkill -9 pppd");
                    sleep(1);
                    unlink(CellmodemLockFile);
                    if (!cellinfo.testing_keepup) {
                        if (cellinfo.no_commands) {
                            cellinfo.no_commands = false;
                            unlink(NoCellCommandsFile);
                        }
                        resetCell("pppd locked up", 2);
                        reactivation_time = Uptime + 900;
                    }
                    waninfo.pppd_exited = true;
                }
                check_pppd_time = Uptime;
            }
        }
        if (cellinfo.no_commands) {
            cellinfo.no_commands = false;
            unlink(NoCellCommandsFile);
        }
        getCellInfo();
        if (waninfo.pppd_exited) {
//            string ceer = getCellResponse("AT+CEER");
//            logError("Connection failed: "+ceer);
            string cause = getPppCloseCause();
            logError("Connection failed: "+cause);
            cellinfo.fail_count++;
            if ( (cause == "failed no carrier") and
                 (antenna_setting == 3) and
                 cellinfo.first_connection and
                 !tried_internal_antenna ) {
//                logError("checking antenna");
//                checkAntenna();
            }
            if (waninfo.reboot and !cellinfo.testing_keepup) {
                reboot("Connection "+cause);
            }
            else if (waninfo.reset_cell and !cellinfo.testing_keepup) {
                reset_cnt++;
                if (reset_cnt > 2) {
                    logError("Connection failure...resetting cell");
                    resetCell("modem not responding", 0);
                    reactivation_time = Uptime + 900;
//                    reset_cnt = 0;
                }
            }
            if ( cellinfo.activation_required and
                 cellinfo.can_activate and
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
        if (!dial_lte()) {
            break;
        }
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

bool dial_lte() {
    bool ret = false;
    string resp;
    resp = getCellResponse("connect",60);
    logError("Response to connect: "+resp);
    if (resp.find("OK") != string::npos) {
        if (cellinfo.needs_dhcp) {
            logError("Starting DHCP Client");
            sleep(1);
            int sysval = str_system("ifconfig "+cellinfo.device+" 0.0.0.0 up");
            sleep(1);
            system("netstat -rn");
            sysval = system("/sbin/route del default gw 192.168.90.1");
            if (sysval != 0) {
                logError("Delete route failed");
            }
            sleep(1);
            sysval = system("pkill udhcpc");
            sysval = str_system("udhcpc -i "+cellinfo.device);
            sleep(1);
            cellinfo.connected = false;
        }
        else {
            cellinfo.connected = true;
        }
        waninfo.pppd_exited = false;
        ret = true;
    }
   return ret;
}

//void checkAntenna() {
//    if (!fileExists(GpsPositionFile)) {
//        getCellInfo();
//        string sig = cellinfo.signal;
//        if (sig.find_first_not_of("0123456789ABCDEF") == string::npos) {
//            int old_sig = stringToInt(sig);
//            setAntenna(0x00);
//            sleep(5);
//            getCellInfo();
//            int new_sig = stringToInt(cellinfo.signal);
//            if ((new_sig-old_sig) > 10) {
//                logError("External antenna is not connected");
//                eventlog.append("NotifySupport External antenna not connected");
//                touchFile(AntennaWrongFile);
//            }
//            else {
//                setAntenna(0x03);
//            }
//            tried_internal_antenna = true;
//        }
//    }
//}

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
        if (waninfo.pid != 0) {
            appendToFile(PppLogFile, "monitor: "+why);
            kill(waninfo.pid, SIGHUP);
            waninfo.pid = 0;
        }
    }
    else if (cellinfo.type == "lte") {
        int sysval = system("/sbin/route del default");
        if (cellinfo.needs_dhcp) {
            sysval = system("cp /ro/etc/resolv.conf /rw/etc/resolv.conf");
            sysval = system("pkill udhcpc");
        }
        string resp = getCellResponse("disconnect",10);
        logError("Response to disconnect: "+resp);
    }
}

void setResolver(string type) {
//    if ((type == "nowan") or fileExists(DisableFile)) {
    if (type == "init") {
        unlink(DnsmasqConf);
        int slvar = symlink(DnsmasqLocalConf, DnsmasqConf);
//        int sysval = system("killall dnsmasq");
    }
    else if (type == "nowan") {
        unlink(DnsmasqConf);
        int slvar = symlink(DnsmasqLocalConf, DnsmasqConf);
        int sysval = system("save_dns_entries");
//        sysval = system("killall dnsmasq");
    }
    else if (type == "ppp") {
        unlink(DnsmasqConf);
        int slvar = symlink(DnsmasqRemoteConf, DnsmasqConf);
//        int sysval = system("killall dnsmasq");
        monWait(1);
    }
    else if (type == "lte") {
        unlink(DnsmasqConf);
        int slvar = symlink(DnsmasqLteConf, DnsmasqConf);
//        int sysval = system("killall dnsmasq");
        monWait(1);
    }
}

void setNtpTime() {
    if (!timeinfo.celltime_available and !fileExists(GpsTimeFile)) {
        logError("Setting time via NTP");
        if (system("ntpdate 0.pool.ntp.org") == 0) {
//            if (timeinfo.state != Good) {
//                setVicRtc();
//            }
            createCertificate();
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
    time_t whitelist_time = 0;
    if (fileExists(WhitelistFile)) {
        whitelist_time = fileTime(WhitelistFile);
    }
    logError("Updating files");
    string update_cmd = "updatefiles";
    if (prlfile != "") {
        update_cmd += " -p " + prlfile;
    }
    update_cmd += " &";
    int sysval = str_system(update_cmd);
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
    if ( fileExists(WhitelistFile) and
       (fileTime(WhitelistFile) > whitelist_time) ) {
        if (!wifi_enabled or !registered) {
            //int sysval = system("autonet_router");
        }
    }
    sysval = system("upgrade_media &");
}

void updatePrl(string prlfile) {
}

void cleanUpgradeDir() {
    if ( (kernelId == "1") or (kernelId == "2") or single_os ) {
        if (fileExists(UpgradeStateFile)) {
            string state = getStringFromFile(UpgradeStateFile);
//            if ( (state == "untarring") or (state == "upgrading") ) {
//                int sysval = system("upgrade -c &");
//            }
//            else if (state != "fetching") {
//                int sysval = str_system(string("rm -rf ") + UpgradeDir + "/*");
//            }
            if ( (state == "starting") or
                 (state == "upgraded") or
                 (state == "failed") ) {
                int sysval = str_system(string("rm -rf ") + UpgradeDir + "/*");
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
                int sysval = system("upgrade -c &");
            }
        }
    }
}

void getMonthlyUsage() {
    string month = getDateTime();
    month = month.substr(0,4) + month.substr(5,2);
    waninfo.usage_month = month;
    int64_t monthly_usage = 0;
    int64_t shortterm_usage = 0;
    string tmp_usage_file = "/tmp/" + month;
    string usage_file = UsageDir;
    usage_file += "/" + month;
    if (fileExists(tmp_usage_file)) {
        monthly_usage = stringToInt64(getStringFromFile(tmp_usage_file));
    }
    else if (fileExists(usage_file)) {
        monthly_usage = stringToInt64(getStringFromFile(usage_file));
    }
    monthly_usage += waninfo.monthly_usage;
    waninfo.monthly_usage = monthly_usage;
    if (!fileExists(tmp_usage_file)) {
        updateFile(tmp_usage_file, toString(monthly_usage));
    }
    if (!fileExists(usage_file)) {
        updateFile(usage_file, toString(monthly_usage));
    }
    if (waninfo.shortterm_starttime != "") {
        if (fileExists(TempShortTermUsageFile)) {
            shortterm_usage = stringToInt64(getStringFromFile(TempShortTermUsageFile));
        }
        else if (fileExists(ShortTermUsageFile)) {
            shortterm_usage = stringToInt64(getStringFromFile(ShortTermUsageFile));
        }
        shortterm_usage += waninfo.shortterm_usage;
        waninfo.shortterm_usage = shortterm_usage;
        if (!fileExists(TempShortTermUsageFile)) {
            updateFile(TempShortTermUsageFile, toString(shortterm_usage));
        }
        if (!fileExists(ShortTermUsageFile)) {
            updateFile(ShortTermUsageFile, toString(shortterm_usage));
        }
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
    if (Uptime < cellinfo.command_timeout) {
        cellinfo.command_timeout = Uptime + CommandTimeout1;
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
            cellinfo.command_timeout = Uptime + CommandTimeout2;
            if (ota_info.enabled) {
                startOmaDm();
            }
        }
//        if (!car_state.ignition and (cellinfo.boot_delay != 0)) {
//            up_wait = Uptime + 60;
//            cellinfo.boot_delay = 0;
//        }

        if (!getWan(false)) {
            //logError("Wan interface went away");
            killWan("Wan interface went away");
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
        if ((up_wait > 0) or (cellinfo.command_timeout > 0)) {
            string commstate = getStringFromFile(CommStateFile);
            if (last_commstate == "") {
                if (commstate == "Connected") {
                    up_wait = Uptime + 60;
                    if (Uptime < cellinfo.command_timeout) {
                        logError("Extending command timeout");
                        cellinfo.command_timeout = Uptime + CommandTimeout2;
                    }
                    last_commstate = commstate;
                }
            }
            if (cellinfo.events_to_send) {
                up_wait = Uptime + 60;
                cellinfo.events_to_send = false;
            }
            if ( (up_wait > 0) and
                 ( (Uptime > up_wait) or
                   (!fileExists(EventCountFile)) ) ) {
                up_wait = 0;
            }
            if ( (cellinfo.command_timeout > 0) and
                 (Uptime > cellinfo.command_timeout) ) {
               cellinfo.command_timeout = 0;
            }
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
                break;
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
    if (waninfo.shortterm_starttime != "") {
        updateFile(TempShortTermUsageFile, toString(waninfo.shortterm_usage));
    }
}

void writeMonthlyUsage() {
    string usage_file = UsageDir;
    usage_file += "/" + waninfo.usage_month;
    updateFile(usage_file, toString(waninfo.monthly_usage));
    if (waninfo.shortterm_starttime != "") {
        updateFile(ShortTermUsageFile, toString(waninfo.shortterm_usage));
    }
    else {
        unlink(ShortTermUsageFile);
    }
}

void waitingLoop() {
    monitor_flags |= IN_WAITLOOP;
    if (needConnection()) {
        tellWhyCantConnect();
    }
    while (keepOn() and (!needConnection() or !canConnect())) {
        if (car_state.low_battery and !ota_info.flashing) {
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
                            int sysval = system("homesync &");
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
            cellinfo.command_timeout = Uptime + CommandTimeout1;
            if (ota_info.enabled) {
                ota_info.start_omadm = true;
            }
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
            int sysval = str_system(string("wpa_supplicant -i wlan0 -c ")+WpaSupplicantFile+" >/var/log/wpa_supplicant.log 2>&1 &");
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
                int sysval = system("ifconfig wlan0 up");
                sleep(1);
                logError("udhcpc -i wlan0");
                sysval = system("pkill udhcpc");
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
                int sysval = str_system(ifconfig_cmd);
                writeStringToFile("/rw/etc/resolv.conf", resolvconf_str);
                if (router != "") {
//                    string route_cmd = "route add default gw "+router;
//                    int sysval = str_system(route_cmd);
                }
            }
        }
    }
    return retval;
}

void ifdownWlan0() {
    if (fileExists(HostapdConfFile)) {
        logError("pkill hostapd");
        int sysval = system("pkill hostapd");
        sleep(1);
    }
    logError("ifdown wlan0");
    int sysval = system("ifdown wlan0");
    wlan0info.off = true;
}

void ifupWlan0() {
    logError("ifup wlan0");
    int sysval = system("ifup wlan0");
    if (fileExists(HostapdConfFile)) {
        int sysval = str_system(string("hostapd -B ")+HostapdConfFile);
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
    int sysval = system("/sbin/route del default");
    sysval = system("cp /ro/etc/resolv.conf /rw/etc/resolv.conf");
    sysval = system("pkill udhcpc");
    sleep(1);
    sysval = system("pkill wpa_supplicant");
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
    int sysval = str_system(logger+" stop");
    unlink("/var/log/messages");
    unlink(Wlan0AssocFile);
    sysval = str_system(logger+" start");
    sysval = system("pkill wifi_counter");
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
        time_t end_t = time(NULL) + timeout;
        while (timeout > 0) {

            fd_set rd, wr, er;
            int nfds = 0;
            FD_ZERO(&rd);
            FD_ZERO(&wr);
            FD_ZERO(&er);
            int va_socket = va.getSocket();
            if (va_socket >= 0) {
                FD_SET(va_socket, &rd);
                nfds = max(nfds, va_socket);
            }
            int notifies_socket = va_notifies.getSocket();
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
                if ((va_socket >= 0) and (FD_ISSET(va_socket, &rd))) {
                    string resp = va.vah_recv();
                    if (resp != "") {
                        handleVaStatus(resp);
                    }
                }
                if ((notifies_socket >= 0) and (FD_ISSET(notifies_socket, &rd))) {
                    string resp = va_notifies.vah_recv();
                    if (resp != "") {
                        processVaNotify(resp);
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

//void checkVicVersion() {
//    if (vic.build != vic.requested_build) {
//        string binfile = backtick("ls /boot/VIC-"+vic.requested_build+"-*.dat | tail -1");
//        if (binfile != "") {
//            updateVic(binfile);
//        }
//    }
//    else if (vic.build == "OBDII") {
//        if (car_state.vin != "") {
//            string make = backtick("vin_to_model -k "+car_state.vin);
//            if ( (make == "Dodge") or (make == "Chrysler") or
//                 (make == "Jeep") or (make == "Ram") ) {
//                vic.getBusStructure(app_id);
//                if (vic.bus1active) {
//                    string requested_build = "CHY11";
//                    if (vic.bus1type == _29bit) {
//                        requested_build = "CHY29";
//                    }
//                    string binfile = backtick("ls /boot/VIC-"+requested_build+"-*.dat | tail -1");
//                    if (binfile != "") {
//                        updateVic(binfile);
//                    }
//                }
//            }
//        }
//    }
//#define ReloadVicFile "/autonet/admin/reload_vic"
//    else if (fileExists(ReloadVicFile)) {
//        string vicfile = readFile(ReloadVicFile);
//        unlink(ReloadVicFile);
//        updateVic(vicfile);
//    }
//}

//void getVicVersion() {
//    if (vicEnabled) {
//       vic.getVersion(app_id);
   //    car_state.vic_version = vic.long_version;
   //    car_state.vic_version_short = vic.version;
   //    car_state.vic_build = vic.build;
   //    car_state.vic_requested_build = vic.requested_build;
   //    logError("VIC version: "+car_state.vic_build+"-"+car_state.vic_version_short);
//       logError("VIC version: "+vic.build+"-"+vic.version_str);
//    }
//}

void processVaNotify(string message) {
    Strings parms = split(message, ": ");
    string var = parms[0];
    string val = parms[1];
    if (var == "faults_scan_complete") {
        if (val == "yes") {
            car_state.check_fault_time = Uptime;
        }
    }
    else if (var == "ecu_scan_complete") {
        if (val == "yes") {
            struct tm *ptm;
            time_t now = time(NULL);
            ptm = gmtime(&now);
            car_state.quickscan_str = string_printf(" QSTime:%04d/%02d/%02dT%02d:%02d:%02d",
                             ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
            car_state.get_quickscan = true;
        }
    }
//    if (type == 0x20) {
//        handleVaStatus(message);
//    }
//    else if (type == 0xFF) {
//        // Error returned
//        int code = buffer[3];
//        logError("Vic returned error: "+toString(code));
//        if (code == 0x01) {
//            // Authentication error - Rtc clock error
//            logError("Vic authentication error...setting RTC");
//            setVicRtc();
//            if (!fileExists(RtcErrorFile)) {
//                string event = "NotifySupport RTC failed";
//                eventlog.append(event);
//                touchFile(RtcErrorFile);
//            }
//        }
//    }
//    else if (type == 0xF0) {
//        // Emergency Notification
//        logError("Got emergency notification");
//        car_state.ecall_state = "starting";
//    }
//    else if (type == 0xF1) {
//	// Ignition Change Notification
//        bool ign = buffer[2];
//        logError("Got ignition change notification:"+toString(ign));
//        if (car_state.ignition != ign) {
//            car_state.ignition = ign;
//            doIgnitionProcess();
//        }
//    }
//    else if (type == 0xF2) {
//        // Low Battery Notification
//        logError("Got LowBattery Notification");
////        if (!car_state.low_battery) {
////            sendLowBatteryEvent();
////            car_state.low_battery = true;
////        }
//    }
//    else if (type == 0xF4) {
//        logError("Got QuickScan Notification");
//        car_state.get_quickscan = true;
//        car_state.quickscan_ecus = buffer[2];
//        int year = buffer[3] + 2000;
//        int mon = buffer[4];
//        int day = buffer[5];
//        int hour = buffer[6];
//        int min = buffer[7];
//        int sec = buffer[8];
//        car_state.quickscan_idx = 0;
//        car_state.quickscan_cnt = 0;
//        car_state.quickscan_str = string_printf(" QSTime:%04d/%02d/%02dT%02d:%02d:%02d",
//                             year, mon, day, hour, min, sec);
//        if (car_state.quickscan_ecus == 0) {
//            car_state.get_quickscan = false;
//        }
//    }
//    else if (type == 0xF5) {
//        car_state.check_fault_time = Uptime;
//    }
//    else if (type == 0xF8) {
//        int status_type = (buffer[2] << 8) | buffer[3];
//        if (status_type == 1) {
//            int remote_aborted_cause = buffer[4];
//            logError("Remote start aborted notification: "+string_printf("%02X", remote_aborted_cause));
//            if (car_state.remote_started) {
//                car_state.remote_aborted_cause = remote_aborted_cause;
//                car_state.remote_aborted_time = Uptime;
//            }
//        }
//        else if (status_type == 2) {
//           int vta_cause = buffer[4];
//           logError("VTA cause: "+string_printf("%02X", vta_cause));
//           if (car_state.alarm_cause == -1) {
//               car_state.alarm_report_time = Uptime + 10;
//           }
//           else {
//               car_state.alarm_report_time = Uptime;
//           }
//           car_state.alarm_cause = vta_cause;
//        }
//        else {
//            logError("Unknown VIC status notification: "+toString(status_type));
//        }
//    }
}

void decodeVaStatus(string status) {
    bool last_engine = car_state.engine;
    string last_alarm = car_state.alarm_state;
    string ign_str = getParm(status, "ignition:", ",");
    string eng_str = getParm(status, "engine:", ",");
    string lck_str = getParm(status, "locks:", ",");
    string door_str = getParm(status, "doors:", ",");
    string trnk_str = getParm(status, "trunk:", ",");
    string seat_str = getParm(status, "seats:", ",");
    string belt_str = getParm(status, "seatbelts:", ",");
    string odom_str = getParm(status, "odometer:", ",");
    string spd_str = getParm(status, "speed:", ",");
    string fuel_str = getParm(status, "fuel:", ",");
    string lowf_str = getParm(status, "low_fuel_indicator:", ",");
    string chkeng_str = getParm(status, "check_engine_indicator:", ",");
    string oil_str = getParm(status, "oil_life:", ",");
    string tire_str = getParm(status, "tire_pressures:", ",");
    string alrm_str = getParm(status, "alarm:", ",");
    string alrmcaus_str = getParm(status, "alarm_cause:", ",");
    string can_a_str = getParm(status, "can_a:", ",");
    string can_b_str = getParm(status, "can_b:", ",");
    string faults_str = getParm(status, "faults:", ",");
    //car_state.batval = buffer[7] / 10. + car_state.batadjust;
    car_state.ignition = ign_str == "on";
    car_state.can_a = can_a_str == "active";
    car_state.can_b = can_b_str == "active";
    car_state.can_active = car_state.can_a or car_state.can_b;
    if (eng_str != "") {
        car_state.engine = eng_str != "off";
    }
    if (spd_str != "") {
        car_state.speed = stringToDouble(spd_str);
        car_state.moving = car_state.speed > 8.;
    }
    if (odom_str != "") {
        car_state.odometer = stringToDouble(odom_str);
    }
    car_state.check_engine = chkeng_str == "on";
    if ((oil_str != "") and (oil_str != "sna")) {
        car_state.oil_life = oil_str;
    }
    if (fuel_str != "") {
        car_state.fuel_level = stringToInt(fuel_str);
    }
    if (alrm_str != "") {
        car_state.alarm_state = alrm_str;
    }
    if (alrmcaus_str != "") {
        car_state.alarm_cause = alrmcaus_str;
    }
    if ((tire_str != "") and (tire_str != "sna/sna/sna/sna") and (tire_str != "///")) {
        car_state.tire_pressures = tire_str;
    }
    if (faults_str != "") {
        car_state.faults = faults_str;
    }
    if (!car_state.can_active) {
        if (fileExists("/tmp/moving")) {
            car_state.moving = (bool)stringToInt(getStringFromFile("/tmp/moving"));
        }
        if (fileExists(EngineFile)) {
            car_state.engine = (bool)stringToInt(getStringFromFile(EngineFile));
        }
    }
    if (fileExists("/tmp/chkeng")) {
        car_state.check_engine = stringToInt(getStringFromFile("/tmp/chkeng")) | 0x02;
    }
    if (fileExists(FakeFaultsFile)) {
        string faults = getStringFromFile(FakeFaultsFile);
        if (faults == "") {
            faults = "none";
        }
        sendFaultsEvent(faults);
        unlink(FakeFaultsFile);
    }
    if (fileExists(FakeEcusFile)) {
        getFakeEcus();
    }
    if (last_engine != car_state.engine) {
        if (car_state.engine) {
            car_state.engine_was_on = true;
            if (fileExists(LowFuelFile)) {
                car_state.low_fuel_timer = Uptime;
            }
        }
        if (car_state.can_active) {
            updateFile(EngineFile, toString(car_state.engine));
        }
    }
    if (door_str != "") {
        car_state.doors = 0;
        for (int i=0; i < 4; i++) {
            if (door_str[i*2] == 'o') {
                car_state.doors |= 1 << i;
            }
        }
    }
    if (trnk_str != "") {
        if (trnk_str == "o") {
            car_state.doors |= 0x10;
            car_state.trunk_opened = true;
        }
        else {
            car_state.trunk_opened = false;
            car_state.doors &= 0xEF;
        }
    }
    if (lck_str != "") {
        car_state.locks = 0;
        for (int i=0; i < 4; i++) {
            if (lck_str[i*2] == 'l') {
                car_state.locks |= 1 << i;
            }
        }
        if (car_state.locks != 0x00) {
            car_state.locks |= 0x10;
        }
    }
    if (seat_str != "") {
        car_state.seats = 0;
        for (int i=0; i < 5; i++) {
            if (seat_str[i*2] == 'o') {
                car_state.seats |= 1 << i;
            }
        }
    }
    if (belt_str != "") {
        car_state.belts = 0;
        for (int i=0; i < 5; i++) {
            if (belt_str[i*2] == 'f') {
                car_state.belts |= 1 << i;
            }
        }
    }
    if (alrm_str != "") {
        if (alrm_str == "disarmed") {
            car_state.alarm_val = 0;
        }
        else if (alrm_str == "armed") {
            car_state.alarm_val = 2;
        }
        else if (alrm_str == "triggered") {
            car_state.alarm_val = 4;
        }
    }
//        if (more_state & 0x1000) {
//            car_state.trunk_opened = true;
//            if (!car_state.trunk_opened_sent) {
//                logError("VIC reported trunk was opened");
//            }
//        }
//        if (vic.protocol_version >= 0x0224) {
//            car_state.ignition_status = (more_state & 0x0E00) >> 9;
    if (lowf_str != "") {
        int lf = 0;
        if (lowf_str == "on") {
            lf = 1;
        }
        if (car_state.engine) {
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
    }
 //           car_state.key_status = (more_state & 0x00C0) >> 6;
 //       }
 //       int capabilities = (buffer[11] << 8) | buffer[12];
 //       updateCarCapabilities(capabilities);
 //   }
}

void handleVaStatus(string status) {
    bool last_ign = car_state.ignition;
    bool last_can = car_state.can_active;
    bool last_moving = car_state.moving;
    bool last_engine = car_state.engine;
    string last_alarm = car_state.alarm_state;
    int last_low_fuel = car_state.low_fuel;
    float last_batval = car_state.batval;
    int prev_seats = car_state.seats;
    int prev_belts = car_state.belts;
    string last_faults = car_state.faults;
    decodeVaStatus(status);
    if (car_state.can_active) {
        if (car_state.no_can) {
            unlink(NoCanFile);
            car_state.no_can = false;
        }
        if (car_state.ignition) {
            if (!fileExists(CurrentVinFile) or (getStringFromFile(CurrentVinFile) == "")) {
                getVin();
                setEssid();
            }
            if (car_state.can_failed) {
                //checkCan();
            }
        }
    }
    else if (car_state.ignition) {
        if (!car_state.no_can) {
            touchFile(NoCanFile);
            unlink(CanStatusFile);
            car_state.no_can = true;
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
                int fuel = car_state.fuel_level;
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
            int fuel = car_state.fuel_level;
            if ( (fuel != -1) and (car_state.stopped_fuel_level != -1) and
                 (car_state.odometer != -1) ) {
                int fuel_diff = fuel - car_state.stopped_fuel_level;
                if (fuel_diff > 5) {
                    string event = "CarState";
                    event += " Odom:" + string_printf("%.1f", car_state.odometer);
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
            if (car_state.ignition) {
                // We just stopped
                // Get odometer and fuel level
                car_state.stopped_fuel_level = car_state.fuel_level;
                car_state.stopped_odometer = car_state.odometer;
                car_state.old_seats = 0;
                if (car_state.seatbelts_check_time > 0) {
                    car_state.seatbelts_stopped_time = Uptime - car_state.seatbelts_check_time;
                }
            }
        }
    }
    if (last_alarm != car_state.alarm_state) {
//        logError("Alarm change: "+toString(last_alarm)+"->"+toString(car_state.alarm_state));
        if (car_state.alarm_state == "triggered") {
            logError("Alarm triggered");
            car_state.alarm_report_state = car_state.alarm_val;
            if (car_state.alarm_cause == "") {
                car_state.alarm_report_time = Uptime + 30;
                car_state.alarm_cause = "unknown";
            }
            else {
                car_state.alarm_report_time = Uptime;
            }
        }
        if (car_state.alarm_state == "armed") {
            updateCarCapabilities(0x0002);
        }
    }
    if (last_batval != car_state.batval) {
        string batval = string_printf("%.1f", car_state.batval);
        updateFile(BatteryValFile, batval);
    }
    if ( car_state.seatbelts_enabled and
         car_state.moving ) {
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
//                    logError("Starting seatbelt delay");
                    car_state.seatbelts_delay_time = Uptime + car_state.seatbelts_delay;
                }
                else if (car_state.seatbelts_delay_time > 0) {
                    if ( (Uptime >= car_state.seatbelts_delay_time) or
                         (car_state.seats == car_state.belts) ) {
//                        logError("Sending initial seats status: "+toString(car_state.seats)+"/"+toString(car_state.belts));
                        notifySeatbelts();
                        car_state.seatbelts_delay_time = 0;
                    }
                }
                else if (car_state.seatbelts_check_time > 0) {
                    if (Uptime >= car_state.seatbelts_check_time) {
                        if ( (car_state.last_seats != car_state.seats) or
                             (car_state.last_belts != car_state.belts) ) {
//                            logError("Sending changed seats status: "+toString(car_state.seats)+"/"+toString(car_state.belts));
                            notifySeatbelts();
                        }
                        car_state.seatbelts_check_time = 0;
                    }
                }
                else if ( (car_state.last_seats != car_state.seats) or
                          (car_state.last_belts != car_state.belts) ) {
                    if (car_state.seats == car_state.belts) {
//                        logError("Sending belted seats status: "+toString(car_state.seats)+"/"+toString(car_state.belts));
                        notifySeatbelts();
                        car_state.seatbelts_check_time = 0;
                    }
                    else {
//                        logError("Starting seatbelt change delay");
                        car_state.seatbelts_check_time = Uptime + car_state.seatbelts_delay;
                    }
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

double getFuelUsed() {
    double fuel_used = 0.0;
    string fuel_str = va.sendQuery("total_ul");
    if (fuel_str != "") {
        fuel_str.erase(0,9);
        fuel_used = stringToDouble(fuel_str) / 1000000.;
    }
    return fuel_used;
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
        int sysval = system("pkill -HUP gps_mon");
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
        else if (!cellinfo.provisioned) {
            reason += " NotProvisioned";
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
    if (car_state.low_battery and !ota_info.flashing) {
       keepon = false;
    }
    else if (fileExists(ShutdownFile)) {
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
        if (wait_on_verify != 0) {
            string verify_pid = pgrep("verify_sys");
            if (wait_on_verify == 1) {
                if (verify_pid != "") {
                    logError("verify_sys running");
                    wait_on_verify = 2;
                }
            }
            else {
                if (verify_pid == "") {
                    logError("verify_sys completed");
                    wait_on_verify = 0;
                }
            }
            if (wait_on_verify != 0) {
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
    string pid;
    if (wlan1info.up) {
        needconnection = false;
    }
    else if (cellinfo.dont_connect) {
        needconnection = false;
    }
    else if (!cellinfo.provisioned) {
        needconnection = false;
    }
    else if (fileExists(ShutdownFile)) {
       needconnection = false;
    }
    else if (car_state.low_battery and !cellinfo.events_to_send and !ota_info.flashing) {
        needconnection = false;
    }
    else {
        if (wlan0info.up) {
            bool wlan0 = true;
//            if ( (wlan0info.ign_off_time > 0) and
//                 (wlan0info.max_on_time > 0) and
//                 ((Uptime-wlan0info.ign_off_time) > wlan0info.max_on_time) ) {
            if (wlan0info.ign_off_time > 0) {
                if (!wifi_enabled or
                     ( (wlan0info.max_on_time > 0) and
                       ((Uptime-wlan0info.ign_off_time) > wlan0info.max_on_time) ) ) {
                    wlan0 = false;
                }
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
        if (ota_info.omadm_running) {
            why += " omadm";
            needconnection = true;
        }
        if (ota_info.downloading) {
            why += " ota_downloading";
            needconnection = true;
        }
        if (ota_info.flashing) {
            why += " ota_flashing";
            needconnection = true;
        }
        if (ota_info.start_timer != 0) {
            why += " ota_wait";
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
            int sysval = str_system(string("cp -f ") + TempCellInfoFile + " " + PermCellInfoFile);
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
//kanaan        string wifi = "AR6103";
//kanaan        if (fileExists(Ar6102File)) {
//kanaan            wifi = "AR6102";
//kanaan        }
//        string wifi = "WILINK8";
//kanaan-end
//        str += ":wifi=" + wifi;
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
                int pos;
                while ((pos=cell_version.find(" ")) != string::npos) {
                    cell_version.replace(pos, 1, "_");
                }
                str += ":cellvers=" + cell_version;
            }
        }
        string flashsize = getFlashSize();
        if (flashsize != "") {
            str += ":flash=" + flashsize;
        }

        string bvFile = BuildVarsFile;
        bvFile.append(getStringFromFile(KernelIdFile));
        Assoc bldvars = readBuildVars(bvFile);
        if (bldvars.find("machine") != bldvars.end()) {
            str += ":board=" + bldvars["machine"];
        }
        if (bldvars.find("Distro") != bldvars.end()) {
            str += ":distro=" + bldvars["Distro"];
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
        string mmcblk = backtick("fw_printenv mmcblk | sed 's/mmcblk=//'");
        string cmd = "dd if=/dev/" + mmcblk + " skip=3 count=1000 2>/dev/null";
        cmd += " | strings | grep -i '^u-boot '";
        cmd += " | sed 's/.*(//' | sed 's/ - .*$//'";
        cmd += " | sed 's/ /_/g'";
        string uboot_vers = backtick(cmd);
        str += ":uboot="+uboot_vers;
//        str += ":vic="+vic.build+"-"+vic.version_str;
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
    if (waninfo.shortterm_starttime != "") {
        str += " ShortTermUsage:";
        str += toString(waninfo.shortterm_usage);
    }
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
//    bool pass = false;
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
    int fuel = car_state.fuel_level;
    if (fuel != -1) {
        if (car_state.obd2_unit) {
            str += " Fuel%:" + toString(fuel);
        }
        else {
            str += " Fuel:" + toString(fuel);
        }
//        pass = true;
    }
    //Oil life = xx (percent)
    if (car_state.oil_life != "") {
        str += " Oil:" + car_state.oil_life;
    }
    //Odometer = xxxxxx (km)
    car_state.stopped_odometer = car_state.odometer;
    if (car_state.odometer != -1) {
        str += " Odom:" + string_printf("%.1f", car_state.odometer);
//        pass = true;
    }
    //Tire pressures = xx/xx/xx/xx/xx(xx/xx) (kpa)
    if (car_state.tire_pressures != "") {
        str += " Tires:"+car_state.tire_pressures;
//            pass = true;
    }
//            int seats = buffer[2];
//            int belts = buffer[3];
//            if ((seats != 0xFF) and (belts != 0xFF)) {
//                updateCarCapabilities(0x0001);
//            }
    if (car_state.locks != 0xFF) {
        //Locks = xx (hex)
        str += " Locks:" + toString(car_state.locks);
//            pass = true;
    }
    if (car_state.doors != 0xFF) {
        //Doors = xx (hex)
        str += " Doors:" + toString(car_state.doors);
//        pass = true;
    }
    string batval = string_printf("%.1f", car_state.batval);
    str += " Bat:" + batval;
//    if (!car_state.ignition) {
        str += " IgnOff:" + toString(car_state.ignition_off_time);
//    }
    string eng = "off";
    if (car_state.engine) {
        eng = "on";
    }
    str += " Eng:" + eng;
    if (car_state.alarm_val != 0x07) {
        str += " Alarm:" + toString(car_state.alarm_val);
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
            double fuel_used = getFuelUsed();
            str += " FuelUsed:" + string_printf("%.4f", fuel_used);
            if (fileExists(RapidAccCountFile)) {
                str += " RapidAcc:"+getStringFromFile(RapidAccCountFile);
                unlink(RapidAccCountFile);
            }
            if (fileExists(RapidDecCountFile)) {
                str += " RapidDec:"+getStringFromFile(RapidDecCountFile);
                unlink(RapidDecCountFile);
            }
            if (fileExists(GpsDistanceFile)) {
                str += " GpsDistance:"+getStringFromFile(GpsDistanceFile);
                unlink(GpsDistanceFile);
            }
//            string faults = getFaults();
//            if (faults != "") {
//            }
            car_state.low_fuel = 0;
            car_state.low_fuel_sent = 0;
            car_state.low_fuel_timer = 0;
        }
    }
//    if (!car_state.no_can) {
//        string passfail = "Failed";
//        if (pass) {
//            passfail = "Passed";
//        }
//        updateFile(CanStatusFile, passfail);
//    }
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
        if (resp == "yes") {
            waninfo.exists = true;
//            cellinfo.connected = true;
        }
        else if (resp == "no") {
            cellinfo.connected = false;
        } else {
            // no response from cell talker
            waninfo.exists = true;
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
            if (waninfo.shortterm_starttime != "") {
                waninfo.shortterm_usage += tx_delta + rx_delta;
            }
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
//kanaan    string iwlist_error = backtick("iwlist wlan0 channel 2>&1 >/dev/null");
//kanaan    if (iwlist_error != "") {
//kanaan        resetWlan0("iwlist:"+trim(iwlist_error));
//kanaan    }
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
    if ((backup_autonet_time != 0) and (Uptime >= backup_autonet_time)) {
        backup_autonet_time = 0;
        backupAutonet();
    }
    if (!wlan0info.off) {
        getWlan0();
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
        //timeinfo.last_celltime = 0;
        //timeinfo.celltime_timer = Uptime + 60;
//        if ((timeinfo.state == Close) or (timeinfo.state == Bad)) {
//            setVicRtc();
//        }
        createCertificate();
        timeinfo.state = Good;
    }
    //else if ((timeinfo.state != Good) or (Uptime >= timeinfo.celltime_timer)) {
    if ((timeinfo.state != Good) or (Uptime >= timeinfo.celltime_timer)) {
        getCellTime();
    }
//    if (Uptime > timeinfo.rtc_timer) {
//        setVicRtc();
//    }
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
        getVaStatus(true);
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
            car_state.command_hold = Uptime + CommandHoldTime;
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
//        int sysval = system("autonet_router");
//    }
    getVaStatus(false);
    checkOtaStatus();
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
    if (fileExists(SshControlFile)) {
        time_t sshtime = stringToLong(readFile(SshControlFile));
        if ((sshtime != 0) and (time(NULL) > sshtime)) {
            int sysval = system("pkill /usr/sbin/sshd");
            unlink(SshControlFile);
        }
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
                cellinfo.provisioned = true;
                //int sysval = system("eeprom -a 30 -w 01 FF FF FF FF");
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
        int tzmin;
        int n = sscanf(celltime.c_str(), "%d/%d/%d %d:%d:%f %d",
                       &year, &mon, &mday, &hour, &min, &f_sec, &tzmin);
        if ((n >= 6) and (year > 2000)) {
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
                if (!fileExists(GpsTimeFile)) {
                    time_t now = time(NULL);
                    if (abs(now-t) > 2) {
                        logError("Setting system time from celltime");
                        struct timeval tv;
                        tv.tv_sec = timegm(&tm);
                        tv.tv_usec = usec;
                        settimeofday(&tv, NULL);
                    }
                    timeinfo.ntp_timer = Uptime + 3600;
//                    setVicRtc();
                }
                timeinfo.last_celltime = t;
                createCertificate();
                timeinfo.state = Good;
                timeinfo.celltime_available = true;
                if (n > 6) {
                    int last_tzmin = 1440;
                    if (fileExists(TimezoneOffsetFile)) {
                        last_tzmin = stringToInt(getStringFromFile(TimezoneOffsetFile));
                    }
                    if (last_tzmin != tzmin) {
                        updateFile(TimezoneOffsetFile, toString(tzmin));
                    }
                }
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

//    string eth_resp = backtick("ethtool eth0");
//    if (eth_resp.find("Link detected: yes") != string::npos) {
//        string speed = getParm(eth_resp, "Speed: ", " \n");
//        string duplex = getParm(eth_resp, "Duplex: ", " \n");
//        eth0info.state = speed + "-" + duplex;
//        eth0info.up = true;
//    }
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
        int wlan0_users = getWlan0Users();
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
//kanaan
//rcg    int sysval = str_system(string("iw dev wlan0 station dump | grep Station | sed 's/Station //' | sed 's/ .*$//' >")+StationListFile+".tmp");
//rcg    int sysval = str_system(string("mv ")+StationListFile+".tmp "+StationListFile);
//
    if (fileExists(StationListFile)) {
        Strings lines = readFileArray(StationListFile);
        users = lines.size();
    }
    return users;
}

void setWifiDefaults() {
    string hostname = getStringFromFile(UnitIdFile);
    string last4 = hostname.substr(hostname.size()-4);
    if (car_state.vin != "") {
        string vin = car_state.vin;
        last4 = vin.substr(vin.size()-4);
    }
    string essid = backtick("getbranding essid") + "-" + last4;
    int sysval = str_system("autonetconfig essid "+essid+" enc_type off");
    string file = WifiPasswdFile;
    string tmpfile = file+".tmp";
    sysval = str_system("sed '/true:false/s/^[^:]*:[^:]*/autonet:$md5/' "+file+" >"+tmpfile);
    rename(tmpfile.c_str(), file.c_str());
}

void readGpsPosition() {
    if (fileExists(GpsPresentFile) and !car_state.location_disabled) {
        if (fileExists(GpsPositionFile)) {
            gpsinfo.position = getStringFromFile(GpsPositionFile);
        }
    }
}

int readGpsSpeed() {
    int kph = -1;
    if (fileExists(GpsSpeedFile)) {
        int mph = stringToInt(getStringFromFile(GpsSpeedFile));
        kph = (int)(mph * 1.60934 + 0.5);
    }
    return kph;
}

void createCertificate() {
    if (timeinfo.state != Good) {
        int sysval = system("/etc/init.d/S12ipsec stop");
        sleep(1);
        sysval = system("/etc/init.d/S12ipsec start");
    }
}

void checkOtaStatus() {
    string pid = "";
    if (ota_info.enabled) {
        pid = backtick("pgrep omadm_client");
        ota_info.omadm_running = false;
        if (pid != "") {
            ota_info.omadm_running = true;
        }
        ota_info.downloading = false;
        pid = pgrep("ota_downloadMgr");
        if (pid != "") {
            ota_info.downloading = true;
        }
        pid = pgrep("ota_installMgr");
        ota_info.flashing = false;
        if (pid != "") {
            ota_info.flashing = true;
        }
        if ((ota_info.start_timer > 0) and (Uptime > ota_info.start_timer)) {
            ota_info.start_timer = 0;
        }
        if ((ota_info.ota_timer > 0) and (Uptime > ota_info.ota_timer)) {
            if (fileExists(OtaDownloadWaitingFile)) {
                int sysval = system("ota_downloadMgr &");
            }
            if (fileExists(OtaInstallWaitingFile)) {
                int sysval = system("ota_installMgr &");
            }
            ota_info.ota_timer = Uptime + 60;
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
        getVaStatus(false);
//        openVic();
//        if (vic.getSocket() != -1) {
//            vic.sendRequest(app_id, 0x20);
//            car_state.last_check_time = Uptime;
//        }
        if (car_state.sms_blocking) {
            int bt_kph = 0;
            if (car_state.no_can) {
                bt_kph = readGpsSpeed();
                if (bt_kph < 5) {
                    bt_kph = 0;
                }
            }
            else {
                bt_kph = car_state.speed;
            }
            if (bt_kph > 254) {
                bt_kph = 254;
            }
            else if (bt_kph == -1) {
                bt_kph = 0;
            }
            if (car_state.no_can) {
                if ((!car_state.ignition) and (bt_kph == 0)) {
                    bt_kph = 0xFF;
                }
            }
            else {
                if ((!car_state.engine) and (bt_kph == 0)) {
                    bt_kph = 0xFF;
                }
            }
            char ef = 'E';
            string ef_trailer = "Z0000 E";
            if (car_state.bluetooth_flag) {
               ef = 'F';
               ef_trailer = "00000 F";
            }
            car_state.bluetooth_flag = !car_state.bluetooth_flag;
            string bt_name = string_printf("cellcon%c%02X001%s",ef,bt_kph,ef_trailer.c_str());
//            int sysval = str_system("hciconfig hci0 name '"+bt_name+"' piscan");
            int sysval = str_system("hciconfig hci0 name '"+bt_name+"'");
//            logError("Setting BT name: "+bt_name);
        }
        car_state.last_check_time = Uptime;
    }
    else if (car_state.get_quickscan) {
        string tmp_ecusfile = "/tmp/ecus";
        unlink(tmp_ecusfile.c_str());
        string ecus_str = va.sendQuery("ecus");
        if (ecus_str != "") {
            ecus_str.erase(0,5);
            Strings ecus = split(ecus_str, ",;");
            for (int idx=0; idx < ecus.size(); idx++) {
                string ecu = ecus[idx];
                string short_ecu = ecu.substr(0,40);
                string data = string_printf(" ECU:%d:", idx);
                car_state.quickscan_str += data + short_ecu;
                Byte buffer[25];
                for (int i=0; i < ecu.size()/2; i++) {
                    string ch2 = ecu.substr(i*2, 2);
                    int byte;
                    sscanf(ch2.c_str(), "%02X", &byte);
                    buffer[i] = byte;
                }
                int hwl1 = buffer[0];
                int hwl2 = buffer[1];
                int hwl3 = buffer[2];
                int swl1 = buffer[3];
                int swl2 = buffer[4];
                int swl3 = buffer[5];
                char partnum[21];
                for (int i=0; i < 10; i++) {
                    partnum[i] = buffer[6+i];
                }
                partnum[10] = '\0';
                bool ok = true;
                for (int i=0; i < strlen(partnum); i++) {
                    char ch = partnum[i];
                    if (!isalnum(ch) and (ch != '_') and (ch != '-')) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) {
                    for (int i=0; i < 10; i++) {
                        sprintf(&partnum[i*2], "%02X", buffer[9+i]);
                    }
                }
                int origin = buffer[16];
                int supplier_id = buffer[17];
                int varient_id = buffer[18];
                int diagnostic_version = buffer[19];
                string type = "unknown";
                int addr_info = 0;
                int req_id = 0;
                int res_id = 0;
                if (ecu.size() > 40) {
                    addr_info = buffer[20];
                    req_id = (buffer[21] << 8) | buffer[22];
                    res_id = (buffer[23] << 8) | buffer[24];
                    int chan = (addr_info & 0x80) >> 7;
                    int proto_enum = (addr_info & 0x30) >> 4;
                    int addr_enum = addr_info & 0x03;
                    string prot = "Unknown";
                    if (proto_enum == 0) {
                        prot = "UDS";
                    }
                    else if (proto_enum == 1) {
                        prot = "KWP";
                    }
                    else if (proto_enum == 2) {
                        prot = "OBDII";
                    }
                    string addr = "Unkown";
                    if (addr_enum == 1) {
                        addr = "11";
                    }
                    else if (addr_enum == 3) {
                        addr = "29";
                    }
                    string grep_str = string_printf("0x%04X,0x%04X,RDTS_%sBIT_ADDR,%d,RDTS_%s",req_id,res_id,addr.c_str(),chan,prot.c_str());
                    string line = grepFile(EcuTypesFile, grep_str);
                    if (line != "") {
                        Strings vals = csvSplit(line, ",");
                        type = csvUnquote(vals[0]);
                    }
                }
                string cnt = backtick(string("grep \",")+partnum+",\" "+tmp_ecusfile+" | wc -l");
                string ext = "";
                if (cnt != "0") {
                    ext = string_printf(",-%d", stringToInt(cnt)+1);
                }
                string ecu_line = string_printf("%s,%d,%d,%d,%s,%d.%d.%d,%d.%d.%d,%d,%d,%d,%d%s",type.c_str(),addr_info,req_id,res_id,partnum,swl1,swl2,swl3,hwl1,hwl2,hwl3,origin,supplier_id,diagnostic_version,varient_id,ext.c_str());
                appendToFile(tmp_ecusfile,ecu_line);
            }
            string event = "CarState";
            event += car_state.quickscan_str;
            logError("Quickscan event: "+event);
            eventlog.append(event);
        }
        car_state.get_quickscan = false;
    }
//        buffer[0] = car_state.quickscan_idx;
//        int len = getVicResponse(0x2E, 1, buffer);
//        if (len > 3) {
//            int hwl1 = buffer[3];
//            int hwl2 = buffer[4];
//            int hwl3 = buffer[5];
//            int swl1 = buffer[6];
//            int swl2 = buffer[7];
//            int swl3 = buffer[8];
//            char partnum[21];
//            for (int i=0; i < 10; i++) {
//                partnum[i] = buffer[9+i];
//            }
//            partnum[10] = '\0';
//            bool ok = true;
//            for (int i=0; i < strlen(partnum); i++) {
//                char ch = partnum[i];
//                if (!isalnum(ch) and (ch != '_') and (ch != '-')) {
//                    ok = false;
//                    break;
//                }
//            }
//            if (!ok) {
//                for (int i=0; i < 10; i++) {
//                    sprintf(&partnum[i*2], "%02X", buffer[9+i]);
//                }
//            }
//            int origin = buffer[19];
//            int supplier_id = buffer[20];
//            int varient_id = buffer[21];
//            int diagnostic_version = buffer[22];
//            string type = "unknown";
//            int addr_info = 0;
//            int req_id = 0;
//            int res_id = 0;
//            if (len > 23) {
//                addr_info = buffer[23];
//                req_id = (buffer[24] << 8) | buffer[25];
//                res_id = (buffer[26] << 8) | buffer[27];
//                int chan = (addr_info & 0x80) >> 7;
//                int proto_enum = (addr_info & 0x30) >> 4;
//                int addr_enum = addr_info & 0x03;
//                string prot = "Unknown";
//                if (proto_enum == 0) {
//                    prot = "UDS";
//                }
//                else if (proto_enum == 1) {
//                    prot = "KWP";
//                }
//                else if (proto_enum == 2) {
//                    prot = "OBDII";
//                }
//                string addr = "Unkown";
//                if (addr_enum == 1) {
//                    addr = "11";
//                }
//                else if (addr_enum == 3) {
//                    addr = "29";
//                }
//                string grep_str = string_printf("0x%04X,0x%04X,RDTS_%sBIT_ADDR,%d,RDTS_%s",req_id,res_id,addr.c_str(),chan,prot.c_str());
//                string line = grepFile(EcuTypesFile, grep_str);
//                if (line != "") {
//                    Strings vals = csvSplit(line, ",");
//                    type = csvUnquote(vals[0]);
//                }
//            }
//            string cnt = backtick(string("grep \",")+partnum+",\" "+tmp_ecusfile+" | wc -l");
//            string ext = "";
//            if (cnt != "0") {
//                ext = string_printf(",-%d", stringToInt(cnt)+1);
//            }
//            string ecu_line = string_printf("%s,%d,%d,%d,%s,%d.%d.%d,%d.%d.%d,%d,%d,%d,%d%s",type.c_str(),addr_info,req_id,res_id,partnum,swl1,swl2,swl3,hwl1,hwl2,hwl3,origin,supplier_id,diagnostic_version,varient_id,ext.c_str());
//            appendToFile(tmp_ecusfile,ecu_line);
//            string data = string_printf(" ECU:%d:", car_state.quickscan_idx);
//            for (int i=0; i < 20; i++) {
//                 data += string_printf("%02X", buffer[i+3]);
//            }
//            if (type != "unknown") {
//                data += ":" + type;
//            }
//            car_state.quickscan_str += data;
//            car_state.quickscan_cnt++;
//            car_state.quickscan_idx++;
//            if ( (car_state.quickscan_idx >= car_state.quickscan_ecus) or
//                 (car_state.quickscan_cnt >= 100)) {
//                string event = "CarState";
//                event += car_state.quickscan_str;
//                logError("Quickscan event: "+event);
//                eventlog.append(event);
//                car_state.quickscan_str = "";
//                car_state.quickscan_cnt = 0;
//            }
//            if (car_state.quickscan_idx >= car_state.quickscan_ecus) {
//                bool ecus_changed = true;
//                if (fileExists(EcusFile)) {
//                    if (str_system("cmp "+tmp_ecusfile+" "+EcusFile) == 0) {
//                        ecus_changed = false;
//                    }
//                }
//                if (ecus_changed) {
//                    int sysval = str_system("cp "+tmp_ecusfile+" "+EcusFile);
//                    if (ota_info.enabled) {
//                        touchFile(EcusChangeFile);
//                        startOmaDm();
//                    }
//                }
//                car_state.get_quickscan = false;
//            }
//        }
//    }
}

void notifySeatbelts() {
    string event = "CarState";
    event += " Seats:"+toString(car_state.seats)+"/"+toString(car_state.belts);
    event += eventGpsPosition();
    eventlog.append(event);
    logError(event);
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
    if ((cause == 0x1C) or (cause == 0x100)) {
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
    string cause = car_state.alarm_cause;
    if (cause != "") {
        reason = cause;
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
    car_state.alarm_cause = "";
}

void getVin() {
    string vin = "";
    string old_vin = "";
    if (fileExists(VinFile)) {
        old_vin = getStringFromFile(VinFile);
    }
    vin = va.sendQuery("vin");
    if (vin != "") {
        vin.erase(0,4);
        if (vin == "ABCDEF1234GHIJK") {
            string hostname = getStringFromFile(UnitIdFile);
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
    updateFile(CurrentVinFile, vin);
    if (vin == "") {
        vin = old_vin;
    }
    car_state.vin = vin;
}

void checkCan() {
    bool pass = false;
    if (car_state.no_can) {
        car_state.can_failed = false;
        return;
    }
    else {
        // Check VIN
        string vin = car_state.vin;
        if (vin == "ABCDEF1234GHIJK") {
        }
        else if (vin == "AUT0NETDUMMY00000") {
            pass = true;
        }
        else {
            pass = true;
        }
        // Check fuel level
        if (car_state.fuel_level != -1) {
            pass = true;
        }
        // Check oil life
        if (car_state.oil_life != "") {
            pass = true;
        }
        // Check odometer
        if (car_state.odometer != -1.) {
            pass = true;
        }
        // Check tire pressure
        if (car_state.tire_pressures != "") {
            pass = true;
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
    tm->tm_year = 2000;
    tm->tm_mon = 1;
    tm->tm_mday = 1;
    tm->tm_hour = 0;
    tm->tm_min = 0;
    tm->tm_sec = 0;
//    if (vicEnabled) {
//       openVic();
//       vic.getRtc(app_id, tm);
//    }
}

//void setVicRtc() {
//    if (vicEnabled) {
//       struct tm tm;
//       getVicRtc(&tm);
//       tm.tm_year -= 1900;
//       tm.tm_mon -= 1;
//       time_t vic_time = timegm(&tm);
//       if (abs(vic_time - time(NULL)) > 2) {
//           logError("Setting VIC RTC");
//           vic.setRtc(app_id);
//           sleep(1);
//       }
//       timeinfo.rtc_timer = Uptime + 3600;
//    }
//}

//void setVicWatchdog() {
//    Byte buffer[16];
//    int watchdog = VicWatchdogTime;
//    string watchdog_s = features.getFeature(VicWatchdog);
//    if (watchdog_s != "") {
//        watchdog = stringToInt(watchdog_s);
//    }
//    buffer[0] = watchdog;
//    getVicResponse(0x07, 1, buffer);
//}

//void setVicQuickScanTimes() {
//    Byte buffer[16];
//    buffer[0] = VicQuickScanTime >> 8;
//    buffer[1] = VicQuickScanTime & 0xFF;
//    buffer[2] = VicQuickScanIgnDelay >> 8;
//    buffer[3] = VicQuickScanIgnDelay & 0xFF;
//    buffer[4] = VicQuickScanToolDisconnectTime >> 8;
//    buffer[5] = VicQuickScanToolDisconnectTime & 0xFF;
//    buffer[6] = VICQuickScanConfigChangeTime >> 8;
//    buffer[7] = VICQuickScanConfigChangeTime & 0xFF;
//    getVicResponse(0x0C, 8, buffer);
//}

void getVaStatus(bool getlast) {
    string VaQuery = "ignition,engine,locks,doors,trunk,seats,seatbelts,odometer,speed,fuel,low_fuel_indicator,check_engine_indicator,oil_life,tire_pressures,alarm,alarm_cause,can_a,can_b";
    if (getlast) {
        VaQuery = "last:," + VaQuery;
    }
    string resp = va.sendQuery(VaQuery);
    if (resp != "") {
        handleVaStatus(resp);
    }
    else {
        logError("Got no response from VA");
    }
//    else {
//        updateFile(VicStatusFile, "Failed");
//    }
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
    logError("Checking for fault codes");
    string tmp_faultsfile = string(FaultsFile) + ".tmp";
    unlink(tmp_faultsfile.c_str());
    string new_faults_str = "";
    string faults_str = va.sendQuery("faults");
    bool got_faults = false;
    if (faults_str != "") {
        faults_str.erase(0,7);
        got_faults = true;
        if (faults_str != "none") {
            Strings faults = split(faults_str, ",;");
            for (int i=0; i < faults.size(); i++) {
                string fault_str = faults[i];
                if (i != 0) {
                    new_faults_str += ",";
                }
                new_faults_str += fault_str;
                int fault, status;
                int n = sscanf(fault_str.c_str(), "%x.%x", &fault, &status);
                int type = fault >> 8;
                int l;
                if ((status&0x0F) != 0) {
                    l = 3;
                    type = (type >> 16) & 0xC0;
                }
                else {
                    l = 2;
                    type >>= 8;
                    type = (type >> 8) & 0xC0;
                }
                char code = ' ';
                if (type == 0x00) {
                    code = 'P';
                }
                else if (type == 0x40) {
                    code = 'C';
                }
                else if (type == 0x80) {
                    code = 'B';
                }
                else {
                    code = 'U';
                }
                string decfault = "";
                if (l == 2) {
                    decfault = string_printf("%c%04X", code, fault&0x3FFF);
                }
                else {
                    decfault = string_printf("%c%04X-%02X", code, (fault>>8)&0x3FFF, fault&0xFF);
                }
                string stattype = "";
                if (status & 0x0F) {
                    if (status & 0x01) {
                        stattype = "Active";
                    }
                    else if (status & 0x04) {
                        stattype = "Pending";
                    }
                    else if (status & 0x08) {
                        stattype = "Stored";
                    }
                }
                else {
                    int tval = status & 0x60;
                    if (tval == 0x20) {
                        stattype = "Stored";
                    }
                    else if (tval == 0x40) {
                        stattype = "Pending";
                    }
                    else if (tval == 0x60) {
                        stattype = "Active";
                    }
                }
                decfault += "," + stattype;
                appendToFile(tmp_faultsfile, decfault);
            }
        }
    }

//    while (true) {
//        buffer[0] = 0x00;
//        buffer[1] = idx;
//        int len = getVicResponse(0x29, 2, buffer);
//        Byte *p = buffer + 4;
//        if ( (len < 5) or (*p == 0xFF) ) {
////        int len = getVicResponse(0x29, 1, buffer);
////        Byte *p = buffer + 3;
////        if ( (len < 4) or (*p == 0xFF) ) {
//            got_faults = false;
//            break;
//        }
//        got_faults = true;
//        int num_faults = *(p++);
//        if (num_faults == 0) {
//            break;
//        }
//        for (int i=0; i < num_faults; i++) {
//            if (faults_str != "") {
//                faults_str += ",";
//            }
//            unsigned long fault = 0;
//            for (int j=0; j < 3; j++) {
//                fault = (fault<<8) | *p;
//                len--;
//                p++;
//            }
//            int status = 0x00;
////            if (car_state.vic_version > "000.027.000") {
//            if (vic.protocol_version >= 0x0217 ) {
//                status = *p;
//                len--;
//                p++;
//            }
//            string fault_str = "";
//            int type = fault >> 16;
//            int l = 3;
//            if ((status&0x0F) == 0) {
//                l = 2;
//                fault >>= 8;
//            }
//            type &= 0xC0;
//            char code = ' ';
//            if (type == 0x00) {
//                code = 'P';
//            }
//            else if (type == 0x40) {
//                code = 'C';
//            }
//            else if (type == 0x80) {
//                code = 'B';
//            }
//            else {
//                code = 'U';
//            }
//            if (l == 2) {
//                faults_str += string_printf("%04X", fault);
//                fault_str = string_printf("%c%04X", code, fault&0x3FFF);
//            }
//            else {
//                faults_str += string_printf("%06X", fault);
//                fault_str = string_printf("%c%04X-%02X", code, (fault>>8)&0x3FFF, fault&0xFF);
//            }
////            if (car_state.vic_version > "000.027.000") {
//            if (vic.protocol_version >= 0x0217) {
//                faults_str += string_printf(".%02X", status);
//                string stattype = "";
//                if (status & 0x0F) {
//                    if (status & 0x01) {
//                        stattype = "Active";
//                    }
//                    else if (status & 0x04) {
//                        stattype = "Pending";
//                    }
//                    else if (status & 0x08) {
//                        stattype = "Stored";
//                    }
//                }
//                else {
//                    int tval = status & 0x60;
//                    if (tval == 0x20) {
//                        stattype = "Stored";
//                    }
//                    else if (tval == 0x40) {
//                        stattype = "Pending";
//                    }
//                    else if (tval == 0x40) {
//                        stattype = "Active";
//                    }
//                }
//                fault_str += "," + stattype;
//            }
//            appendToFile(tmp_faultsfile, fault_str);
//        }
//        if (num_faults < 10) {
//            break;
//        }
//        idx += 10;
//    }
    if (got_faults) {
        if (new_faults_str == "") {
            new_faults_str = "none";
            unlink(FaultsFile);
        }
        else {
            rename(tmp_faultsfile.c_str(), FaultsFile);
        }
    }
    return new_faults_str;
}

void getCellProfile() {
 /*   string profile = getCellResponse("getprofile");
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
        int pos = 0;*/
    string imsi = getCellResponse("./data/modem_client getimsi");
    if (imsi != "") {
    	logError("profile:"+imsi);
     	cellinfo.imsi = getParm(imsi , "ok ", ",");
    }
    string imei = getCellResponse("./data/modem_client getimei");
    if (imei != "") {
    	logError("imei:"+imei);
     	cellinfo.imei = getParm(imei , "ok ", ",");
    }
    string iccid = getCellResponse("./data/modem_client geticcid");
    if (iccid != "") {
    	logError("iccid:"+iccid);
     	cellinfo.iccid = getParm(iccid , "ok ", ",");
    }
    string mdn = getCellResponse("./data/modem_client getmdn");
    if (mdn != "") {
    	logError("mdn:"+mdn);
     	cellinfo.iccid = getParm(mdn , "ok ", ",");
    }
    string provider = getCellResponse("./data/modem_client getoperator");
    if (mdn != "") {
    	logError("provider:"+);
     	cellinfo.provider = getParm(provide , "ok ", ",");
    }
 //   while ((pos=provider.find(" ")) != string::npos) {
 //         provider.replace(pos,1,"_");
 //    }
 // //      cellinfo.provider = provider;
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
    int sysval = system("cell_shell activate >/tmp/activate.out &");
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
    if (fileExists(NoCellCommandsFile) or cellinfo.no_commands) {
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
            if (!(cellinfo.error_flags & 0x0001)) {
                logError("Attempting to connect to cell_talker");
            }
            int sock = -1;
            if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
                if (!(cellinfo.error_flags & 0x0001)) {
                    logError("Could not open cell talker socket");
                }
                cellinfo.error_flags |= 0x0001;
            }
            else {
                struct sockaddr_un a;
                memset(&a, 0, sizeof(a));
                a.sun_family = AF_UNIX;
                strcpy(a.sun_path, CellTalkerPath);
                alarm(5);
                monitor_flags |= IN_CELLCONNECT;
                if (connect(sock, (struct sockaddr *)&a, sizeof(a)) < 0) {
                    if (!(cellinfo.error_flags & 0x0002)) {
                        logError("Could not connect to cell talker socket");
                    }
                    cellinfo.error_flags |= 0x0002;
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
                        if (!(cellinfo.error_flags & 0x0004)) {
                            eventlog.append("CellTalker connect blocked...killing celltalker");
                            logError("CellTalker connect blocked...killing "+talker);
                            cellinfo.error_flags |= 0x0004;
                            killCellTalker();
                        }
                        else if (!(cellinfo.error_flags & 0x0008)) {
                            eventlog.append("CellTalker connect still blocked");
                            eventlog.append("CellTalker connect still blocked");
                            cellinfo.error_flags |= 0x0008;
                        }
                        return resp;
                    }
                }
            }
            cellinfo.socket = sock;
        }
        else {
            if (!(cellinfo.error_flags & 0x0010)) {
                logError("Cell talker socket does not exist");
                cellinfo.error_flags |= 0x0010;
            }
            return resp;
        }
        if ((cellinfo.socket < 0) and !fileExists(CellModemDevice)) {
            if (cellinfo.fail_time != 0) {
                if ((Uptime-cellinfo.fail_time) > 20) {
                    if (!(cellinfo.error_flags & 0x0020)) {
                        cellinfo.error_flags |= 0x0020;
                        logError("Cell module failed...resetting");
                        eventlog.append("CellModule failed...reseting");
                        cellinfo.fail_time = 0;
                        turnOffOnCell();
                    }
                    else if (!(cellinfo.error_flags & 0x0040)) {
                        cellinfo.error_flags |= 0x0040;
                        logError("Cell module failed again");
                        eventlog.append("CellModule failed again");
                    }
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
                if (!(cellinfo.error_flags & 0x0080)) {
                    cellinfo.error_flags |= 0x0080;
                    eventlog.append("CellTalker send blocked...killing celltalker");
                    logError("CellTalker send blocked...killing "+talker);
                    killCellTalker();
                }
                else if (!(cellinfo.error_flags & 0x0100)) {
                    cellinfo.error_flags |= 0x0100;
                    eventlog.append("CellTalker send still blocked");
                    logError("CellTalker send still blocked");
                }
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
                                if (!(cellinfo.error_flags & 0x0200)) {
                                    cellinfo.error_flags |= 0x0200;
                                    eventlog.append("CellTalker recv blocked...killing celltalker");
                                    logError("CellTalker recv blocked...killing "+talker);
                                    killCellTalker();
                                }
                                else if (!(cellinfo.error_flags & 0x0400)) {
                                    cellinfo.error_flags |= 0x0400;
                                    eventlog.append("CellTalker recvd still blocked");
                                    logError("CellTalker recv still blocked");
                                }
                                break;
                            }
                        }
                    }
                }
                else if (nread == 0) {
                    if (!(cellinfo.error_flags & 0x0800)) {
                        cellinfo.error_flags |= 0x0800;
                        logError("Nothing read from cell_socket");
                    }
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
            if (!(cellinfo.error_flags & 0x1000)) {
                cellinfo.error_flags |= 0x2000;
                logError("Got error from cell talker... closing socket");
                close(cellinfo.socket);
                cellinfo.socket = -1;
                cellinfo.fail_time = Uptime;
                if (!cellinfo.testing_keepup) {
                    eventlog.append("CellModule lockup...resetting");
                    turnOffOnCell();
                }
            }
            else if (!(cellinfo.error_flags & 0x4000)) {
                cellinfo.error_flags |= 0x4000;
                logError("Got 2nd error from cell talker");
                eventlog.append("Got 2nd error from cell talker");
            }
        }
        resp = "";
    }
    else {
        cellinfo.error_count = 0;
        cellinfo.error_flags = 0;
    }
    monitor_flags &= ~IN_CELLWAIT;
    return resp;
}

void killCellTalker() {
    alarm(2);
    close(cellinfo.socket);
    alarm(0);
    cellinfo.socket = -1;
    int sysval = str_system("pkill "+cellinfo.talker);
    sleep(1);
}

bool isOptionModule() {
    bool retval = false;
    if (cellinfo.model == "GTM609") {
        retval = true;
    }
    return retval;
}

bool isTelitModule() {
    bool retval = false;
    if ( (cellinfo.model == "DE910-DUAL") or
         (cellinfo.model == "CE910-DUAL") or
         (beginsWith(cellinfo.model, "LE910")) ) {
        retval = true;
    }
    return retval;
}

bool turnOnCell() {
    bool retval = true;
    if (!fileExists(CellTalkerPath)) {
        if (isOptionModule()) {
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
        else if (isTelitModule()) {
            logError("Pulsing Telit ON/OFF");
            cellPulseOnKey();
            sleep(10);
            if (!fileExists(CellTalkerPath)) {
                logError("Pulsing Telit ON/OFF again");
                cellPulseOnKey();
                sleep(10);
                if (!fileExists(CellTalkerPath)) {
                    retval = false;
                }
            }
        }
    }
    return retval;
}

void cellPulseOnKey() {
//    Byte buffer[64];
//    buffer[0] = 2;
//    int len = getVicResponse(0x05, 1, buffer);
}

void turnOffOnCell() {
    if (isOptionModule()) {
        cellPulseOnKey();
        sleep(3);
        cellPulseOnKey();
    }
    else if (isTelitModule()) {
        cellPulseOnKey();
        sleep(15);
        cellPulseOnKey();
        sleep(15);
    }
}

void resetCellHdw() {
//    Byte buffer[64];
//    buffer[0] = 1;
//    int len = getVicResponse(0x05, 1, buffer);
//    sleep(1);
//    turnOnCell();
}

void resetCell(string reason, int state) {
    if (fileExists("/autonet/admin/no_cell_reset")) {
        return;
    }
    if (state > cellinfo.reset_state) {
        cellinfo.reset_state = state;
    }
    if (isTelitModule()) {
        if (cellinfo.reset_state == 0) {
            string event = "Connection failure:"+reason+"...off/on";
            eventlog.append(event);
            turnOffOnCell();
            cellinfo.reset_state = 1;
        }
        else if (cellinfo.reset_state == 1) {
            string event = "Connection failure:"+reason+"...soft reset";
            eventlog.append(event);
            getCellResponse("reset", 20);
            cellinfo.reset_state = 2;
        }
        else if (cellinfo.reset_state == 2) {
            string event = "Connection failure:"+reason+"...soft reset1";
            eventlog.append(event);
            getCellResponse("reset1", 20);
            cellinfo.reset_state = 3;
        }
        else if (cellinfo.reset_state == 3) {
            string event = "Connection failure:"+reason+"...hard reset";
            eventlog.append(event);
            resetCellHdw();
            cellinfo.reset_state = 4;
        }
    }
    else {
        string event = "Connection failure:"+reason+"...soft reset";
        eventlog.append(event);
        getCellResponse("reset");
    }
}

void checkFeaturesChange() {
    if (features.checkFeatures()) {
        if ( features.featureChanged(FindMyCarFeature) ) {
            getPositionReportInterval();
        }
        if ( features.featureChanged(PwrCntrl) or
             features.featureChanged(PwrSched) ) {
            getPowerControlSettings(false);
        }
        if ( features.featureChanged(RateLimits)) {
            int sysval = system("ratelimit");
        }
//        if ( features.featureChanged(VicWatchdog)) {
//            setVicWatchdog();
//        }
        if ( features.featureChanged(WifiFeature) or
             features.featureChanged(RegFeature) ) {
            getWifiEnable();
        }
        if ( features.featureChanged(DisableLocationFeature) ) {
            getDisableLocation();
        }
        if ( features.featureChanged(ValetFeature) ) {
            getValetFeature();
        }
        if ( features.featureChanged(FleetFeature) ) {
            getFleetFeature();
        }
        if ( features.featureChanged(SeatbeltsFeature) ) {
            getSeatbeltsFeature();
        }
        if ( features.featureChanged(SmsBlockingFeature) ) {
            getSmsBlockingFeature();
        }
    }
}

void getWifiEnable() {
//    string enable_str = features.getFeature(WifiFeature);
    bool was_enabled = wifi_enabled;
    bool was_registered = registered;
    wifi_enabled = features.isEnabled(WifiFeature);
    string wifi_data = features.getData(WifiFeature);
    if (wifi_data != "") {
        Strings data_vals = split(wifi_data, ",");
        string starttime = data_vals[0];
        if (fileExists(ShortTermStarttimeFile)) {
             waninfo.shortterm_starttime = getStringFromFile(ShortTermStarttimeFile);
//             logError("Got shortterm starttime: '"+waninfo.shortterm_starttime+"'");
        }
        if (starttime != waninfo.shortterm_starttime) {
//            logError("shortterm starttime changed: "+waninfo.shortterm_starttime+" -> "+starttime);
            waninfo.shortterm_starttime = starttime;
            waninfo.shortterm_usage = 0;
            updateMonthlyUsage();
            updateFile(ShortTermStarttimeFile, starttime);
        }
    }
    else {
        if (waninfo.shortterm_starttime != "") {
            waninfo.shortterm_starttime = "";
            waninfo.shortterm_usage = 0;
            unlink(TempShortTermUsageFile);
            unlink(ShortTermUsageFile);
            unlink(ShortTermStarttimeFile);
        }
    }
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
        //int sysval = system("autonet_router");
    }
}

void getWifiMaxOnTime() {
    wlan0info.max_on_time = 1800;
    string maxstr = config.getString("monitor.WifiMaxOnTime");
    if (maxstr != "") {
        wlan0info.max_on_time = stringToInt(maxstr) * 60;
    }
}

void getPositionReportInterval() {
    string intval_str = config.getString("monitor.PosRptInt");
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
    string hold_str = config.getString("monitor.CellHoldTime");
    if (hold_str != "") {
        car_state.cell_hold_time = stringToInt(hold_str);
    }
//    int len = car_state.getPowerScheduleLength();
//    Byte msg[64];
//    int state0_time = car_state.getIgnitionOffTime(0);
//    msg[0] = car_state.cell_hold_time;
//    msg[1] = (Byte)(state0_time >> 8);
//    msg[2] = (Byte)(state0_time & 0xFF);
//    msg[3] = (Byte)(len - 1);
//    int msg_len = 4;
//    Byte *p = &msg[4];
//    for (int i=1; i < len; i++) {
//        int state_time = car_state.getIgnitionOffTime(i);
//        *p++ = (Byte)(state_time >> 8);
//        *p++ = (Byte)(state_time & 0xFF);
//        int cc_time = car_state.getCycleCellTime(i);
//        *p++ = (Byte)(cc_time);
//        msg_len += 3;
//    }
//    getVicResponse(0x03, msg_len, msg);
//    msg_len = 2;
//    msg[0] = (int)((car_state.lowbatval-car_state.batadjust)*10+.5);
//    msg[1] = (int)((car_state.critbatval-car_state.batadjust)*10+.5);
//    if (vic.protocol_version >= 0x0222) {
//        msg[2] = car_state.lowbat_polltime;
//        msg_len = 3;
//    }
//    getVicResponse(0x0B, msg_len, msg);
}

void getCarKeySettings() {
}

void getAntennaSettings() {
    string settings = config.getString("monitor.AntennaSettings");
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
    //setAntenna(val);
    antenna_setting = val;
    if (last_gps_ext != gps_ext) {
        sleep(1);
        clearGpsReceiver();
        updateFile(LastGpsAntStateFile, toString(gps_ext));
    }
}

//void setAntenna(int val) {
//    Byte buffer[16];
//    buffer[0] = val;
//    getVicResponse(0x09, 1, buffer);
//}

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

void getFleetFeature() {
    gpsinfo.fleet_enabled = false;
    if (features.isEnabled(FleetFeature)) {
        gpsinfo.fleet_enabled = true;
    }
}
void getSmsBlockingFeature() {
     bool last_sms = car_state.sms_blocking;
     car_state.sms_blocking = false;
     if (features.isEnabled(SmsBlockingFeature)) {
         car_state.sms_blocking = true;
     }
     if (car_state.sms_blocking != last_sms) {
         if (car_state.sms_blocking) {
             int sysval = system("hciconfig hci0 piscan >/dev/null 2>&1");
         }
         else {
             int sysval = system("hciconfig hci0 noscan >/dev/null 2>&1");
         }
     }
}

void getProvisionedConfig() {
    string prov_f = config.getString("monitor.Provisioned");
    if (prov_f != "") {
        bool prov = true;
        if (prov_f == "false") {
            prov = false;
        }
        if (prov != cellinfo.provisioned) {
            if (prov) {
                cellinfo.provisioned = true;
                cellinfo.activate = false;
                //int sysval = system("eeprom -a 30 -w 01 FF FF FF FF");
            }
            else {
                setWifiDefaults();
                //int sysval = system("eeprom -a 30 -w 02 FF FF FF FF");
            }
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
        int sysval = system("pkill -9 pppd");
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
        waninfo.child_pid = pid;
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
    int sysval = str_system(string("cat ") + src + " >>" + dst + " 2>/dev/null");
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
        int sysval = system("pkill unitcomm");
    }
    comminfo.state = COMM_STOPPED;
}

void sendStopToUnitcomm() {
//    kill(comminfo.pid, SIGHUP);
    int sysval = system("pkill -HUP unitcomm");
    comminfo.state = COMM_STOPPING;
}

void restartUnitcomm() {
//    kill(comminfo.pid, SIGUSR1);
    int sysval = system("pkill -USR1 unitcomm");
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
    int sysval = system("pkill -USR1 respawner");
    sleep(1);
    sysval = system("pkill event_logger; pkill gps_mon");
    sysval = system("cp /tmp/loggedin_macs /autonet/loggedin_macs");
//    backupAutonet();
    if (fileExists(PowercontrollerDevice)) {
        sysval = system("powercontroller shutdown 5 10");
    }
    else {
        sysval = system("real_reboot");
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
    if (cellinfo.reset_state != 0) {
        reason = "CellLocked";
        type = 0x03;
        timer = 1;
    }
    string power_off_state = reason + ":" +
                             toString(car_state.powered_off_time) + ":" +
                             toString(car_state.ignition_off_time);
    writeStringToFile(PowerOffStateFile, power_off_state);
    writeMonthlyUsage();
    completeUpgrade();
//    if (getOppositeState() == "bad") {
//        waitForVerifyToComplete();
//    }
    UpgradeInfo upgrade_info;
    upgrade_info.read();
    if (upgrade_info.check()) {
        upgrade_info.remove();
        dupFs();
    }
    string cellresp;
//    cellresp = getCellResponse("ATH");
//    logError("ATH response: "+cellresp);
    stopGps();
    if (!fileExists("/autonet/admin/no_lowpower")) {
        sleep(5);
        cellresp = getCellResponse("lowpower");
        logError("lowpower response: "+cellresp);
    }
//    setVicRtc();
    disableWatchdog();
    int sysval = system("pkill -USR1 respawner");
    sleep(1);
    sysval = system("pkill event_logger; pkill gps_mon");
//    getCellResponse("AT_OGPS=0");
    logError("Sending shutdown command:"+toString(type)+"/"+toString(timer));
    bool usb_off = false;
    if (fileExists("/autonet/admin/usb_off")) {
       usb_off = true;
    }
    unmountAutonet();
    sleep(2);
//    Byte buffer[16];
//    buffer[0] = 10;
//    buffer[1] = type;
//    buffer[2] = timer;
//    getVicResponse(0x04, 3, buffer);
    // Kill vic_talker so if we don't shutdown vic will kick us
    sysval = system("pkill vic_talker");
    unlink(TempStateFile);
    if (usb_off) {
        sysval = system("writeGPIO 1 5 off");
    }
    exit(0);
}

void waitForVerifyToComplete() {
    monitor_flags |= IN_VERIFYWAIT;
    logError("Waiting for verify to complete");
    while (true) {
        string verify_pid = pgrep("verify_sys");
        if (verify_pid == "") {
            break;
        }
        sleep(1);
        getStateInfo();
    }
    monitor_flags &= ~IN_VERIFYWAIT;
}

void updateUpgradeInfo() {
    UpgradeInfo upgrade_info;
    upgrade_info.read();
    string current_version = getStringFromFile(SwVersionFile);
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
                int sysval = str_system(upgrade_cmd);
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
        version = getStringFromFile(SwVersionFile);
    }
    else {
        string opposite = "2";
        if (kernelId == "2") {
            opposite = "1";
        }
        //logError("Setting .kernel to "+opposite);
        //setUbootVariable(".kernel", opposite);
        logError("Setting kernel to "+opposite);
        setUbootVariable("kernel", opposite);
        setUbootVariable("state"+opposite, "programmed");
        boot_dir = "/mnt/tmp/boot/";
        int sysval = system("flash_mount -o");
        version = getStringFromFile(string("/mnt/tmp")+SwVersionFile);
        /* Kanaan put bootscript into flash
        if (fileExists("/mnt/tmp/boot/bootscript.img")) {
            if (system("cmp -s /boot/bootscript.img /mnt/tmp/boot/bootscript.img") != 0) {
                logError("Updating bootscript");
                int sysval = system("flash_erase /dev/mtd0 0x60000 1");
                sysval = system("nandwrite -p -s 0x60000 /dev/mtd0 /mnt/boot/bootscript.img");
            }
        } */
        updateFile(UpgradedVersionFile, version);
    }
    UpgradeInfo upgrade_info;
    upgrade_info.sw_version = version;
    upgrade_info.time_of_upgrade = time(NULL);
    upgrade_info.write();
    upgradeCellModule(boot_dir);
//    string vic_vers = "VIC-"+vic.build+"-"+vic.version_str+".dat";
//    if (!fileExists(boot_dir+vic_vers)) {
//        string vic_base = "VIC-"+vic.build+"-";
//        string new_vers = backtick("ls "+boot_dir+vic_base+"* 2>/dev/null");
//        if (new_vers == "") {
//            logError("No new VIC version");
//            string event = "NotifyEngineering Missing VIC firmare "+vic_base;
//            eventlog.append(event);
//        }
//        else {
//            updateVic(new_vers);
//        }
//    }
    if (!single_os) {
        int sysval = system("flash_mount -u");
    }
}

//void updateVic(string new_vers) {
//    logError("Updating VIC to "+new_vers);
//    writeMonthlyUsage();
//    disableWatchdog();
//    vic.closeSocket();
//    int exit_val = str_system("vic_loader "+new_vers);
//    sleep(1);
//    vic.openSocket();
//    enableWatchdog();
//    if (exit_val == 0) {
//        sleep(2);
//        int sysval = system("pkill -USR1 respawner");
//        sleep(1);
//        sysval = system("pkill event_logger; pkill gps_mon");
//        sleep(2);
//        logError("Resetting VIC after loading VIC firmware");
//        stopGps();
////        getCellResponse("AT_OGPS=0");
//        logError("Unmounting autonet");
//        unmountAutonet();
//        sleep(2);
//        // Resetting the VIC bootloader will reset the MX35
////        logError("Sending Reset to VIC");
////        va.vah_send("shutdown 5");
//        exit(0);
//    }
//}

void upgradeCellModule(string bootdir) {
    if (cellinfo.model == "GTM609") {
        upgradeOptionModule(bootdir);
    }
    else if ( (cellinfo.model == "DE910-DUAL") or
              (cellinfo.model == "CE910-DUAL") or
              (beginsWith(cellinfo.model, "LE910")) ) {
        upgradeTelitModule(bootdir);
    }
}

void upgradeOptionModule(string bootdir) {
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
                int sysval = str_system("ln -s "+bootdir+"FW FW");
                sysval = str_system("ln -s "+bootdir+"CUST CUST");
                sysval = str_system("ln -s "+loader_file+" loader.cfg");
                disableWatchdog();
                sysval = system("FWLoader");
                enableWatchdog();
            }
        }
    }
}

void upgradeTelitModule(string bootdir) {
}

string readUbootVariable(string var) {
    string val = backtick("fw_printenv "+var);
    if (val == "") {
        val = backtick("fw_printenv "+var);
    }
    if (val != "") {
        val = val.substr(var.size()+1);
    }
    return val;
}

void setUbootVariable(string var, string val) {
    string cmnd = "fw_setenv "+var+" "+val+" >/dev/null 2>&1";
    if (str_system(cmnd) != 0) {
        int sysval = str_system(cmnd);
    }
}

string getOppositeState() {
    string opposite_state = "";
    if ((kernelId == "1") or (kernelId == "2")) {
        string opposite = "2";
        if (kernelId == "2") {
            opposite = "1";
        }
        opposite_state = readUbootVariable(string(".state")+opposite);
    }
    return opposite_state;
}

void dupFs() {
    string opposite_state = getOppositeState();
//    logError("opposite_state:"+opposite_state);
    if (opposite_state == "verified") {
        logError("Duplicating upgraded filesystem");
        int sysval = system("dup_fs &");
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

void stopGps() {
    string cellresp = getCellResponse("stopgps");
    logError("stopgps response: "+cellresp);
}

void unmountAutonet() {
    int sysval = system("/etc/init.d/mount_autonet stop");
}

void backupAutonet() {
//    int sysval = system("backup_autonet");
//    string autonet = backtick("mount | grep '/autonet '");
//    string autonet_alt = backtick("mount | grep '/autonet.alt '");
//    cout << "mount autonet: " << autonet << endl;
//    cout << "mount autonet.alt: " << autonet_alt << endl;
//    if (autonet != "") {
//        sync();
//        buildToc("/autonet");
//        sync();
//        if (autonet_alt != "") {
//            int sysval = system("rm -rf /autonet.alt/*");
//            int sysval = system("cp -a /autonet/* /autonet.alt/");
//            sync();
//        }
//    }
}

void buildToc(string autonet) {
    int sysval = str_system("verify_fs -f " + autonet + "/etc/verify_autonet.conf " + autonet + " >" + autonet + "/verify_autonet.chk");
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
    if (waninfo.child_pid != 0) {
        pid_t child = waitpid(waninfo.child_pid,&status,WNOHANG);
        if (child == waninfo.child_pid) {
            waninfo.pid = 0;
            waninfo.child_pid = 0;
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
    killWan("Got term signal");
    int sysval = system("pkill save_dns_entries");
    sysval = system("pkill update_default_sites");
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
        sysval = system("cp /tmp/loggedin_macs /autonet/loggedin_macs");
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
        if (monitor_flags & IN_VERIFYWAIT) {
            monflags_str += " VERIFYWAIT";
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
//rcg            string ps = backtick("ps ax");
            string ps = backtick("ps");
            logError("ps:"+ps);
            logError("Process dir does not exist");
        }
    }
    logError(reason);
    writeLastEvent(reason);
    if (cellinfo.no_commands) {
       unlink(NoCellCommandsFile);
    }
    writeMonthlyUsage();
    disableWatchdog();
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

void startOmaDm() {
    ota_info.omadm_running = true;
    if (fileExists(RunOmadmClientFile)) {
        unlink(RunOmadmClientFile);
    }
    int sysval = system("omadm_client >/var/log/omadm_client.log 2>&1 &");
    if (ota_info.start_omadm) {
        ota_info.start_omadm = false;
    }
    else {
        touchFile(LastOmaDmTimeFile);
    }
}

void getFakeEcus() {
    logError("FakeEcusFile present");
    Strings lines = readFileArray(FakeEcusFile);
    string ecus_str = "";
    for (int i=0; i < lines.size(); i++) {
        string line = lines[i];
        if (line == "") {
            continue;
        }
        logError(line);
        Strings vals = split(line, ",");
        string type = vals[0];
        string addr_info = vals[1];
        string req_id = vals[2];
        string res_id = vals[3];
        string part = vals[4];
        string sw_level = vals[5];
        string hw_level = vals[6];
        int origin = stringToInt(vals[7]);
        int supplier_id = stringToInt(vals[8]);
        int diagnostic_version = stringToInt(vals[9]);
        int varient_id = stringToInt(vals[10]);
        string ecu_str = string_printf(" ECU:%d:", i);
        int b1, b2, b3;
        sscanf(hw_level.c_str(), "%d.%d.%d", &b1, &b2, &b3);
        ecu_str += string_printf("%02X%02X%02X",b1,b2,b3);
        sscanf(sw_level.c_str(), "%d.%d.%d", &b1, &b2, &b3);
        ecu_str += string_printf("%02X%02X%02X",b1,b2,b3);
        for (int j=0; j < part.size(); j++) {
            ecu_str += string_printf("%02X", part.at(j));
        }
        ecu_str += string_printf("%02X%02X%02X%02X",origin,supplier_id,varient_id,diagnostic_version);
        ecu_str += ":" + type;
        ecus_str += ecu_str;
    }
    string event = "CarState";
    event += ecus_str;
    eventlog.append(event);
    logError(event);
    bool ecus_changed = true;
    if (fileExists(EcusFile)) {
        if (str_system(string("cmp ")+FakeEcusFile+" "+EcusFile) == 0) {
            ecus_changed = false;
        }
    }
    if (ecus_changed) {
        int sysval = str_system(string("cp ")+FakeEcusFile+" "+EcusFile);
        if (ota_info.enabled) {
            touchFile(EcusChangeFile);
            startOmaDm();
        }
    }
    unlink(FakeEcusFile);
}
