#ifndef _PARAMETERS_H_
#define _PARAMETERS_H_

typedef struct __attribute__((__packed__)) _MAC_ADDR
{
    BYTE v[6];
} MAC_ADDR;

#define IP_ADDR		DWORD_VAL

typedef struct __attribute__((__packed__)) _NODE_INFO
{
    IP_ADDR     IPAddr;
    MAC_ADDR    MACAddr;
} NODE_INFO;

#define EEPROM_HEADER       0xA7

#define OUTLET_COUNT        8

#define NETBIOSNAME_LEN     16
#define PRODUCTTYPE_LEN     16
#define FIRMVERSION_LEN     8
#define SYSTEMNAME_LEN      16
#define USERNAME_LEN        16
#define PASSWORD_LEN        16
#define LOCATIONNAME_LEN    16

#define OUTLETNAME_LEN      12

typedef struct __attribute__((__packed__)) _APP_CONFIG 
{	
	BYTE        header;
    WORD        size;
	
	IP_ADDR		MyIPAddr;
	IP_ADDR		MyMask;
	IP_ADDR		MyGateway;
	IP_ADDR		PrimaryDNSServer;
	IP_ADDR		SecondaryDNSServer;
	IP_ADDR		DefaultIPAddr;
	IP_ADDR		DefaultMask;
	BYTE		NetBIOSName[NETBIOSNAME_LEN];
	struct
	{
		unsigned char : 6;
		unsigned char bIsDHCPEnabled : 1;
		unsigned char bInConfigMode : 1;
	} Flags;
	MAC_ADDR	MyMACAddr;
	
	BYTE        UserName[USERNAME_LEN];
	BYTE        Password[PASSWORD_LEN];
	
	BYTE        sum;
} APP_CONFIG;

extern APP_CONFIG AppConfig;

extern BOOL LoadParameters(void);
extern void SaveParameters(void);
extern void ClearParameters(void);
extern void LoadDefaultParameters(void);

#endif
