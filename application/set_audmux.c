/*
 *  @file       test_audmux.c
 *
 *  @brief      This file contains an application program which interact with Helio_Audmux driver for switching the Audmux path 
 *
 *  @author     VVDN TECHNOLOGIES
 *
 *  @license    Copyright (C) 2012 Autonetmobile
 *              This program is free software; you can redistribute it and/or modify it
 *              under the terms of the GNU General Public License version 2 as
 *              published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>


#define HELIO_AUDMUX_MAJOR                          202

#define HELIO_AUDMUX_IOC_MAGIC                      'm'
#define  SET_AUDMUX_SSI0_BT        _IO(HELIO_AUDMUX_IOC_MAGIC,  0)
#define  SET_AUDMUX_SSI0_3G        _IO(HELIO_AUDMUX_IOC_MAGIC,  1)
#define  SET_AUDMUX_3G_BT          _IO(HELIO_AUDMUX_IOC_MAGIC,  2)
#define  SET_AUDMUX_SSI0_3G_BT     _IO(HELIO_AUDMUX_IOC_MAGIC,  3)
#define  GET_AUDMUX_SETTING        _IOR(HELIO_AUDMUX_IOC_MAGIC, 4, struct helio_audmux)


#define DEVICE_FILE_NAME "/dev/helio_audmux"

/*mode  Variable can have following values*/
#define AUDMUX_SSI0_BT          1
#define AUDMUX_SSI0_3G          2
#define AUDMUX_3G_BT            3
#define AUDMUX_SSI0_3G_BT       4
#define READ_AUDMUX		5

struct helio_audmux {
	int mode;	  /*current mode*/
	int master;       /*current master*/
	int slave_tx;	  /*current tx slave*/
	int slave_rx;      /*current rx slave*/
	int synchronous;  /*current SYNC status*/
};


void main(int argc, char *argv[])
{
	int fd,md=0;
	char mod[5][20]={" ", "SSI0<->BT", "SSI0<->3G", "3G<->BT", "SSI0->3G->BT"};
	char mast[3][4]={" ", "3G", "SSI0"};
	char slave[3][5]={" ", "SSI0", "BT"};
	char synch[2][10]={"ASYNCH", "SYNCH"};
	struct helio_audmux admx;
	fd = open(DEVICE_FILE_NAME, 0);
	md=atoi(argv[1]);
	switch(md)
	{
	case AUDMUX_SSI0_BT:
		ioctl(fd, SET_AUDMUX_SSI0_BT, &admx);
		printf("Setting SSI0<->BT\n");
		break;
	case AUDMUX_SSI0_3G:
		ioctl(fd, SET_AUDMUX_SSI0_3G, &admx);
		printf("Setting SSI0<->3G\n");
		break;
	case AUDMUX_3G_BT:
		ioctl(fd, SET_AUDMUX_3G_BT, &admx);
		printf("Setting 3G<->BT\n");
		break;
	case AUDMUX_SSI0_3G_BT:
		ioctl(fd, SET_AUDMUX_SSI0_3G_BT, &admx);
		printf("Setting SSI0->3G->BT\n");
		break;
	case READ_AUDMUX:
		ioctl(fd, GET_AUDMUX_SETTING, &admx);
		printf("mode:%s\t\tmaster:%s\t\tslave_tx:%s\t\tslave_rx:%s\t\tsynchronous:%s\n", mod[admx.mode], mast[admx.master], 
			slave[admx.slave_tx], slave[admx.slave_rx], synch[admx.synchronous]);
		break;
	default:
		break;
	}
	close(fd);
}
