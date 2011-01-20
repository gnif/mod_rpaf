#!/usr/bin/perl

use strict;
use warnings;
use Data::Dumper;
use LWP::UserAgent;
use HTTP::Request;

my $localhost_ip    = '127.0.0.1';
my $forward_ip      = '140.211.11.130';
my $x_forwarded_for = "$localhost_ip; $forward_ip";
my $http_host       = "$localhost_ip:2500";
my $x_host          = 'www.apache.org';

my $test = shift || die("Missing a testcase");
my $ua;
if ($test ne 'test6') {
    $ua = LWP::UserAgent->new();
} else {
    $ua = LWP::UserAgent->new(keep_alive => 1);
}
my $request = HTTP::Request->new(GET => 'http://127.0.0.1:2500/cgi-bin/env.cgi');

run_test1() if $test eq 'test1';
run_test2() if $test eq 'test2';
run_test3() if $test eq 'test3';
run_test4() if $test eq 'test4';
run_test5() if $test eq 'test5';
run_test6() if $test eq 'test6';
run_test6() if $test eq 'test7';

sub run_test1 {
    # two tests - one without X-Forwarded-For and one with 

    execute_test($request, { REMOTE_ADDR => $localhost_ip, HTTP_HOST => $http_host, HTTP_X_FORWARDED_FOR => ''});
    
    $request->header('X-Forwarded-For' => $x_forwarded_for);
    execute_test($request, { REMOTE_ADDR => $localhost_ip, HTTP_HOST => $http_host, HTTP_X_FORWARDED_FOR => $x_forwarded_for});
}

sub run_test2 {
    # three tests - one without X-Forwarded-For; one with; and one with X-Host

    execute_test($request, { REMOTE_ADDR => $localhost_ip, HTTP_HOST => $http_host, HTTP_X_FORWARDED_FOR => ''});
    
    $request->header('X-Forwarded-For' => $x_forwarded_for);
    execute_test($request, { REMOTE_ADDR => $forward_ip, HTTP_HOST => $http_host, HTTP_X_FORWARDED_FOR => $x_forwarded_for});

    $request->header('X-Host' => $x_host);
    execute_test($request, { REMOTE_ADDR => $forward_ip, HTTP_HOST => $http_host, HTTP_X_FORWARDED_FOR => $x_forwarded_for});
}

sub run_test3 {
    # three tests one without X-Host; one with; and one with X-Forwarded-Host
    
    $request->header('X-Forwarded-For' => $x_forwarded_for);
    execute_test($request, { REMOTE_ADDR => $forward_ip, HTTP_HOST => $http_host, HTTP_X_FORWARDED_FOR => $x_forwarded_for});

    $request->header('X-Host' => $x_host);
    execute_test($request, { REMOTE_ADDR => $forward_ip, HTTP_HOST => $x_host, HTTP_X_FORWARDED_FOR => $x_forwarded_for});

    $request->remove_header('X-Host');
    $request->header('X-Forwarded-Host' => $x_host);
    execute_test($request, { REMOTE_ADDR => $forward_ip, HTTP_HOST => $x_host, HTTP_X_FORWARDED_FOR => $x_forwarded_for});
}

sub run_test4 {
    # three tests one without X-Host; one with; and one with X-Forwarded-Host
    
    $request->header('X-Forwarded-For' => $x_forwarded_for);
    execute_test($request, { REMOTE_ADDR => $forward_ip, HTTP_HOST => $http_host, HTTP_X_FORWARDED_FOR => $x_forwarded_for});

    $request->header('X-Host' => $x_host);
    execute_test($request, { REMOTE_ADDR => $forward_ip, HTTP_HOST => $x_host, HTTP_X_FORWARDED_FOR => $x_forwarded_for});

    $request->remove_header('X-Host');
    $request->header('X-Forwarded-Host' => $x_host);
    execute_test($request, { REMOTE_ADDR => $forward_ip, HTTP_HOST => $x_host, HTTP_X_FORWARDED_FOR => $x_forwarded_for});
}

sub run_test5 {
    # two tests with X-Real-IP; one without X-Host; one with
    $request->remove_header('X-Forwarded-For');
    $request->header('X-Real-IP' => $x_forwarded_for);
    execute_test($request, { REMOTE_ADDR => $forward_ip, HTTP_HOST => $http_host, HTTP_X_REAL_IP => $x_forwarded_for});

    $request->header('X-Host' => $x_host);
    execute_test($request, { REMOTE_ADDR => $forward_ip, HTTP_HOST => $x_host, HTTP_X_REAL_IP => $x_forwarded_for});

}

sub run_test6 {
    # attempt to test keep-alive
    print "Testing Keep-Alive\n";
    my $static_request = HTTP::Request->new(GET => 'http://127.0.0.1:2500/index.html');
    print "First using X-F-F: $x_forwarded_for\n";
    $static_request->header('X-Forwarded-For' => $x_forwarded_for);
    my $response = $ua->request($static_request);
    if ($response->is_success()) {
	
    } else {
	print "Something went wrong in requesting file..";
    }
    my $reverse_xff = "$localhost_ip; " . (join ".", reverse split /\./, $forward_ip);
    print "Then using X-F-F: $reverse_xff\n";
    $static_request->header('X-Forwarded-For' => $reverse_xff);
    $response = $ua->request($static_request);
    if ($response->is_success()) {
	
    } else {
	print "Something went wrong in requesting file..";
    }
    print "Now requesting without X-F-F\n";
    $static_request->remove_header('X-Forwarded-For');
    $response = $ua->request($static_request);
    if ($response->is_success()) {
	
    } else {
	print "Something went wrong in requesting file..";
    }
    print "Done\n";
}


sub execute_test {
    my $request = shift;
    my $expected = shift;

    my $response = $ua->request($request);
    if ($response->is_success()) {
	my $returned = {};
	my $content = $response->content();
	my @rows = split /\n/, $content;
	foreach my $row (@rows) {
	    my ($key,$value) = split / \= /, $row;
	    $returned->{$key} = $value;
	}
	# compare with $expected
	foreach my $key (sort keys %$expected) {
	    print qq{Expected $key "$expected->{$key}" };
	    print qq{Response "$returned->{$key}" } if defined($returned->{$key});
	    my $status = (defined($returned->{$key}) && $returned->{$key} eq $expected->{$key} ? 'OK' : 'NOT OK' );
	    print qq{$status\n};
	    die(Dumper($response)) if $status ne 'OK';
	}
	# Everything looks like it worked
	print qq{*** Test passed ***\n};
    } else {
	die($response);
    }
}
