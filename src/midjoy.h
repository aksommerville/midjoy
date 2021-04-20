#ifndef MIDJOY_H
#define MIDJOY_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>

struct mj_input;
struct mj_output;

/* Input.
 ****************************************************/
 
struct mj_input {
  char *srcpath;
  // Devices introduce themselves with an empty Sysex packet (f0f7), and farewell with an empty packet.
  int (*cb)(int devid,const void *src,int srcc,void *userdata);
  void *userdata;
};

void mj_input_cleanup(struct mj_input *input);
int mj_input_set_srcdir(struct mj_input *input,const char *src,int srcc);
int mj_input_ready(struct mj_input *input);

int mj_input_update(struct mj_input *input,int to_ms);

/* Output.
 ****************************************************/
 
struct mj_output {
  char *dstpath;
};

void mj_output_cleanup(struct mj_output *output);
int mj_output_set_dstdev(struct mj_output *output,const char *src,int srcc);
int mj_output_ready(struct mj_output *output);

int mj_output_connect_device(struct mj_output *output,int devid);
int mj_output_disconnect_device(struct mj_output *output,int devid);
int mj_output_events(struct mj_output *output,int devid,const void *src,int srcc);

#endif
