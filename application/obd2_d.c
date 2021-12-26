#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
 
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
 
#include <linux/can.h>
#include <linux/can/raw.h>


//
// defines
//

// can ids
#define D_RQ_11BIT        0x7DF
#define D_RS1_11BIT       0x7E8
#define D_RS2_11BIT       0x7E9
#define D_RS3_11BIT       0x7EA
#define D_RS4_11BIT       0x7EB
#define D_RS5_11BIT       0x7EC
#define D_RS6_11BIT       0x7ED
#define D_RS7_11BIT       0x7EE
#define D_RS8_11BIT       0x7EF
#define D_RQ_29BIT        0x18DB33F1
#define D_RS_29BIT        0x18DAF100
#define UNUSED_ID         0xFFFFFFFF

// types
#define PCI_TYPE_0	0x00	// single frame
#define PCI_TYPE_1	0x10    // first frame
#define PCI_TYPE_2	0x20	// consecutive frame
#define PCI_TYPE_3	0x30	// flow control frame
#define PCI_TYPE_MASK	0xF0
#define PCI_TYPE_INDEX	0x0F

// services
#define SID_01_RQ	0x01    // read current data 
#define SID_01_RS	0x41
#define SID_03_RQ	0x03    // read diagnostic data
#define SID_03_RS	0x43
#define SID_09_RQ	0x09    // read vehicle information
#define SID_09_RS	0x49
#define SID_3E		0x3E    // tester present

// pids
#define PID_01_DTC	0x01
#define PID_02_VIN	0x02
#define PID_0C_RPM	0x0c
#define PID_0D_SPEED	0x0d
#define PID_2F_FUEL	0x2f

// general
#define MAX_TRANSFER_SIZE       30	// max bytes per multi-frame transmission
#define VIN_MESSAGE_LENGTH      20	// length of VIN response message
#define VIN_LENGTH  		17
#define DTC_SIZE_BYTES          4
#define DTC_MAX_NUMBER          128
#define ACTIVE_DTC_CODE         0x60
#define RDT_DTC_MAX_NUMBER	16
#define CAN_MAX_DATA		8	// maximum number of data bytes in CAN message
#define CAN_ID_LIST_SIZE	20	// max number of can xmit messages
#define TX_INTERVAL             500     // number of milliseconds for xmit timer
#define false 0
#define true  1

//
// typedefs
//
typedef int bool;
typedef unsigned char BYTE;
typedef unsigned int WORD;
typedef unsigned long ULONG;
typedef unsigned long DWORD;
typedef unsigned char uint8;
typedef unsigned int uint16;
typedef unsigned long uint32;
typedef signed char sint8;
typedef signed int sint16;
typedef signed long sint32;

typedef union {
	unsigned long l;
	unsigned char b[4];
	unsigned short w[2];
} ULONG_UNION;

typedef struct can_tx_msg_struct {
    ULONG_UNION id;                   // ID bytes
    BYTE data[CAN_MAX_DATA];          // data bytes
    BYTE dlc;                         // data length code
} CAN_TX_MSG_STRUCT;

typedef struct can_periodic_tx_msg_struct {
    CAN_TX_MSG_STRUCT txMessage;
    WORD wPeriod;
    WORD wRunTime;
    WORD wInitialDelay;
    BYTE fTxRequest:1;
    BYTE fTxEnable:1;
} CAN_PERIODIC_TX_MSG_STRUCT;

typedef struct {
	BYTE bPci;
	BYTE bSid;
	BYTE bPid;
	BYTE bData[5];
} SingleFramePdu_t;

typedef struct {
	BYTE bPci;
	BYTE bLength;
	BYTE bSid;
	BYTE bPid;
	BYTE bData[4];
} FirstFramePdu_t;

typedef struct {
	BYTE bPci;
	BYTE bData[7];
} ConsecutiveFramePdu_t;

typedef struct {
	BYTE bPci;
	BYTE bData[7];
} GenericFramePdu_t;

typedef struct {
	BYTE bLength;
	BYTE bSid;
	BYTE bPid;
	BYTE bData[MAX_TRANSFER_SIZE];
} FullFramePdu_t;

typedef struct {
    unsigned char DTC_data[DTC_SIZE_BYTES];
} DTCDescriptor_t;

typedef struct {
    unsigned short dtcs_valid;
    unsigned int   dtcNotificationTimer;
    unsigned short count;
    unsigned short current_idx;
    DTCDescriptor_t dtc[DTC_MAX_NUMBER];
} FaultStorage_t;


//
// globals
//
int sock;
int cflag=0, dflag=0, rflag=0, xflag=0, iflag=0, vflag=0, tflag=0, sflag=0, oflag=0;
int msgInProgress=0, msgFrameCount=0, msgFrameLength;
FullFramePdu_t fullFrame;
unsigned char sVIN[VIN_LENGTH+1], fVinValid=0;
unsigned char gMIL=0, fDtcValid=0, fRequestDtc=0;
FaultStorage_t FaultStorage;



#if 1
// tx table indexes, must match table below
typedef enum
{
    hVIN,
    hVEHICLE_SPEED,
    hFUEL_LEVEL,
    hENGINE_RPM,
    hDTC_STATUS,
} TxMessageHandle_e;

// tx table, Period and Initial delay must be multiple of TX_INTERVAL
CAN_PERIODIC_TX_MSG_STRUCT CanC_11bit_Tx[CAN_ID_LIST_SIZE] = {
    //Msg ID     Msg Data                                          DLC   Period(ms)  Run Time    Initial Delay     Flags
    {D_RQ_11BIT, {0x02, SID_09_RQ, PID_02_VIN,     0, 0, 0, 0, 0}, 8,    5000,       0,          0,                0},
    {D_RQ_11BIT, {0x02, SID_01_RQ, PID_0D_SPEED,   0, 0, 0, 0, 0}, 8,    5000,       0,          500,              0},
    {D_RQ_11BIT, {0x02, SID_01_RQ, PID_2F_FUEL,    0, 0, 0, 0, 0}, 8,    30000,      0,          1000,             0},
    {D_RQ_11BIT, {0x02, SID_01_RQ, PID_0C_RPM,     0, 0, 0, 0, 0}, 8,    5000,       0,          1500,             0},
    {D_RQ_11BIT, {0x02, SID_01_RQ, PID_01_DTC,     0, 0, 0, 0, 0}, 8,    5000,       0,          2000,             0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
    {UNUSED_ID,  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0,    0,          0,          0,                0},
};

CAN_PERIODIC_TX_MSG_STRUCT RequestDTCMessage =
    //Msg ID     Msg Data                                          DLC   Period(ms)   Run Time    Initial Delay    Flags
    {D_RQ_11BIT, {0x01, SID_03_RQ, 0, 0, 0, 0, 0, 0},              8,    1000,          0,          0,             0};
#endif


//
// function defs
//
#if 1
void EnableCanTx(CAN_PERIODIC_TX_MSG_STRUCT *messageList, BYTE bIndex);
void DisableCanTx(CAN_PERIODIC_TX_MSG_STRUCT *messageList, BYTE bIndex);
BYTE AddMessageCanTxList(CAN_PERIODIC_TX_MSG_STRUCT *message, CAN_PERIODIC_TX_MSG_STRUCT *messageList);
BYTE DeleteMessageCanTxList(CAN_PERIODIC_TX_MSG_STRUCT *message, CAN_PERIODIC_TX_MSG_STRUCT *messageList);
#endif


//
// routines
//
int canOpen(char *ifname, bool filter)
{
    int recv_own_msgs;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct timeval tv;

    if ((sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        fprintf(stderr, "Error opening socket\n");
        return -1;
    }

    if (filter) {
        //struct can_filter rfilter[4];
        struct can_filter rfilter[1];
        //rfilter[0].can_id   = 0x80001234;   /* Exactly this EFF frame */
        //rfilter[0].can_mask = CAN_EFF_MASK; /* 0x1FFFFFFFU all bits valid */
        //rfilter[1].can_id   = 0x123;        /* Exactly this SFF frame */
        //rfilter[1].can_mask = CAN_SFF_MASK; /* 0x7FFU all bits valid */
        //rfilter[2].can_id   = 0x200 | CAN_INV_FILTER; /* all, but 0x200-0x2FF */
        //rfilter[2].can_mask = 0xF00;        /* (CAN_INV_FILTER set in can_id) */
        //rfilter[3].can_id   = 0;            /* don't care */
        //rfilter[3].can_mask = 0;            /* ALL frames will pass this filter */
        rfilter[0].can_id   = 0x7E0;
        rfilter[0].can_mask = 0xFF0;          /* 7E0-7EF */
        setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter[0], sizeof(rfilter));
    }

    strcpy(ifr.ifr_name, ifname);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        fprintf(stderr, "Error getting socket ifindex\n");
        return -2;
    }

    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Error in socket bind\n");
        return -3;
    }

    //recv_own_msgs = 0; /* 0 = disabled (default), 1 = enabled */
    //setsockopt(sock, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS,
    //                &recv_own_msgs, sizeof(recv_own_msgs));

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval)) < 0) {
        fprintf(stderr, "Error setting socket timeout\n");
        return -4;
    }
    //setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (struct timeval *)&tv, sizeof(struct timeval));

    return 0;
}


void canClose()
{
    close(sock);
}


int sendQuery(canid_t can_id, int sid, int pid)
{
    int nbytes;
    struct can_frame frame;

    if (dflag) printf("sendQuery id:%03lx sid:%02x pid:%02x\n", can_id, sid, pid);

    frame.can_id  = can_id;
    frame.can_dlc = 8;
    frame.data[0] = 0x02;    // single frame and 2 bytes
    frame.data[1] = sid; 
    frame.data[2] = pid;
    frame.data[3] = 0x55;
    frame.data[4] = 0x55;
    frame.data[5] = 0x55;
    frame.data[6] = 0x55;
    frame.data[7] = 0x55;

    nbytes = write(sock, &frame, sizeof(struct can_frame));
    if (nbytes < 0) {
        fprintf(stderr, "Write sock failed\n");
    }

    return nbytes;
}

int sendDtcRequest(canid_t can_id)
{
    int nbytes;
    struct can_frame frame;

    if (dflag) printf("sendDTCrequest\n");

    frame.can_id  = can_id;
    frame.can_dlc = 8;
    frame.data[0] = 0x01;
    frame.data[1] = SID_03_RQ;
    frame.data[2] = 0x00;
    frame.data[3] = 0x00;
    frame.data[4] = 0x00;
    frame.data[5] = 0x00;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;

    nbytes = write(sock, &frame, sizeof(struct can_frame));
    if (nbytes < 0) {
        fprintf(stderr, "Write sock failed\n");
    }

    return nbytes;
}

int sendFlowControlFrame(canid_t can_id)
{
    int nbytes;
    struct can_frame frame;

    if (dflag) printf("sendFlowControlFrame\n");
  
    frame.can_id  = can_id;
    frame.can_dlc = 8;
    frame.data[0] = 0x30;	// clear to send
    frame.data[1] = 0x00;	// send without delay
    frame.data[2] = 0x00;
    frame.data[3] = 0x00;
    frame.data[4] = 0x00;
    frame.data[5] = 0x00;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;
    
    nbytes = write(sock, &frame, sizeof(struct can_frame));
    if (nbytes < 0) {
        fprintf(stderr, "Write sock failed\n");
    }

    return nbytes;
}

BYTE GetRequestIdFromReponseId(canid_t can_id, DWORD *pdwRequestID)
{
    BYTE bRet = 0;

    // if this is a valid 11-bit diagnostic response
    if ((can_id >= D_RS1_11BIT) && (can_id <= D_RS8_11BIT)) {
        *pdwRequestID = can_id - 0x08;
        bRet = 1;
    }
    return bRet;
}


void dump(char *p, int n)
{
    int i;

    for (i = 0; i<n; i++) {
        printf(" %02x", *p++);
    }
    printf("\n");
}


void processRPM(FullFramePdu_t *pdu)
{
    FILE *fp;
    unsigned int rpm;

    rpm = ((unsigned int)pdu->bData[0]*256 + pdu->bData[1])/4;
    if (dflag) printf("Vehicle rpm: %d\n", rpm);

    fp = fopen("/tmp/vehicle_rpm.tmp", "w+");
    fprintf(fp, "%d\n", rpm);
    fclose(fp);
    rename("/tmp/vehicle_rpm.tmp", "/tmp/vehicle_rpm");
}

void processSPEED(FullFramePdu_t *pdu)
{
    FILE *fp;
    unsigned int speed;

    speed = (unsigned int)pdu->bData[0];
    if (dflag) printf("Vehicle speed: %dkph\n", speed);

    fp = fopen("/tmp/vehicle_speed.tmp", "w+");
    fprintf(fp, "%d\n", speed);
    fclose(fp);
    rename("/tmp/vehicle_speed.tmp", "/tmp/vehicle_speed");
}

void processFUEL(FullFramePdu_t *pdu)
{
    FILE *fp;
    unsigned int fuel;

    fuel = (unsigned int)pdu->bData[0]*100/255;
    if (dflag) printf("Vehicle fuel: %d%%\n", fuel);

    fp = fopen("/tmp/vehicle_fuel.tmp", "w+");
    fprintf(fp, "%d%%\n", fuel);
    fclose(fp);
    rename("/tmp/vehicle_fuel.tmp", "/tmp/vehicle_fuel");
}

void processVIN(FullFramePdu_t *pdu)
{
    FILE *fp;

    memcpy(sVIN, &pdu->bData[1], VIN_LENGTH);
    sVIN[VIN_LENGTH] = 0;
    if (dflag) printf("Vehicle vin: %s\n", sVIN);

    fp = fopen("/tmp/vehicle_vin.tmp", "w+");
    fprintf(fp, "%s\n", sVIN);
    fclose(fp);
    rename("/tmp/vehicle_vin.tmp", "/tmp/vehicle_vin");

    // stop requesting the vin
    DisableCanTx(CanC_11bit_Tx, hVIN);
}

void processMIL(void)
{
    FILE *fp;

    fp = fopen("/tmp/vehicle_mil.tmp", "w+");
    fprintf(fp, "%d\n", gMIL);
    fclose(fp);
    rename("/tmp/vehicle_mil.tmp", "/tmp/vehicle_mil");
}

void processDTC(void)
{
    FILE *fp;
    int i;
    DTCDescriptor_t *pFaultData; 

    fp = fopen("/tmp/vehicle_faults.tmp", "w+");
    pFaultData = &FaultStorage.dtc[0];
    for (i = 0; i < FaultStorage.count; i++) {
        fprintf(fp, "%02X%02X.%02X", pFaultData->DTC_data[0],
                                     pFaultData->DTC_data[1],
                                     pFaultData->DTC_data[3]);
        pFaultData++;
        if (i < FaultStorage.count-1)
            fprintf(fp, ",");
    }
    fprintf(fp, "\n");
    fclose(fp);
    rename("/tmp/vehicle_faults.tmp", "/tmp/vehicle_faults");
}

void processDTCstatus(canid_t can_id, FullFramePdu_t *pdu)
{
    unsigned char bDTC;

    // mil on?
    bDTC = pdu->bData[0];
    if (bDTC & 0x80) 
        gMIL = 1;

    if (dflag) printf("DTC status: %02x\n", bDTC);
    if (dflag) printf("Vehicle mil: %d\n", gMIL);

    // are there any dtc's? (lower 7 bits)
    bDTC &= 0x7f;
    if (bDTC) {
        if (GetRequestIdFromReponseId(can_id, &RequestDTCMessage.txMessage.id.l)) {
            // add a ECU-specific diagnostic request to the TX table
            AddMessageCanTxList(&RequestDTCMessage, &CanC_11bit_Tx[0]);
        }
    }

    // stop requesting the dtc's
    FaultStorage.dtcs_valid = 1;
    DisableCanTx(CanC_11bit_Tx, hDTC_STATUS);
}

void processDTCrequest(canid_t can_id, FullFramePdu_t *pdu)
{
    FILE *fp;
    int i;
    BYTE *pData, bNumDTCs;
    DTCDescriptor_t *pFaultData; 

    // no PID in this message. this is the number of DTCs
    bNumDTCs = pdu->bPid;

    if (dflag) printf("Recv'd %d DTCs\n", bNumDTCs);

    pFaultData = &FaultStorage.dtc[FaultStorage.count];
    pData = pdu->bData;
    //while (bNumDTCs && FaultStorage.count < RDT_DTC_MAX_NUMBER) {
    while (bNumDTCs && FaultStorage.count < DTC_MAX_NUMBER) {
        pFaultData->DTC_data[0] = *pData++;
        pFaultData->DTC_data[1] = *pData++;
        pFaultData->DTC_data[2] = 0;
        pFaultData->DTC_data[3] = ACTIVE_DTC_CODE;
        pFaultData++;
        FaultStorage.count++;
        bNumDTCs--;
    }

    // once we have a response, remove the request message from the TX list
    if (GetRequestIdFromReponseId(can_id, &RequestDTCMessage.txMessage.id.l)) {
        DeleteMessageCanTxList(&RequestDTCMessage, &CanC_11bit_Tx[0]); 
    }
}



void processFullFrame(canid_t can_id, FullFramePdu_t *pdu)
{
    if (dflag) dump((char *)pdu, pdu->bLength+1);
    switch (pdu->bSid) {
        case SID_01_RS:
            switch (pdu->bPid) {
                case PID_0C_RPM:
                    processRPM(pdu);
                    break;
    
                case PID_0D_SPEED:
                    processSPEED(pdu);
                    break;
    
                case PID_2F_FUEL:
                    processFUEL(pdu);
                    break;
    
                case PID_01_DTC:
                    processDTCstatus(can_id, pdu);
                    break;
            }
            break;

        case SID_09_RS:
            switch (pdu->bPid) {
                case PID_02_VIN:
                    processVIN(pdu);
                    break;
            }
            break;

        case SID_03_RS:
            processDTCrequest(can_id, pdu);
            break;

    }
}


void processSingleFrame(struct can_frame *frame)
{
    SingleFramePdu_t *pdu = (SingleFramePdu_t *)&frame->data[0];

    // single frame is a full frame
    processFullFrame(frame->can_id, (FullFramePdu_t *)pdu);
}


void processFirstFrame(struct can_frame *frame)
{
    FirstFramePdu_t *pdu = (FirstFramePdu_t *)&frame->data[0];
    canid_t can_id;
 
    msgFrameLength = (((unsigned int)pdu->bPci & 0x0f) << 8) + pdu->bLength;
    if (msgFrameLength <= MAX_TRANSFER_SIZE) {
        // copy frame minus pci
        memcpy(&fullFrame.bLength, &pdu->bLength, 7);
        msgFrameLength -= 6;
        msgFrameCount = 0;
        msgInProgress = 1;
        if (GetRequestIdFromReponseId(frame->can_id, (DWORD *)&can_id)) {
            sendFlowControlFrame(can_id);
        }
    }
}


void processConsecutiveFrame(struct can_frame *frame)
{
    int index;
    ConsecutiveFramePdu_t *pdu = (ConsecutiveFramePdu_t *)&frame->data[0];

    // are we in a multiframe msg?
    if (!msgInProgress)
        return; 

    // inc rcv frame count
    msgFrameCount++;

    // does index match expected frame count?
    index = (pdu->bPci & PCI_TYPE_INDEX);
    if (index != msgFrameCount) {
        msgInProgress = 0;
        return;
    }

    // copy it, cf frames are 7 bytes, 4 bytes were copied from first frame
    if (msgFrameLength > 7) {
        memcpy(4 + &fullFrame.bData[(index-1) * 7], &pdu->bData[0], 7);
        msgFrameLength -= 7;
    } else {
        memcpy(4 + &fullFrame.bData[(index-1) * 7], &pdu->bData[0], msgFrameLength);
        msgFrameLength = 0;
    }

    // are we done?
    if (msgFrameLength == 0) {
        msgInProgress = 0;
        processFullFrame(frame->can_id, &fullFrame);
    }
}


int processFrame(struct can_frame *frame)
{
    GenericFramePdu_t *pdu = (GenericFramePdu_t *)&frame->data[0];

    if (dflag) printf("canRecv: %03lx", frame->can_id);
    if (dflag) dump(&frame->data[0], frame->can_dlc);

    switch (frame->can_id) {
        case D_RS1_11BIT:
        case D_RS2_11BIT:
        case D_RS3_11BIT:
        case D_RS4_11BIT:
        case D_RS5_11BIT:
        case D_RS6_11BIT:
        case D_RS7_11BIT:
        case D_RS8_11BIT:
            switch (pdu->bPci & PCI_TYPE_MASK) {
                case PCI_TYPE_0:
                    processSingleFrame(frame);
                    break;
                case PCI_TYPE_1:
                    processFirstFrame(frame);
                    break;
                case PCI_TYPE_2:
                    processConsecutiveFrame(frame);
                    break;
                case PCI_TYPE_3:
                    // flow control frame
                    break;
            }
            return 0;
    }
    return -1;
}


int canRcv()
{
    int nbytes;
    struct can_frame frame;

    nbytes = read(sock, &frame, sizeof(struct can_frame));
    if (nbytes < 0) {
        //fprintf(stderr, "Read sock failed\n");
        return nbytes;
    }
    if (nbytes > 0) {
        processFrame(&frame);
    } 
    return nbytes;
}


struct can_frame *waitForResponse() {
    int nbytes;
    time_t start_time;
    static struct can_frame frame;

    start_time = time(NULL);
    while ((time(NULL) - start_time) < 2) {
        nbytes = read(sock, &frame, sizeof(struct can_frame));
        if (nbytes > 0) {
            switch (frame.can_id) {
                case D_RS1_11BIT:
                case D_RS2_11BIT:
                case D_RS3_11BIT:
                case D_RS4_11BIT:
                case D_RS5_11BIT:
                case D_RS6_11BIT:
                case D_RS7_11BIT:
                case D_RS8_11BIT:
                    return &frame;
            }
        }
    }
    return NULL;
}


#if 0
int canSnd()
{
    static time_t xmit_time;
    static int init = 0;
    static int seconds;

    // init
    if (init == 0) {
        init = 1;
        seconds = 0;
        xmit_time = time(NULL);
    }

    // send each message 1x every 5 seconds
    if ((time(NULL) - xmit_time) >= 1) {
        xmit_time = time(NULL);
        switch (++seconds) {
            case 1:
                 if (!fVinValid)
                     sendQuery(D_RQ_11BIT, SID_09_RQ, PID_02_VIN);
                 break;
            case 2:
                 sendQuery(D_RQ_11BIT, SID_01_RQ, PID_0D_SPEED);
                 break;
            case 3:
                 sendQuery(D_RQ_11BIT, SID_01_RQ, PID_0C_RPM);
                 break;
            case 4:
                 sendQuery(D_RQ_11BIT, SID_01_RQ, PID_2F_FUEL);
                 break;
            case 5:
                 if (!fDtcValid)
                     sendQuery(D_RQ_11BIT, SID_01_RQ, PID_01_DTC);
                 if (fRequestDtc) {
                     //sendDtcRequest(can_id);
                     //sendDtcRequest(0x7e0);
                     sendDtcRequest(0x7df);
                 }
                      
                 seconds = 0;
                 break;
        }
    }
}
#else

#include "can.c"

int canXmit(CAN_TX_MSG_STRUCT *pTxMessage)
{
    int nbytes;
    struct can_frame frame;

    if (dflag) printf("canXmit: %03lx", pTxMessage->id.l);
    if (dflag) dump(pTxMessage->data, 8);

    frame.can_id  = pTxMessage->id.l;
if (sflag) {
    frame.can_id  = 0x7df;
}
    frame.can_dlc = pTxMessage->dlc;
    memcpy(&frame.data[0], pTxMessage->data, 8);

    nbytes = write(sock, &frame, sizeof(struct can_frame));
    if (nbytes < 0) {
        fprintf(stderr, "Write sock failed\n");
    }

    return nbytes;
}


void InitializeScheduler(void)
{
    CAN_PERIODIC_TX_MSG_STRUCT *pTxTable;

    // init
    FaultStorage.count = 0;
    FaultStorage.dtcNotificationTimer = 0;
    FaultStorage.dtcs_valid = 0;


    // schedule all CAN-C periodic messages
    for (pTxTable = &CanC_11bit_Tx[0]; pTxTable < &CanC_11bit_Tx[CAN_ID_LIST_SIZE]; pTxTable++) {
        if (pTxTable->txMessage.id.l == 0xffffffff) {
            pTxTable->wPeriod = 0;
            pTxTable->fTxEnable = false;
        }
        else if (pTxTable->wPeriod != 0) {
            // if period is non-zero and ID is defined, this message needs to be scheduled
            //pTxTable->wRunTime = pTxTable->wPeriod + pTxTable->wInitialDelay;
            pTxTable->wRunTime = pTxTable->wInitialDelay;
            pTxTable->fTxEnable = true;
        }
    }
}


void TxMessageScheduler(int sig)
{
    static int init = 0;
    CAN_PERIODIC_TX_MSG_STRUCT *pTxTable;
    int fXmit;

    // init
    if (init == 0) {
        init = 1;
        InitializeScheduler();
    }

    // loop thru xmit list
    fXmit=0;
    for (pTxTable = &CanC_11bit_Tx[0]; pTxTable < &CanC_11bit_Tx[CAN_ID_LIST_SIZE]; pTxTable++) {
        if (pTxTable->fTxEnable) {
            if (pTxTable->wRunTime == 0) {
                // xmit it (only 1 xmit per interval)
                if (!fXmit) {
                    fXmit = 1;
                    canXmit(&pTxTable->txMessage);

                    // reset period
                    pTxTable->wRunTime = pTxTable->wPeriod;
                }
            } else {
                pTxTable->wRunTime -= TX_INTERVAL;
            }
        }
    }
    if (!fXmit) {
        if (dflag) printf(" Empty tx slot\n");
    }
}
#endif


int configureBaudRate(char *iname)
{
    char cmd[80];

    if (dflag) printf("Configuring baud rate\n");

    // try 500K
    canClose();
    sprintf(cmd, "ip link set %s down", iname); system(cmd);
    //sprintf(cmd, "ip link set %s type can bitrate 500000 listen-only on", iname); system(cmd);
    sprintf(cmd, "ip link set %s type can bitrate 1000000 listen-only on", iname); system(cmd);
    sprintf(cmd, "ip link set %s up", iname); system(cmd);
    canOpen(iname, false);
    if (canRcv() > 0) {
        // good to go
        canClose();
        sprintf(cmd, "ip link set %s down", iname); system(cmd);
        //sprintf(cmd, "ip link set %s type can bitrate 500000 listen-only off", iname); system(cmd);
        sprintf(cmd, "ip link set %s type can bitrate 1000000 listen-only off", iname); system(cmd);
        sprintf(cmd, "ip link set %s up", iname); system(cmd);
        canOpen(iname, true);
        if (dflag) printf("Connected at 500K\n");
        return 0;
    }

    // try 250K
    canClose();
    sprintf(cmd, "ip link set %s down", iname); system(cmd);
    //sprintf(cmd, "ip link set %s type can bitrate 250000 listen-only on", iname); system(cmd);
    sprintf(cmd, "ip link set %s type can bitrate 500000 listen-only on", iname); system(cmd);
    sprintf(cmd, "ip link set %s up", iname); system(cmd);
    canOpen(iname, false);
    if (canRcv() > 0) {
        // good to go
        canClose();
        sprintf(cmd, "ip link set %s down", iname); system(cmd);
        //sprintf(cmd, "ip link set %s type can bitrate 250000 listen-only off", iname); system(cmd);
        sprintf(cmd, "ip link set %s type can bitrate 500000 listen-only off", iname); system(cmd);
        sprintf(cmd, "ip link set %s up", iname); system(cmd);
        canOpen(iname, true);
        if (dflag) printf("Connected at 250K\n");
        return 0;
    }

    // try 500K xmit
    canClose();
    sprintf(cmd, "ip link set %s down", iname); system(cmd);
    //sprintf(cmd, "ip link set %s type can bitrate 500000 listen-only off", iname); system(cmd);
    sprintf(cmd, "ip link set %s type can bitrate 1000000 listen-only off", iname); system(cmd);
    sprintf(cmd, "ip link set %s up", iname); system(cmd);
    canOpen(iname, true);
    sendQuery(D_RQ_11BIT, SID_01_RQ, PID_0C_RPM);
    if (canRcv() > 0) {
        // good to go
        if (dflag) printf("Connected at 500K\n");
        return 0;
    }

    // try 250K xmit
    canClose();
    sprintf(cmd, "ip link set %s down", iname); system(cmd);
    //sprintf(cmd, "ip link set %s type can bitrate 250000 listen-only off", iname); system(cmd);
    sprintf(cmd, "ip link set %s type can bitrate 500000 listen-only off", iname); system(cmd);
    sprintf(cmd, "ip link set %s up", iname); system(cmd);
    canOpen(iname, true);
    sendQuery(D_RQ_11BIT, SID_01_RQ, PID_0C_RPM);
    if (canRcv() > 0) {
        // good to go
        if (dflag) printf("Connected at 250K\n");
        return 0;
    }

    // uh-oh
    canClose();
    if (dflag) printf("Could not determine baud rate\n");
    return -1;
}


void on_int(int sig)
{
    canClose();
    exit(0);
}


int main(int argc, char **argv)
{
    int c;
    static char iname[20];
    time_t update_time;
    int fUpdate;
    static char usage[] = "usage: %s [-drxctvso] -i ifname\n";

    while ((c = getopt(argc, argv, "cdrxti:vso")) != -1) {
        switch (c) {
            case 'c': cflag = 1; break;
            case 'd': dflag = 1; break;
            case 'r': rflag = 1; break;
            case 'x': xflag = 1; break;
            case 'v': vflag = 1; break;
            case 't': tflag = 1; break;
            case 's': sflag = 1; break;
            case 'o': oflag = 1; break;
            case 'i': iflag = 1; strcpy(iname, optarg); break;
            case '?': fprintf(stderr, usage, argv[0]);
                      exit(1);
        }
    }

    signal(SIGHUP, on_int);
    signal(SIGINT, on_int);
    signal(SIGQUIT, on_int);
    signal(SIGTERM, on_int);
 
    if (iflag == 0) {     // -i is mandatory
        fprintf(stderr, "%s: missing -i option\n", argv[0]);
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }

    if (oflag == 1) {
        if (canOpen(iname, false) < 0) {
            printf("Exiting\n");
            canClose();
            exit(1);
        }
    }

    if (cflag == 1) {
        while (configureBaudRate(iname) < 0) {
            fprintf(stderr, "Configure baud rate failed!\n");
            fprintf(stderr, "Sleeping and then will retry\n");
            sleep(5);
        }
    }

    if (tflag == 1) {
        while (1) {
           time_t test_time = time(NULL);
           sendQuery(D_RQ_11BIT, SID_09_RQ, PID_02_VIN);
           while ((time(NULL) - test_time) < 5)
               canRcv();
        }
        //sendQuery(D_RQ_11BIT, SID_01_RQ, PID_01_DTC);
        //sendDtcRequest(0x7e0);
        //sendDtcRequest(0x7df);
    }

    if (xflag == 1) {
        struct itimerval it_val;

        // upon SIGALRM, all TxMessageScheduler
        if (signal(SIGALRM, (void (*)(int)) TxMessageScheduler) == SIG_ERR) {
            perror("Unable to catch SIGALRM");
            exit(1);
        }

        // set interval timer.  We want frequency in ms, 
        // but the setitimer call needs seconds and useconds.
        it_val.it_value.tv_sec  = TX_INTERVAL/1000;
        it_val.it_value.tv_usec = (TX_INTERVAL*1000) % 1000000;
        it_val.it_interval = it_val.it_value;
        if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
            perror("error calling setitimer()");
            exit(1);
        }
    }

    fUpdate = 0;
    update_time = time(NULL);
    while (1) {
        if (rflag) {
            canRcv();
        }

        if (!fUpdate && ((time(NULL) - update_time) > 20)) {
            fUpdate = 1;
            processMIL();
            processDTC();
        }
    }

    return 0;
}

