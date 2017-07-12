#!/usr/bin/perl

my $cmd = 'rm -f run_*.log';
system("$cmd");

for (my $i=64; $i<=1536; $i+=64) {
    my $j = 1600 - $i;

    my $s0=$i;
    my $s1=$j;

    my $port0_cfg = " +*vlan0_qnum0_qshaper_rate=30 +*vlan0_qnum0_minsize=$s0 +*vlan0_qnum0_maxsize=$s0";
    my $port1_cfg = " +*vlan0_qnum1_qshaper_rate=30 +*vlan0_qnum1_minsize=$s1 +*vlan0_qnum1_maxsize=$s1";

    my $cmd = "./tm_osched_model.out -f knobs/v1q2.knobs +*vlan0_vshaper_rate=50" . $port0_cfg . $port1_cfg . " >& run_$i.log";
    
    print "$cmd\n";
    system ("$cmd");
}

$cmd = "tail -n 9 run_*.log |grep -v 'Done Reporting' | grep -v 'Completed' | grep -v 'Start' | grep -v 'Percentage' |perl -pi -e 's/.*run_(\\d+).log.*/\$1/' |perl -pi -e 's/.*Output Rate.* = (\\d+\.\\d+)/\$1/' |perl -pi -e 's/.*Total.* (\\d+\.\\d+)/\$1/' |xargs -n 1  |xargs -n 4 | sort -n > run_results.csv";

print "$cmd\n";
system ("$cmd");

my $cmd = "gnuplot -e \"set key right center; plot 'run_results.csv' using 1:2 with linespoints title 'q0_shaper: 30G', '' using 1:3 with linespoints title 'q1_shaper: 30G', '' using 1:4 with linespoints title 'vlan_shaper: 50G'; set term png; set output 'v1q2_50g_1x30g_1x30g_diff.png'; replot; set term x11;\"";


system("$cmd");

