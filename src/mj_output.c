#include "midjoy.h"
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define MJ_OUTPUT_DEFAULT_DSTDEV "/dev/uinput"

/* Cleanup.
 */
 
static void mj_output_device_cleanup(struct mj_output_device *device) {
  if (device->fd>0) close(device->fd);
}

void mj_output_cleanup(struct mj_output *output) {
  if (output->dstpath) free(output->dstpath);
  if (output->devicev) {
    while (output->devicec-->0) mj_output_device_cleanup(output->devicev+output->devicec);
    free(output->devicev);
  }
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
  return 0;
}

/* Device list.
 */
 
static struct mj_output_device *mj_output_device_by_devid(const struct mj_output *output,int devid) {
  struct mj_output_device *device=output->devicev;
  int i=output->devicec;
  for (;i-->0;device++) if (device->devid==devid) return device;
  return 0;
}
 
static int mj_output_fd_by_devid(const struct mj_output *output,int devid) {
  const struct mj_output_device *device=output->devicev;
  int i=output->devicec;
  for (;i-->0;device++) if (device->devid==devid) return device->fd;
  return -1;
}
 
static int mj_output_devid_by_fd(const struct mj_output *output,int fd) {
  const struct mj_output_device *device=output->devicev;
  int i=output->devicec;
  for (;i-->0;device++) if (device->fd==fd) return device->devid;
  return -1;
}

static void mj_output_drop_device_by_devid(struct mj_output *output,int devid) {
  int i=0;
  struct mj_output_device *device=output->devicev;
  for (;i<output->devicec;i++,device++) {
    if (device->devid==devid) {
      mj_output_device_cleanup(device);
      output->devicec--;
      memmove(device,device+1,sizeof(struct mj_output_device)*(output->devicec-i));
      return;
    }
  }
}

static int mj_output_add_device(struct mj_output *output,int fd,int devid) {
  if (output->devicec>=output->devicea) {
    int na=output->devicea+8;
    if (na>INT_MAX/sizeof(struct mj_output_device)) return -1;
    void *nv=realloc(output->devicev,sizeof(struct mj_output_device)*na);
    if (!nv) return -1;
    output->devicev=nv;
    output->devicea=na;
  }
  struct mj_output_device *device=output->devicev+output->devicec++;
  memset(device,0,sizeof(struct mj_output_device));
  device->fd=fd;
  device->devid=devid;
  return 0;
}

/* Perform the uinput handshake.
 */
  
static int mj_output_handshake(int fd,int devid) {
  struct uinput_user_dev uud={0};
  
  snprintf(uud.name,sizeof(uud.name),"MIDI %d",devid);
  
  uud.absmin[ABS_X]=uud.absmin[ABS_Y]=-1;
  uud.absmax[ABS_X]=uud.absmax[ABS_Y]=1;
  
  if (write(fd,&uud,sizeof(uud))<0) return -1;
  
  if (ioctl(fd,UI_SET_EVBIT,EV_ABS)<0) return -1;
  if (ioctl(fd,UI_SET_EVBIT,EV_KEY)<0) return -1;
  
  if (ioctl(fd,UI_SET_ABSBIT,ABS_X)<0) return -1;
  if (ioctl(fd,UI_SET_ABSBIT,ABS_Y)<0) return -1;
  
  if (ioctl(fd,UI_SET_KEYBIT,BTN_SOUTH)<0) return -1;
  if (ioctl(fd,UI_SET_KEYBIT,BTN_WEST)<0) return -1;
  if (ioctl(fd,UI_SET_KEYBIT,BTN_EAST)<0) return -1;
  if (ioctl(fd,UI_SET_KEYBIT,BTN_NORTH)<0) return -1;
  if (ioctl(fd,UI_SET_KEYBIT,BTN_START)<0) return -1;
  if (ioctl(fd,UI_SET_KEYBIT,BTN_SELECT)<0) return -1;
  
  if (ioctl(fd,UI_DEV_CREATE)<0) return -1;
  
  return 0;
}

/* Connect device.
 */

int mj_output_connect_device(struct mj_output *output,int devid) {
  
  int fd=open(output->dstpath,O_RDWR);
  if (fd<0) {
    fprintf(stderr,"%s: Failed to open uinput for devid %d.\n",output->dstpath,devid);
    return -1;
  }
  
  if (mj_output_handshake(fd,devid)<0) {
    close(fd);
    return -1;
  }
  
  if (mj_output_add_device(output,fd,devid)<0) {
    close(fd);
    return -1;
  }
  
  return 0;
}

/* Disconnect device.
 */

int mj_output_disconnect_device(struct mj_output *output,int devid) {
  mj_output_drop_device_by_devid(output,devid);
  return 0;
}

/* Map MIDI notes to Linux event codes.
 * (value) is only relevant when (type==EV_ABS).
 */

static int mj_event_from_note(int *type,int *code,int *value,uint8_t noteid) {
  *value=0;
  switch (noteid%10) {
    case 0: *type=EV_ABS; *code=ABS_X; *value=-1; return 0;
    case 1: *type=EV_ABS; *code=ABS_X; *value=1; return 0;
    case 2: *type=EV_ABS; *code=ABS_Y; *value=-1; return 0;
    case 3: *type=EV_ABS; *code=ABS_Y; *value=1; return 0;
    case 4: *type=EV_KEY; *code=BTN_SOUTH; return 0;
    case 5: *type=EV_KEY; *code=BTN_WEST; return 0;
    case 6: *type=EV_KEY; *code=BTN_EAST; return 0;
    case 7: *type=EV_KEY; *code=BTN_NORTH; return 0;
    case 8: *type=EV_KEY; *code=BTN_START; return 0;
    case 9: *type=EV_KEY; *code=BTN_SELECT; return 0;
  }
  return -1;
}

/* Note Off.
 */
 
static int mj_output_note_off(
  struct mj_output *output,
  struct mj_output_device *device,
  uint8_t noteid
) {
  int type,code,value;
  if (mj_event_from_note(&type,&code,&value,noteid)<0) return 0;
  switch (type) {
    case EV_ABS: switch (code) {
        case ABS_X: {
            if (value==device->x) {
              device->x=0;
              struct input_event event={
                .type=EV_ABS,
                .code=ABS_X,
                .value=0,
              };
              if (write(device->fd,&event,sizeof(event))<0) return -1;
            }
          } break;
        case ABS_Y: {
            if (value==device->y) {
              device->y=0;
              struct input_event event={
                .type=EV_ABS,
                .code=ABS_Y,
                .value=0,
              };
              if (write(device->fd,&event,sizeof(event))<0) return -1;
            }
          } break;
      } break;
    case EV_KEY: {
        struct input_event event={
          .type=EV_KEY,
          .code=code,
          .value=0,
        };
        if (write(device->fd,&event,sizeof(event))<0) return -1;
      } break;
  }
  return 0;
}

/* Note On.
 */
 
static int mj_output_note_on(
  struct mj_output *output,
  struct mj_output_device *device,
  uint8_t noteid
) {
  int type,code,value;
  if (mj_event_from_note(&type,&code,&value,noteid)<0) return 0;
  switch (type) {
    case EV_ABS: switch (code) {
        case ABS_X: {
            device->x=value;
            struct input_event event={
              .type=EV_ABS,
              .code=ABS_X,
              .value=value,
            };
            if (write(device->fd,&event,sizeof(event))<0) return -1;
          } break;
        case ABS_Y: {
            device->y=value;
            struct input_event event={
              .type=EV_ABS,
              .code=ABS_Y,
              .value=value,
            };
            if (write(device->fd,&event,sizeof(event))<0) return -1;
          } break;
      } break;
    case EV_KEY: {
        struct input_event event={
          .type=EV_KEY,
          .code=code,
          .value=1,
        };
        if (write(device->fd,&event,sizeof(event))<0) return -1;
      } break;
  }
  return 0;
}

/* Read, translate, and send one event.
 * TODO Do we need to worry about Running Status?
 */
 
static int mj_process_event(
  struct mj_output *output,
  struct mj_output_device *device,
  const uint8_t *src,int srcc
) {
  int srcp=0;
  
  if (srcp>=srcc) return -1;
  uint8_t lead=src[srcp++];
  uint8_t opcode=lead&0xf0;
  uint8_t chid=lead&0x0f;
  
  switch (opcode) {
  
    case 0x80: { // Note Off
        if (srcp>srcc-2) return -1;
        uint8_t noteid=src[srcp];
        srcp+=2;
        if (mj_output_note_off(output,device,noteid)<0) return -1;
      } break;
      
    case 0x90: { // Note On
        if (srcp>srcc-2) return -1;
        uint8_t noteid=src[srcp];
        srcp+=2;
        if (mj_output_note_on(output,device,noteid)<0) return -1;
      } break;
      
    case 0xa0: { // Note Adjust
        srcp+=2;
      } break;
      
    case 0xb0: { // Control Change
        //TODO maybe use mod wheel or expression pedal?
        srcp+=2;
      } break;
      
    case 0xc0: { // Program Change
        srcp+=1;
      } break;
      
    case 0xd0: { // Channel Pressure
        srcp+=1;
      } break;
      
    case 0xe0: { // Pitch Wheel
        //TODO pitch wheel would make a pretty good axis controller
        srcp+=2;
      } break;
      
    case 0xf0: switch (lead) {
        case 0xf0: { // Sysex
            while (srcp<srcc) {
              if (src[srcp++]==0xf7) break;
            }
          } break;
        default: break; // System and Realtime. Assume single byte, tho that isn't always the case.
      } break;
  }
  return srcp;
}

/* Receive events.
 */
 
int mj_output_events(struct mj_output *output,int devid,const void *src,int srcc) {
  struct mj_output_device *device=mj_output_device_by_devid(output,devid);
  if (!device) return 0;
  const uint8_t *SRC=src;
  int srcp=0;
  while (srcp<srcc) {
    int err=mj_process_event(output,device,SRC+srcp,srcc-srcp);
    if (err<=0) return -1;
    srcp+=err;
  }
  struct input_event event={.type=EV_SYN,.code=SYN_REPORT};
  if (write(device->fd,&event,sizeof(event))<0) return -1;
  return 0;
}
