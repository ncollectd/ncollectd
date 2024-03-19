package NCollectd::Plugins::Test01;

use strict;
use warnings;

use NCollectd qw( :all );

sub test01_read
{
    plugin_log(LOG_NOTICE, "test01_read called");

    my $fam = {
        name => 'metric_counter',
        help => 'help for counter test',
        unit => 'some counter',
        type => METRIC_TYPE_COUNTER,
        metrics => [
            {
                time => 0,
                interval => 10,
                labels => { labela => 'keya', labelb => 'keyb' },
                value => 99
            },
            {
                time => 0,
                interval => 10,
                labels => { labelc => 'keyc', labeld => 'keyd' },
                value => 999
            },
        ]
    };

    plugin_dispatch_metric_family ($fam);

    $fam = {
        name => 'metric_gauge',
        help => 'help for gauge test',
        unit => 'some gauge',
        type => METRIC_TYPE_GAUGE,
        metrics => [
            {
                time => 0,
                interval => 10,
                labels => { labela => 'keya', labelb => 'keyb' },
                value => 3.1415
            },
            {
                time => 0,
                interval => 10,
                labels => { labelc => 'keyc', labeld => 'keyd' },
                value => 2.7182
            },
        ]
    };

    plugin_dispatch_metric_family ($fam);

    return 1;
}

plugin_register (TYPE_READ, "test01", "test01_read");
