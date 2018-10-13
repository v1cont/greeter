#ifndef __GREET_H__
#define __GREET_H__

#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>
#include <stdlib.h>
#include <time.h>

#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 1
#endif
#include <unistd.h>

#ifdef USE_PAM
#include <security/pam_appl.h>
#endif

#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Xfuncproto.h>
#include <X11/Xmd.h>
#include <X11/Xauth.h>
#include <X11/Intrinsic.h>
#include <X11/Xdmcp.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>

#define MIN_UID 100

#define BORDERS 5

#define WELCOME "<span font_desc=\"Sans Bold Italic 24\" foreground=\"blue\">Welcome to %s</span>"

#define USERLABEL   "<i>Username:</i>"
#define PASSLABEL   "<i>Password:</i>"
#define SESSLABEL   "<i>Session:</i>"

#define ICONNAME    "computer"

#define REBOOT_CMD     "/sbin/reboot"
#define POWEROFF_CMD   "/sbin/poweroff"
#define SUSPEND_CMD    "echo mem > /sys/power/state"
#define HIBERNATE_CMD  "echo disk > /sys/power/state"

#define DEFAULT_PATH   "/bin:/usr/bin:/usr/local/bin:/opt/bin:/sbin:/usr/sbin"

typedef struct {
  gint min_uid;
  gboolean auto_login;
  gchar *auto_user;
  gboolean allow_root;
  gchar *welcome;
  gchar *rcfile;
  gchar *user_lbl;
  gchar *pwd_lbl;
  gchar *sess_lbl;
  gchar *session;
  gchar *icon;
  gchar *path;
  gint borders;
  gint width;
  gchar *reboot_cmd;
  gchar *poweroff_cmd;
  gchar *suspend_cmd;
  gchar *hibernate_cmd;
} ConfigData;

/*** xdm definitions ***/

typedef struct displayType {
  unsigned int location:1;
  unsigned int lifetime:1;
  unsigned int origin:1;
} DisplayType;

#define Local          1        /* server runs on local host */
#define Foreign        0        /* server runs on remote host */

#define Permanent      1        /* session restarted when it exits */
#define Transient      0        /* session not restarted when it exits */

#define FromFile       1        /* started via entry in servers file */
#define FromXDMCP      0        /* started with XDMCP */

typedef enum displayStatus {
  running,
  notRunning,
  zombie,
  phoenix
} DisplayStatus;

typedef enum fileState {
  NewEntry,
  OldEntry,
  MissingEntry
} FileState;

struct display {
  struct display *next;
  /* Xservers file / XDMCP information */
  char *name;                   /* DISPLAY name */
  char *class;                  /* display class (may be NULL) */
  DisplayType displayType;      /* method to handle with */
  char **argv;                  /* program name and arguments */
  /* display state */
  DisplayStatus status;         /* current status */
  pid_t pid;                    /* process id of child */
  pid_t serverPid;              /* process id of server (-1 if none) */
  FileState state;              /* state during HUP processing */
  int startTries;               /* current start try */
  time_t lastReserv;            /* time of last reserver crash */
  int reservTries;              /* current reserver try */
  /* XDMCP state */
  CARD32 sessionID;             /* ID of active session */
  XdmcpNetaddr peer;            /* display peer address */
  int peerlen;                  /* length of peer address */
  XdmcpNetaddr from;            /* XDMCP port of display */
  int fromlen;
  CARD16 displayNumber;
  int useChooser;               /* Run the chooser for this display */
  ARRAY8 clientAddr;            /* for chooser picking */
  CARD16 connectionType;        /* ... */
  int xdmcpFd;
  /* server management resources */
  int serverAttempts;           /* number of attempts at running X */
  int openDelay;                /* open delay time */
  int openRepeat;               /* open attempts to make */
  int openTimeout;              /* abort open attempt timeout */
  int startAttempts;            /* number of attempts at starting */
  int reservAttempts;           /* allowed start-IO error sequences */
  int pingInterval;             /* interval between XSync */
  int pingTimeout;              /* timeout for XSync */
  int terminateServer;          /* restart for each session */
  int grabServer;               /* keep server grabbed for Login */
  int grabTimeout;              /* time to wait for grab */
  int resetSignal;              /* signal to reset server */
  int termSignal;               /* signal to terminate server */
  int resetForAuth;             /* server reads auth file at reset */
  char *keymaps;                /* binary compat with DEC */
  char *greeterLib;             /* greeter shared library name */
  /* session resources */
  char *resources;              /* resource file */
  char *xrdb;                   /* xrdb program */
  char *setup;                  /* Xsetup program */
  char *startup;                /* Xstartup program */
  char *reset;                  /* Xreset program */
  char *session;                /* Xsession program */
  char *userPath;               /* path set for session */
  char *systemPath;             /* path set for startup/reset */
  char *systemShell;            /* interpreter for startup/reset */
  char *failsafeClient;         /* a client to start when the session fails */
  char *chooser;                /* chooser program */
  /* authorization resources */
  int authorize;                /* enable authorization */
  char **authNames;             /* authorization protocol names */
  unsigned short *authNameLens; /* authorization protocol name lens */
  char *clientAuthFile;         /* client specified auth file */
  char *userAuthDir;            /* backup directory for tickets */
  int authComplain;             /* complain when no auth for XDMCP */
  /* information potentially derived from resources */
  int authNameNum;              /* number of protocol names */
  Xauth **authorizations;       /* authorization data */
  int authNum;                  /* number of authorizations */
  char *authFile;               /* file to store authorization in */

  int version;                  /* to keep dynamic greeter clued in */
  /* add new fields only after here.  And preferably at the end. */

  /* Hack for making "Willing to manage" configurable */
  char *willing;                /* "Willing to manage" program */
  Display *dpy;                 /* Display */
  char *windowPath;             /* path to server "window" */
};

struct dlfuncs {
  int (*_PingServer) (struct display * d, Display * alternateDpy);
  void (*_SessionPingFailed) (struct display * d);
  void (*_Debug) (const char *fmt, ...);
  void (*_RegisterCloseOnFork) (int fd);
  void (*_SecureDisplay) (struct display * d, Display * dpy);
  void (*_UnsecureDisplay) (struct display * d, Display * dpy);
  void (*_ClearCloseOnFork) (int fd);
  void (*_SetupDisplay) (struct display * d);
  void (*_LogError) (const char *fmt, ...);
  void (*_SessionExit) (struct display * d, int status, int removeAuth);
  void (*_DeleteXloginResources) (struct display * d, Display * dpy);
  int (*_source) (char **environ, char *file);
  char **(*_defaultEnv) (void);
  char **(*_setEnv) (char **e, char *name, char *value);
  char **(*_putEnv) (const char *string, char **env);
  char **(*_parseArgs) (char **argv, char *string);
  void (*_printEnv) (char **e);
  char **(*_systemEnv) (struct display * d, char *user, char *home);
  void (*_LogOutOfMem) (const char *fmt, ...);
  void (*_setgrent) (void);     /* no longer used */
  struct group *(*_getgrent) (void);    /* no longer used */
  void (*_endgrent) (void);     /* no longer used */
  struct spwd *(*_getspnam) (const char *);
  void (*_endspent) (void);
  struct passwd *(*_getpwnam) (const char *);
  void (*_endpwent) (void);
  char *(*_crypt) (const char *, const char *);
#ifdef USE_PAM
  pam_handle_t **(*_thepamhp) (void);
#endif
};

typedef enum {
  Greet_Session_Over = 0,       /* session managed and over */
  Greet_Success = 1,            /* greet succeeded, session not managed */
  Greet_Failure = -1            /* greet failed */
} greet_user_rtn;

struct greet_info {
  char *name;                   /* user name */
  char *password;               /* user password */
  char *string;                 /* random string */
  char *passwd;                 /* binary compat with DEC */
  int version;                  /* for dynamic greeter to see */
  /* add new fields below this line, and preferably at the end */
  Boolean allow_null_passwd;    /* allow null password on login */
  Boolean allow_root_login;     /* allow direct root login */
};

struct verify_info {
  int uid;                      /* user id */
  int gid;                      /* group id */
  char **argv;                  /* arguments to session */
  char **userEnviron;           /* environment for session */
  char **systemEnviron;         /* environment for startup/reset */
  int version;                  /* for dynamic greeter to see */
  /* add new fields below this line, and preferably at the end */
};

/*
 * Force the shared library to call through the function pointer
 * initialized during the initial call into the library.
 */

#define	PingServer		(*__xdm_PingServer)
#define	SessionPingFailed	(*__xdm_SessionPingFailed)
#define	Debug			(*__xdm_Debug)
#define	RegisterCloseOnFork	(*__xdm_RegisterCloseOnFork)
#define	SecureDisplay		(*__xdm_SecureDisplay)
#define	UnsecureDisplay		(*__xdm_UnsecureDisplay)
#define	ClearCloseOnFork	(*__xdm_ClearCloseOnFork)
#define	SetupDisplay		(*__xdm_SetupDisplay)
#define	LogError		(*__xdm_LogError)
#define	SessionExit		(*__xdm_SessionExit)
#define	DeleteXloginResources	(*__xdm_DeleteXloginResources)
#define	source			(*__xdm_source)
#define	defaultEnv		(*__xdm_defaultEnv)
#define	setEnv			(*__xdm_setEnv)
#define	putEnv			(*__xdm_putEnv)
#define	parseArgs		(*__xdm_parseArgs)
#define	printEnv		(*__xdm_printEnv)
#define	systemEnv		(*__xdm_systemEnv)
#define	LogOutOfMem		(*__xdm_LogOutOfMem)
#define	setgrent		(*__xdm_setgrent)
#define	getgrent		(*__xdm_getgrent)
#define	endgrent		(*__xdm_endgrent)
#define	getspnam		(*__xdm_getspnam)
#define	endspent		(*__xdm_endspent)
#define	getpwnam		(*__xdm_getpwnam)
#define	endpwent		(*__xdm_endpwent)
#define	crypt			(*__xdm_crypt)
#ifdef USE_PAM
# define thepamhp		(*__xdm_thepamhp)
#endif

#endif /* __GREET_H__ */
