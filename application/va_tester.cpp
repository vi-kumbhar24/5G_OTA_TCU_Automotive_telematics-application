#define VA2

#include <string>
#include <iostream>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "autonet_types.h"
#include "getopts.h"
#ifdef VA2
#include "lpVa.h"
#endif
#include "readFileArray.h"
#include "readconfig.h"
#include "va_helper.h"

void on_int(int sig);
static string ev_variables = "ev_charge_state,ev_battery_capacity,ev_discharge_rate,ev_battery_compartment_temp,ev_induction_coil_alignment,ev_plug_connection_status";
static string v2x_variables = "v2x_brakes_lf,v2x_brakes_lr,v2x_brakes_rf,v2x_brakes_rr,v2x_traction,v2x_abs,v2x_scs,v2x_brake_boost,v2x_aux_brakes,v2x_low_beam,v2x_high_beam,v2x_left_turn,v2x_right_turn,v2x_hazard_signal,v2x_automatic_light_control,v2x_daytime_running_lights,v2x_fog_light,v2x_parking_light,v2x_acceleration,v2x_yaw,v2x_steering_wheel_angle,v2x_front_wiper_status,v2x_rear_wiper_status";
static string variables = "";
static string can_messages = "can0;334,can1;334";

#ifdef VA2
void va2_state_callback(char type, const char *key, const char *value);
#else
static VaHelper va;
static VaHelper va_notifies;
static VaHelper va_cc;
#endif
static bool poll = false;
static bool receive_can_messages = false;

int main(int argc, char *argv[]) {
    string command = "";
    GetOpts opts(argc, argv, "cef:hlpvC:");
    if (opts.exists("h")) {
        cout << "usage: va_tester [-l] [-p] [-c] [-e] [-v] [-f varfile] [-C command] [vars . . .]" << endl;
        cout << "where:" << endl;
        cout << "  -l          : Lists currently supported variables" << endl;
        cout << "  -p          : Enables polling mode instead of notificion mode" << endl;
        cout << "  -h          : Outputs this help" << endl;
        cout << "  -c          : Selects CAN variables" << endl;
        cout << "  -e          : Selects EV variables" << endl;
        cout << "  -v          : Selects V2X variables" << endl;
        cout << "  -C command  : Command to send" << endl;
        cout << "  -f varfile  : Reads variables from 'varfile'" << endl;
        cout << "  vars        : Variables to monitor" << endl;
        exit(0);
    }
    if (opts.exists("l")) {
        Config config;
#ifdef VA2
        char key[] = "va.key.*";
        config.readConfigManager(key);
        Strings vars = config.getVars();
        for (int i=0; i < vars.size(); i++) {
            string var = vars[i].substr(strlen(key)-1);
            if (var.find(".owner") != string::npos) {
                int pos = var.find(".");
                var = var.substr(0, pos);
                cout << var << endl;
            }
        }
#else
        char key[] = "va_state_*";
        config.readConfigManager(key);
        Strings vars = config.getVars();
        for (int i=0; i < vars.size(); i++) {
            string var = vars[i].substr(strlen(key)-1);
            cout << var << endl;
        }
#endif
        exit(0);
    }
    poll = opts.exists("p");
    receive_can_messages = opts.exists("c");
    if (poll and receive_can_messages) {
        cout << "Option -c(CAN) cannot be used in polling mode(-p)" << endl;
        exit(1);
    }
    if (opts.exists("f")) {
        string var_file = opts.get("f");
        Strings vars = readFileArray(var_file);
        for (int i=0; i < vars.size(); i++) {
            if (variables != "") {
                variables += ",";
            }
            variables += vars[i];
        }
    }
    if (opts.exists("e")) {
        if (variables != "") {
            variables += ",";
        }
        variables += ev_variables;
    }
    if (opts.exists("v")) {
        if (variables != "") {
            variables += ",";
        }
        variables += v2x_variables;
    }
    if (opts.exists("C")) {
        command = opts.get("C");
    }
    for (int i=1; i < argc; i++) {
         if (variables != "") {
             variables += ",";
         }
         variables += argv[i];
    }
    Assoc status;
    signal(SIGINT, &on_int);
    string resp;
#ifdef VA2
    if (lp_va_initFromConfig("vic") != lp_va_ok) {
        cerr << "Failed lp_va_initfromConfig" << endl;
        exit(1);
    }
    if (lp_va_setCallback(va2_state_callback) != lp_va_ok) {
        cerr << "Failed lp_va_setCallback" << endl;
        exit(1);
    }
    char resp_buff[1024];
#else
    va.createSocket("ev", false);
    va_notifies.createSocket("ev", true);
    resp = va.sendQuery("ignition",5,1);
    if (resp == "") {
        cout << "VA not responding" << endl;
        exit(1);
    }
    else {
        cout << "Talking to VA" << endl;
    }
#endif
    if (command != "") {
#ifdef VA2
        Strings args = split(command, " ");
        string command = args[0];
        string arg1 = args[1];
        string arg2 = "";
        if (args.size() > 2) {
            arg2 = args[2];
        }
        if (command == "get") {
            size_t buf_len = sizeof(resp_buff);
            if (lp_va_query(arg1.c_str(),resp_buff, &buf_len, LP_VA_WAIT_DEFAULT) != lp_va_ok) {
                exit(1);
            }
        }
        else {
            if (command == "set") {
                command = arg1 + ":" + arg2;
            }
            else {
                command += ":" + arg1;
            }
            if (lp_va_cmd(command.c_str()) != lp_va_ok) {
                cerr << "Failed lp_va_cmd" << endl;
                exit(1);
            }
        }
#else
        va_cc.createSocket("cc", false);
        cout << "Sending command:" << command << endl;
        va_cc.vah_send(command);
#endif
    }
    if (poll) {
        while (true) {
#ifdef VA2
            size_t buf_len = sizeof(resp_buff);
            if (lp_va_query(variables.c_str(),resp_buff, &buf_len, LP_VA_WAIT_DEFAULT) != lp_va_ok) {
                 cerr << "Failed lp_va_query" << endl;
            }
            resp = resp_buff;
#else
            resp = va.sendQuery(variables);
#endif
            Strings states = split(resp,",");
            for (int i=0; i <  states.size(); i++) {
                Strings varval = split(states[i], ":");
                string var = varval[0];
                string val = "";
                if (varval.size() > 1) {
                    val = varval[1];
                }
                if ((status.find(var) == status.end()) or (status[var] != val)) {
                    cout << states[i] << endl;
                    status[var] = val;
                }
            }
            sleep(1);
        }
    }
    else {
#ifdef VA2
        if (lp_va_register(variables.c_str()) != lp_va_ok) {
            cerr << "Failed lp_va_register" << endl;
            exit(1);
        }
#else
        Strings vars = split(variables, ",");
        for (int i=0; i < vars.size(); i++) {
            va_notifies.vah_send(string("register:")+vars[i]);
        }
        if (receive_can_messages) {
            Strings messages = split(can_messages, ",");
            for (int i=0; i < messages.size(); i++) {
                va_notifies.vah_send(string("register_bus:")+messages[i]);
            }
        }
#endif
        while (true) {
#ifdef VA2
            sleep(1);
#else
            string resp = va_notifies.vah_recv();
            if (resp != "") {
                cout << resp << endl;
            }
#endif
        }
    }
}

#ifdef VA2
void va2_state_callback(char type, const char *key, const char *value) {
    if (type == LP_VA_CBNOTIFY) {
        string state = string(key) + ":" + value;
        cout << state << endl;
    }
}
#endif

void on_int(int sig) {
    if (!poll) {
#ifdef VA2
        if (lp_va_unregister(variables.c_str()) != lp_va_ok) {
            cerr << "Failed lp_va_unregister" << endl;
        }
        lp_va_deinit();
#else
        Strings vars = split(variables, ",");
        for (int i=0; i < vars.size(); i++) {
            va_notifies.vah_send(string("unregister:")+vars[i]);
        }
        if (receive_can_messages) {
            Strings messages = split(can_messages, ",");
            for (int i=0; i < messages.size(); i++) {
                va_notifies.vah_send(string("unregister_bus:")+messages[i]);
            }
        }
#endif
    }
    exit(0);
}
