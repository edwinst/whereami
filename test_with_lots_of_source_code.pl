use warnings;
use strict;
use File::Find::Rule;
use IPC::Run3 qw(run3);

my @source_dirs = qw( c:\cygwin\home\edwin\git\pdf_analyzer d:\third_party_source );

my $rule = File::Find::Rule->file->name('*.c', '*.h', '*.cpp', '*.hpp')
           ->start(@source_dirs);

while (my $path = $rule->match) {
    print "Testing file $path\n";
    for my $exe (
            'whereami',
            #'whereami_debug',
            #'whereami_nowin32',
        )) {
        my @cmd = ($exe, $path, '0');
        run3 \@cmd, \undef, \undef, \*STDERR;
        $? == 0 or die "error: FAILED command: ".join(' ', @cmd)."\n";
    }
}

print "OK\n";
