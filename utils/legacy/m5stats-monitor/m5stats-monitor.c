#include <errno.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>

static void usage(void);
static int parse_argv(int argc, char *argv[], char **m5stats_file, pid_t *pid);
static int create_file_monitor(const char *file);
static int monitor_file(int fd, pid_t pid);
static int handle_inotify_event(int fd, pid_t pid);
static int read_inotify_event(int fd, struct inotify_event *out);

int
main(int argc, char *argv[])
{
    char *m5stats_file;
    pid_t pid;
    int status = parse_argv(argc, argv, &m5stats_file, &pid);
    if (status)
        return status;

    int fd  = create_file_monitor(m5stats_file);
    if (fd < 0)
        return -fd;

    return monitor_file(fd, pid);
}

int
monitor_file(int fd, pid_t pid)
{
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
        .revents = 0
    };

    for (;;) {
        int status = poll(&pfd, 1, -1);
        if (status < 0)
            return errno;

        // inotify event pending
        status = handle_inotify_event(fd, pid);
        if (status)
            return status;
    }
}

int
handle_inotify_event(int fd, pid_t pid)
{
    struct inotify_event ie;
    int status = read_inotify_event(fd, &ie);
    if (status)
        return status;

    // Check inotify event here
    if (kill(pid, SIGUSR1))
        return errno;

    return 0;
}

int
read_inotify_event(int fd, struct inotify_event *out)
{
    ssize_t len = 0;
    char *buf = (void *)out;

    while (len != sizeof(*out)) {
        ssize_t res = read(fd, &buf[len], sizeof(*out)-len);
        if (res < 0)
            return errno;
        len += res;
    }

    return 0;
}

int
create_file_monitor(const char *file)
{
    int fd = inotify_init();
    if (fd < 0)
        return fd;

    int wd = inotify_add_watch(fd, file, IN_MODIFY|IN_CREATE);
    if (wd < 0)
        return wd;

    return fd;
}

int
parse_argv(int argc, char *argv[], char **m5stats_file, pid_t *pid)
{
    if (argc != 3) {
        usage();
        return EINVAL;
    }

    if (m5stats_file)
        *m5stats_file = argv[1];

    if (pid)
        *pid = atoi(argv[2]);

    return 0;
}

void
usage(void)
{
    fprintf(stderr,
            "Usage: m5stat-monitor PATH PID\n"
            "\n"
            "       PATH   Path to M5 stats.txt\n"
            "       PID    PID of the process to notify. On each modification\n"
            "              to the monitored file SIGUSR1 is sent to PID.\n");
}
