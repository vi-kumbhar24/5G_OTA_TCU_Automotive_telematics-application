#ifndef TW_LOADER_H
#define TW_LOADER_H

#define	TFIVER		_T("TFI Ver7.9")	// CE910-DUAL, CE910-SC

#define	FEATURE_DLLOG
#define	FEATURE_ERRLOG

typedef  unsigned short WORD;
typedef  unsigned short USHORT;
typedef  unsigned char BYTE;
typedef  unsigned int UINT;
typedef  unsigned int  DWORD;
typedef  unsigned char BOOL;

#define	MS_NORMAL		0x01
#define	MS_DLOAD		0x02
#define	MS_EDLOAD		0x03
#define	MS_INVALID		0xFF

#define	DEFAULT_BAUDRATE		230400

#define	RAMLOADER_SIZE	0x00100000
#define	HEXABINARY_SIZE	0x02000000		// Module Binary Data 최대 크기 4Mbyte

#define	MAX_MODEM_COUNTER	10			// Module의 최대 개수
#define	MAX_NVDATA_SIZE		128			// Nv Data의 최대 크기

#define	MAX_RECOVERY_NV_COUNT	100

#define	MAX_PACKET_SIZE		2048		// Packet의 최대 크기
#define LINECHARSIZE		2048		// Character Counter of One Line

#define	PRL_BLOCK_SIZE		120		/* Size in bytes of the PR_LIST block */
#define	MAX_PRL_SIZE		8196

#define	SOFTVERSION_SIZE	8			// Software Version의 문자열 길이
#define	SPCCODESIZE			6			// Special Programming Code의 문자열 길이

#define VERNO_DATE_STRLEN	11
#define VERNO_TIME_STRLEN	8
#define VERNO_DIR_STRLEN	8

#define	MAX_NAM_COUNT		5
#define	MAX_MIN_COUNT		2
#define	DUMMYDATA_SIZE		16

#define	MAX_COMPORT_CNT		255

#define	DLOADDATA_SIZE		0x200		// Download Data 최대 크기 512byte
#define	STREAMWRITE_SIZE	0x400		// Stream Write 최대 크기 1024byte
#define	RAMLD_HEADERSIZE	7
#define	RAMLD_DATASIZE		1017		// DLOADDATA_SIZE - RAMLD_HEADERSIZE

#define	ASYNC_TYPE			1			// Asyncronous Transaction
#define	SYNC_TYPE			2			// Syncronous Transaction

#define	RETRY_COUNT			3

#define	NOT_USED			0
#define	DM_MODE				1
#define	DOWN_MODE			2
#define	DATA_MODE			3

#define	RESPONSE_TIMEOUT	6000
#define	MODERESET_WAITTIME	12000
#define	FLASHWRITE_TIMEOUT	3000

#define	UWM_DIALOG_CLOSE		WM_USER + 1
#define	UWM_RECEIVE_PKT			WM_USER + 2
#define	UWM_PROCESS_MSG			WM_USER + 3
#define	UWM_PROGRESS_MSG		WM_USER + 4
#define	UWM_DOWNLOADTIME_MSG	WM_USER + 5
#define	UWM_SCRIPTTIME_MSG		WM_USER + 6
#define	UWM_NVITEMRDWR_MSG		WM_USER + 7
#define	UWM_DMPROCESS_CLOSE		WM_USER + 8
//#define	UWM_DOWNLOAD_START		WM_USER + 9

//******************************************
// Process Error Message Code
#define	COMPORTOPEN_FAIL				0x80
#define	MODULEFIND_FAIL					0x81
#define	PACKETSEND_ERROR				0x82
#define	MODECHANGE_ERROR				0x83
#define	RECVPKT_CRCERROR				0x84
#define	INVALID_SCRIPT					0x85
#define	NO_RESPONSE						0x86
#define	NVITEMREAD_FAIL					0x87
#define	PRLREAD_FAIL					0x88
#define	RAMLOADER_LOAD_FAIL				0x89
#define	DOWNLOAD_FAIL					0x8A
#define	NVITEMWRITE_FAIL				0x8B
#define	ATCMDPROC_FAIL					0x8C
#define MODEOFFLINE_FAIL				0x8D
#define	SPC_FAIL						0x8E
#define	MODERESET_FAIL					0x9F
#define	FILEOPEN_ERROR					0x90
#define	FILEREAD_ERROR					0x91
#define	FILESAVE_ERROR					0x92
#define	FILEWRITE_ERROR					0x93
#define	NVSTATUS_ERROR					0x94
#define	PRLSTATUS_ERROR					0x95
#define	INVALID_ESNWRCMD				0x96

#define	CNV_DOWNLOAD_ERROR				0x97
#define	CNV_UPLOAD_ERROR				0x98

// Final Check Error Message Code
#define	INVALID_ESN						0xF1
#define	ESNREAD_FAIL					0xF2
#define	INVALID_SOFTVER					0xF3
#define	SOFTVERREAD_FAIL				0xF4
#define	NVITEM_RECHECK					0xF5
//******************************************

#define	PROCESS_COMPLETE	TRUE
#define	PROCESS_ERROR		FALSE

#define	START_TIMERID		1
#define	DOWNLOAD_TIMERID	2
#define	DOWNLOAD_TIME		1000

#define	MULTIDOWNSTEP_START				0x00
#define	MULTIDOWNSTEP_NVREAD			0x01
#define	MULTIDOWNSTEP_NVWRITE			0x02
#define	MULTIDOWNSTEP_PRLREAD			0x03
#define	MULTIDOWNSTEP_PRLWRITE			0x04
#define	MULTIDOWNSTEP_PRLCHECK			0x05
#define	MULTIDOWNSTEP_PRLUPDATE			0x06
#define	MULTIDOWNSTEP_DOWNLOAD			0x07
#define	MULTIDOWNSTEP_AKEYWRITE			0x08
#define	MULTIDOWNSTEP_ERASEALL			0x09
#define	MULTIDOWNSTEP_END				0x0A

//******************************************
// Process Step Error Code
#define	MULTIDOWNSTEP_START_ERROR		0xF0
#define	MULTIDOWNSTEP_DOWNLOAD_ERROR	0xF1
#define	MULTIDOWNSTEP_DISPLAYMSG		0xF2
#define	MULTIDOWNSTEP_ERRORMSG			0xF3

#define	RECTYPE_DATA		0x00
#define	RECTYPE_END			0x01
#define	RECTYPE_EXTADDR		0x02
#define	RECTYPE_STRADDR		0x03
#define	RECTYPE_LINADDR		0x04

#define	ISDECIMAL(x)		(( '0' <= x ) && ( x <= '9' ))
#define	ISHEXADECIMAL(x)	((( '0' <= x ) && ( x <= '9' )) || (( 'a' <= x ) && ( x <= 'f' )) || (( 'A' <= x ) && ( x <= 'F' )))
#define ISBOOLEAN(x)		((! strcmp(x, "true")) || (! strcmp(x, "false")) || (! strcmp(x, "TRUE")) || (! strcmp(x, "FALSE")))

// NV Status Code
#define	NV_DONE_S			0x0000		/* Request completed okay */
#define	NV_BUSY_S			0x0001		/* Request is queued */
#define	NV_BADCMD_S			0x0002		/* Unrecognizable command field */
#define	NV_FULL_S			0x0003		/* The NVM is full */
#define	NV_FAIL_S			0x0004		/* Command failed, reason other than NVM was full */
#define	NV_NOTACTIVE_S		0x0005		/* Variable was not active */
#define	NV_BADPARM_S		0x0006		/* Bad parameter in command block */
#define	NV_READONLY_S		0x0007		/* Parameter is write-protected and thus read only */
#define	NV_BADTG_S			0x0008		/* Item not valid for Target */
#define	NV_NOMEM_S			0x0009		/* free memory exhausted */
#define	NV_NOTALLOC_S		0x000A		/* address is not a valid allocation */

#define ASYNC_FLAG          0x7E		// Flag character value.  Corresponds
#define ASYNC_ESCAPE        0x7D		// Escape sequence 1st character value
#define ASYNC_ESC_COMP		0x20		// Escape sequence complement value

// NV Item
#define DS_QCMIP				459
#define DS_MIP_GEN_USER_PROF	465
#define DS_MIP_SS_USER_PROF		466

#define	DLSTEP_NOTUSED			0
#define	DLSTEP_NVBACKUPLIST		1
#define	DLSTEP_NVBACKUP			2
#define	DLSTEP_RAMLOADER		3
#define	DLSTEP_ALL				4
#define	DLSTEP_BUA				5
#define	DLSTEP_UA				6
#define	DLSTEP_DELTA			7
#define	DLSTEP_FLAG				8
#define	DLSTEP_NVRESTORE		9
#define	DLSTEP_QCNUPLOAD		10
#define	DLSTEP_FACTORYRESET		11
#define	DLSTEP_NVCLEAR			12
#define	DLSTEP_SCRIPT			13
#define	DLSTEP_CHECK			14
#define	DLSTEP_PRL				15
#define	DLSTEP_TWOBINARY		16
#define	DLSTEP_RAM_NUMONYX		17
#define	DLSTEP_RAM_SPANSION		18
#define	DLSTEP_ALL_NUMONYX		19
#define	DLSTEP_ALL_SPANSION		20
#define	DLSTEP_MAXCOUNT			22

#define	MEMTYPE_SPANSION		1
#define	MEMTYPE_NUMONYX			2

typedef enum
{
	SPRINT			= 1,
	VERIZON			= 2,
	AERIS			= 3,
	BELL			= 4,
	CARDIONET		= 5

} Carrier_Id_Enum;


#pragma pack(1)

struct	MODE_REQ
{
	BYTE	Cmd;
	WORD	Mode;
};

struct	MODE_RSP
{
	BYTE	Cmd;
	WORD	Mode;
};

struct	SPC_REQ
{
	BYTE	Cmd;
	BYTE	Spc[SPCCODESIZE];
};

struct	SPC_RSP
{
	BYTE	Cmd;
	BYTE	boolSpcCodeOk;
};

struct	VERNO_REQ
{
	BYTE	Cmd;
};

struct	VERNO_RSP
{
	BYTE	Cmd;
	char	comp_date[VERNO_DATE_STRLEN];	// Compile date Jun 11 1991
	char	comp_time[VERNO_TIME_STRLEN];	// Compile time hh:mm:ss
	char	rel_date [VERNO_DATE_STRLEN];	// Release date
	char	rel_time [VERNO_TIME_STRLEN];	// Release time
	char	ver_dir  [VERNO_DIR_STRLEN];	// Version directory
	BYTE	scm;							// Station Class Mark
	BYTE	mob_cai_rev;					// CAI rev
	BYTE	mob_model;						// Mobile Model
	WORD	mob_firm_rev;					// Firmware Rev
	BYTE	slot_cycle_index;				// Slot Cycle Index
	BYTE	voc_maj;						// Vocoder major version
	BYTE	voc_min;						// Vocoder minor version
	WORD	Status;
};

struct	NVPACKET
{
	BYTE	Cmd;						// Command Code (ex. NV_WRITE, NV_READ ...)
	WORD	NvItem;						// NV Item Code (ex. esn, verno_maj ...)
	BYTE	ItemData[MAX_NVDATA_SIZE];	// NV Item Data
	WORD	Status;
};

struct	PRLIST_RD_REQ
{
	BYTE	Cmd;			/* Command code */
	BYTE	Seq_num;		/* Sequence number */
	BYTE	Nam;			/* which NAM this is associated with */
};

struct	PRLIST_RD_RSP
{
	BYTE	Cmd;									/* Command code */
	BYTE	RL_status;								/* Status of block, as above */
	WORD	NV_status;								/* Status returned by NV */
	BYTE	Seq_num;								/* Sequence number */
	BYTE	More;									/* More to come? */
	WORD	Num_bits;								/* Number of valid data bits */
	BYTE	Pr_list_data[PRL_BLOCK_SIZE];	/* The block of PR_LIST */
};

struct	PRLIST_WR_REQ
{
	BYTE	Cmd;									/* Command code */
	BYTE	Seq_num;								/* Sequence number */
	BYTE	More;									/* More to come? */
	BYTE	Nam;									/* which NAM this is associated with */
	WORD	Num_bits;								/* length in bits of valid data */
	BYTE	Pr_list_data[PRL_BLOCK_SIZE];	/* The block of PR_LIST */
};

struct	PRLIST_WR_RSP
{
	BYTE	Cmd;			/* Command code */
	BYTE	RL_status;		/* Status of block, as above */
	WORD	NV_status;		/* NV write status if OK_DONE */
};

struct	CHECKLOADER
{
	BYTE	Cmd;
	BYTE	Dummy[DUMMYDATA_SIZE];
};

struct	DOWNLOAD_F
{
	BYTE	Cmd;
};

struct	CODEDIFF_DL_REQ
{
	BYTE	Cmd;
};

struct	CODEDIFF_DL_RSP
{
	BYTE	Cmd;
	BYTE	Response;
};

struct	STATUS_QUERY_REQ
{
	BYTE	Cmd;
};

struct	STATUS_QUERY_RSP
{
	BYTE	Cmd;
	BYTE	Size;
	BYTE	Status[13];
};

struct	DLOAD_QUERY_RSP
{
	BYTE	Cmd;
	BYTE	Size;
	BYTE	Status[MAX_NVDATA_SIZE];
};

struct	PROTOCOL_LOOPBACK_REQ
{
	BYTE	Cmd;
	BYTE	Dummy[10];
};

struct	PROTOCOL_LOOPBACK_RSP
{
	BYTE	Cmd;
	BYTE	Dummy[10];
};

struct	EXT_BUILD_ID_REQ
{
	BYTE	Cmd;
};

struct	EXT_BUILD_ID_RSP
{
	BYTE	Cmd;
	BYTE	Dummy[11];
	BYTE	SoftVer[10];
};

struct	SUBSYS_CMD_VER_2_REQ
{
	BYTE	Cmd;
	BYTE	Dummy[3];
};

struct	SUBSYS_CMD_VER_2_RSP
{
	BYTE	Cmd;
	BYTE	Dummy[11];
};

struct	CNV_FILENAME_REQ
{
	BYTE	Cmd;
	USHORT	Type;
	BYTE	Dummy[9];
	char	FileName[10];
};

struct	CNV_FILENAME_RSP
{
	BYTE	Cmd;
	USHORT	Type;
	BYTE	Dummy[9];
};

struct	SUBSYS_FS_REQ
{
	BYTE	Cmd;
	BYTE	SubsysId;
	USHORT	SubsysCmd;
	DWORD	OpenFlag;
	DWORD	Mode;
	char	Path[24];
};

struct	SUBSYS_FS_RSP
{
	BYTE	Cmd;
	BYTE	SubsysId;
	USHORT	SubsysCmd;
	DWORD	OpenFlag;
	DWORD	Mode;
};

struct	CNV_DOWNLOAD_REQ
{
	BYTE	Cmd;
	USHORT	Type;
	BYTE	Dummy[5];
	UINT	DLSize;
	UINT	Index;
};

struct	CNV_DOWNLOAD_RSP
{
	BYTE	Cmd;
	USHORT	Type;
	BYTE	Dummy1[5];
	UINT	Index;
	UINT	DLSize;
	BYTE	Dummy2[4];
	BYTE	CNVData[516];
};

struct	CNV_UPLOAD_REQ
{
	BYTE	Cmd;
	USHORT	Type;
	BYTE	Dummy[5];
	UINT	ULSize;
	BYTE	CNVData[512];
};

struct	CNV_UPLOAD_RSP
{
	BYTE	Cmd;
	USHORT	Type;
	BYTE	Dummy1[5];
	UINT	Index;
	UINT	ULSize;
	BYTE	Dummy2[4];
};

struct	RAMLOADER_WR_REQ
{
	BYTE	Cmd;
	BYTE	BaseAdd[2];
	BYTE	Offset[2];
	BYTE	Size[2];
	BYTE	Data[RAMLD_DATASIZE];
};

struct	RAMLOADER_WR_RSP
{
	BYTE	Cmd;
};

struct	RAMLOADER_START_REQ
{
	BYTE	Cmd;
	BYTE	BaseAdd[4];
};

struct	RAMLOADER_START_RSP
{
	BYTE	Cmd;
};

struct	HELLO_REQ
{
	BYTE	Cmd;
	BYTE	Magic[32];
	BYTE	Version;
	BYTE	Compatible;
	BYTE	Feature;
};

struct	HELLO_RSP
{
	BYTE	Cmd;
	BYTE	Magic[32];
	BYTE	Version;
	BYTE	Compatible;
	DWORD	MaxBlkSize;
	DWORD	BaseAddress;
	BYTE	FlashIDLength;
	BYTE	FlashID[255];
};

struct	DL_PROTOCOL_REQ
{
	BYTE	Cmd;
	BYTE	Protocol[32];
	BYTE	Dummy[3];
};

struct	DL_PROTOCOL_RSP
{
	BYTE	Cmd;
	BYTE	Dummy[2];
};

struct	READ_REQ
{
	BYTE	Cmd;
	DWORD	Address;
	WORD	Length;
};

struct	READ_RSP
{
	BYTE	Rsp;
	DWORD	Address;
	BYTE	Data[255];
};

struct	SECURITY_MODE_REQ
{
	BYTE	Cmd;
	BYTE	Mode;
};

struct	SECURITY_MODE_RSP
{
	BYTE	Rsp;
};

struct	SPEC_CMD_VER_REQ
{
	BYTE	Cmd;
	BYTE	SubCmd;
};

struct	SPEC_CMD_VER_RSP
{
	BYTE	Cmd;
	BYTE	SubCmd;
	BYTE	Length;
	char	Dummy[255];
};

struct	MEMORY_ERASE_REQ
{
	BYTE	Cmd;
	BYTE	SubCmd;
	BYTE	EraseType;
	BYTE	Length;
	char	PartitionName[6];
};

struct	MEMORY_ERASE_RSP
{
	BYTE	Cmd;
	BYTE	SubCmd;
};

struct	PARTI_TBL_WR_REQ
{
	BYTE	Cmd;
	BYTE	Override;
	BYTE	PartiTbl[DLOADDATA_SIZE];
};

struct	PARTI_TBL_WR_RSP
{
	BYTE	Rsp;
	BYTE	Status;
};

struct	OPEN_MULTI_IMG_REQ
{
	BYTE	Cmd;
	BYTE	Mode;
};

struct	OPEN_MULTI_IMG_PL_REQ
{
	BYTE	Cmd;
	BYTE	Mode;
	BYTE	Binary[DLOADDATA_SIZE];
};

struct	OPEN_MULTI_IMG_RSP
{
	BYTE	Rsp;
	BYTE	Status;
};

struct	CLOSE_MULTI_IMG_REQ
{
	BYTE	Cmd;
};

struct	CLOSE_MULTI_IMG_RSP
{
	BYTE	Rsp;
};

struct	STREAM_WRITE_REQ
{
	BYTE	Cmd;
	DWORD	Address;
	BYTE	Binary[STREAMWRITE_SIZE];
};

struct	STREAM_WRITE_RSP
{
	BYTE	Rsp;
	DWORD	Address;
};

struct	QCSBLHD_WR_REQ
{
	BYTE	Cmd;
	DWORD	Address;
	BYTE	Data[DLOADDATA_SIZE];
};

struct	QCSBLHD_WR_RSP
{
	BYTE	Cmd;
	BYTE	Dummy[7];
};

struct	QCSBL_WR_REQ
{
	BYTE	Cmd;
	DWORD	Address;
	BYTE	Data[DLOADDATA_SIZE];
};

struct	QCSBL_WR_RSP
{
	BYTE	Cmd;
	BYTE	Dummy[7];
};

struct	UNLOCK_REQ
{
	BYTE	Cmd;
	BYTE	Sec_Code[8];
};

struct	UNLOCK_RSP
{
	BYTE	Cmd;
	BYTE	Sec_Code[8];
};

struct	RESET_REQ
{
	BYTE	Cmd;
};

struct	RESET_RSP
{
	BYTE	Cmd;
	WORD	Result;
};

struct	CLOSE_REQ
{
	BYTE	Cmd;
};

struct	CLOSE_RSP
{
	BYTE	Cmd;
};

struct	MODE_CHANGE_REQ
{
	BYTE	Cmd;			/* Command code */
};

struct	MODE_CHANGE_RSP
{
	BYTE	Cmd;			/* Command code */
	BYTE	ModeChanged;	/* Boolean whether mode changed */
};

struct	FLAG_SET_REQ
{
	BYTE	Cmd;
	BYTE	Address[4];
	BYTE	Data[4];
};

struct	FLAG_SET_RSP
{
	BYTE	Cmd;
	BYTE	Address[4];
};

struct	CRC_CHECK_REQ
{
	BYTE	Cmd;
	BYTE	Crc[2];
};

struct	CRC_CHECK_RSP
{
	BYTE	Cmd;
};

struct	FACTORY_RESET_REQ
{
	BYTE	Cmd;
};

struct	FACTORY_RESET_RSP
{
	BYTE	Cmd;
};

struct	READ_MEMTYPE_REQ
{
	BYTE	Cmd;
};

struct	READ_MEMTYPE_RSP
{
	BYTE	Cmd;
	BYTE	MemType[10];
};

// EXT_DM Command 자료구조
struct	EXT_DMCOMMAND
{
	BYTE	Cmd;						// AT_Command Code at DM_Mode (ex.200)
	BYTE	CmdData[MAX_PACKET_SIZE-1];	// AT Command String
};

// AT Command 자료구조
struct	ATCOMMAND
{
	BYTE	CmdData[MAX_PACKET_SIZE];	// AT Command String
};

// DM Packet & AT Command 자료구조 공용체
union	PACKETDATA
{
	MODE_REQ				ModeReq;
	MODE_RSP				ModeRsp;
	SPC_REQ					SpcReq;
	SPC_RSP					SpcRsp;
	VERNO_REQ				VerNoReq;
	VERNO_RSP				VerNoRsp;
	NVPACKET				NvPkt;
	PRLIST_RD_REQ			PrListRdReq;
	PRLIST_RD_RSP			PrListRdRsp;
	PRLIST_WR_REQ			PrListWrReq;
	PRLIST_WR_RSP			PrListWrRsp;
	DOWNLOAD_F				Download_f;

	READ_MEMTYPE_REQ		ReadMemTypeReq;
	READ_MEMTYPE_RSP		ReadMemTypeRsp;

	CODEDIFF_DL_REQ			CodeDiffDlReq;
	CODEDIFF_DL_RSP			CodeDiffDlRsp;
	STATUS_QUERY_REQ		Status_Query_Req;
	STATUS_QUERY_RSP		Status_Query_Rsp;
	DLOAD_QUERY_RSP			DLoad_Query_Rsp;
	PROTOCOL_LOOPBACK_REQ	Protocol_LB_Req;
	PROTOCOL_LOOPBACK_RSP	Protocol_LB_Rsp;
	EXT_BUILD_ID_REQ		ExtBuildIdReq;
	EXT_BUILD_ID_RSP		ExtBuildIdRsp;
	SUBSYS_CMD_VER_2_REQ	SubSysCmdVer2Req;
	SUBSYS_CMD_VER_2_RSP	SubSysCmdVer2Rsp;
	CNV_FILENAME_REQ		CNV_Filename_Req;
	CNV_FILENAME_RSP		CNV_Filename_Rsp;
	SUBSYS_FS_REQ			SubsysFSReq;		
	SUBSYS_FS_RSP			SubsysFSRsp;		
	CNV_DOWNLOAD_REQ		CNV_Download_Req;
	CNV_DOWNLOAD_RSP		CNV_Download_Rsp;
	CNV_UPLOAD_REQ			CNV_Upload_Req;
	CNV_UPLOAD_RSP			CNV_Upload_Rsp;
	RAMLOADER_WR_REQ		RAMLoaderWrReq;
	RAMLOADER_WR_RSP		RAMLoaderWrRsp;
	RAMLOADER_START_REQ		RAMLoaderStartReq;
	RAMLOADER_START_RSP		RAMLoaderStartRsp;
	HELLO_REQ				HelloReq;
	HELLO_RSP				HelloRsp;
	DL_PROTOCOL_REQ			DLProtocolReq;
	DL_PROTOCOL_RSP			DLProtocolRsp;
	READ_REQ				ReadReq;
	READ_RSP				ReadRsp;
	SECURITY_MODE_REQ		SecurityModeReq;
	SECURITY_MODE_RSP		SecurityModeRsp;
	SPEC_CMD_VER_REQ		SpecCmdVerReq;
	SPEC_CMD_VER_RSP		SpecCmdVerRsp;
	MEMORY_ERASE_REQ		MemoryEraseReq;
	MEMORY_ERASE_RSP		MemoryEraseRsp;
	PARTI_TBL_WR_REQ		PartiTblWrReq;
	PARTI_TBL_WR_RSP		PartiTblWrRsp;
	OPEN_MULTI_IMG_REQ		OpenMultiImgReq;
	OPEN_MULTI_IMG_PL_REQ	OpenMultiImgPLReq;
	OPEN_MULTI_IMG_RSP		OpenMultiImgRsp;
	CLOSE_MULTI_IMG_REQ		CloseMultiImgReq;
	CLOSE_MULTI_IMG_RSP		CloseMultiImgRsp;
	STREAM_WRITE_REQ		StreamWrReq;
	STREAM_WRITE_RSP		StreamWrRsp;
	CLOSE_REQ				CloseReq;
	CLOSE_RSP				CloseRsp;
	UNLOCK_REQ				UnlockReq;
	UNLOCK_RSP				UnlockRsp;
	FLAG_SET_REQ			FlagSetReq;
	FLAG_SET_RSP			FlagSetRsp;
	CRC_CHECK_REQ			CrcCheckReq;
	CRC_CHECK_RSP			CrcCheckRsp;
	FACTORY_RESET_REQ		FactoryResetReq;
	FACTORY_RESET_RSP		FactoryResetRsp;
	RESET_REQ				ResetReq;
	RESET_RSP				ResetRsp;

	EXT_DMCOMMAND			ExtDmCmd;
	ATCOMMAND				AtCmd;
};

// DM Packet & AT Command 자료구조
struct	SENDPACKET
{
	BOOL		bCrcHead;
	BYTE		Packet[MAX_PACKET_SIZE];
	int			nPktSize;
	void*		pRecvPktPos;
};

struct	RECVPACKET
{
	BYTE		Packet[MAX_PACKET_SIZE];
	int			nPktSize;
	BOOL		bCrcError;		// 수신된 데이터의 CRC 오류 유무.
};

struct	PACKET
{
	BYTE		Data[MAX_PACKET_SIZE];
	int			nSize;
};

#endif
