// SPDX-License-Identifier: GPL-2.0-or-later
#include "entropy_tasks.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t packet[32];
static uint32_t now;
static void put32(uint8_t *p, uint32_t n) { for (unsigned i=0;i<4;++i) p[i]=n>>(8*i); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void request(uint8_t op, uint32_t gen) {
    memset(packet,0,sizeof(packet));packet[0]=0xD7;packet[1]=1;packet[2]=op;packet[3]=42;put32(packet+4,gen);
}
static uint8_t send(void) {
    uint8_t header[8];memcpy(header,packet,8);
    assert(entropy_tasks_receive(packet,32,now));
    assert(!memcmp(header,packet,8));return packet[8];
}
static void meta(uint32_t gen, uint8_t slot, uint8_t id, uint8_t state, uint8_t length) {
    request(2,gen);packet[8]=slot;packet[9]=state;packet[10]=id;packet[26]=length;assert(send()==0);
}
static void title(uint32_t gen, uint8_t slot, uint8_t offset, const char *s, uint8_t length) {
    request(3,gen);packet[8]=slot;packet[9]=offset;packet[10]=length;memcpy(packet+11,s,length);assert(send()==0);
}
static void stage(uint32_t gen, uint8_t id, uint8_t state) {
    request(1,gen);assert(send()==0);
    meta(gen,0,id,state,5);title(gen,0,0,"Alpha",5);
    for (uint8_t i=1;i<6;++i) meta(gen,i,0,0,0);
}
static void commit(uint32_t gen, uint8_t status) { request(4,gen);assert(send()==status); }
static void reset(void) {entropy_tasks_reset();now=100;entropy_tasks_tick(now);}

static void atomic_validation(void) {
    reset();request(0,0);assert(send()==0);assert(!memcmp(packet+9,"MPT1",4));
    request(1,1);assert(send()==0);meta(1,0,1,2,5);title(1,0,0,"Al",2);
    commit(1,1);assert(!entropy_tasks_active());
    for(uint8_t i=1;i<6;++i) meta(1,i,0,0,0);
    commit(1,1);assert(!entropy_tasks_active());
    title(1,0,2,"pha",3);commit(1,0);
    assert(!strcmp(entropy_tasks_title(0),"Alpha"));
    stage(2,2,3);meta(2,0,2,3,5); // Repeated metadata must discard old chunks.
    commit(2,1);assert(!strcmp(entropy_tasks_title(0),"Alpha"));
    title(2,0,0,"Bravo",5);commit(2,0);
    assert(!strcmp(entropy_tasks_title(0),"Bravo"));
    stage(3,0,3);commit(3,1); // Occupied slots require a nonzero ID.
    stage(3,2,3);meta(3,1,2,2,1);title(3,1,0,"B",1);commit(3,1); // No duplicate IDs.
    stage(3,2,3);meta(3,1,3,0,0);commit(3,1); // Empty slots have no ID.
    stage(3,2,3);meta(3,0,2,3,2);title(3,0,0,"\xc0\xaf",2);commit(3,1);
    meta(3,0,2,3,2);title(3,0,0,"\xd1\x8f",2);commit(3,0);
    assert(!strcmp(entropy_tasks_title(0),"\xd1\x8f"));
    request(3,3);packet[8]=255;assert(send()==1);
    request(0,0);packet[1]=2;assert(send()==1);
    packet[0]=0xD8;assert(!entropy_tasks_receive(packet,32,now));
    packet[0]=0xD7;assert(!entropy_tasks_receive(packet,31,now));
}
static void matching_events_and_lease(void) {
    reset();stage(10,1,2);commit(10,0);
    entropy_tasks_press(0,true);entropy_tasks_press(0,false);entropy_tasks_press(5,true);
    stage(11,2,3);commit(11,2); // Cannot change labels with an unacknowledged event.
    request(5,10);assert(send()==0);assert(packet[17]==0);assert(get32(packet+13)==10);
    uint32_t event=get32(packet+9);
    request(6,11);put32(packet+8,event);assert(send()==0); // Wrong generation does not acknowledge.
    request(5,10);assert(send()==0);assert(get32(packet+9)==event);
    request(6,10);put32(packet+8,event);assert(send()==0);
    request(5,10);assert(send()==0);assert(packet[17]==255);
    commit(11,2); // Three-second interaction guard still applies.
    now+=3000;commit(11,0);
    entropy_tasks_press(0,true);
    request(5,11);assert(send()==0);assert(packet[17]==0 && get32(packet+13)==11);
    request(7,10);assert(send()==1);assert(entropy_tasks_active());
    now+=5999;entropy_tasks_tick(now);assert(entropy_tasks_active());
    now++;entropy_tasks_tick(now);assert(!entropy_tasks_active());
    assert(!entropy_tasks_occupied(0));assert(!strcmp(entropy_tasks_title(0),""));
    request(5,11);assert(send()==3);assert(packet[17]==255);
}
static void status_and_blink(void) {
    reset();stage(20,1,2);commit(20,0);
    codex_light_t light;
    assert(!entropy_tasks_notification(now,&light,-1)); // Initial sync isn't a completion.
    entropy_tasks_interaction();stage(21,1,3);commit(21,0); // Same-ID status updates aren't frozen.
    for(unsigned i=0;i<10;++i) {
        assert(entropy_tasks_notification(now+i*250,&light,-1));
        assert(light.brightness==(i%2 ? 0 : 255));assert(light.color==0x165B43);
    }
    assert(!entropy_tasks_notification(now+2500,&light,-1));
    assert(!entropy_tasks_notification(now,&light,1));
    now+=3000;stage(22,2,2);commit(22,0);
    assert(!entropy_tasks_notification(now,&light,-1)); // Replacing an ID doesn't fake a transition.
    request(7,22);assert(send()==0);assert(!entropy_tasks_active());
    now=UINT32_MAX-100;stage(23,1,2);commit(23,0);
    entropy_tasks_tick(5898);assert(entropy_tasks_active());
    entropy_tasks_tick(5899);assert(!entropy_tasks_active()); // Timer wraps safely.
}
int main(void) {
    atomic_validation();matching_events_and_lease();status_and_blink();
    puts("Entropy task protocol tests passed");return 0;
}
