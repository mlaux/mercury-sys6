/* This file is a wrapper around MacTCP and dnr.c. */

#include <Devices.h>
#include <MacTCP.h>
#include <MacTypes.h>
#include <MixedMode.h>

#include "AddressXlation.h"
#include "mercury.h"

#define RECV_BUF_SIZE 8192

static short gRefNum;
static TCPNotifyUPP gNotifyProc;
static ResultUPP gLookupDoneProc;
static Ptr gRecvBuf;

static char *gDataReceivedPtr;

static void mem_fill(char val, void *p, int len)
{
	int k;
	char *cp;
	cp = (char *) p;
	for(k = 0; k != len; ++k)
		*cp++ = val;
}

/* Called when a TCP event occurs */
static pascal void tcp_notify_callback(StreamPtr tcpStream, unsigned short eventCode, 
		Ptr userDataPtr, unsigned short terminReason, ICMPReport *icmpMsg)
{
	if(eventCode == TCPDataArrival && gDataReceivedPtr) {
		*gDataReceivedPtr = 1;
	}
}

/* Called when a DNS lookup finishes */
static pascal void dns_lookup_done(struct hostInfo *info, char *userData)
{
	*userData = 1;
}

OSErr tcp_init(void)
{
	OSErr err;
	err = OpenDriver("\p.IPP", &gRefNum);
	if(err != noErr) {
		return err;
	}
	gRecvBuf = NewPtr(RECV_BUF_SIZE);
	if(gRecvBuf == NULL) {
		return memFullErr;
	}
	gNotifyProc = NewTCPNotifyUPP(tcp_notify_callback);
	return err;
}

StreamPtr tcp_create_stream(void)
{
	TCPiopb pb;
	OSErr err;
	
	pb.ioCRefNum = gRefNum;
	pb.csCode = TCPCreate;
	
	pb.csParam.create.rcvBuff = gRecvBuf;
	pb.csParam.create.rcvBuffLen = RECV_BUF_SIZE;
	pb.csParam.create.notifyProc = gNotifyProc;
	pb.csParam.create.userDataPtr = NULL;
	
	err = PBControl((ParamBlockRec *) &pb, false);
	
	if(err != noErr) {
		te_append("TCPCreate error %d", err);
		return 0;
	}
	
	if(pb.ioResult != noErr) {
		te_append("ioResult %d", pb.ioResult);
		return 0;
	}
	
	return pb.tcpStream;
}

OSErr tcp_write(StreamPtr stream, const char *buf, int len)
{
	wdsEntry wds[2];
	TCPiopb pb;
	OSErr err;
	
	wds[0].length = len;
	wds[0].ptr = (Ptr) buf;
	wds[1].length = 0;
	
	pb.ioCRefNum = gRefNum;
	pb.csCode = TCPSend;
	pb.tcpStream = stream;
	
	mem_fill(0, &pb.csParam.send, sizeof(TCPSendPB));
	
	pb.csParam.send.wdsPtr = (Ptr) wds;
	pb.csParam.send.pushFlag = true;
	
	err = PBControl((ParamBlockRec *) &pb, false);
	
	if(err != noErr) {
		return err;
	}
		
	return pb.ioResult;
}

OSErr tcp_read(StreamPtr stream, Ptr buf, unsigned short *len)
{
	TCPiopb pb;
	OSErr err;
	
	pb.ioCRefNum = gRefNum;
	pb.csCode = TCPRcv;
	pb.tcpStream = stream;
	
	mem_fill(0, &pb.csParam.receive, sizeof(TCPReceivePB));
	
	pb.csParam.receive.rcvBuff = buf;
	pb.csParam.receive.rcvBuffLen = *len;
	
	err = PBControl((ParamBlockRec *) &pb, false);
	
	if(err != noErr) {
		return err;
	}
	
	if(pb.ioResult == noErr) {
		*len = pb.csParam.receive.rcvBuffLen;
	} else {
		*len = 0;
	}
	
	return pb.ioResult;
}

unsigned short tcp_available(StreamPtr stream)
{
	TCPiopb pb;
	OSErr err;
	
	pb.ioCRefNum = gRefNum;
	pb.csCode = TCPStatus;
	pb.tcpStream = stream;
	
	mem_fill(0, &pb.csParam.status, sizeof(TCPStatusPB));
		
	err = PBControl((ParamBlockRec *) &pb, false);
	
	if(err != noErr) {
		te_append("TCPStatus error %d", err);
		return 0;
	}
		
	return pb.csParam.status.amtUnreadData;
}

OSErr tcp_release_stream(StreamPtr stream)
{
	TCPiopb pb;
	
	pb.ioCRefNum = gRefNum;
	pb.csCode = TCPRelease;
	pb.tcpStream = stream;

	pb.csParam.create.rcvBuff = gRecvBuf;
	pb.csParam.create.rcvBuffLen = RECV_BUF_SIZE;
	pb.csParam.create.notifyProc = gNotifyProc;
	pb.csParam.create.userDataPtr = NULL;
	
	return PBControl((ParamBlockRec *) &pb, false);
}

OSErr tcp_connect(StreamPtr stream, ip_addr ip, tcp_port port)
{
	TCPiopb pb;
	OSErr err;
	
	pb.ioCRefNum = gRefNum;
	pb.csCode = TCPActiveOpen;
	pb.tcpStream = stream;
	
	mem_fill(0, &pb.csParam.open, sizeof(TCPOpenPB));
	
	pb.csParam.open.remoteHost = ip;
	pb.csParam.open.remotePort = port;
	
	err = PBControl((ParamBlockRec *) &pb, false);
	
	if(err != noErr) {
		return err;
	}
	
	return pb.ioResult;
}

void tcp_cleanup(void)
{
	DisposeRoutineDescriptor(gNotifyProc);
	DisposePtr(gRecvBuf);
}

OSErr dns_init(const char *hosts_file)
{
	return OpenResolver((char *) hosts_file);
}

ip_addr dns_lookup(const char *host)
{
	hostInfo info;
	OSErr err;
	char done;
	
	done = 0;
	if(gLookupDoneProc == 0) {
		gLookupDoneProc = NewResultProc(dns_lookup_done);
	}
	err = StrToAddr((char *) host, &info, gLookupDoneProc, &done);
	
	if(err == noErr) {
		return info.addr[0];
	} else if(err == cacheFault) {
		while(!done)
			;
		
		return info.addr[0];
	} else {
		return 0;
	}
}

const char *dns_format_ip(unsigned long ip)
{
	static char str[16];
	int err;
	
	err = AddrToStr(ip, str);
	if(err != noErr) {
		return 0;
	}
	return str;
}

OSErr dns_cleanup(void)
{
	DisposeRoutineDescriptor(gLookupDoneProc);
	return CloseResolver();
}
