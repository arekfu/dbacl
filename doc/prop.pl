#!/usr/bin/perl

$special = $normal = 0;
while(<STDIN>) {
    $special++ if /^ >/;
    $normal++;
    print;
}
$prop = $special/$normal;
print "proportion: $prop\n";


