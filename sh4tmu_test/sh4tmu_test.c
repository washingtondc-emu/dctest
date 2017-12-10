/*******************************************************************************
 *
 * Copyright (c) 2017, snickerbockers <chimerasaurusrex@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

/*
 * The purpose of this test is to test that the TMU is generating interrupts.
 * KallistiOS' timer.h header includes a warning against scheduling interrupts
 * on the TMU directly because KOS needs to be able to do that.  Instead, this
 * test will poll the timer_ms_gettime64 function to make sure it changes.  This
 * function does depend on TMU channel 2 interrupts to update its counter.
 */

#include <stdio.h>
#include <arch/timer.h>
#include <arch/types.h>

/*
 * it's hard to pick a good value here.  Sh4 is a 200MHz CPU and each iteration
 * should take several cycles so in theory it should be safe to put the value
 * well below 200 million.  For now I go with 300 million just to keep false
 * failures from happening.  WashingtonDC is not cycle accurate, and I don't
 * have a good grip on how inaccurate it is yet.
 *
 * TODO: come up with some hand-coded assembly so I can be absolutely sure how
 * many cycles each loop iteration ought to take.
 */
#define MAX_ITERATIONS ((unsigned long long)(300 * 1000 * 1000))

struct timestamp {
    uint32 secs, msecs;
};

int main(int argc, char **argv) {
    /*
     * XXX We don't have any printf statements until after the main loop because
     * we don't want the scif to interfere with our timings.  Ideally we would
     * initialize KallistiOS with INIT_QUIET so it doesn't print anything
     * either, but I *think* that would disable printf as well, and we don't
     * want that.
     */

    struct timestamp initial_timestamp, cur_time;
    timer_ms_gettime(&initial_timestamp.secs, &initial_timestamp.msecs);

    unsigned long long iterations = 0;
    do {
        timer_ms_gettime(&cur_time.secs, &cur_time.msecs);
    } while (cur_time.secs == initial_timestamp.secs &&
             ++iterations < MAX_ITERATIONS);

    /*
     * print out some stats.  Because WashingtonDC is not a cycle-accurate
     * emulator you cannot expect these stats to match between WashingtonDC and
     * a real Dreamcast , but I do want to keep an eye on what sorts of
     * discrepancies exist.
     *
     * The evaluation script will ignore these three lines.
     */
    printf("the initial timestamp was %u secs, %u msecs\n",
           (unsigned)initial_timestamp.secs, (unsigned)initial_timestamp.msecs);
    printf("the final timestamp was %u and cur_time.msecs is %u\n",
           (unsigned)cur_time.secs, (unsigned)cur_time.msecs);
    printf("%llu iterations of the loop were performed\n", iterations);

    unsigned total_ms = (1000 * cur_time.secs + cur_time.msecs) -
        (1000 * initial_timestamp.secs + initial_timestamp.msecs);
    double iterations_per_ms = (double)iterations / (double)total_ms;

    printf("That comes out to %f iterations per millisecond.\n",
           iterations_per_ms);

    /*
     * What we're *really* here to test is that the TMU channel 2 interrupt
     * fired.  KallistiOS has it configured to go off once per second; it's
     * what counts how many whole seconds have elapsed since bootup.  Ergo, if
     * cur_time.secs != initial_timestamp.secs, then we got our interrupt.
     */
    if (cur_time.secs != initial_timestamp.secs)
        printf("SUCCESS - the TMU channel 2 interrupt went off\n");
    else
        printf("FAILURE - the TMU channel 2 interrupt was never received\n");

    return 0;
}
