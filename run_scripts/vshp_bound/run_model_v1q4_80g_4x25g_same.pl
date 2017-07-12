#!/usr/bin/perl

my $cmd = 'rm -f run_*.log';
system("$cmd");

for (my $i=64; $i<=1536; $i+=64) {
    my $s1=$i;
    my $s0=$i;

    my $port0_cfg = " +*vlan0_qnum0_qshaper_rate=25 +*vlan0_qnum0_minsize=$s0 +*vlan0_qnum0_maxsize=$s0";
    my $port1_cfg = " +*vlan0_qnum1_qshaper_rate=25 +*vlan0_qnum1_minsize=$s1 +*vlan0_qnum1_maxsize=$s1";
    my $port2_cfg = " +*vlan0_qnum2_qshaper_rate=25 +*vlan0_qnum2_minsize=$s0 +*vlan0_qnum2_maxsize=$s0";
    my $port3_cfg = " +*vlan0_qnum3_qshaper_rate=25 +*vlan0_qnum3_minsize=$s1 +*vlan0_qnum3_maxsize=$s1";

    my $cmd = "./tm_osched_model.out -f knobs/v1q4.knobs +*vlan0_vshaper_rate=80" . $port0_cfg . $port1_cfg . $port2_cfg . $port3_cfg . " >& run_$i.log";
    
    print "$cmd\n";
    system ("$cmd");
}

$cmd = "tail -n 9 run_*.log |grep -v 'Done Reporting' | grep -v 'Completed' | grep -v 'Start' |perl -pi -e 's/.*run_(\\d+).log.*/\$1/' |perl -pi -e 's/.*Output Rate.* = (\\d+\.\\d+)/\$1/' |perl -pi -e 's/.*Total.* (\\d+\.\\d+)/\$1/' |xargs -n 1  |xargs -n 6 | sort -n > run_results.csv";

print "$cmd\n";
system ("$cmd");

my $cmd = "gnuplot -e \"set key right center; plot 'run_results.csv' using 1:2 with linespoints title 'q0_shaper: 25G', '' using 1:3 with linespoints title 'q1_shaper: 25G', '' using 1:4 with linespoints title 'q2_shaper: 25G', '' using 1:5 with linespoints title 'q3_shaper: 25G' , '' using 1:6 with linespoints title 'vlan_shaper: 80G'; set term png; set output 'v1q4_80g_4x25g_same.png'; replot; set term x11; \"";

system("$cmd");

