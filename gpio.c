#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

/*

    Relevant portions from: ioreg -c IOSerialBSDClient

    | |   |               {
    | |   |                 "IOClass" = "IOSerialBSDClient"
    | |   |                 "CFBundleIdentifier" = "com.apple.iokit.IOSerialFamily"
    | |   |                 "IOProviderClass" = "IOSerialStreamSync"
    | |   |                 "IOTTYBaseName" = "usbmodem"
    | |   |                 "IOSerialBSDClientType" = "IOModemSerialStream"
    | |   |                 "IOProbeScore" = 1000
    | |   |                 "IOCalloutDevice" = "/dev/cu.usbmodem411"
    | |   |                 "IODialinDevice" = "/dev/tty.usbmodem411"
    | |   |                 "IOMatchCategory" = "IODefaultMatchCategory"
    | |   |                 "IOTTYDevice" = "usbmodem411"
    | |   |                 "IOResourceMatch" = "IOBSD"
    | |   |                 "IOTTYSuffix" = "411"
    | |   |               }
*/

/*
  From maglev.pdf:
	The relationship between rotation & output pulses signal from 3rd wire are as follows: (a) 1 Rotation=2 Pulses(4 poles' motor)
	(b) 1 Rotation=3 Pulses(6 poles' motor)
	(c) 1 Rotation=4 Pulses(8 poles' motor)

	4 pulses per rotation

	1 pulse =

  V-----V

  +--+  +
	|  |  |
	|  |  |
	+  +--+
*/

const char gpio_devname[] = "/dev/cu.usbmodem411";
const int pulses_per_rotation = 4;
static int early_usec = 2000;
static int gpio_no = 0;

void syntax()
{
	printf("gpio <set | clr> <gpio num>\n");
}

int set(int dev, int io)
{
	char buf[32];
	sprintf(buf, "gpio set 0%d\r", io);
	return write(dev, buf, strlen(buf));
}

int clr(int dev, int io)
{
	char buf[32];
	sprintf(buf, "gpio clear 0%d\r", io);
	return write(dev, buf, strlen(buf));
}

int ms_per_signal(int rps)
{
	int ms_per_rotation = 1000 / rps;
	return ms_per_rotation / (2 * pulses_per_rotation);
}

int open_dev()
{
	int dev;
	dev = open(gpio_devname, O_WRONLY | O_NONBLOCK);

	if (dev == -1) {
		fprintf(stderr, "failed to open %s: %s\n", gpio_devname, strerror(errno));
		return -1;
	}
	return dev;
}

enum STATE {SET, UNSET};

void pulse(int dev, int rps) {
	struct timespec ts;
	struct timeval tv;
	struct timeval interval;
	struct timeval target;
	struct timeval diff;
	struct timeval now;
	int each_ms = ms_per_signal(rps);
	enum STATE state = UNSET;

	interval.tv_sec = 0;
	interval.tv_usec = each_ms * 1000;

	printf("signal every %d ms\n", each_ms);

  // This is now
	gettimeofday(&tv, NULL);
	// This is our first wake
	timeradd(&tv, &interval, &target);


	while (1) {
		// Get good now
		gettimeofday(&now, NULL);
		// How much to sleep until target
		timersub(&target, &now, &diff);

		if (!diff.tv_sec) { // This essentially says "if it's not later"
			//printf("sleep for %lu:%d\n", diff.tv_sec, diff.tv_usec);

			// Sleep
			if (diff.tv_usec > early_usec) {
				usleep(diff.tv_usec - early_usec);
			}

			gettimeofday(&now, NULL);
			while (timercmp(&now, &target, <)) {
				gettimeofday(&now, NULL);
			}
		}

		// When did we wake
		gettimeofday(&now, NULL);

    // How much late are we
		timersub(&now, &target, &diff);
		//printf("[%lu:%d] %d ms late\n", target.tv_sec, target.tv_usec, diff.tv_usec / 1000);
		if (state == SET) {
			clr(dev, gpio_no);
			state = UNSET;
		} else {
			set(dev, gpio_no);
			state = SET;
		}

    // temp hold the target for the add
		while (timercmp(&target, &now, <=)) {
			tv = target;
			timeradd(&tv, &interval, &target);
		}
		// now we should have a new target
	}

}

int main(int argc, char *argv[])
{
	int i, io;
	const char* cmd = "none";
	int dev;
	int rps = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "set") ||
	      !strcmp(argv[i], "clr")) {
			cmd = argv[i];
			i++;
			if (i >= argc) {
				syntax();
				return 0;
			}
			io = atoi(argv[i]);
		} else if (!strcmp(argv[i], "pulse")) {
			cmd = argv[i];
			i++;
			if (i >= argc) {
				syntax();
				return 0;
			}
			rps = atoi(argv[i]);
		} else {
			printf("I don't know %s\n", argv[i]);
			syntax();
			return 0;
		}
	}

	if (!strcmp(cmd, "set")) {
		if (-1 == (dev = open_dev())) {
			return -1;
		}
		if (-1 == set(dev, io)) {
			fprintf(stderr, "set write failed: %s\n", strerror(errno));
			return -1;
		}
	} else if (!strcmp(cmd, "clr")) {
		if (-1 == (dev = open_dev())) {
			return -1;
		}
		if (-1 == clr(dev, io)) {
			fprintf(stderr, "clear write failed: %s\n", strerror(errno));
			return -1;
		}
	} else if (!strcmp(cmd, "pulse")) {
		if (-1 == (dev = open_dev())) {
			return -1;
		}
		pulse(dev, rps);
	}

	return 0;
}
