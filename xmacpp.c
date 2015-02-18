#include <MacTCP.h>
#include <Limits.h>
#include <MacTypes.h>
#include <Resources.h>
#include <QuickDraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Windows.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Menus.h>
#include <Devices.h>
#include <ToolUtils.h>
#include <MacMemory.h>
#include <Files.h>
#include <OSUtils.h>
#include <DiskInit.h>
#include <Traps.h>
#include <Controls.h>
#include <ControlDefinitions.h>
#include <Sound.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "irc.h"
#include "netutil.h"
#include "tcp.h"

#define MBAR_MENU 128

#define MENU_APPLE 128
#define MENU_CONNECTION 129
#define MENU_CHANNEL 130

#define MENU_APPLE_ABOUT 1
#define MENU_CONNECTION_QUIT 1

#define DLOG_CONN_INFO 128
#define DLOG_CONN_INFO_CONNECT 1
#define DLOG_CONN_INFO_QUIT 2
#define DLOG_CONN_INFO_HOST 6
#define DLOG_CONN_INFO_PORT 7
#define DLOG_CONN_INFO_NICK 8

#define ALRT_WARNING 128

#define HOST_MAX 64
#define NICK_MAX 32
#define CHAN_MAX 32

#define HANDLER_TYPES (const char *, const char *, const char *, const char *, const char **, int)
#define HANDLER_FN(name) void handle_ ## name HANDLER_TYPES

// TODO: fix this macro
#define COMMAND(name) { #name , handle_ ## name }

typedef struct ConnectionInfo {
	char host[HOST_MAX];
	tcp_port port;
	char nick[NICK_MAX];
} ConnectionInfo;

typedef struct IncomingCommand {
	const char *command;
	void (*handler) HANDLER_TYPES;
} IncomingCommand;

typedef struct Tab {
	TEHandle textEdit;
	ControlHandle scrollBar;
	ControlActionUPP scrollProc;
	char name[CHAN_MAX];
} Tab;

#define TE(tab) (**(tab)->textEdit)

// No longer defined in the library
QDGlobals qd;

StreamPtr gStream;
WindowPtr gMainWindow;
ControlActionUPP gScrollProc;

Tab gServerTab;
Tab *gCurrentTab;

#define NUM_COMMAND_HANDLERS 2

HANDLER_FN(PING);
HANDLER_FN(NOTICE);

IncomingCommand command_handlers[NUM_COMMAND_HANDLERS] = {
	{ "PING", handle_PING },
	{ "NOTICE", handle_NOTICE }
	//COMMAND(PING),
	//COMMAND(NOTICE)
};

void strncpy_s(char *dest, const char *src, int n)
{
	strncpy(dest, src, n);
	dest[n - 1] = 0;
}

void toolbox_init(void)
{
	InitGraf(&qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs(NULL);
	InitCursor();
}

void menu_init(void)
{
	Handle mbar;
	
	mbar = GetNewMBar(MBAR_MENU);
	SetMenuBar(mbar);
	DisposeHandle(mbar);
	AppendResMenu(GetMenuHandle(MENU_APPLE), 'DRVR');
	DrawMenuBar();
}

void window_init(void)
{
	Rect wind_bounds;

	wind_bounds.top = 80;
	wind_bounds.left = 80;
	wind_bounds.bottom = 680;
	wind_bounds.right = 880;
	
	gMainWindow = NewWindow(NULL, &wind_bounds, "\pMercury", true, documentProc, (WindowPtr) -1, true, 0);
	SetPort(gMainWindow);
}

void cleanup_and_exit(void)
{
	if(gStream) {
		tcp_release_stream(gStream);
	}
	if(gMainWindow) {
		DisposeWindow(gMainWindow);
	}
	tcp_cleanup();
	dns_cleanup();
	ExitToShell();
}

void te_addline(const char *line, TEHandle te)
{
	static const char newline = '\r';
	int len = strlen(line);
	TEInsert((char *) line, len, te);
	TEInsert(&newline, 1, te);
	TESelView(te);
}

void log(const char *fmt, ...)
{
	static char message[IRC_LINE_MAX];
	va_list list;
    
	va_start(list, fmt);
	vsprintf(message, fmt, list);
	va_end(list);
	
	te_addline(message, gCurrentTab->textEdit);
}

StreamPtr connect(const char *host, const unsigned short port)
{
	StreamPtr stream;
	ip_addr ip;
	OSErr err;
	
	ip = dns_lookup(host);
	if(ip == 0) {
		log("Could not look up %s", host);
		return 0;
	}
	log("Connecting to host %s on port %d...", dns_format_ip(ip), port);
	stream = tcp_create_stream();
	if(stream == 0) {
		log("stream failed");
		return 0;
	}
	err = tcp_connect(stream, ip, port);
	if(err != noErr) {
		log("Connection failed with error code %d.", err);
	}
	return stream;
}

char *extract_prefix(char *line, char **prefix, char **user, char **host)
{
	char *sep;
	*prefix = NULL;
	*user = NULL;
	*host = NULL;
	
	if(*line == ':') {
		*prefix = line + 1;
		line = strchr(line, ' ');
		*line++ = 0; // null-terminate and advance past prefix
		
		// If it has !, we need to extract user and host
		if((sep = strchr(*prefix, '!')) != NULL) {
			*sep++ = 0; // null-terminate nick
			*user = sep;
			sep = strchr(sep, '@');
			*sep++ = 0;
			*host = sep;
		}
	}
	return line;
}

void handle_numeric(const char *prefix, int numeric, const char *params[], int nParams)
{
	switch(numeric) {
		default:
			if(nParams > 0) {
				log("%d %s", numeric, params[nParams - 1]);
			}
			break;
	}
}

void handle_PING(const char *prefix, const char *user, const char *host, 
					const char *command, const char **params, int nParams)
{
	send_pong(gStream, params[0]);
}

void handle_NOTICE(const char *prefix, const char *user, const char *host, 
					const char *command, const char **params, int nParams)
{
	log("<%s> %s", prefix, params[nParams - 1]);
}

int dispatch_command(const char *prefix, const char *user, const char *host, 
						const char *command, const char **params, int nParams)
{
	int k;
	for(k = 0; k < NUM_COMMAND_HANDLERS; ++k) {
		IncomingCommand *handler = &command_handlers[k];
		if(strcmp(handler->command, command) == 0) {
			handler->handler(prefix, user, host, command, params, nParams);
			return 1;
		}
	}
	return 0;
}

void process_line(char *line)
{
	char *prefix, *user, *host;
	char *sp, *command;
	char *lastParam;
	char *params[IRC_PARAM_MAX];
	int k, nParams = 0;
	long numeric;
		
	// get prefix and advance line pointer
	line = extract_prefix(line, &prefix, &user, &host);
	
	sp = strchr(line, ' ');
	*sp++ = 0;
	command = line;
	line = sp;
	
	lastParam = strchr(line, ':');
	if(lastParam) {
		++lastParam;
	}
	
	while((sp = strchr(line, ' ')) != NULL && (lastParam == NULL || sp < lastParam)) {
		*sp++ = 0;
		params[nParams++] = line;
		line = sp;
	}
	
	if(lastParam) {
		params[nParams++] = lastParam;
	}
	
	// check if command is a numeric
	// we must explicitly specify 10 for the base since lots of numerics
	// start with 0, which a 0 base parameter would interpret as octal
	numeric = strtol(command, NULL, 10);
	if(numeric != 0) {
		handle_numeric(prefix, numeric, params, nParams);
	} else {
		if(!dispatch_command(prefix, user, host, command, params, nParams)) {
			// line has long been butchered and filled with \0s
			// TODO: log the original line
			// log(line);
		}
	}
	
}

void scroll_proc(ControlRef theControl, ControlPartCode partCode)
{

}

void handle_te_click(Point pt)
{
	GlobalToLocal(&pt);
	if(PtInRect(pt, &(**gCurrentTab->textEdit).viewRect)) {
		TEClick(pt, false, gCurrentTab->textEdit);
	}
}

void handle_control_click(WindowPtr window, Point where)
{
	ControlPartCode code;
	ControlHandle control;
	short value;
	
	GlobalToLocal(&where);
	
	code = FindControl(where, window, &control);
	switch(code) {
		case 0:
			break;
		case kControlIndicatorPart:
			value = GetControlValue(control);
			code = TrackControl(control, where, NULL);
			if(code != 0) {
				
			}
			break;
		default:
			TrackControl(control, where, gScrollProc);
			break;
		
	}
}

void menu_command(long action)
{
	short menu;
	short item;
	if(action > 0) {
		menu = HiWord(action);
		item = LoWord(action);
		switch(menu) {
			case MENU_APPLE:
				if(item == MENU_APPLE_ABOUT) {
					SysBeep(1);
				}
				break;
				
			case MENU_CONNECTION:
				if(item == MENU_CONNECTION_QUIT) {
					cleanup_and_exit();
				}
				break;
				
			case MENU_CHANNEL:
				break;
		}
		HiliteMenu(0);
	}
}

void handle_mouse_down(EventRecord *event, WindowPtr window, WindowPartCode clicked_part)
{
	switch(clicked_part) {
		case inMenuBar:
			menu_command(MenuSelect(event->where));
			break;
		case inDrag:
			DragWindow(window, event->where, &qd.screenBits.bounds);
			break;
		case inGoAway:
			if(TrackGoAway(window, event->where)) {
				DisposeWindow(window);
			}
			break;
		case inContent:
			if (window != FrontWindow()) {
				SelectWindow(window);
			} else {
				handle_te_click(event->where);
				handle_control_click(window, event->where);
			}
			break;
		case inSysWindow:
			SystemClick(event, window);
			break;
		
	}
}

void ask_for_deets(ConnectionInfo *ci)
{
	DialogPtr dlg;
	DialogItemIndex hit;
	DialogItemType type;
	Handle host_edit;
	Handle port_edit;
	Handle nick_edit;
	Rect box;
	
	dlg = GetNewDialog(DLOG_CONN_INFO, NULL, (WindowPtr) -1);
	
	GetDialogItem(dlg, DLOG_CONN_INFO_HOST, &type, &host_edit, &box);
	GetDialogItem(dlg, DLOG_CONN_INFO_PORT, &type, &port_edit, &box);
	GetDialogItem(dlg, DLOG_CONN_INFO_NICK, &type, &nick_edit, &box);

	ShowWindow(dlg);
	ModalDialog(NULL, &hit);

	if(hit == DLOG_CONN_INFO_CONNECT) {
		Str255 host, port, nick;
		char *c_host, *c_port, *c_nick;
		
		GetDialogItemText(host_edit, host);
		GetDialogItemText(port_edit, port);
		GetDialogItemText(nick_edit, nick);
		
		c_host = P2CStr(host);
		c_port = P2CStr(port);
		c_nick = P2CStr(nick);
		
		strncpy_s(ci->host, c_host, HOST_MAX);
		strncpy_s(ci->nick, c_nick, NICK_MAX);
		
		ci->port = atoi(c_port);
		
		DisposeDialog(dlg);
	} else {
		cleanup_and_exit();
	}
}

int get_window_width(WindowPtr wp)
{
	return wp->portRect.right - wp->portRect.left;
}

int get_window_height(WindowPtr wp)
{
	return wp->portRect.bottom - wp->portRect.top;
}

#define FONT "\pMonaco"

#define TABS_WIDTH 100
#define INPUT_HEIGHT 20
#define TEXT_MARGIN 10
#define SCROLL_WIDTH 16

/* TODO: Make sure TEStyleNew and TESetStyle are supported on System 6 */
void tab_new(Tab *tab, const char *name)
{
	short fontNum;
	TextStyle style;
	Rect rect = gMainWindow->portRect;
	
	rect.left += TABS_WIDTH + TEXT_MARGIN;
	rect.top += TEXT_MARGIN;
	rect.right -= SCROLL_WIDTH + TEXT_MARGIN;
	rect.bottom -= INPUT_HEIGHT + TEXT_MARGIN;
	
	strncpy_s(tab->name, name, CHAN_MAX);
	tab->textEdit = TEStyleNew(&rect, &rect);
	TEAutoView(true, tab->textEdit);
	GetFNum(FONT, &fontNum);
	style.tsFont = fontNum;
	TESetStyle(doFont, &style, true, tab->textEdit);
	TEActivate(tab->textEdit);
	
	rect = gMainWindow->portRect;
	rect.left = rect.right - SCROLL_WIDTH + 1;
	rect.right++;
	rect.bottom -= (SCROLL_WIDTH - 2); // make borders coincide with grow thing
	
	tab->scrollBar = NewControl(gMainWindow, &rect, "\p", true, 50, 0, 100, scrollBarProc, 0);
}

void tab_make_current(Tab *tab)
{
	if(gCurrentTab) {
		TEDeactivate(gCurrentTab->textEdit);
	}
	gCurrentTab = tab;
	TEActivate(gCurrentTab->textEdit);
}

int main(void)
{
	EventRecord event;
	ConnectionInfo connection;
	
	toolbox_init();
	menu_init();
	tcp_init();
	dns_init(NULL);
	
	ask_for_deets(&connection);
	window_init();
	
	gScrollProc = NewControlActionUPP(scroll_proc);
	
	tab_new(&gServerTab, connection.host);
	tab_make_current(&gServerTab);
	
	gStream = connect(connection.host, connection.port);
	send_userinfo(gStream, connection.nick, "hello", "test client");
	
	while(1) {
		WindowPtr window;
		WindowPartCode clicked_part;
		Boolean got_event;
		char key;
		
		got_event = WaitNextEvent(everyEvent, &event, LONG_MAX, NULL);
		if(got_event) {
			switch(event.what) {
				case mouseDown:
					clicked_part = FindWindow(event.where, &window);
					handle_mouse_down(&event, window, clicked_part);
					break;
				case keyDown:
				case autoKey: 
					key = event.message & charCodeMask;
					if (event.modifiers & cmdKey) {
						if (event.what == keyDown) {
							menu_command(MenuKey(key));
						}
					} else {
						// TODO: Add a text box for command/text entry
					}
					break;
				case updateEvt:
					window = (WindowPtr) event.message;
					BeginUpdate(window);
					if(window == gMainWindow) {
						EraseRect(&(**gCurrentTab->textEdit).viewRect);
						TEUpdate(&window->portRect, gCurrentTab->textEdit);
						DrawControls(window);
						DrawGrowIcon(window);
					}
					EndUpdate(window);
					break;
				case nullEvent:
					spoll(gStream, process_line);
					break;
				
			}
		} else {
			spoll(gStream, process_line);
		}
	}
	
	return 0;
}
