#ifndef MIDJOY_H
#define MIDJOY_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>

struct mj_input;
struct mj_output;
struct pollfd;

/* Input.
 ****************************************************/
 
struct mj_input {

  /* Devices introduce themselves with an empty Sysex packet (f0f7), and farewell with an empty packet.
   * (devid) is unique among connected devices, and derived from the file name, eg 2 for "/dev/midi2".
   */
  int (*cb)(int devid,const void *src,int srcc,void *userdata);
  void *userdata;
  
  char *srcpath;
  struct pollfd *pollfdv;
  int pollfda;
  int infd;
  int refresh; // Set nonzero to scan directory at next update
  struct mj_input_device {
    int fd,devid;
  } *devicev;
  int devicec,devicea;
};

void mj_input_cleanup(struct mj_input *input);
int mj_input_set_srcdir(struct mj_input *input,const char *src,int srcc);
int mj_input_ready(struct mj_input *input);

int mj_input_update(struct mj_input *input,int to_ms);

/* Output.
 ****************************************************/
 
struct mj_output {
  char *dstpath;
  struct mj_output_device {
    int fd,devid;
    int x,y;
  } *devicev;
  int devicec,devicea;
};

void mj_output_cleanup(struct mj_output *output);
int mj_output_set_dstdev(struct mj_output *output,const char *src,int srcc);
int mj_output_ready(struct mj_output *output);

int mj_output_connect_device(struct mj_output *output,int devid);
int mj_output_disconnect_device(struct mj_output *output,int devid);
int mj_output_events(struct mj_output *output,int devid,const void *src,int srcc);

#endif
