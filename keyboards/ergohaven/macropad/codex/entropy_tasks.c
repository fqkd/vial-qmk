// SPDX-License-Identifier: GPL-2.0-or-later
#include "entropy_tasks.h"
#include <string.h>

// D7 / v1: header = command, version, operation, request sequence, generation LE.
// Payload starts at byte 8. Responses echo the header; byte 8 is a status.
typedef struct { uint8_t id[16], state, length; char title[97]; } row_t;
static row_t rows[6], pending[6];
static uint8_t received[6][ENTROPY_TITLE_BYTES], row_mask;
static codex_light_t lights[6];
static uint32_t generation, staging_generation, clock_now, lease, interaction;
static bool active, staging, interacted, blinking;
static uint32_t blink_start;
static uint8_t blink_slot;
typedef struct { uint32_t sequence, generation; uint8_t slot; } event_t;
static event_t events[8];
static uint8_t event_count;
static uint32_t event_sequence;
static uint32_t read32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void write32(uint8_t *p, uint32_t n) { for (int i=0;i<4;++i) p[i] = n >> (8*i); }
static bool utf8(const char *s, unsigned n) {
    for (unsigned i=0;i<n;) {
        uint8_t c=(uint8_t)s[i++]; unsigned more; uint32_t cp, min;
        if (c<0x80) { if (c<32 || c==127) return false; continue; }
        if (c>=0xC2 && c<=0xDF) {more=1;cp=c&31;min=0x80;}
        else if(c>=0xE0 && c<=0xEF) {more=2;cp=c&15;min=0x800;}
        else if(c>=0xF0 && c<=0xF4) {more=3;cp=c&7;min=0x10000;}
        else return false;
        if (i+more>n) return false;
        while(more--) { c=(uint8_t)s[i++]; if ((c&0xC0)!=0x80) return false; cp=(cp<<6)|(c&63); }
        if(cp<min || cp>0x10FFFF || (cp>=0xD800 && cp<=0xDFFF)) return false;
    }
    return true;
}
void entropy_tasks_reset(void) {
    active=staging=interacted=blinking=false; event_count=0;
    memset(rows,0,sizeof(rows)); memset(lights,0,sizeof(lights));
}
void entropy_tasks_tick(uint32_t now) {
    clock_now=now;
    if (active && now-lease>=6000) entropy_tasks_reset();
}
bool entropy_tasks_active(void) { return active; }
bool entropy_tasks_occupied(uint8_t slot) { return active && slot<6 && rows[slot].state!=0; }
bool entropy_tasks_completed(uint8_t slot) { return entropy_tasks_occupied(slot) && rows[slot].state==3; }
const char *entropy_tasks_title(uint8_t slot) { return entropy_tasks_occupied(slot) ? rows[slot].title : ""; }
const codex_light_t *entropy_tasks_lights(void) { return lights; }
void entropy_tasks_interaction(void) { interaction=clock_now; interacted=true; }
void entropy_tasks_press(uint8_t slot, bool pressed) {
    if (!pressed || !entropy_tasks_occupied(slot)) return;
    entropy_tasks_interaction();
    if(event_count<8) events[event_count++]=(event_t){++event_sequence,generation,slot};
}
bool entropy_tasks_notification(uint32_t now, codex_light_t *light, int slot) {
    if (!active || !blinking) return false;
    uint32_t elapsed=now-blink_start;
    if(elapsed>=2500) {blinking=false;return false;}
    if(slot>=0 && slot!=blink_slot) return false;
    *light=lights[blink_slot];
    if((elapsed/250)&1) light->brightness=0;
    return true;
}
bool entropy_tasks_receive(uint8_t *p, uint8_t length, uint32_t now) {
    if(length!=32 || p[0]!=0xD7) return false;
    entropy_tasks_tick(now);
    uint8_t op=p[2], slot=p[8], status=0;
    uint32_t gen=read32(p+4);
    uint8_t reply[32]={0}; memcpy(reply,p,8);
    if(p[1]!=1) status=1;
    else if(op==0) memcpy(reply+9,"MPT1",4);
    else if(op==1) {
        memset(pending,0,sizeof(pending)); memset(received,0,sizeof(received));
        row_mask=0; staging_generation=gen; staging=true;
    } else if(op==2 && staging && gen==staging_generation && slot<6) {
        if(p[9]>5 || p[26]>96 || p[27]!=0) status=1;
        else {
            memset(&pending[slot],0,sizeof(row_t));
            memset(received[slot],0,sizeof(received[slot]));
            pending[slot].state=p[9];memcpy(pending[slot].id,p+10,16);pending[slot].length=p[26];row_mask|=1<<slot;
        }
    } else if(op==3 && staging && gen==staging_generation && slot<6) {
        unsigned off=p[9], n=p[10];
        if(!(row_mask&(1<<slot)) || n>21 || off+n>pending[slot].length) status=1;
        else {memcpy(pending[slot].title+off,p+11,n);memset(received[slot]+off,1,n);}
    } else if(op==4 && staging && gen==staging_generation) {
        bool changed=false;
        if(row_mask!=63) status=1;
        for(unsigned i=0;i<6;++i) {
            row_t *r=&pending[i];
            static const uint8_t zero_id[16]={0};
            bool has_id=memcmp(r->id,zero_id,16)!=0;
            if(r->state && (!has_id || !r->length || !utf8(r->title,r->length))) status=1;
            if(!r->state && (has_id || r->length)) status=1;
            for(unsigned j=0;j<i;++j) if(r->state && pending[j].state && !memcmp(r->id,pending[j].id,16)) status=1;
            for(unsigned j=0;j<r->length;++j) if(!received[i][j]) status=1;
            changed|=memcmp(rows[i].id,r->id,16)!=0;
        }
        if(!status && active && (event_count || (changed && interacted && now-interaction<3000))) status=2;
        if(!status) {
            static const uint32_t colors[]={0,0x243447,0x38A8FF,0x36ED95,0xFFBE32,0xFF5270};
            for(unsigned i=0;i<6;++i) {
                if(blinking && blink_slot==i && pending[i].state==1) blinking=false;
                if(active && pending[i].state>=2 && rows[i].state!=pending[i].state && !memcmp(rows[i].id,pending[i].id,16)) {
                    blinking=true;blink_start=now;blink_slot=i;
                }
                lights[i]=(codex_light_t){.color=colors[pending[i].state],.brightness=pending[i].state?255:0,.effect=pending[i].state?1:0};
            }
            memcpy(rows,pending,sizeof(rows)); generation=gen;active=true;staging=false;lease=now;
        }
    } else if(op==5) {
        if(active && gen==generation) lease=now;
        else status=3;
        if(event_count) {write32(reply+9,events[0].sequence);write32(reply+13,events[0].generation);reply[17]=events[0].slot;}
        else reply[17]=255;
    } else if(op==6) {
        if(event_count && events[0].sequence==read32(p+8) && gen==events[0].generation) {
            --event_count;memmove(events,events+1,event_count*sizeof(event_t));
        }
    } else if(op==7 && active && gen==generation) entropy_tasks_reset();
    else status=1;
    reply[8]=status;memcpy(p,reply,32);return true;
}
