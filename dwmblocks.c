#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<signal.h>
#include<X11/Xlib.h>
#define LENGTH(X)               (sizeof(X) / sizeof (X[0]))
#define CMDLENGTH		1024


/* structures */
typedef struct {
    char* command;
    unsigned int interval;
    unsigned int signal;
} Block;

/* configuration */
static const Block blocks[] = {
        /* command        interval  signal */
        { "~/.local/src/dwmblocks/scripts/dwm_date",     3600,     1      },
        { "~/.local/src/dwmblocks/scripts/dwm_time",     60,       2      }
};

static const char* delim = " | ";

/* definitions */
void buttonhandler(int sig, siginfo_t *si, void *ucontext);
void getcmds(int time);
void getcmd(const Block *block, char *output);
int getstatus(char *str, char *last);
#ifndef __OpenBSD__
void getsigcmds(int signal);
void setupsignals();
void sighandler(int signum);
#endif
void replace(char *str, char old, char new);
void sighandler(int num);
void setroot();
void statusloop();
void termhandler(int signum);
void pstdout();

/* variables */
static Display *dpy;
static int screen;
static Window root;
static char statusbar[LENGTH(blocks)][CMDLENGTH] = {0};
static char statusstr[2][1024];
static char exportstring[CMDLENGTH + 22] = "export BLOCK_BUTTON=-;";
static int button = 0;
static int statusContinue = 1;
static void (*writestatus) () = setroot;


/* implementations */
#ifndef __OpenBSD__
void
buttonhandler(int sig, siginfo_t *si, void *ucontext)
{
    button = si->si_value.sival_int & 0xff;
    getsigcmds(si->si_value.sival_int >> 8);
    writestatus();
}

void
sighandler(int signum)
{
    getsigcmds(signum-SIGRTMIN);
    writestatus();
}
#endif

void
replace(char *str, char old, char new)
{
    int N = strlen(str);
    for(int i = 0; i < N; i++)
        if(str[i] == old)
            str[i] = new;
}

/* opens process *cmd and stores output in *output */
void
getcmd(const Block *block, char *output)
{

    if (block->signal)
    {
        output[0] = block->signal;
        output++;
    }
    char* cmd;
    FILE *cmdf;
    if (button)
    {
        cmd = strcat(exportstring, block->command);
        cmd[20] = '0' + button;
        button = 0;
        cmdf = popen(cmd,"r");
        cmd[22] = '\0';
    }
    else
    {
        cmd = block->command;
        cmdf = popen(cmd,"r");
    }
    if (!cmdf)
        return;
    fgets(output, CMDLENGTH, cmdf);
    int i = strlen(output);

    if (!(strlen(delim) == 1 && delim[0] == '\0') && i){
        for(int j = 0; j < strlen(delim); j++){
            if(delim[j] == '\n')
                break;
            output[i++] = delim[j];
        }
    }
    output[i++] = '\0';
    pclose(cmdf);
}

void
getcmds(int time)
{
    const Block* current;
    for(int i = 0; i < LENGTH(blocks); i++)
    {
        current = blocks + i;
        if ((current->interval != 0 && time % current->interval == 0) || time == -1)
            getcmd(current,statusbar[i]);
    }
}

#ifndef __OpenBSD__
void
getsigcmds(int signal)
{
        const Block *current;
        for (int i = 0; i < LENGTH(blocks); i++) {
                current = blocks + i;
                if (signal == 0 || current->signal == signal) {
                        getcmd(current, statusbar[i]);
                }
        }
}

        void
setupsignals()
{
    struct sigaction sa;
    for(int i = 0; i < LENGTH(blocks); i++)
    {
        if (blocks[i].signal > 0)
        {
            signal(SIGRTMIN+blocks[i].signal, sighandler);
            sigaddset(&sa.sa_mask, SIGRTMIN+blocks[i].signal);
        }
    }
    signal(SIGRTMIN+0, sighandler);
    sigaddset(&sa.sa_mask, SIGRTMIN+0);
    sa.sa_sigaction = buttonhandler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &sa, NULL);
}
#endif

int
getstatus(char *str, char *last)
{
    strcpy(last, str);
    str[0] = '\0';
    for(int i = 0; i < LENGTH(blocks); i++)
        strcat(str, statusbar[i]);
    str[strlen(str)-1] = '\0';
    return strcmp(str, last); /* 0 if they are the same */
}

void
setroot()
{
    if (!getstatus(statusstr[0], statusstr[1])) /* Only set root if text has changed. */
        return;
    Display *d = XOpenDisplay(NULL);
    if (d) {
        dpy = d;
    }
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    XStoreName(dpy, root, statusstr[0]);
    XCloseDisplay(dpy);
}

void
pstdout()
{
    if (!getstatus(statusstr[0], statusstr[1])) /* Only write out if text has changed. */
        return;
    fflush(stdout);
}


void
statusloop()
{
#ifndef __OpenBSD__
    setupsignals();
#endif
    int i = 0;
    getcmds(-1);
    while(statusContinue)
    {
        getcmds(i);
        writestatus();
        sleep(1.0);
        i++;
    }
}

void
termhandler(int signum)
{
    statusContinue = 0;
    exit(0);
}

int
main(int argc, char** argv)
{
    for(int i = 0; i < argc; i++)
    {
        if (!strcmp("-d",argv[i]))
            delim = argv[++i];
        else if(!strcmp("-p",argv[i]))
            writestatus = pstdout;
    }
    signal(SIGTERM, termhandler);
    signal(SIGINT, termhandler);
    statusloop();
}
