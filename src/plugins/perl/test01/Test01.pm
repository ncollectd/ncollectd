package Test01;

use strict;
use warnings;

use NCollectd qw(:all);

sub reader
{
    my $fam = NCollectd::MetricFamily->new("metric_unknow", METRIC_UNKNOWN);
    $fam->append(NCollectd::MetricUnknownDouble->new(42, undef, 1592748157.125));
    $fam->dispatch();

    $fam = NCollectd::MetricFamily->new("metric_gauge", METRIC_GAUGE);
    $fam->append(NCollectd::MetricGaugeDouble->new(42, undef, 1592748157.125));
    $fam->dispatch();

    $fam = NCollectd::MetricFamily->new("metric_counter_with_label", METRIC_COUNTER);
    my $labels = {
        alpha => "first",
        beta  => "second",
    };
    $fam->append(NCollectd::MetricCounterULong->new(0, $labels, 1592748157.125));
    $fam->dispatch();

    $fam = NCollectd::MetricFamily->new("escaped_label_value", METRIC_COUNTER);
    $labels = {
        alpha => "first \"value\"",
        beta  => "second value",
    };
    $fam->append(NCollectd::MetricCounterULong->new(18446744073709551615, $labels,
                                                    1592748157.125));
    $fam->dispatch();

    $fam = NCollectd::MetricFamily->new("system_uname", METRIC_INFO);
    my $value = {
        machine  => "riscv128",
        nodename => "arrakis.canopus",
        release  => "998",
        sysname  => "Linux",
        version  => "#1 SMP PREEMPT_DYNAMIC 10191",
    };
    $labels = {
        hostname => "arrakis.canopus",
    };
    $fam->append(NCollectd::MetricInfo->new($value, $labels, 1592748157.125));
    $fam->dispatch();

    $fam = NCollectd::MetricFamily->new("stateset", METRIC_STATE_SET);
    my $state = {
        a   => 0,
        bb  => 1,
        ccc => 0,
    };
    $labels = {
        hostname => "arrakis.canopus",
    };
    $fam->append(NCollectd::MetricStateSet->new($state, $labels, 1592748157.125));
    $fam->dispatch();

    $fam = NCollectd::MetricFamily->new("summary", METRIC_SUMMARY);
    my $quantiles = [[0.5,  0.232227334],
                     [0.90, 0.821139321],
                     [0.95, 1.528948804],
                     [0.99, 2.829188272],
                     [1,    34.283829292]];
    $labels = {
        hostname => "arrakis.canopus",
    };
    $fam->append(NCollectd::MetricSummary->new(8953.332, 27892, $quantiles, $labels,
                                               1592748157.125));
    $fam->dispatch();

    $fam = NCollectd::MetricFamily->new("histogram", METRIC_HISTOGRAM);
    my $buckets = [[27892, "inf"],
                   [27890, 25],
                   [27881, 10],
                   [27814, 5],
                   [27534, 2.5],
                   [26351, 1],
                   [24101, 0.5],
                   [14251, 0.25],
                   [8954, 0.1],
                   [1672, 0.05],
                   [8, 0.025],
                   [0, 0.01]];
    $labels = {
        hostname => "arrakis.canopus",
    };
    $fam->append(NCollectd::MetricHistogram->new(8953.332, $buckets, $labels, 1592748157.125));
    $fam->dispatch();

    $fam = NCollectd::MetricFamily->new("gauge_histogram", METRIC_GAUGE_HISTOGRAM);
    $buckets = [[120, "inf"],
                [115, 1048576],
                [107, 786432],
                [98, 524288],
                [96, 262144],
                [85, 131072],
                [61, 65536],
                [42, 32768],
                [26, 16384],
                [22, 8192],
                [10, 4096],
                [4, 1024]];
    $labels = {
        hostname => "arrakis.canopus",
    };
    $fam->append(NCollectd::MetricGaugeHistogram->new(120, $buckets, $labels, 1592748157.125));
    $fam->dispatch();
}

NCollectd::register_read(\&reader);

1;
