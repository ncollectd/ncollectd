# SPDX-License-Identifier: GPL-2.0-only
# SPDX-FileCopyrightText: Copyright (C) 2007-2009  Sebastian Harl
# SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>

package NCollectd;

use strict;
use warnings;

use Config;

use threads;
use threads::shared;

BEGIN {
    if (! $Config{'useithreads'}) {
        die "Perl does not support ithreads!";
    }
}

require Exporter;

our @ISA = qw( Exporter );

our %EXPORT_TAGS = (
    'plugin' => [ qw(
            plugin_register
            plugin_unregister
            plugin_dispatch_metric_family
            plugin_get_interval
            plugin_flush
            plugin_flush_one
            plugin_flush_all
            plugin_dispatch_notification
            plugin_log
    ) ],
    'types' => [ qw(
            TYPE_INIT
            TYPE_READ
            TYPE_WRITE
            TYPE_SHUTDOWN
            TYPE_LOG
            TYPE_NOTIF
            TYPE_FLUSH
            TYPE_CONFIG
    ) ],
    'metric_type' => [ qw(
            METRIC_TYPE_UNKNOWN
            METRIC_TYPE_GAUGE
            METRIC_TYPE_COUNTER
            METRIC_TYPE_STATE_SET
            METRIC_TYPE_INFO
            METRIC_TYPE_SUMMARY
            METRIC_TYPE_HISTOGRAM
            METRIC_TYPE_GAUGE_HISTOGRAM
    ) ],
    'log' => [ qw(
            ERROR
            WARNING
            NOTICE
            INFO
            DEBUG
            LOG_ERR
            LOG_WARNING
            LOG_NOTICE
            LOG_INFO
            LOG_DEBUG
    ) ],
    'notif' => [ qw(
            NOTIF_FAILURE
            NOTIF_WARNING
            NOTIF_OKAY
    ) ],
    'globals' => [ qw(
            $interval_g
    ) ],
);

{
    my %seen;
    push @{$EXPORT_TAGS{'all'}}, grep {! $seen{$_}++ } @{$EXPORT_TAGS{$_}}
        foreach keys %EXPORT_TAGS;
}

# global variables
our $interval_g;

Exporter::export_ok_tags ('all');

my @plugins : shared = ();
my %cf_callbacks : shared = ();

my %types = (
    TYPE_CONFIG,   "config",
    TYPE_INIT,     "init",
    TYPE_READ,     "read",
    TYPE_WRITE,    "write",
    TYPE_SHUTDOWN, "shutdown",
    TYPE_LOG,      "log",
    TYPE_NOTIF,    "notify",
    TYPE_FLUSH,    "flush"
);

foreach my $type (keys %types) {
    $plugins[$type] = &share ({});
}

sub _log {
    my $caller = shift;
    my $lvl    = shift;
    my $msg    = shift;

    if ("NCollectd" eq $caller) {
        $msg = "perl: $msg";
    }
    return plugin_log ($lvl, $msg);
}

sub ERROR   { _log (scalar caller, LOG_ERR,     shift); }
sub WARNING { _log (scalar caller, LOG_WARNING, shift); }
sub NOTICE  { _log (scalar caller, LOG_NOTICE,  shift); }
sub INFO    { _log (scalar caller, LOG_INFO,    shift); }
sub DEBUG   { _log (scalar caller, LOG_DEBUG,   shift); }

sub plugin_call_all
{
    my $type = shift;

    my %plugins;

    our $cb_name = undef;

    if (! defined $type) {
        return;
    }

    if ($type != TYPE_LOG) {
        DEBUG ("NCollectd::plugin_call_all: type = \"$type\" ("
            . $types{$type} . "), args=\""
            . join(', ', map { defined($_) ? $_ : '<undef>' } @_) . "\"");
    }

    if (! defined $plugins[$type]) {
        ERROR ("NCollectd::plugin_call_all: unknown type \"$type\"");
        return;
    }

    {
        lock %{$plugins[$type]};
        %plugins = %{$plugins[$type]};
    }

    foreach my $plugin (keys %plugins) {
        $cb_name = $plugins{$plugin};
        my $status = call_by_name (@_);

        if (! $status) {
            my $err = undef;

            if ($@) {
                $err = $@;
            } else {
                $err = "callback returned false";
            }

            if ($type != TYPE_LOG) {
                ERROR ("Execution of callback \"$cb_name\" failed: $err");
            }

            $status = 0;
        }

        if ($status) {
            #NOOP
        } elsif ($type == TYPE_INIT) {
            ERROR ("${plugin}->init() failed with status $status Plugin will be disabled.");

            foreach my $type (keys %types) {
                plugin_unregister ($type, $plugin);
            }
        } elsif ($type != TYPE_LOG) {
            WARNING ("${plugin}->$types{$type}() failed with status $status.");
        }
    }
    return 1;
}

# NCollectd::plugin_register (type, name, data).
#
# type:
#   init, read, write, shutdown
#
# name:
#   name of the plugin
#
# data:
#   reference to the plugin's subroutine that does the work or the data set
#   definition
sub plugin_register
{
    my $type = shift;
    my $name = shift;
    my $data = shift;

    DEBUG ("NCollectd::plugin_register: "
        . "type = \"$type\" (" . $types{$type}
        . "), name = \"$name\", data = \"$data\"");

    if (! ((defined $type) && (defined $name) && (defined $data))) {
        ERROR ("Usage: NCollectd::plugin_register (type, name, data)");
        return;
    }

    if ((! defined $plugins[$type]) && ($type != TYPE_CONFIG)) {
        ERROR ("NCollectd::plugin_register: Invalid type \"$type\"");
        return;
    }

    if (($type == TYPE_CONFIG) && (! ref $data)) {
        my $pkg = scalar caller;

        if ($data !~ m/^$pkg\:\:/) {
            $data = $pkg . "::" . $data;
        }

        lock %cf_callbacks;
        $cf_callbacks{$name} = $data;
    } elsif (! ref $data) {
        my $pkg = scalar caller;
        if ($data !~ m/^$pkg\:\:/) {
            $data = $pkg . "::" . $data;
        }
        if ($type == TYPE_READ) {
            return plugin_register_read($name, $data);
        }
        if ($type == TYPE_WRITE) {
            return plugin_register_write($name, $data);
        }
        if ($type == TYPE_LOG) {
            return plugin_register_log($name, $data);
        }
        if ($type == TYPE_NOTIF) {
            return plugin_register_notification($name, $data);
        }
        if ($type == TYPE_FLUSH) {
            #For collectd-5.6 only
            lock %{$plugins[$type]};
            $plugins[$type]->{$name} = $data;
            return plugin_register_flush($name, $data);
        }
        lock %{$plugins[$type]};
        $plugins[$type]->{$name} = $data;
    } else {
        ERROR ("NCollectd::plugin_register: Invalid data.");
        return;
    }
    return 1;
}

sub plugin_unregister
{
    my $type = shift;
    my $name = shift;

    DEBUG ("NCollectd::plugin_unregister: type = \"$type\" ("
        . $types{$type} . "), name = \"$name\"");

    if (! ((defined $type) && (defined $name))) {
        ERROR ("Usage: NCollectd::plugin_unregister (type, name)");
        return;
    }

    if ($type == TYPE_CONFIG) {
        lock %cf_callbacks;
        delete $cf_callbacks{$name};
    } elsif ($type == TYPE_READ) {
        return plugin_unregister_read ($name);
    } elsif ($type == TYPE_WRITE) {
        return plugin_unregister_write($name);
    } elsif ($type == TYPE_LOG) {
        return plugin_unregister_log ($name);
    } elsif ($type == TYPE_NOTIF) {
        return plugin_unregister_notification($name);
    } elsif ($type == TYPE_FLUSH) {
        return plugin_unregister_flush($name);
    } elsif (defined $plugins[$type]) {
        lock %{$plugins[$type]};
        delete $plugins[$type]->{$name};
    } else {
        ERROR ("NCollectd::plugin_unregister: Invalid type.");
        return;
    }
}

sub plugin_write
{
    my %args = @_;

    my @plugins    = ();
    my @datasets   = ();
    my @valuelists = ();

    if (! defined $args{'valuelists'}) {
        ERROR ("NCollectd::plugin_write: Missing 'valuelists' argument.");
        return;
    }

    DEBUG ("NCollectd::plugin_write:"
        . (defined ($args{'plugins'}) ? " plugins = $args{'plugins'}" : "")
        . (defined ($args{'datasets'}) ? " datasets = $args{'datasets'}" : "")
        . " valueslists = $args{'valuelists'}");

    if (defined ($args{'plugins'})) {
        if ("ARRAY" eq ref ($args{'plugins'})) {
            @plugins = @{$args{'plugins'}};
        } else {
            @plugins = ($args{'plugins'});
        }
    } else {
        @plugins = (undef);
    }

    if ("ARRAY" eq ref ($args{'valuelists'})) {
        @valuelists = @{$args{'valuelists'}};
    } else {
        @valuelists = ($args{'valuelists'});
    }

    if (defined ($args{'datasets'})) {
        if ("ARRAY" eq ref ($args{'datasets'})) {
            @datasets = @{$args{'datasets'}};
        } else {
            @datasets = ($args{'datasets'});
        }
    } else {
        @datasets = (undef) x scalar (@valuelists);
    }

    if ($#datasets != $#valuelists) {
        ERROR ("NCollectd::plugin_write: Invalid number of datasets.");
        return;
    }

    foreach my $plugin (@plugins) {
        for (my $i = 0; $i < scalar (@valuelists); ++$i) {
            _plugin_write ($plugin, $datasets[$i], $valuelists[$i]);
        }
    }
}

sub plugin_flush
{
    my %args = @_;

    my $timeout = -1;
    my @plugins = ();
    my @ids     = ();

    DEBUG ("NCollectd::plugin_flush:"
        . (defined ($args{'timeout'}) ? " timeout = $args{'timeout'}" : "")
        . (defined ($args{'plugins'}) ? " plugins = $args{'plugins'}" : "")
        . (defined ($args{'identifiers'})
            ? " identifiers = $args{'identifiers'}" : ""));

    if (defined ($args{'timeout'}) && ($args{'timeout'} > 0)) {
        $timeout = $args{'timeout'};
    }

    if (defined ($args{'plugins'})) {
        if ("ARRAY" eq ref ($args{'plugins'})) {
            @plugins = @{$args{'plugins'}};
        } else {
            @plugins = ($args{'plugins'});
        }
    } else {
        @plugins = (undef);
    }

    if (defined ($args{'identifiers'})) {
        if ("ARRAY" eq ref ($args{'identifiers'})) {
            @ids = @{$args{'identifiers'}};
        } else {
            @ids = ($args{'identifiers'});
        }
    } else {
        @ids = (undef);
    }

    foreach my $plugin (@plugins) {
        foreach my $id (@ids) {
            _plugin_flush($plugin, $timeout, $id);
        }
    }
}

sub _plugin_dispatch_config
{
    my $plugin = shift;
    my $config = shift;

    our $cb_name = undef;

    if (! (defined ($plugin) && defined ($config))) {
        return;
    }

    if (! defined $cf_callbacks{$plugin}) {
        WARNING ("Found a configuration for the \"$plugin\" plugin, but "
            . "the plugin isn't loaded or didn't register "
            . "a configuration callback.");
        return;
    }

    {
        lock %cf_callbacks;
        $cb_name = $cf_callbacks{$plugin};
    }
    call_by_name ($config);
}

1;
