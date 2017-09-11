#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2003 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

top_filename("t/t_display.v");

compile (
	 verilator_flags2 => ["-O0"],
	 );

execute (
	 check_finished=>1,
	 expect=>dequote(
q{[0] In top.t: Hi
[0] In top.t.sub (sub)
[0] In top.t.sub.subblock (sub)
[0] In top.t.sub2 (sub2)
[0] In top.t.sub2.subblock2 (sub2)
[0] Back \ Quote "
[0] %b=000001100 %0b=1100  %b=00000101010111011101110111100110011001100 %0b=101010111011101110111100110011001100  %b=000001010101111000001001000110100010101100111100000010010001101000101011001111000 %0b=1010101111000001001000110100010101100111100000010010001101000101011001111000
[0] %B=000001100 %0B=1100  %B=00000101010111011101110111100110011001100 %0B=101010111011101110111100110011001100  %B=000001010101111000001001000110100010101100111100000010010001101000101011001111000 %0B=1010101111000001001000110100010101100111100000010010001101000101011001111000
[0] %d= 12 %0d=12  %d=  46099320012 %0d=46099320012
[0] %D= 12 %0D=12  %D=  46099320012 %0D=46099320012
[0] %h=00c %0h=c  %h=00abbbbcccc %0h=abbbbcccc  %h=00abc1234567812345678 %0h=abc1234567812345678
[0] %H=00c %0H=c  %H=00abbbbcccc %0H=abbbbcccc  %H=00abc1234567812345678 %0H=abc1234567812345678
[0] %o=014 %0o=14  %o=00527356746314 %0o=527356746314  %o=012570110642547402215053170 %0o=12570110642547402215053170
[0] %O=014 %0O=14  %O=00527356746314 %0O=527356746314  %O=012570110642547402215053170 %0O=12570110642547402215053170
[0] %x=00c %0x=c  %x=00abbbbcccc %0x=abbbbcccc  %x=00abc1234567812345678 %0x=abc1234567812345678
[0] %X=00c %0X=c  %X=00abbbbcccc %0X=abbbbcccc  %X=00abc1234567812345678 %0X=abc1234567812345678
[0] %C=m %0C=m
[0] %c=m %0c=m
[0] %v=St0 St0 St0 St0 St0 St1 St1 St0 St0  %0v=St0 St0 St0 St0 St0 St1 St1 St0 St0   %v=St0 St0 St0 St0 St0 St1 St0 St1 St0 St1 St0 St1 St1 St1 St0 St1 St1 St1 St0 St1 St1 St1 St0 St1 St1 St1 St1 St0 St0 St1 St1 St0 St0 St1 St1 St0 St0 St1 St1 St0 St0  %0v=St0 St0 St0 St0 St0 St1 St0 St1 St0 St1 St0 St1 St1 St1 St0 St1 St1 St1 St0 St1 St1 St1 St0 St1 St1 St1 St1 St0 St0 St1 St1 St0 St0 St1 St1 St0 St0 St1 St1 St0 St0   %v=St0 St0 St0 St0 St0 St1 St0 St1 St0 St1 St0 St1 St1 St1 St1 St0 St0 St0 St0 St0 St1 St0 St0 St1 St0 St0 St0 St1 St1 St0 St1 St0 St0 St0 St1 St0 St1 St0 St1 St1 St0 St0 St1 St1 St1 St1 St0 St0 St0 St0 St0 St0 St1 St0 St0 St1 St0 St0 St0 St1 St1 St0 St1 St0 St0 St0 St1 St0 St1 St0 St1 St1 St0 St0 St1 St1 St1 St1 St0 St0 St0  %0v=St0 St0 St0 St0 St0 St1 St0 St1 St0 St1 St0 St1 St1 St1 St1 St0 St0 St0 St0 St0 St1 St0 St0 St1 St0 St0 St0 St1 St1 St0 St1 St0 St0 St0 St1 St0 St1 St0 St1 St1 St0 St0 St1 St1 St1 St1 St0 St0 St0 St0 St0 St0 St1 St0 St0 St1 St0 St0 St0 St1 St1 St0 St1 St0 St0 St0 St1 St0 St1 St0 St1 St1 St0 St0 St1 St1 St1 St1 St0 St0 St0  <
[0] %V=St0 St0 St0 St0 St0 St1 St1 St0 St0  %0V=St0 St0 St0 St0 St0 St1 St1 St0 St0   %V=St0 St0 St0 St0 St0 St1 St0 St1 St0 St1 St0 St1 St1 St1 St0 St1 St1 St1 St0 St1 St1 St1 St0 St1 St1 St1 St1 St0 St0 St1 St1 St0 St0 St1 St1 St0 St0 St1 St1 St0 St0  %0V=St0 St0 St0 St0 St0 St1 St0 St1 St0 St1 St0 St1 St1 St1 St0 St1 St1 St1 St0 St1 St1 St1 St0 St1 St1 St1 St1 St0 St0 St1 St1 St0 St0 St1 St1 St0 St0 St1 St1 St0 St0   %V=St0 St0 St0 St0 St0 St1 St0 St1 St0 St1 St0 St1 St1 St1 St1 St0 St0 St0 St0 St0 St1 St0 St0 St1 St0 St0 St0 St1 St1 St0 St1 St0 St0 St0 St1 St0 St1 St0 St1 St1 St0 St0 St1 St1 St1 St1 St0 St0 St0 St0 St0 St0 St1 St0 St0 St1 St0 St0 St0 St1 St1 St0 St1 St0 St0 St0 St1 St0 St1 St0 St1 St1 St0 St0 St1 St1 St1 St1 St0 St0 St0  %0V=St0 St0 St0 St0 St0 St1 St0 St1 St0 St1 St0 St1 St1 St1 St1 St0 St0 St0 St0 St0 St1 St0 St0 St1 St0 St0 St0 St1 St1 St0 St1 St0 St0 St0 St1 St0 St1 St0 St1 St1 St0 St0 St1 St1 St1 St1 St0 St0 St0 St0 St0 St0 St1 St0 St0 St1 St0 St0 St0 St1 St1 St0 St1 St0 St0 St0 St1 St0 St1 St0 St1 St1 St0 St0 St1 St1 St1 St1 St0 St0 St0  <
[0] %p='hc %0p='hc  %p='habbbbcccc %0p='habbbbcccc  %p='habc1234567812345678 %0p='habc1234567812345678
[0] %P='hc %0P='hc  %P='habbbbcccc %0P='habbbbcccc  %P='habc1234567812345678 %0P='habc1234567812345678
[0] %P="sv-str"
[0] %u=dcba %0u=dcba
[0] %U=dcba %0U=dcba
[0] %D= 12 %d= 12 %01d=12 %06d=000012 %6d=    12
[0] %t=                   0 %03t=  0 %0t=0

[0] %s=! %s= what! %s= hmmm!1234
[0] hello, from a very long string. Percent %s are literally substituted in.
hello, from a concatenated string.
hello, from a concatenated format string [0].
extra argument: 0000000000000000
0000000000000000: pre argument
[0] Embedded <#013> return
[0] Embedded
multiline
*-* All Finished *-*
}),
     );

ok(1);

# Don't put control chars into our source repository, pre-compress instead
sub dequote { my $s = shift; $s =~ s/<#013>/\r/g; $s; }

1;
