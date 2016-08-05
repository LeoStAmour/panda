/* PANDABEGINCOMMENT
 * 
 * Authors:
 *  Tim Leek               tleek@ll.mit.edu
 *  Ryan Whelan            rwhelan@ll.mit.edu
 *  Joshua Hodosh          josh.hodosh@ll.mit.edu
 *  Michael Zhivich        mzhivich@ll.mit.edu
 *  Brendan Dolan-Gavitt   brendandg@gatech.edu
 * 
 * This work is licensed under the terms of the GNU GPL, version 2. 
 * See the COPYING file in the top-level directory. 
 * 
PANDAENDCOMMENT */
// This needs to be defined before anything is included in order to get
// the PRIx64 macro
#define __STDC_FORMAT_MACROS

extern "C" {

#include <assert.h>
    
#include "rr_log.h"    
#include "qemu-common.h"
#include "cpu.h"
#include "panda_plugin.h"
#include "pandalog.h"
#include "panda_common.h"

#include "../osi/osi_types.h"
#include "../osi/osi_ext.h"

    // this provides the fd resolution magic

#include "../osi_linux/osi_linux_ext.h"

#include "stdio.h"
#include "config.h"
#include "qemu-common.h"

#include "../syscalls2/gen_syscalls_ext_typedefs.h"
#include "../syscalls2/syscalls_common.h"
#include "panda_plugin_plugin.h"

}

// These need to be extern "C" so that the ABI is compatible with
// QEMU/PANDA, which is written in C
extern "C" {

bool init_plugin(void *);
void uninit_plugin(void *);

}

#include <map>

std::map< std::pair<target_ulong, uint32_t>, const char *> asidfd_to_filename;

uint32_t guest_strncpy(CPUState *env, char *buf, size_t maxlen, target_ulong guest_addr) {
    buf[0] = 0;
    unsigned i;
    for (i=0; i<maxlen; i++) {
        uint8_t c;
        panda_virtual_memory_rw(env, guest_addr+i, &c, 1, 0);
        buf[i] = c;
        if (c==0) {
            break;
        }
    }
    buf[maxlen-1] = 0;
    return i;
}

void close_handle(CPUState* env,target_ulong pc,uint32_t fd) {
    //OsiProc *p = get_current_process(env);
    //const char *filename = NULL;
    //if(!p) {
    //    filename = "(unknown process)";
    //} else {
    //    filename = osi_linux_fd_to_filename(env, p, fd);
    //}
    //if(!filename) {
    //    filename = "(unknown filename)";    
    //}
    target_ulong asid = panda_current_asid(env);
    const char *the_filename; 
    if (asidfd_to_filename.find(std::make_pair(asid, fd)) != asidfd_to_filename.end()) 
        the_filename = asidfd_to_filename[std::make_pair(asid, fd)]; 
    else
        the_filename = "(unknown filename)";
    printf("Closed %s with fd: %d\n", the_filename, fd); 
    //free_osiproc(p);
}

#ifdef TARGET_I386
void open_handle(CPUState* env,target_ulong pc,uint32_t filename,int32_t flags,int32_t mode) {
    int MAX_FILENAME = 255;
    char *the_filename = (char *) malloc(MAX_FILENAME);

    target_ulong the_asid = panda_current_asid(env);
    guest_strncpy(env, the_filename, MAX_FILENAME, filename);    

    asidfd_to_filename[std::make_pair(the_asid, EAX)] = the_filename;

    // TODO: memory leaks...
    printf("Opened %s as fd: %d\n", the_filename, EAX);
}
#endif

bool init_plugin(void *self) {
    panda_require("osi");
    assert(init_osi_api());    
    panda_require("osi_linux");
    assert(init_osi_linux_api());
    panda_require("syscalls2");

    PPP_REG_CB("syscalls2", on_sys_close_enter, close_handle);
#ifdef TARGET_I386
    PPP_REG_CB("syscalls2", on_sys_open_return, open_handle);
#endif
    return true;
}

void uninit_plugin(void *self) { }
