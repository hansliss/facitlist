#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

struct fentry {
  uint32_t info;
  char fname[12];
};

void usage(char *progname)
{
  fprintf(stderr, "Usage: %s -d <outdir> -f <file>\n", progname);
}

#define DIRSIZE 4096
#define ESIZE (sizeof(struct fentry))
#define ENTRIES ((DIRSIZE) / (ESIZE))

int main(int argc, char *argv[]) {
  char *dirname = NULL;
  FILE *infile = NULL;
  int o;
  unsigned char inbuf[4096], secbuf[256];
  char ofnamebuf[128];
  int i, j, k;
  while ((o=getopt(argc, argv, "d:f:"))!=-1)
    {
      switch (o)
	{
	case 'd':
	  dirname=optarg;
	  break;
	case 'f':
	  if (!(infile=fopen(optarg, "r"))) {
	    perror(optarg);
	    return -1;
	  }
	  break;
	default:
	  usage(argv[0]);
	  return -1;
	  break;
	}
    }
  if (infile == NULL || dirname == NULL) {
    usage(argv[0]);
    return -1;
  }
  while (strlen(dirname) > 1 && dirname[strlen(dirname) - 1] == '/') {
    dirname[strlen(dirname)-1] = '\0';
  }
  
  fseek(infile, 16 * 256, SEEK_SET);
  int n;
  if ((n=fread(inbuf, 1, 4096, infile)) != 4096) {
    fprintf(stderr, "Short read (%d)!\n", n);
  }
  for (i=0; i < ENTRIES; i++) {
    struct fentry *f = (struct fentry *)&(inbuf[i * ESIZE]);
    char fname[32];
    if (f->fname[0] >= 32 && f->fname[0] <= 127) {
      k=0;
      for (j=0; j<sizeof(f->fname)-1; j++) {
	if (j == 8) {
	  while (k > 0 && fname[k-1]==' ') {
	    k--;
	  }
	  fname[k++] = '.';
	}
	fname[k++] = f->fname[j];
      }
      while (k > 0 && (fname[k-1]==' ' || fname[k-1]=='.')) {
	k--;
      }
      fname[k] = '\0';
      uint32_t offset = 0x20 * (((f->info & 0xFF) << 8) | ((f->info & 0xFF00) >> 8));
      printf("%04x/%u:\t%s\n", offset, offset, fname);
      fseek(infile, offset, SEEK_SET);
      if ((n=fread(secbuf, 1, 256, infile)) != 256) {
	fprintf(stderr, "Short read (%d)!\n", n);
      }
      sprintf(ofnamebuf, "%s/%s_meta", dirname, fname);
      FILE *outfile = fopen(ofnamebuf, "w");
      fwrite(secbuf, 1, 256, outfile);
      fclose(outfile);
      k=4;
      int lastval = -1;
      while (k < 256-2 && secbuf[k] != 0xff && secbuf[k+1] != 0xff) {
	uint32_t val = (secbuf[k] * 256 + secbuf[k + 1]);
	if (lastval == -1) {
	  lastval = val;
	}
	printf("\t%04x/%u = %04x/%u, diff=%d\n", val, val, 0x20 * val, 0x20 * val, val - lastval);
	lastval = val;
	k += 2;
      }
      int block = 1;
      int done = 0;
      int entryid = ((i & 0x0f) << 4) | ((i & 0xf0) >> 4);
      printf("Entry id = %02X\n", entryid);
					 
      sprintf(ofnamebuf, "%s/%s", dirname, fname);
      outfile = fopen(ofnamebuf, "w");
      while (!done) {
	fseek(infile, offset + block * 256, SEEK_SET);
	if ((n=fread(secbuf, 1, 256, infile)) != 256) {
	  fprintf(stderr, "Short read (%d)!\n", n);
	}
	if (secbuf[0] != entryid || secbuf[1] != (block & 0xFF) || secbuf[2] != ((block & 0xFF00) >> 8)) {
	  done = 1;
	} else {
	  fwrite(secbuf + 3, 1, 256 - 3, outfile);
	  block++;
	}
      }
      fclose(outfile);
    }
  }
  fclose(infile);
  return 0;
}
