#include "midjoy.h"
#include <signal.h>

static volatile int mj_sigc=0;

static void mj_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++mj_sigc>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

static int mj_daemonize() {
  //TODO daemonize
  return 0;
}

static int mj_rcvin(int devid,const void *src,int srcc,void *userdata) {
  fprintf(stderr,"%s %d, %d bytes\n",__func__,devid,srcc);
  struct mj_output *output=userdata;
  if (!srcc) {
    return mj_output_disconnect_device(output,devid);
  } else if ((srcc==2)&&!memcmp(src,"\xf0\xf7",2)) {
    return mj_output_connect_device(output,devid);
  } else {
    return mj_output_events(output,devid,src,srcc);
  }
}

static void mj_print_help(const char *exename) {
  fprintf(stderr,
    "Usage: %s [--daemonize] [--srcdir=PATH] [--dstdev=PATH]\n",exename
  );
  fprintf(stderr,"  srcdir defaults to \"/dev/\", we look here for MIDI devices named \"midiN\"\n");
  fprintf(stderr,"  dstdir defaults to \"/dev/uinput\"\n");
}

int main(int argc,char **argv) {
  int status=0;
  struct mj_output output={0};
  struct mj_input input={
    .cb=mj_rcvin,
    .userdata=&output,
  };
  
  int argp=1;
  while (argp<argc) {
    const char *arg=argv[argp++];
    if (!strcmp(arg,"--help")||!strcmp(arg,"-h")) {
      mj_print_help(argv[0]);
      return 0;
    } else if (!strcmp(arg,"--daemonize")||!strcmp(arg,"-d")) {
      if (mj_daemonize()<0) return -1;
    } else if (!memcmp(arg,"--srcdir=",9)) {
      if (mj_input_set_srcdir(&input,arg+9,-1)<0) return 1;
    } else if (!memcmp(arg,"--dstdev=",9)) {
      if (mj_output_set_dstdev(&output,arg+9,-1)<0) return 1;
    } else {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",argv[0],arg);
    }
  }
  
  if (
    (mj_input_ready(&input)<0)||
    (mj_output_ready(&output)<0)
  ) return 1;
  
  signal(SIGINT,mj_rcvsig);
  
  while (!mj_sigc) {
    if (mj_input_update(&input,100)<0) { status=1; break; }
  }
  
  mj_input_cleanup(&input);
  mj_output_cleanup(&output);
  return status;
}
