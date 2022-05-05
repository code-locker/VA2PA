#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

ssize_t readn(int fd, void *buf, size_t count)
{
	char *cbuf = buf;
	ssize_t nr, n = 0;

	while (n < count) {
		nr = read(fd, &cbuf[n], count - n);
		if (nr == 0) {
			//EOF
			break;
		} else if (nr == -1) {
			if (errno == -EINTR) {
				//retry
				continue;
			} else {
				//error
				return -1;
			}
		}
		n += nr;
	}

	return n;
}

/**
 * @return -1: error, -2: not present, other: physical address
 */
uint64_t virt_to_phys(int fd, int pid, uint64_t virtaddr)
{
	int pagesize;
	uint64_t tbloff, tblen, pageaddr, physaddr;
	off_t offset;
	ssize_t nr;

	uint64_t tbl_present;
	uint64_t tbl_swapped;
	uint64_t tbl_shared;
	uint64_t tbl_pte_dirty;
	uint64_t tbl_swap_offset;
	uint64_t tbl_swap_type;

	//1PAGE = typically 4KB, 1entry = 8bytes
	pagesize = (int)sysconf(_SC_PAGESIZE);
	//see: linux/Documentation/vm/pagemap.txt
	tbloff = virtaddr / pagesize * sizeof(uint64_t);

	offset = lseek(fd, tbloff, SEEK_SET);
	if (offset == (off_t)-1) {
		perror("lseek");
		return -1;
	}
	if (offset != tbloff) {
		fprintf(stderr, "Cannot found virt:0x%08llx, "
				"tblent:0x%08llx, returned offset:0x%08llx.\n",
				(long long)virtaddr, (long long)tbloff,
				(long long)offset);
		return -1;
	}

	nr = readn(fd, &tblen, sizeof(uint64_t));
	if (nr == -1 || nr < sizeof(uint64_t)) {
		fprintf(stderr, "Cannot found virt:0x%08llx, "
				"tblent:0x%08llx, returned offset:0x%08llx, "
				"returned size:0x%08x.\n",
				(long long)virtaddr, (long long)tbloff,
				(long long)offset, (int)nr);
		return -1;
	}

	tbl_present   = (tblen >> 63) & 0x1;
	tbl_swapped   = (tblen >> 62) & 0x1;
	tbl_shared    = (tblen >> 61) & 0x1;
	tbl_pte_dirty = (tblen >> 55) & 0x1;
	if (!tbl_swapped) {
		tbl_swap_offset = (tblen >> 0) & 0x7fffffffffffffULL;
	} else {
		tbl_swap_offset = (tblen >> 5) & 0x3ffffffffffffULL;
		tbl_swap_type = (tblen >> 0) & 0x1f;
	}
	pageaddr = tbl_swap_offset * pagesize;
	physaddr = (uint64_t)pageaddr | (virtaddr & (pagesize - 1));

	if (tbl_present) {
		return physaddr;
	} else {
		return -2;
	}
}

int main(int argc, char *argv[])
{
	char procname[1024] = "";
	int pid, fd = -1, pagesize;
	uint64_t virtaddr, physaddr;
	int result = -1;
	int *ptr=(int *)malloc(5*sizeof(int));
	for(int i=0;i<5;i++)
		ptr[i]=i+1;

	pid = (int)getpid();
	virtaddr =(uint64_t) ptr;

	memset(procname, 0, sizeof(procname));
	snprintf(procname, sizeof(procname) - 1,
			"/proc/%d/pagemap", pid);
	fd = open(procname, O_RDONLY);
	if (fd == -1) {
		perror("open");
		goto err_out;
	}

	pagesize = (int)sysconf(_SC_PAGESIZE);
	physaddr = virt_to_phys(fd, pid, virtaddr);
	//show result
	if (physaddr == -1) {
		printf("pid:%d, virt:0x%08llx, (%s)\n",pid,
				(long long)virtaddr, "not valid virtual address");
	} else if (physaddr == -2) {
		printf("pid:%d, virt:0x%08llx, phys:(%s)\n",
				pid,(long long)virtaddr, "not present");
	} else {
		printf("pid:%d,virt:0x%08llx, phys:0x%08llx\n",
				pid,(long long)virtaddr, (long long)physaddr);
	}
	while(1);

	result = 0;

err_out:
	if (fd != -1) {
		close(fd);
		fd = -1;
	}

	return result;
}

