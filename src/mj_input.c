#include "midjoy.h"
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/inotify.h>
#include <sys/poll.h>

#define MJ_INPUT_DEFAULT_SRCDIR "/dev/"

static const uint8_t MJ_INPUT_HELLO_EVENT[]={0xf0,0xf7};

/* Cleanup.
 */
 
static void mj_input_device_cleanup(struct mj_input_device *device) {
  if (device->fd>0) close(device->fd);
}

void mj_input_cleanup(struct mj_input *input) {
  if (input->srcpath) free(input->srcpath);
  if (input->infd>0) close(input->infd);
  if (input->pollfdv) free(input->pollfdv);
  if (input->devicev) {
    while (input->devicec-->0) mj_input_device_cleanup(input->devicev+input->devicec);
    free(input->devicev);
  }
  memset(input,0,sizeof(struct mj_input));
}

/* Trivial accessors.
 */
 
int mj_input_set_srcdir(struct mj_input *input,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (input->srcpath) free(input->srcpath);
  input->srcpath=nv;
  return 0;
}

/* Finish configuration.
 */
 
int mj_input_ready(struct mj_input *input) {
  if (!input->srcpath) {
    if (mj_input_set_srcdir(input,MJ_INPUT_DEFAULT_SRCDIR,-1)<0) return -1;
  }
  if ((input->infd=inotify_init())<0) return -1;
  if (inotify_add_watch(input->infd,input->srcpath,IN_CREATE|IN_ATTRIB)<0) return -1;
  input->refresh=1;
  return 0;
}

/* Device list.
 */
 
static int mj_input_fd_by_devid(const struct mj_input *input,int devid) {
  const struct mj_input_device *device=input->devicev;
  int i=input->devicec;
  for (;i-->0;device++) if (device->devid==devid) return device->fd;
  return -1;
}
 
static int mj_input_devid_by_fd(const struct mj_input *input,int fd) {
  const struct mj_input_device *device=input->devicev;
  int i=input->devicec;
  for (;i-->0;device++) if (device->fd==fd) return device->devid;
  return -1;
}

static void mj_input_drop_device_by_fd(struct mj_input *input,int fd) {
  int i=0;
  struct mj_input_device *device=input->devicev;
  for (;i<input->devicec;i++,device++) {
    if (device->fd==fd) {
      mj_input_device_cleanup(device);
      input->devicec--;
      memmove(device,device+1,sizeof(struct mj_input_device)*(input->devicec-i));
      return;
    }
  }
}

static int mj_input_add_device(struct mj_input *input,int fd,int devid) {
  if (input->devicec>=input->devicea) {
    int na=input->devicea+8;
    if (na>INT_MAX/sizeof(struct mj_input_device)) return -1;
    void *nv=realloc(input->devicev,sizeof(struct mj_input_device)*na);
    if (!nv) return -1;
    input->devicev=nv;
    input->devicea=na;
  }
  struct mj_input_device *device=input->devicev+input->devicec++;
  memset(device,0,sizeof(struct mj_input_device));
  device->fd=fd;
  device->devid=devid;
  return 0;
}

/* Consider one file, by basename.
 */
 
static int mj_input_check_file(struct mj_input *input,const char *base) {
  
  // (base) must start "midi" and be followed by a positive decimal integer.
  if (memcmp(base,"midi",4)) return 0;
  int basep=4,devid=0;
  for (;base[basep];basep++) {
    char ch=base[basep];
    if (ch<'0') return 0;
    if (ch>'9') return 0;
    devid*=10;
    devid+=ch-'0';
    if (devid>1000000) return 0; // let's be reasonable here
  }
  if (basep==4) return 0; // devid required
  
  // If we already have it, no worries, we're done.
  if (mj_input_fd_by_devid(input,devid)>=0) return 0;
  
  // Get full path.
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/%s",input->srcpath,base);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  
  // Attempt to open the file. If it fails, just stop, no big deal.
  // (important that we not fail -- we might be waiting for a udev rule to make the device readable).
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  
  // Add to our list.
  if (mj_input_add_device(input,fd,devid)<0) {
    close(fd);
    return -1;
  }
  
  // Send the hello event.
  if (input->cb(devid,MJ_INPUT_HELLO_EVENT,sizeof(MJ_INPUT_HELLO_EVENT),input->userdata)<0) return -1;
  
  return 0;
}

/* Scan directory.
 */
 
static int mj_input_scan(struct mj_input *input) {
  DIR *dir=opendir(input->srcpath);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (mj_input_check_file(input,de->d_name)<0) {
      closedir(dir);
      return -1;
    }
  }
  closedir(dir);
  return 0;
}

/* Read from inotify.
 */
 
static int mj_input_update_inotify(struct mj_input *input) {
  char buf[1024];
  int bufc=read(input->infd,buf,sizeof(buf));
  if (bufc<=0) {
    fprintf(stderr,"%s: Failed to read from inotify. We will not detect any more connections.\n",input->srcpath);
    close(input->infd);
    input->infd=-1;
    return 0;
  }
  int bufp=0;
  while (bufp<=bufc-(int)sizeof(struct inotify_event)) {
    struct inotify_event *event=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    if (bufp>bufc-event->len) break;
    bufp+=event->len;
    if (mj_input_check_file(input,event->name)<0) return -1;
  }
  return 0;
}

/* Read from device.
 */
 
static int mj_input_update_fd(struct mj_input *input,int fd) {
  int devid=mj_input_devid_by_fd(input,fd);
  char buf[1024];
  int bufc=read(fd,buf,sizeof(buf));
  if (bufc<=0) {
    int err=input->cb(devid,0,0,input->userdata);
    mj_input_drop_device_by_fd(input,fd);
    return 0;
  }
  if (input->cb(devid,buf,bufc,input->userdata)<0) return -1;
  return 0;
}

/* Populate pollfdv.
 */
 
static struct pollfd *mj_input_require_pollfdv(struct mj_input *input,int pvc) {
  if (pvc<input->pollfda) return input->pollfdv+pvc;
  int na=input->pollfda+8;
  if (na>INT_MAX/sizeof(struct pollfd)) return 0;
  void *nv=realloc(input->pollfdv,sizeof(struct pollfd)*na);
  if (!nv) return 0;
  input->pollfdv=nv;
  input->pollfda=na;
  struct pollfd *pollfd=input->pollfdv+pvc;
  pollfd->fd=0;
  pollfd->events=POLLIN|POLLERR|POLLHUP;
  pollfd->revents=0;
  return pollfd;
}
 
static int mj_input_populate_pollfdv(struct mj_input *input) {
  int pollfdc=0;
  struct pollfd *pollfd;
  if (input->infd>0) {
    if (!(pollfd=mj_input_require_pollfdv(input,pollfdc))) return -1;
    pollfdc++;
    pollfd->fd=input->infd;
  }
  const struct mj_input_device *device=input->devicev;
  int i=input->devicec;
  for (;i-->0;device++) {
    if (!(pollfd=mj_input_require_pollfdv(input,pollfdc))) return -1;
    pollfdc++;
    pollfd->fd=device->fd;
  }
  return pollfdc;
}

/* Update.
 */

int mj_input_update(struct mj_input *input,int to_ms) {
  if (!input->cb) return -1;
  if (input->refresh) {
    input->refresh=0;
    return mj_input_scan(input);
  }
  int pollfdc=mj_input_populate_pollfdv(input);
  if (pollfdc<0) return -1;
  if (!pollfdc) {
    if (to_ms>0) usleep(to_ms*1000);
    return 0;
  }
  if (poll(input->pollfdv,pollfdc,to_ms)<0) {
    if (errno==EINTR) return 0;
    return -1;
  }
  struct pollfd *pollfd=input->pollfdv;
  int i=pollfdc;
  for (;i-->0;pollfd++) {
    if (pollfd->revents) {
      if (pollfd->fd==input->infd) {
        if (mj_input_update_inotify(input)<0) return -1;
      } else {
        if (mj_input_update_fd(input,pollfd->fd)<0) return -1;
      }
    }
  }
  return 0;
}
