#   File:       Mercury.make
#   Target:     Mercury
#   Created:    Saturday, February 7, 2015 04:30:15 PM


MAKEFILE        = Makefile

ObjDir          = :
Includes        = 

Sym-68K         = -sym off

COptions        = {Includes} {Sym-68K} -model near -noMapCR


### Source Files ###

SrcFiles        = tcp.c �
				  dnr.c �
				  irc.c �
				  netutil.c �
				  str_compat.c �
				  mercury.c


### Object Files ###

ObjFiles-68K    = "{ObjDir}tcp.c.o" �
				  "{ObjDir}dnr.c.o" �
				  "{ObjDir}irc.c.o" �
				  "{ObjDir}netutil.c.o" �
				  "{ObjDir}str_compat.c.o" �
				  "{ObjDir}mercury.c.o"


### Libraries ###

LibFiles-68K    = "{Libraries}MathLib.o" �
				  "{CLibraries}StdCLib.o" �
				  "{Libraries}MacRuntime.o" �
				  "{Libraries}IntEnv.o" �
				  "{Libraries}ToolLibs.o" �
				  "{Libraries}Interface.o"

RezFiles        = mercury.r
				  

### Default Rules ###

.c.o  �  .c
	{C} {depDir}{default}.c -o {targDir}{default}.c.o {COptions}
	
.r    �  .rc
	DeRez {depDir}{default}.rc > {targDir}{default}.r


### Build Rules ###

Mercury  ��  {ObjFiles-68K} {LibFiles-68K} {RezFiles}
	ILink �
		-o {Targ} �
		{ObjFiles-68K} �
		{LibFiles-68K} �
		{Sym-68K} �
		-mf -d �
		-t 'APPL' �
		-c 'oioi' �
		-model near �
		-state rewrite �
		-compact -pad 0
	Rez -rd -o {Targ} mercury.r -append
	If "{Sym-68K}" =~ /-sym �[nNuU]�/
		ILinkToSYM {Targ}.NJ -mf -sym 3.2 -c 'sade'
	End


### Required Dependencies ###

"{ObjDir}tcp.c.o"  �  tcp.c
"{ObjDir}dnr.c.o"  �  dnr.c
"{ObjDir}irc.c.o"  �  irc.c
"{ObjDir}netutil.c.o" � netutil.c
"{ObjDir}str_compat.c.o" � str_compat.c
"{ObjDir}mercury.c.o" � mercury.c
"{ObjDir}mercury.r" � mercury.rc