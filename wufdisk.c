#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <math.h>

#define BLKROGET _IO(0x12,94)
#define BLKSSZGET _IO(0x12,104)
#define BLKBSZGET _IOR(0x12,112,size_t)
#define HDIO_GETGEO 0x0301
#define BLKGETSIZE64 _IOR(0x12,114,size_t)
#define BLKRRPART _IO(0x12,95)


#define VERS_MJR 0
#define VERS_MNR 2

struct prt{
	uint8_t boot;
	uint8_t hds1;
	uint8_t sct1;
	uint8_t cyl1;
	uint8_t typ;
	uint8_t hds2;
	uint8_t sct2;
	uint8_t cyl2;
	uint32_t strt;
	uint32_t scts;
}pts[50];

struct hdgs{
	uint8_t heads;
	uint8_t sectors;
	uint16_t cylinders;
	uint32_t start;
}hdg;


uint16_t chs[3];
void clc_chs(uint32_t blks) {
	chs[0]=(uint16_t)(blks/(hdg.sectors*hdg.heads));
	chs[1]=(uint8_t)((blks/hdg.sectors)%hdg.heads);
	chs[2]=(uint8_t)(blks%hdg.sectors)+1;
}
uint32_t clc_blks(void) {
	return (chs[0]*hdg.heads+chs[1])*hdg.sectors+chs[2]-1;
}

int main(int argc,char *argv[]) {
	if (argc<3) {
		printf("wufdisk v%d.%d\nUsage: wufdisk command [options] device\n",VERS_MJR,VERS_MNR);
		printf("commands:\n   add\ttries to add a partition to the given device\n   show\tshows the partition table of the given device\n   del\tdeletes a partition of the given device\n   wipe\twipes the partition table of the given device\n   rrpt\treread the partition table of the given device\n\n");
		printf("\noptions for command add:\n   --boot=1\t sets the bootflag, any other digit as 1 is ignored\n   --type=..\t the hexvalue of the partition type. default is 0x83\n   --size=...\t the size in bytes the new partion shall at least have.\n                 valid suffixes are K(ilo),M(ega),G(iga) and T(era)\n                 !Note: wufisk uses 2^10 as conversion factor and NOT 10^3!\n");
		printf("\noptions for command show:\n   --geom\t displays the geometry data of the device\n");
		printf("\n");
		exit(0);
	}
	uint8_t i,i2;
	char opmod;
	char *p;
	char buf[20];
	char dev[80];
	uint8_t prm_boot=0;
	uint8_t prm_type=131;
	uint64_t prm_size=1024;

	if (strncmp("add",argv[1],3)==0)
		opmod='a';
	else if (strncmp("del",argv[1],3)==0)
		opmod='d';
	else if (strncmp("show",argv[1],4)==0)
		opmod='s';
	else if (strncmp("wipe",argv[1],4)==0)
		opmod='w';
	else if (strncmp("rrpt",argv[1],4)==0)
		opmod='r';
	else {
		printf("unknown command\n");
		exit(2);
	}

	FILE *fp;
	switch (opmod) {
		case 'd':
			printf("deletion of partitions is not implemented yet - sorry\n");
			exit(0);
			break;
		case 'r':
			for(i=2;i<argc;++i) {
				memset(&buf[0],0,20);
				p=&argv[i][0];
				if (strncmp("/dev/",p,5)==0)
					strcpy(&dev[0],p);
				else {
					printf("unknown option: %s\n",p);
					exit(1);
				}
			}
			int fd=open(dev,O_RDONLY|O_NONBLOCK);
			if (fd<0) {
				fprintf(stderr,"wufdisk: cannot open device %s\n",dev);
				exit(1);
			}
			int err=ioctl(fd,BLKRRPART);
			if (err) {
				fprintf(stderr,"wufdisk: ioctl error on %s\n",dev);
				exit(1);
			}
			close(fd);

			exit(0);
			break;
		case 'w':
			for(i=2;i<argc;++i) {
				memset(&buf[0],0,20);
				p=&argv[i][0];
				if (strncmp("/dev/",p,5)==0)
					strcpy(&dev[0],p);
				else {
					printf("unknown option: %s\n",p);
					exit(1);
				}
			}

			fp=fopen(dev,"r+");
			if (fp==NULL) {
				fprintf(stderr,"wufdisk: cannot open device %s\n",dev);
				exit(2);
			}
			/* jump to partition table */
			(void)fseek(fp,0x01BE,SEEK_SET);
			/* wiping partition table */
			struct prt *pp;
			pp=&pts[0];
			memset(pp,0,64); /* TODO: ok this is plain -.- */
			printf("DEBUG: wiping partition table of %s\n",dev);
			if (fwrite(pp,64,1,fp)!=1) {
				fprintf(stderr,"wufdisk: error writing %s\n",dev);
				exit(2);
			}
			exit(0);
			break;
		case 'a':
			for(i=2;i<argc;++i) {
				memset(&buf[0],0,20);
				p=&argv[i][0];
				if (strncmp("--boot=",p,7)==0) {
					prm_boot=atoi(strncpy(&buf[0],(p+7),1));
					prm_boot=(prm_boot==1)?0x80:0;
				} else if (strncmp("--type=",p,7)==0)
					prm_type=strtol(strncpy(&buf[0],(p+7),2),NULL,16);
				else if (strncmp("--size=",p,7)==0) {
					p+=7;
					uint8_t l=strlen(p);
					uint8_t shft;
					switch (*(p+l-1)) {
						case 'K': shft=10; --l; break;
						case 'M': shft=20; --l; break;
						case 'G': shft=30; --l; break;
						case 'T': shft=40; --l; break;
						default: shft=0; break;
					}
					prm_size=strtoull(strncpy(&buf[0],p,l),NULL,10);
					prm_size<<=shft;
				} else if (strncmp("/dev/",p,5)==0)
					strcpy(&dev[0],p);
				else {
					printf("unknown option: %s\n",p);
					exit(1);
				}
			}
			printf("DEBUG adding | boot: %d | type: 0x%02x | size: %"PRIu64" bytes\n",prm_boot,prm_type,prm_size);
			break;
		case 's':
			for(i=2;i<argc;++i) {
				memset(&buf[0],0,20);
				p=&argv[i][0];
				if (strncmp("--geom",p,6)==0)
					opmod='g'; // TODO: this works, but is not _that_ nice ;-)
				else if (strncmp("/dev/",p,5)==0)
					strcpy(&dev[0],p);
				else {
					printf("unknown option: %s\n",p);
					exit(1);
				}
			}
			break;
	}

	/** try getting physical geometry of drive */
	int fd=open(dev,O_RDONLY|O_NONBLOCK);
	if (fd<0) {
		fprintf(stderr,"wufdisk: cannot open device %s\n",dev);
		exit(1);
	}
	int ro=0;
	int sz=0;
	int bs=0;
	uint64_t bytes;
	hdg.start=0;
	int err=0;
	err+=ioctl(fd,BLKROGET,&ro);
	err+=ioctl(fd,BLKSSZGET,&sz);
	err+=ioctl(fd,BLKBSZGET,&bs);
	err+=ioctl(fd,HDIO_GETGEO,&hdg);
	err+=ioctl(fd,BLKGETSIZE64,&bytes);
	if (err) {
		fprintf(stderr,"wufdisk: ioctl error on %s\n",dev);
		exit(1);
	}
	close(fd);
	if (opmod=='g')
		printf("<geometry>\naccess=r%c\nsectorsize=%d\nblocksize=%d\nheads=%"PRIu8"\nsectorspertrack=%"PRIu8"\ncylinders=%"PRIu32"\nstartsector=%"PRIu32"\nbytesize=%"PRIu64"\n</geometry>\n",(ro?'o':'w'),sz,bs,hdg.heads,hdg.sectors,hdg.cylinders,hdg.start,bytes);

	/* try reading device */
	fp=fopen(dev,"r");
	if (fp==NULL) {
		fprintf(stderr,"wufdisk: cannot open device %s\n",dev);
		exit(2);
	}
	/* jump to partition table */
	(void)fseek(fp,0x01BE,SEEK_SET);
	/* read partition table */
	struct prt *pp;
	pp=&pts[0];
	if (fread(pp,sizeof(struct prt),4,fp)!=4) {
		fprintf(stderr,"wufdisk: error reading %s\n",dev);
		exit(2);
	}

	uint32_t nxtjmps=0;
	uint64_t nxtjmpb=0;
	uint32_t bsjmps=0;
	for(i=1;i<5;++i) {
		if ((*pp).typ==5)
			bsjmps=(*pp).strt;
		++pp;
	}

	while (bsjmps) {
		nxtjmps+=bsjmps;
		//printf("DEBUG need to jump to an extended at: %"PRIu32"\n", nxtjmps);
		nxtjmpb=nxtjmps<<9;
		//printf("DEBUG should be at byte: %"PRIu64"\n", nxtjmpb);
		(void)fseek(fp,nxtjmpb+0x01BE,SEEK_SET);
		if (fread(pp,sizeof(struct prt),2,fp)!=2) {
			fprintf(stderr,"wufdisk: error reading %s\n",dev);
			exit(2);
		}
		++i;
		++pp;
		if ((*pp).typ==5)
			nxtjmps=(*pp).strt;
		else
			bsjmps=0;
	}
	fclose(fp);

	--i;
	if ((opmod=='s')||(opmod=='g')) {
		printf("no\tboot\ttype\t   start\t   sectors\n");
		for (i2=0;i2<i;i2++) {
			pp=&pts[i2];
			switch ((*pp).boot) {
				case 0x80: buf[0]='*'; break;
				case 0x00: buf[0]=' '; break;
				default: buf[0]='U'; break;
			}
			printf("%2d\t%c\t0x%02x\t% 10d\t% 10d\n",(i2+1),buf[0],(*pp).typ,(*pp).strt,(*pp).scts);
		}
		exit(0);
	}

	if (opmod=='a') {

		uint64_t frspc=0;
		char ee=0;

		// next version - single step through primaries
		if (pts[0].typ==0x00) {
			// find out how much space we have ahead
			frspc=bytes;
			for(i=1;i<4;i++)
				if (pts[i].typ) {
					frspc=pts[i].strt;
					break;
				}
			printf("DEBUG part 1 is empty and can span a space of %"PRIu64"\n",frspc);
			if (frspc>prm_size) {
				pts[0].boot=prm_boot;
				pts[0].typ=prm_type;
				pts[0].strt=1;
				pts[0].scts=ceil(prm_size/512);
				(void)clc_chs(1);
				pts[0].hds1=chs[1];
				pts[0].sct1=((chs[0]&0x0300)>>2)|chs[2];
				pts[0].cyl1=chs[0]&0xFF;
				(void)clc_chs(pts[0].scts);
				chs[1]=hdg.heads-1;
				chs[2]=hdg.sectors;
				pts[0].scts=clc_blks();
				(void)clc_chs(pts[0].scts);
				pts[0].hds2=chs[1];
				pts[0].sct2=((chs[0]&0x0300)>>2)|chs[2];
				pts[0].cyl2=chs[0]&0xFF;
				fp=fopen(dev,"r+");
				if (fp==NULL) {
					fprintf(stderr,"wufdisk: cannot open device for writing %s\n",dev);
					exit(2);
				}
				/* jump to partition table */
				(void)fseek(fp,0x01BE,SEEK_SET);
				/* write partition table */
				if (fwrite(&pts[0],sizeof(struct prt),4,fp)!=4) {
					fprintf(stderr,"wufdisk: error writing %s\n",dev);
					exit(2);
				}
				fclose(fp);
				printf("%s1\n",dev);
				exit(0);
			}
		} else if (pts[0].typ==0x05)
			ee=1;

		if (pts[1].typ==0x00) {
			// find out how much space we have ahead
			frspc=bytes;
			for(i=2;i<4;i++)
				if (pts[i].typ) {
					frspc=pts[i].strt;
					break;
				}
			printf("DEBUG part 2 is empty and can span a space of %"PRIu64"\n",frspc);
			if (frspc>prm_size) {
				pts[1].boot=prm_boot;
				pts[1].typ=prm_type;
				pts[1].strt= ( pts[0].strt+pts[0].scts );
				pts[1].scts=ceil(prm_size/512);
				(void)clc_chs(pts[1].strt);
				pts[1].hds1=chs[1];
				pts[1].sct1=((chs[0]&0x0300)>>2)|chs[2];
				pts[1].cyl1=chs[0]&0xFF;
				(void)clc_chs( pts[1].strt+pts[1].scts );
				chs[1]=hdg.heads-1;
				chs[2]=hdg.sectors;
				pts[1].scts=clc_blks()-pts[1].strt;
				(void)clc_chs( pts[1].strt+pts[1].scts );
				pts[1].hds2=chs[1];
				pts[1].sct2=((chs[0]&0x0300)>>2)|chs[2];
				pts[1].cyl2=chs[0]&0xFF;
				++pts[1].scts; // WTF?!
				fp=fopen(dev,"r+");
				if (fp==NULL) {
					fprintf(stderr,"wufdisk: cannot open device for writing %s\n",dev);
					exit(2);
				}
				/* jump to partition table */
				(void)fseek(fp,0x01BE,SEEK_SET);
				/* write partition table */
				if (fwrite(&pts[0],sizeof(struct prt),4,fp)!=4) {
					fprintf(stderr,"wufdisk: error writing %s\n",dev);
					exit(2);
				}
				fclose(fp);
				printf("%s2\n",dev);
				exit(0);
			}
		} else if (pts[1].typ==0x05)
			ee=2;


		if (pts[2].typ==0x00) {
			// find out how much space we have ahead
			frspc=bytes;
			for(i=3;i<4;i++)
				if (pts[i].typ) {
					frspc=pts[i].strt;
					break;
				}
			printf("DEBUG part 3 is empty and can span a space of %"PRIu64"\n",frspc);
			if (frspc>prm_size) {
				pts[2].boot=prm_boot;
				pts[2].typ=prm_type;
				pts[2].strt= ( pts[1].strt+pts[1].scts );
				pts[2].scts=ceil(prm_size/512);
				(void)clc_chs(pts[2].strt);
				pts[2].hds1=chs[1];
				pts[2].sct1=((chs[0]&0x0300)>>2)|chs[2];
				pts[2].cyl1=chs[0]&0xFF;
				(void)clc_chs( pts[2].strt+pts[2].scts );
				chs[1]=hdg.heads-1;
				chs[2]=hdg.sectors;
				pts[2].scts=clc_blks()-pts[2].strt;
				(void)clc_chs( pts[2].strt+pts[2].scts );
				pts[2].hds2=chs[1];
				pts[2].sct2=((chs[0]&0x0300)>>2)|chs[2];
				pts[2].cyl2=chs[0]&0xFF;
				++pts[2].scts; // WTF?!
				fp=fopen(dev,"r+");
				if (fp==NULL) {
					fprintf(stderr,"wufdisk: cannot open device for writing %s\n",dev);
					exit(2);
				}
				/* jump to partition table */
				(void)fseek(fp,0x01BE,SEEK_SET);
				/* write partition table */
				if (fwrite(&pts[0],sizeof(struct prt),4,fp)!=4) {
					fprintf(stderr,"wufdisk: error writing %s\n",dev);
					exit(2);
				}
				fclose(fp);
				printf("%s3\n",dev);
				exit(0);
			}
		} else if (pts[2].typ==0x05)
			ee=3;

		/* on ee we just run like the other three */
		if (pts[3].typ==0x00) {

			if (ee) {
				// TODO - ee is elsewhere and we need a regular part here
			} else {
				/* create an extended spanning on the rest of the space */
				pts[3].boot=0x00;
				pts[3].typ=0x05;
				pts[3].strt=pts[2].strt+pts[2].scts;
				pts[3].scts=bytes/512 -pts[3].strt;

				(void)clc_chs(pts[3].strt);
				pts[3].hds1=chs[1];
				pts[3].sct1=((chs[0]&0x0300)>>2)|chs[2];
				pts[3].cyl1=chs[0]&0xFF;
				(void)clc_chs( pts[3].strt+pts[3].scts );
				pts[3].hds2=chs[1];
				pts[3].sct2=((chs[0]&0x0300)>>2)|chs[2];
				pts[3].cyl2=chs[0]&0xFF;

				fp=fopen(dev,"r+");
				if (fp==NULL) {
					fprintf(stderr,"wufdisk: cannot open device for writing %s\n",dev);
					exit(2);
				}
				/* jump to partition table */
				(void)fseek(fp,0x01BE,SEEK_SET);
				/* write partition table */
				if (fwrite(&pts[0],sizeof(struct prt),4,fp)!=4) {
					fprintf(stderr,"wufdisk: error writing %s\n",dev);
					exit(2);
				}
				fclose(fp);

				ee=4;
				goto SRCH_LGCL;
			}
		} else if (pts[3].typ==0x05)
			ee=4;

SRCH_LGCL:
		if (!ee) {
			// at this point there should have been an extended partition
			fprintf(stderr,"wufdisk: no partition free and no extended partition found\n");
			exit(2);
		}
		--ee;


		exit(0);
	}
	// so at this point there should be only "del" running - but thats just not implemented, yet

	return 0;
}
