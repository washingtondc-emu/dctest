#!/usr/bin/env perl

################################################################################
#
#
#    dctest: Dreamcast test program collection
#    Copyright (C) 2017 snickerbockers
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#
################################################################################

################################################################################
#
# This script serves as a simple evaluation test for sh4div_test.
#
# It attaches to an already-running WashingtonDC session (launched with the
# -t and -c flags) via the cmd prompt and the serial server.  It begin's
# execution and logs the output.  That output is then compared to a capture of
# sh4div_test which was previously taken from a real Dreamcast over dc-tool
# (ideally it would have been taken over serial but I haven't gotten around to
# building my coder's cable yet).  Certain lines are ignored if they match
# certain regexp patterns (see the do_ignore subroutine), but other than that
# the output from WashingtonDC should perfectly match the capture from the
# Dreamcast since they're both running the same program.
#
# this script requires the Net::Telnet perl package to function.
#
################################################################################

use Net::Telnet();

$t = new Net::Telnet(Timeout => 10, port => 1998);
$t->open("localhost");

sleep 5;

$cmd_con = new Net::Telnet(Timeout => 10, port => 2000);
$cmd_con->open("localhost");

$cmd_con->cmd("begin-execution");

$found_beg=0;

$wash_log = "wash_log.txt";
$dc_log = "dc_log.txt";

print "saving WashingtonDC logs to $wash_log\n";
open($out_file, "> $wash_log") || die "failed to open $wash_log";

@wash_log_lines = ( );

while (my $line = $t->getline()) {
    if ($line =~ /vid_set_mode:.*/) {
        next;
    }

    print $out_file $line;
    push @wash_log_lines, $line;

    if ($line =~ /maple: final stats.*/) {
        last;
    }
}

close($out_file);

open($dc_log_file, "< $dc_log");
@dc_log_lines = <$dc_log_file>;
close($dc_log_file);

# returns true if the given line does not need to match between Dreamcast and
# WashingtonDC
sub do_ignore {
    my $line = shift;

    # KallistiOS enumerates the attached maple devices on bootup.  This can vary
    # between WashingtonDC (which is currently hardcoded to a single controller
    # plugged into A0) and a real Dreamcast (which can have a number of maple
    # devices plugged in.
    if ($line =~ /  [A-D][0-9]:/) {
        return 1;
    }

    # This value depends on what's plugged into the video port.  WashingtonDC is
    # (i think) hardcoded for a component cable connected to an NTSC display,
    # but real Dreamcasts can be plugged into many different devices.
    if ($line =~ /vid_set_mode:/) {
        return 1;
    }

    # this counts the number of vblanks that have occurred as well as the
    # number of maple devices plugged in; obviously these values can 
    # depending on what's plugged into your Dreamcast.
    #
    # TOD: I *do* think there's something wrong with WashingtonDC's SPG because
    # the vblank counter here is way off, but I still have to comment out this
    # line for reasons enumerated above
    if ($line =~ /maple: final stats/) {
        return 1;
    }

    # This line happens because I got the Dreamcast captures via dc-tool (over
    # broadband adapter), but the WashingtonDC captures are taken via serial.
    if ($line =~ /dc-load console support enabled/) {
        return 1;
    }
    return 0;
}

@dc_log_filtered = ( );
@wash_log_filtered = ( );

foreach $line ( @dc_log_lines ) {
    if (not do_ignore($line)) {
        push @dc_log_filtered, $line;
    }
}

foreach $line ( @wash_log_lines ) {
    if (not do_ignore($line)) {
        push @wash_log_filtered, $line;
    }
}

$wash_line_count = scalar(@wash_log_filtered);
$dc_line_count = scalar(@dc_log_filtered);

printf("line counts (post-filter) - %d and %d\n",
       $wash_line_count, $dc_line_count);

if ($wash_line_count != $dc_line_count) {
    die "line length mismatch";
}

for (my $line_no = 0; $line_no < $wash_line_count; $line_no++) {
    if ($wash_log_filtered[$line_no] != $dc_log_filtered[$line_no]) {
        die "mismatch at line $line_no";
    }
}

print "WashingtonDC's log is consistent with Dreamcast capture\n";

$cmd_con->cmd("exit");

sleep 5;

exit $exit_code
