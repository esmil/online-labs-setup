/*
 * Copyright 2015 Emil Renner Berthing
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/signal.h>
#include <linux/nbd.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define log(fmt, ...) printf("shutdown[%d]: " fmt, getpid(), __VA_ARGS__)

#ifdef TEST
static int
_test_kill(pid_t pid, int sig)
{
	return printf("== kill(%d, %d)\n", pid, sig);
}
#define kill _test_kill

static int
_test_mount(const char *source, const char *target,
            const char *fstype, unsigned long flags,
            const void *data)
{
	return printf("== mount('%s', '%s', '%s', %lu, %p)\n",
	              source, target, fstype, flags, data);
}
#define mount _test_mount

static int
_test_umount(const char *target)
{
	return printf("== umount('%s')\n", target);
}
#define umount _test_umount

static int
_test_reboot(int cmd)
{
	int ret = printf("== reboot(%d)\n", cmd);
	if (ret >= 0)
		exit(0);
	return ret;
}
#define reboot _test_reboot

static int
_test_open(const char *path, int flags)
{
	if ((flags & O_ACCMODE) == O_RDONLY)
		return open(path, flags);
	printf("== open('%s', 0x%x)\n", path, flags);
	return 10124;
}
#define open _test_open

static int
_test_close(int fd)
{
	if (fd == 10124) {
		printf("== close(%d)\n", fd);
		return 0;
	}
	return close(fd);
}
#define close _test_close

static int
_test_ioctl(int fd, int req, ...)
{
	return printf("== ioctl(%d, %d)\n", fd, req);
}
#define ioctl _test_ioctl
#endif

static int
readpid(pid_t *pid, const char *file)
{
	char buf[PAGE_SIZE];
	char *p = buf;
	size_t len = sizeof(buf) - 1;
	int fd;
	char *end;
	long int n;

	fd = open(file, O_CLOEXEC|O_RDONLY);
	if (fd < 0)
		return -errno;

	while (len > 0) {
		ssize_t size = read(fd, p, len);

		if (size < 0) {
			int ret = -errno;
			(void)close(fd);
			return ret;
		}

		if (size == 0)
			break;

		p += size;
		len -= size;
	}
	*p = '\0';

	if (close(fd) < 0)
		return -errno;

	n = strtol(buf, &end, 10);
	if (*end != '\n' || n > INT_MAX || n < INT_MIN)
		return -EINVAL;

	*pid = (pid_t)n;
	return 0;
}

static const char *const mountpoints[] = {
	"/oldroot/dev",
	"/oldroot/proc",
	"/oldroot/sys",
	"/oldroot/run",
};

static int
move_mounts(void)
{
	int i;

	for (i = 0; i < (int)(sizeof(mountpoints)/sizeof(mountpoints[0])); i++) {
		if (mount(mountpoints[i], mountpoints[i] + 8, NULL, MS_MOVE, NULL) < 0) {
			log("error moving '%s': %s\n",
					mountpoints[i], strerror(errno));
			if (errno != ENOENT)
				return -errno;
		}
	}
	return 0;
}

static int
nbd_disconnect(void)
{
	pid_t client;
	int nbd;
	int ret;

	ret = readpid(&client, "/sys/block/nbd0/pid");
	if (ret < 0)
		return ret;

	if (kill(client, SIGUSR1) < 0)
		return -errno;

	nbd = open("/dev/nbd0", O_CLOEXEC|O_RDWR);
	if (nbd < 0)
		return -errno;

	if (ioctl(nbd, NBD_CLEAR_QUE) < 0) {
		ret = -errno;
		goto err;
	}

	if (ioctl(nbd, NBD_DISCONNECT) < 0) {
		ret = -errno;
		goto err;
	}

	if (ioctl(nbd, NBD_CLEAR_SOCK) < 0) {
		ret = -errno;
		goto err;
	}

	if (close(nbd) < 0)
		return -errno;

	return 0;
err:
	(void)close(nbd);
	return ret;
}

int
main(int argc, char *argv[])
{
	int flag = RB_POWER_OFF;
	int ret;

	if (argc > 1) {
		if (!strcmp(argv[1], "reboot"))
			flag = RB_AUTOBOOT;
		else if (!strcmp(argv[1], "halt"))
			flag = RB_HALT_SYSTEM;
	}

	ret = move_mounts();
	if (ret < 0)
		goto fail;

	log("Unmounting %s\n", "/oldroot");
	ret = umount("/oldroot");
	if (ret < 0) {
		log("error unmounting '%s': %s\n",
				"/oldroot", strerror(errno));
		goto fail;
	}

	log("Disconnecting %s\n", "/dev/nbd0");
	ret = nbd_disconnect();
	if (ret < 0) {
		log("error disconnecting '%s': %s\n",
			"/dev/nbd0", strerror(-ret));
		goto fail;
	}

	(void)reboot(flag);
fail:
	sleep(10);
	sync();
	(void)reboot(flag);
	return EXIT_FAILURE;
}
