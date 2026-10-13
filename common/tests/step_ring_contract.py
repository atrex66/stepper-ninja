#!/usr/bin/env python3
"""Run the actual LinuxCNC ring functions with fake alarms and PIO FIFOs.

No device/network is accessed. This checks scheduling, order and IRQ backpressure,
not physical pulse timing. Run: python3 common/tests/step_ring_contract.py cc
"""
import pathlib
import re
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
source = (root / 'firmware/src/main.c').read_text()


def function(name):
    match = re.search(r'^.*\b' + name + r'\)??\([^;\n]*\)\s*\{', source, re.M)
    if not match:
        raise AssertionError('Missing firmware function: ' + name)
    brace = source.index('{', match.start())
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


start = source.index('    #define STEP_RING_BUFFER_SIZE')
ring_globals = source[start:source.index('    #endif', start)]
fake = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define __time_critical_func(name) name
typedef unsigned uint;
static uint8_t timer_started, checksum_error;
static int alarm_num = -1;
static uint32_t TIMEOUT_US = 100000;
static uint32_t dir_pin[stepgens];
static struct {int pio, sm;} stepgen_pio[stepgens];
static struct {uint32_t stepgen_command[stepgens];} rx_storage, *rx_buffer=&rx_storage;
static uint64_t now, target;
static bool armed, full[stepgens], miss_once;
static unsigned missed, forced, callback_changes, cancelled, unclaimed;
static unsigned irq_disabled, writes[stepgens], expected[stepgens], directions;
static bool check_order;
static uint64_t time_us_64(void){return now;}
static uint32_t time_us_32(void){return (uint32_t)now;}
static uint32_t save_and_disable_interrupts(void){uint32_t old=irq_disabled;irq_disabled=1;return old;}
static void restore_interrupts(uint32_t old){irq_disabled=old;}
static int hardware_alarm_claim_unused(bool required){(void)required;return 0;}
static void hardware_alarm_set_callback(uint n,void(*cb)(uint)){(void)n;(void)cb;callback_changes++;}
static uint64_t from_us_since_boot(uint64_t t){return t;}
static bool hardware_alarm_set_target(uint n,uint64_t t){
    (void)n;
    if(miss_once || t<=now){miss_once=false;missed++;return true;}
    armed=true;target=t;return false;
}
static void hardware_alarm_force_irq(uint n){(void)n;forced++;armed=true;target=now;}
static void hardware_alarm_cancel(uint n){(void)n;armed=false;cancelled++;}
static void hardware_alarm_unclaim(uint n){(void)n;unclaimed++;}
static bool pio_sm_is_tx_fifo_full(int p,int sm){(void)p;return full[sm];}
static void gpio_put(uint pin,uint value){(void)pin;(void)value;directions++;}
static void pio_sm_put(int p,int sm,uint32_t word){
    (void)p;assert(!full[sm]);writes[sm]++;
    if(check_order) assert(word==++expected[sm]);
}
static void pio_sm_put_blocking(int p,int sm,uint32_t word){
    assert(!"Buffered timer must never call blocking PIO writes");
    (void)p;(void)sm;(void)word;
}
void timer_callback(uint n);
'''
tests = r'''
static void fresh(uint64_t start){
    reset_step_ring_buffer();step_timer_stop_pending=false;timer_started=0;
    alarm_num=-1;now=start;armed=false;miss_once=false;checksum_error=0;
    missed=forced=callback_changes=cancelled=unclaimed=directions=0;
    memset(full,0,sizeof(full));memset(writes,0,sizeof(writes));
    memset(expected,0,sizeof(expected));check_order=false;
    for(int i=0;i<stepgens;i++){stepgen_pio[i].sm=i;dir_pin[i]=(uint32_t)i;}
}
static void queue(uint32_t word){
    uint32_t commands[stepgens];
    for(int i=0;i<stepgens;i++)commands[i]=word;
    enqueue_stepgen_commands(commands);
}
static void fire(void){
    assert(armed);now=target;armed=false;
    uint32_t irq=save_and_disable_interrupts();timer_callback(0);restore_interrupts(irq);
}
static void fill(void){queue(1);now+=1000;queue(2);now+=1000;queue(3);}
static void stream(unsigned jitter,uint64_t start,unsigned servo_us){
    fresh(start);check_order=true;uint64_t arrival=start;
    for(unsigned i=1;i<=200000;i++){
        arrival+=(i&1)?servo_us+jitter:servo_us-jitter;
        while(armed&&target<=arrival)fire();
        now=arrival;queue(i);
        assert(!step_ring_overflow && !step_ring_underflow);
    }
    while(armed)fire();
    for(int i=0;i<stepgens;i++)assert(writes[i]==200000);
    assert(!timer_started && step_ring_underflow);
    printf("PASS: %d axes, 200000 ordered records, period %u us +/- %u us, start=%llu\n",
        stepgens,servo_us,jitter,(unsigned long long)start);
}
static void irregular_jitter(void){
    fresh(100000);check_order=true;uint32_t seed=123456789;
    for(unsigned i=1;i<=200000;i++){
        seed=seed*1664525u+1013904223u;
        int32_t offset=(int32_t)(seed%801u)-400;
        uint64_t arrival=100000ull+i*1000ull+offset;
        while(armed&&target<=arrival)fire();
        now=arrival;queue(i);
        if(step_ring_overflow||step_ring_underflow){fprintf(stderr,"jitter failure at %u, overflow=%d underflow=%d fill=%u period=%llu\n",i,step_ring_overflow,step_ring_underflow,step_ring_count,(unsigned long long)(step_timer_period_q16>>16));}
        assert(!step_ring_overflow&&!step_ring_underflow);
    }
    while(armed)fire();
    for(int i=0;i<stepgens;i++)assert(writes[i]==200000);
    puts("PASS: 200000 ordered records with irregular +/-400 us arrival jitter");
}
int main(void){
    (void)apply_stepgen_commands;(void)stepgen_update_handler;
    stream(0,100000,1000);stream(100,100000,1000);stream(500,100000,1000);
    stream(500,UINT32_MAX-5000ull,1000);stream(0,0,1000);
    stream(250,100000,500);stream(1000,100000,2000);irregular_jitter();
    fresh(0);fill();check_order=true;while(armed)fire();
    assert(writes[0]==3);
    fresh(100000);fill();fire();
    now=target+1001;armed=false;timer_callback(0);
    assert(armed&&target>now&&timer_started&&writes[0]==2);
    fire();assert(writes[0]==3);fire();assert(!timer_started);
    puts("PASS: late IRQ rearms and drains in order");
    fresh(100000);fill();miss_once=true;fire();
    assert(missed==1&&forced==2&&armed&&timer_started);
    while(armed)fire();
    assert(writes[0]==3);
    puts("PASS: SDK set-target race is recovered with a forced IRQ");
    fresh(100000);fill();full[stepgens-1]=true;uint8_t tail=step_ring_tail;
    fire();assert(armed&&step_ring_count==3&&step_ring_tail==tail&&directions==0);
    for(int i=0;i<stepgens;i++)assert(writes[i]==0);
    full[stepgens-1]=false;check_order=true;
    while(armed)fire();
    for(int i=0;i<stepgens;i++)assert(writes[i]==3);
    puts("PASS: full last-axis FIFO defers the whole record without blocking or partial writes");
    fresh(100000);fill();checksum_error=1;check_order=true;
    while(armed)fire();
    for(int i=0;i<stepgens;i++)assert(writes[i]==3);
    puts("PASS: subsequent checksum failure does not discard validated queued motion");
    fresh(100000);fill();queue(4);assert(step_ring_overflow);fire();queue(4);
    assert(step_ring_overflow);reset_step_ring_buffer();assert(!step_ring_overflow);
    puts("PASS: genuine overflow remains visible until ring reset");
    fresh(100000);fill();unsigned before=callback_changes;stop_timer();
    assert(step_timer_stop_pending&&callback_changes==before&&unclaimed==0);
    fire();assert(writes[0]==0&&!armed);service_step_timer_stop();
    assert(!step_timer_stop_pending&&!timer_started&&alarm_num==-1&&step_ring_count==0);
    assert(unclaimed==1&&cancelled==1&&callback_changes==before+1);
    assert(step_timer_period_q16==(1000ull<<16)&&step_timer_fraction==0&&step_period_samples==0&&step_period_window==2&&!have_step_packet_time);
    timer_callback(0);assert(!armed&&writes[0]==0);
    now+=1000;fill();check_order=true;while(armed)fire();
    assert(writes[0]==3);
    puts("PASS: watchdog posts stop, owner cancels, stale IRQ is ignored, reconnect starts fresh");
    fresh(100000);uint32_t zero[stepgens]={0};enqueue_stepgen_commands(zero);
    now+=1000;enqueue_stepgen_commands(zero);now+=1000;enqueue_stepgen_commands(zero);
    while(armed)fire();
    assert(directions==0&&writes[0]==0);
    puts("PASS: zero motion records make no PIO or direction writes");
}
'''
functions = '\n'.join(function(name) for name in (
    'apply_stepgen_commands', 'stepgen_update_handler', 'reset_step_ring_buffer',
    'schedule_step_timer', 'arm_step_timer', 'enqueue_stepgen_commands',
    'service_step_timer_stop', 'timer_callback', 'stop_timer'))
with tempfile.TemporaryDirectory(prefix='sn-step-ring-contract-') as directory:
    path = pathlib.Path(directory)
    (path / 'contract.c').write_text(fake + ring_globals + functions + tests)
    for axes in (1, 4, 8):
        subprocess.run([sys.argv[1] if len(sys.argv)>1 else 'cc', '-std=c11',
                        '-O2', '-Wall', '-Wextra', '-Werror',
                        '-Duse_timer_interrupt=1',
                        '-Dstepgens='+str(axes), str(path / 'contract.c'),
                        '-o', str(path / 'contract')], check=True)
        subprocess.run([str(path / 'contract')], check=True)
