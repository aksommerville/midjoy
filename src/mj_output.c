#include "midjoy.h"

#define MJ_OUTPUT_DEFAULT_DSTDEV "/dev/uinput"

/* Cleanup.
 */

void mj_output_cleanup(struct mj_output *output) {
  if (output->dstpath) free(output->dstpath);
  memset(output,0,sizeof(struct mj_output));
}

/* Trivial accessors.
 */
 
int mj_output_set_dstdev(struct mj_output *output,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (output->dstpath) free(output->dstpath);
  output->dstpath=nv;
  return 0;
}

/* Finish configuration.
 */
 
int mj_output_ready(struct mj_output *output) {
  if (!output->dstpath) {
    if (mj_output_set_dstdev(output,MJ_OUTPUT_DEFAULT_DSTDEV,-1)<0) return -1;
  }
  //TODO open uinput
  return 0;
}

/* Connect device.
 */

int mj_output_connect_device(struct mj_output *output,int devid) {
  fprintf(stderr,"TODO %s %d\n",__func__,devid);
  return 0;
}

/* Disconnect device.
 */

int mj_output_disconnect_device(struct mj_output *output,int devid) {
  fprintf(stderr,"TODO %s %d\n",__func__,devid);
  return 0;
}

/* Receive events.
 */
 
int mj_output_events(struct mj_output *output,int devid,const void *src,int srcc) {
  fprintf(stderr,"TODO %s %d srcc=%d\n",__func__,devid,srcc);
  return 0;
}
