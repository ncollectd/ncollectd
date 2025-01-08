%global _hardened_build 1
%global __requires_exclude ^.*(libdb2|libmqic_r|libmqm_r|libclntsh)[.]so.*$

%if 0%{?git_tag:1}
%define _version %{git_tag}
%else
%define _version 0.0.0.git
%endif

%define build_with_arp 0%{!?_without_arp:1}
%define build_with_arpcache 0%{!?_without_arpcache:1}
%define build_with_ats 0%{!?_without_ats:1}
%define build_with_apache 0%{!?_without_apache:1}
%define build_with_apcups 0%{!?_without_apcups:1}
%define build_with_avccache 0%{!?_without_avccache:1}
%define build_with_battery 0%{!?_without_battery:1}
%define build_with_beanstalkd 0%{!?_without_beanstalkd:1}
%define build_with_bcache 0%{!?_without_bcache:1}
%define build_with_bind 0%{!?_without_bind:1}
%define build_with_bonding 0%{!?_without_bonding:1}
%define build_with_btrfs 0%{!?_without_btrfs:1}
%define build_with_buddyinfo 0%{!?_without_buddyinfo:1}
%define build_with_ceph 0%{!?_without_ceph:1}
%define build_with_cert 0%{!?_without_cert:1}
%define build_with_cgroups 0%{!?_without_cgroups:1}
%define build_with_chrony 0%{!?_without_chrony:1}
%define build_with_cifs 0%{!?_without_cifs:1}
%define build_with_conntrack 0%{!?_without_conntrack:1}
%define build_with_contextswitch 0%{!?_without_contextswitch:1}
%define build_with_cpu 0%{!?_without_cpu:1}
%define build_with_cpufreq 0%{!?_without_cpufreq:1}
%define build_with_cpusleep 0%{!?_without_cpusleep:1}
%define build_with_cups 0%{!?_without_cups:1}
%define build_with_dcpmm 0%{!?_without_dcpmm:1}
%define build_with_df 0%{!?_without_df:1}
%define build_with_disk 0%{!?_without_disk:1}
%define build_with_dns 0%{!?_without_dns:1}
%define build_with_dnsmasq 0%{!?_without_dnsmasq:1}
%define build_with_dmi 0%{!?_without_dmi:1}
%define build_with_docker 0%{!?_without_docker:1}
%define build_with_ds389 0%{!?_without_ds389:1}
%define build_with_drbd 0%{!?_without_drbd:1}
%define build_with_edac 0%{!?_without_edac:1}
%define build_with_entropy 0%{!?_without_entropy:1}
%define build_with_ethstat 0%{!?_without_ethstat:1}
%define build_with_exec 0%{!?_without_exec:1}
%define build_with_fcgi 0%{!?_without_fcgi:1}
%define build_with_fchost 0%{!?_without_fchost:1}
%define build_with_freeradius 0%{!?_without_freeradius:1}
%define build_with_fhcount 0%{!?_without_fhcount:1}
%define build_with_filecount 0%{!?_without_filecount:1}
%define build_with_fscache 0%{!?_without_fscache:1}
%define build_with_gps 0%{!?_without_gps:1}
%define build_with_gpu_intel 0%{!?_without_gpu_intel:1}
%define build_with_haproxy 0%{!?_without_haproxy:1}
%define build_with_http 0%{!?_without_http:1}
%define build_with_hugepages 0%{!?_without_hugepages:1}
%define build_with_infiniband 0%{!?_without_infiniband:1}
%define build_with_info 0%{!?_without_info:1}
%define build_with_interface 0%{!?_without_interface:1}
%define build_with_ipc 0%{!?_without_ipc:1}
%define build_with_ipmi 0%{!?_without_ipmi:1}
%define build_with_iptables 0%{!?_without_iptables:1}
%define build_with_ipvs 0%{!?_without_ipvs:1}
%define build_with_irq 0%{!?_without_irq:1}
%define build_with_java 0%{!?_without_java:1}
%define build_with_jolokia 0%{!?_without_jolokia:1}
%define build_with_journal 0%{!?_without_journal:1}
%define build_with_kafka 0%{!?_without_kafka:1}
%define build_with_kea 0%{!?_without_kea:1}
%define build_with_keepalived 0%{!?_without_keepalived:1}
%define build_with_ksm 0%{!?_without_ksm:1}
%define build_with_ldap 0%{!?_without_ldap:1}
%define build_with_load 0%{!?_without_load:1}
%define build_with_locks 0%{!?_without_locks:1}
%define build_with_logind 0%{!?_without_logind:1}
%define build_with_log_file 0%{!?_without_log_file:1}
%define build_with_log_gelf 0%{!?_without_log_gelf:1}
%define build_with_log_syslog 0%{!?_without_log_syslog:1}
%define build_with_log_systemd 0%{!?_without_log_systemd:1}
%define build_with_lua 0%{!?_without_lua:1}
%define build_with_lvm 0%{!?_without_lvm:1}
%define build_with_match_csv 0%{!?_without_match_csv:1}
%define build_with_match_jsonpath 0%{!?_without_match_jsonpath:1}
%define build_with_match_regex 0%{!?_without_match_regex:1}
%define build_with_match_table 0%{!?_without_match_table:1}
%define build_with_match_xpath 0%{!?_without_match_xpath:1}
%define build_with_md 0%{!?_without_md:1}
%define build_with_memcached 0%{!?_without_memcached:1}
%define build_with_meminfo 0%{!?_without_meminfo:1}
%define build_with_memory 0%{!?_without_memory:1}
%define build_with_mmc 0%{!?_without_mmc:1}
%define build_with_modbus 0%{!?_without_modbus:1}
%define build_with_mongodb 0%{!?_without_mongodb:1}
%define build_with_mosquitto 0%{!?_without_mosquitto:1}
%define build_with_mssql 0%{!?_without_mssql:1}
%define build_with_mysql 0%{!?_without_mysql:1}
%define build_with_nagios_check 0%{!?_without_nagios_check:1}
%define build_with_nfacct 0%{!?_without_nfacct:1}
%define build_with_nfconntrack 0%{!?_without_nfconntrack:1}
%define build_with_nfs 0%{!?_without_nfs:1}
%define build_with_nfsd 0%{!?_without_nfsd:1}
%define build_with_nftables 0%{!?_without_nftables:1}
%define build_with_nginx 0%{!?_without_nginx:1}
%define build_with_nginx_vts 0%{!?_without_nginx_vts:1}
%define build_with_notify_email 0%{!?_without_notify_email:1}
%define build_with_notify_exec 0%{!?_without_notify_exec:1}
%define build_with_notify_nagios 0%{!?_without_notify_nagios:1}
%define build_with_notify_snmp 0%{!?_without_notify_snmp:1}
%define build_with_nsd 0%{!?_without_nsd:1}
%define build_with_ntpd 0%{!?_without_ntpd:1}
%define build_with_numa 0%{!?_without_numa:1}
%define build_with_nut 0%{!?_without_nut:1}
%define build_with_odbc 0%{!?_without_odbc:1}
%define build_with_olsrd 0%{!?_without_olsrd:1}
%define build_with_openldap 0%{!?_without_openldap:1}
%define build_with_openvpn 0%{!?_without_openvpn:1}
%define build_with_pcap 0%{!?_without_pcap:1}
%define build_with_perl 0%{!?_without_perl:1}
%define build_with_pgbouncer 0%{!?_without_pgbouncer:1}
%define build_with_ping 0%{!?_without_ping:1}
%define build_with_podman 0%{!?_without_podman:1}
%define build_with_postfix 0%{!?_without_postfix:1}
%define build_with_postgresql 0%{!?_without_postgresql:1}
%define build_with_pdns 0%{!?_without_pdns:1}
%define build_with_pressure 0%{!?_without_pressure:1}
%define build_with_processes 0%{!?_without_processes:1}
%define build_with_protocols 0%{!?_without_protocols:1}
%define build_with_proxysql 0%{!?_without_proxysql:1}
%define build_with_python 0%{!?_without_python:1}
%define build_with_quota 0%{!?_without_quota:1}
%define build_with_rapl 0%{!?_without_rapl:1}
%define build_with_recursor 0%{!?_without_recursor:1}
%define build_with_redis 0%{!?_without_redis:1}
%define build_with_resctrl 0%{!?_without_resctrl:1}
%define build_with_routeros 0%{!?_without_routeros:1}
%define build_with_rtcache 0%{!?_without_rtcache:1}
%define build_with_schedstat 0%{!?_without_schedstat:1}
%define build_with_scraper 0%{!?_without_scraper:1}
%define build_with_sendmail 0%{!?_without_sendmail:1}
%define build_with_sensors 0%{!?_without_sensors:1}
%define build_with_serial 0%{!?_without_serial:1}
%define build_with_sigrok 0%{!?_without_sigrok:1}
%define build_with_slabinfo 0%{!?_without_slabinfo:1}
%define build_with_slurm 0%{!?_without_slurm:1}
%define build_with_smart 0%{!?_without_smart:1}
%define build_with_snmp 0%{!?_without_snmp:1}
%define build_with_sockstat 0%{!?_without_sockstat:1}
%define build_with_softirq 0%{!?_without_softirq:1}
%define build_with_softnet 0%{!?_without_softnet:1}
%define build_with_squid 0%{!?_without_squid:1}
%define build_with_statsd 0%{!?_without_statsd:1}
%define build_with_swap 0%{!?_without_swap:1}
%define build_with_synproxy 0%{!?_without_synproxy:1}
%define build_with_systemd 0%{!?_without_systemd:1}
%define build_with_table 0%{!?_without_table:1}
%define build_with_tail 0%{!?_without_tail:1}
%define build_with_tape 0%{!?_without_tape:1}
%define build_with_tc 0%{!?_without_tc:1}
%define build_with_tcpconns 0%{!?_without_tcpconns:1}
%define build_with_timex 0%{!?_without_timex:1}
%define build_with_thermal 0%{!?_without_thermal:1}
%define build_with_turbostat 0%{!?_without_turbostat:1}
%define build_with_ubi 0%{!?_without_ubi:1}
%define build_with_uname 0%{!?_without_uname:1}
%define build_with_unbound 0%{!?_without_unbound:1}
%define build_with_uptime 0%{!?_without_uptime:1}
%define build_with_users 0%{!?_without_users:1}
%define build_with_uuid 0%{!?_without_uuid:1}
%define build_with_varnish 0%{!?_without_varnish:1}
%define build_with_virt 0%{!?_without_virt:1}
%define build_with_vmem 0%{!?_without_vmem:1}
%define build_with_wireguard 0%{!?_without_wireguard:1}
%define build_with_wireless 0%{!?_without_wireless:1}
%define build_with_write_amqp 0%{!?_without_write_amqp:1}
%define build_with_write_amqp1 0%{!?_without_write_amqp1:1}
%define build_with_write_exporter 0%{!?_without_write_exporter:1}
%define build_with_write_file 0%{!?_without_write_file:1}
%define build_with_write_http 0%{!?_without_write_http:1}
%define build_with_write_kafka 0%{!?_without_write_kafka:1}
%define build_with_write_log 0%{!?_without_write_log:1}
%define build_with_write_mongodb 0%{!?_without_write_mongodb:1}
%define build_with_write_mqtt 0%{!?_without_write_mqtt:1}
%define build_with_write_postgresql 0%{!?_without_write_postgresql:1}
%define build_with_write_redis 0%{!?_without_write_redis:1}
%define build_with_write_tcp 0%{!?_without_write_tcp:1}
%define build_with_write_udp 0%{!?_without_write_udp:1}
%define build_with_xencpu 0%{!?_without_xencpu:1}
%define build_with_xfrm 0%{!?_without_xfrm:1}
%define build_with_xfs 0%{!?_without_xfs:1}
%define build_with_zfs 0%{!?_without_zfs:1}
%define build_with_zoneinfo 0%{!?_without_zoneinfo:1}
%define build_with_zookeeper 0%{!?_without_zookeeper:1}
%define build_with_zram 0%{!?_without_zram:1}
%define build_with_zswap 0%{!?_without_zswap:1}

# plugin lpar disabled, requires AIX
%define build_with_lpar 0%{?_with_lpar:1}
# plugin intel_rdt disabled, requires intel-cmt-cat
%define build_with_intel_rdt 0%{?_with_intel_rdt:1}
# plugin pf disabled, requires BSD
%define build_with_pf 0%{?_with_pf:1}
# plugin ipstats disabled, requires BSD
%define build_with_ipstats 0%{?_with_ipstats:1}
# plugin build_with_netstat_udp disabled, requires BSD
%define build_with_netstat_udp 0%{?_with_netstat_udp:1}
# plugin build_with_zone disabled, requires Solaris
%define build_with_zone 0%{?_with_zone:1}
# plugin gpu_nvidia requires cuda-nvml-dev
# get it from https://developer.nvidia.com/cuda-downloads
# then install cuda-nvml-dev-10-1 or other version
%define build_with_gpu_nvidia 0%{?_with_gpu_nvidia:1}

%define build_with_oracle 0%{?_with_oracle:1}
%define build_with_db2 0%{?_with_db2:1}
%define build_with_mq 0%{?_with_mq:1}

# Plugins not buildable on OpenSUSE
%define build_with_iptables 0%{?_with_iptables:1}
%define build_with_dcpmm 0%{?_with_dcpmm:1}
%define build_with_mongodb 0%{?_with_mongodb:1}
%define build_with_write_mongodb 0%{?_with_write_mongodb:1}
%define build_with_gpu_intel 0%{?_with_gpu_intel:1}
%define build_with_perl 0%{?_with_perl:1}
%define build_with_python 0%{?_with_perl:1}
%define build_with_nftables 0%{?_with_nftables:1}

Summary: Statistics collection and monitoring daemon
Name: ncollectd
Version: %{_version}
Release: 1
URL: https://ncollectd.org
Source: https://ncollectd.org/files/%{name}-%{version}.tar.xz
License: GPLv2
Group: System Environment/Daemons
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildRequires: kernel-headers
BuildRequires: gcc, binutils, cpp, glibc-devel, bison, flex, gperf
BuildRequires: cmake
Vendor: ncollectd development team

Source1: systemd.ncollectd.service

%{?systemd_requires}
BuildRequires: systemd

# BuildRequires: libcap
BuildRequires: xfsprogs-devel

%description
ncollectd is a small daemon which collects system information periodically and
provides mechanisms to monitor and store the values in a variety of ways.
Since the daemon doesn't need to start up every time it wants to update the
values it's very fast and easy on the system. Also, the statistics are very fine
grained since the files are updated every 10 seconds by default.


%if %{build_with_apache}
%package apache
Summary: Apache plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description apache
The apache plugin collects data provided by Apache's mod_status.
%endif

%if %{build_with_ats}
%package ats
Summary: ATS plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description ats
The ATS plugin collects data provided by Apache Traffic Server.
%endif

%if %{build_with_bind}
%package bind
Summary: Bind plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libxml2-devel, curl-devel
%description bind
The bind plugin retrieves this information that's encoded in XML and provided
via HTTP and submits the values to ncollectd.
%endif

%if %{build_with_cert}
%package cert
Summary: Cert plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: openssl-devel
%description cert
The cert plugin retrieves information of expiration date of certificates.
%endif

%if %{build_with_cups}
%package cups
Summary: Cups plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: cups-devel
%description cups
The cups plugin collect printer and jobs metrics from a cups server.
%endif

%if %{build_with_db2}
%package db2
Summary: DB2 plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
%description db2
The db2 plugin retrives information from a DB2 database,
%endif

%if %{build_with_dcpmm}
%package dcpmm
Summary: dcpmm plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libipmctl-devel
%description dcpmm
The dcpmm plugin will collect Intel(R) Optane(TM) DC Persistent Memory (DCPMM)
related performance and health statistics.
%endif

%if %{build_with_dns}
%package dns
Summary: dns plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: c-ares-devel
%description dns
This dns plugin retrives information from dns server responses.
%endif

%if %{build_with_docker}
%package docker
Summary: Docker plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description docker
The docker plugin collect metrics from the contariners running with docker.
%endif

%if %{build_with_ds389}
%package ds389
Summary: Ds389 plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: openldap2-devel
%description ds389
This plugin reads monitoring information from DS389 server.
%endif

%if %{build_with_gps}
%package gps
Summary: GPS plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: gpsd-devel
%description gps
This plugin monitor gps related data through gpsd.
%endif

%if %{build_with_gpu_intel}
%package gpu_intel
Summary: GPU_intel plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: oneapi-level-zero
%description gpu_intel
The gpu_intel ncollectd plugin collects Intel GPU metrics.
%endif

%if %{build_with_gpu_nvidia}
%package gpu_nvidia
Summary: GPU_nvidia plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: cuda-nvml-dev-10-1
%description gpu_nvidia
The gpu_nvidia ncollectd plugin collects NVidia GPU metrics.
%endif

%if %{build_with_haproxy}
%package haproxy
Summary: HAProxy plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description haproxy
The haproxy plugin collects metrics from a haproxy server.
%endif

%if %{build_with_http}
%package http
Summary: Http plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description http
The http plugin queries a http server and parser the data using the match plugins.
%endif

%if %{build_with_ipmi}
%package ipmi
Summary: IPMI plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: OpenIPMI-devel
%description ipmi
The IPMI plugin uses the OpenIPMI library to read hardware sensors from servers
using the Intelligent Platform Management Interface (IPMI).
%endif

%if %{build_with_intel_rdt}
%package intel_rdt
Summary: Intel RDT plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: intel-cmt-cat
%description intel_rdt
The intel_rdt plugin collects information provided by monitoring features of
Intel Resource Director Technology (Intel(R) RDT).
%endif

%if %{build_with_iptables}
%package iptables
Summary: IPtables plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: iptables-devel
%description iptables
The IPtables plugin can gather statistics from your ip_tables based packet
filter (aka. firewall) for both the IPv4 and the IPv6 protocol. It can collect
the byte- and packet-counters of selected rules and submit them to ncollectd.
%endif

%if %{build_with_java}
%package java
Summary: Java plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: java-devel >= 1.6, jpackage-utils >= 1.6
Requires: java >= 1.6, jpackage-utils >= 1.6
%description java
This plugin for ncollectd allows plugins to be written in Java and executed
in an embedded JVM.
%endif

%if %{build_with_jolokia}
%package jolokia
Summary: Jolokia plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description jolokia
The jolokia plugin collects values from MBeanServevr - servlet engines equipped
with the jolokia MBean.
%endif

%if %{build_with_kafka}
%package kafka
Summary: Kafka plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: librdkafka-devel
%description kafka
The kafka plugin collect metrics fron a kafka broker.
%endif

%if %{build_with_lua}
%package lua
Summary: Lua plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: lua-devel
%description lua
The Lua plugin embeds a Lua interpreter into ncollectd and exposes the
application programming interface (API) to Lua scripts.
%endif

%if %{build_with_match_xpath}
%package match_xpath
Summary: Curl_xml plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libxml2-devel
%description match_xpath
The match_xpath plugin parser data as Extensible Markup Language (XML) using
xpath expressions.
%endif

%if %{build_with_modbus}
%package modbus
Summary: modbus plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libmodbus-devel
%description modbus
The modbus plugin collects values from Modbus/TCP enabled devices
%endif

%if %{build_with_mongodb}
%package mongodb
Summary: Mongodb plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: mongo-c-driver-devel
%description mongodb
The mongodb pluginb provides metrics from a mongodb database.
%endif

%if %{build_with_mosquitto}
%package mosquitto
Summary: mosquitto plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: mosquitto-devel
%description mosquitto
The mosquitto plugin ncollectd metrics from a mosquitto broker.
%endif

%if %{build_with_mq}
%package mq
Summary: MQ plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
%description mq
The mq plugin get metrics from a MQ broker.
%endif

%if %{build_with_mssql}
%package mssql
Summary: MS SQL plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: freetds-devel
%description mssql
MS SQL querying plugin.
%endif

%if %{build_with_mysql}
%package mysql
Summary: MySQL plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libmariadb-devel
%description mysql
MySQL querying plugin. This plugin provides data of issued commands, called
handlers and database traffic.
%endif

%if %{build_with_nftables}
%package nftables
Summary: NFTables plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libnftnl-devel
%description nftables
The nftables plugin get counters fron nftables.
%endif

%if %{build_with_nginx}
%package nginx
Summary: Nginx plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description nginx
This plugin gets metrics provided by nginx.
%endif

%if %{build_with_nginx_vts}
%package nginx_vts
Summary: Nginx plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description nginx_vts
This plugin gets metrics provided by the Nginx virtual host traffic status module.
%endif

%if %{build_with_notify_email}
%package notify_email
Summary: Notify email plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libesmtp-devel
%description notify_email
The notify Email plugin uses libESMTP to send notifications to a configured
email address.
%endif

%if %{build_with_notify_snmp}
%package notify_snmp
Summary: Notify snmp plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: net-snmp-devel
%description notify_snmp
The notify smp plugin send notification as SNMP traps.
%endif

%if %{build_with_nsd}
%package nsd
Summary: Nsd plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: openssl-devel
%description nsd
This plugin collect metrics from the Name Server Daemon (NSD).
%endif

%if %{build_with_nut}
%package nut
Summary: Nut plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: nut-devel
%description nut
This plugin for ncollectd provides Network UPS Tools support.
%endif

%if %{build_with_ldap}
%package ldap
Summary: Ldap plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: openldap2-devel
%description ldap
The ldap plugin execute queries on a ldap server and read back the result.
%endif

%if %{build_with_odbc}
%package odbc
Summary: ODBC plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: unixODBC-devel
%description odbc
The odbc plugin uses ODBC, a database abstraction library, to execute SQL
statements on a database and read back the result.
%endif

%if %{build_with_openldap}
%package openldap
Summary: Openldap plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: openldap2-devel
%description openldap
This plugin reads monitoring information from OpenLDAP's cn=Monitor subtree.
%endif

%if %{build_with_oracle}
%package oracle
Summary: Oracle plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
%description oracle
The oracle plugin get information from a Oracle database.
%endif

%if %{build_with_pcap}
%package pcap
Summary: pcap plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libpcap-devel >= 1.0
%description pcap
The pcap plugin uses libpcap to get a copy of all traffic and generates statistics
of some protocols such as DNS.
%endif

%if %{build_with_perl}
%package perl
Summary: Perl plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
Requires: perl(:MODULE_COMPAT_%(eval "`%{__perl} -V:version`"; echo $version))
BuildRequires: perl-ExtUtils-Embed
%description perl
The Perl plugin embeds a Perl interpreter into ncollectd and exposes the
application programming interface (API) to Perl-scripts.
%endif

%if %{build_with_pgbouncer}
%package pgbouncer
Summary: PgBouncer plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: postgresql-devel
%description pgbouncer
The pgbouncer plugin collects metrics from a PgBouncer instance.
%endif

%if %{build_with_podman}
%package podman
Summary: Podman plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description podman
The podman plugin collect metrics from the contariners running with podman.
%endif

%if %{build_with_postgresql}
%package postgresql
Summary: PostgreSQL plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: postgresql-devel
%description postgresql
The PostgreSQL plugin connects to and executes SQL statements on a PostgreSQL database.
%endif

%if %{build_with_proxysql}
%package proxysql
Summary: ProxySQL plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libmariadb-devel
%description proxysql
The ProxySQL plugin collect metrics from the MySQL proxy: ProxySQL.
%endif

%if %{build_with_python}
%package python
Summary: Python plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: python3-devel
%description python
The Python plugin embeds a Python interpreter into collectd and exposes the
application programming interface (API) to Python-scripts.
%endif

%if %{build_with_redis}
%package redis
Summary: Redis plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: hiredis-devel
%description redis
The Redis plugin connects to one or more instances of Redis, a key-value store,
and collects usage information using the hiredis library.
%endif

%if %{build_with_scraper}
%package scraper
Summary: Scraper plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description scraper
The scraper plugin queries a http endpoint to get metrics in openmetrics format.
%endif

%if %{build_with_sensors}
%package sensors
Summary: Sensors plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libsensors4-devel
%description sensors
This plugin for ncollectd provides querying of sensors supported by lm_sensors.
%endif

%if %{build_with_sigrok}
%package sigrok
Summary: sigrok plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
%description sigrok
Uses libsigrok as a backend, allowing any sigrok-supported device to have its
measurements fed to ncollectd. This includes multimeters, sound level meters,
thermometers, and much more.
%endif

%if %{build_with_slurm}
%package slurm
Summary: Slurm plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: slurm-devel
%description slurm
The slurm plugin collects per-partition SLURM node and job state information, as
well as internal health statistics.
%endif

%if %{build_with_smart}
%package smart
Summary: SMART plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libatasmart-devel
%description smart
Collect SMART statistics, notably load cycle count, temperature and bad
sectors.
%endif

%if %{build_with_snmp}
%package snmp
Summary: SNMP plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: net-snmp-devel
%description snmp
This plugin for ncollectd allows querying of network equipment using SNMP.
%endif

%if %{build_with_squid}
%package squid
Summary: Squid plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description squid
The squid plugin collects data provided by squid proxy.
%endif

%if %{build_with_unbound}
%package unbound
Summary: Unbound plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: openssl-devel
%description unbound
This plugin collect metrics from the Unbound dns resolver.
%endif

%if %{build_with_varnish}
%package varnish
Summary: Varnish plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: varnish-devel
%description varnish
The Varnish plugin collects information about Varnish, an HTTP accelerator.
%endif

%if %{build_with_virt}
%package virt
Summary: Virt plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libvirt-devel
%description virt
This plugin collects information from virtualized guests.
%endif

%if %{build_with_write_amqp}
%package write_amqp
Summary: Write AMQP 0.9 plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: librabbitmq-devel
%description write_amqp
The AMQP 0.9 plugin transmits values collected by ncollectd via the
Advanced Message Queuing Protocol v0.9 (AMQP).
%endif

%if %{build_with_write_amqp1}
%package write_amqp1
Summary: Write AMQP 1.0 plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: qpid-proton-devel
%description write_amqp1
The AMQP 1.0 plugin transmits values collected by ncollectd via the
Advanced Message Queuing Protocol v1.0 (AMQP1).
%endif

%if %{build_with_write_exporter}
%package write_exporter
Summary: Write exporter plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: libmicrohttpd-devel
%description write_exporter
The write_exporter plugin exposes ncollected values using an embedded HTTP
server, turning the ncollectd daemon into a openmetrics exporter.
%endif

%if %{build_with_write_http}
%package write_http
Summary: Write http plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: curl-devel
%description write_http
The write_http plugin sends the values collected by ncollectd to a web-server
using HTTP POST requests.
%endif

%if %{build_with_write_kafka}
%package write_kafka
Summary: Write kafka plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: librdkafka-devel
%description write_kafka
The write_kafka plugin sends values to kafka, a distributed messaging system.
%endif

%if %{build_with_write_mongodb}
%package write_mongodb
Summary: Write mongodb plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: mongo-c-driver-devel
%description write_mongodb
The write_mongodb plugin write values to a mongodb database.
%endif

%if %{build_with_write_mqtt}
%package write_mqtt
Summary: Write mqtt plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: mosquitto-devel
%description write_mqtt
The MQTT plugin publishes and subscribes to MQTT topics.
%endif

%if %{build_with_write_postgresql}
%package write_postgresql
Summary: Write PostgreSQL plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: postgresql-devel
%description write_postgresql
The write_postgresql plugin write values to a postgresql database.
database.
%endif

%if %{build_with_write_redis}
%package write_redis
Summary: Redis plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: hiredis-devel
%description write_redis
The Redis plugin write values to a redis database with the RedisTimeSeries module.
%endif

%if %{build_with_xencpu}
%package xencpu
Summary: Xencpu plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: xen-devel
%description xencpu
The xencpu plugin collects CPU statistics from Xen.
%endif

%prep
%setup -q

%build

%if %{build_with_arp}
%define _build_with_arp -DPLUGIN_ARP:BOOL=ON
%else
%define _build_with_arp -DPLUGIN_ARP:BOOL=OFF
%endif

%if %{build_with_arpcache}
%define _build_with_arpcache -DPLUGIN_ARPCACHE:BOOL=ON
%else
%define _build_with_arpcache -DPLUGIN_ARPCACHE:BOOL=OFF
%endif

%if %{build_with_ats}
%define _build_with_ats -DPLUGIN_ATS:BOOL=ON
%else
%define _build_with_ats -DPLUGIN_ATS:BOOL=OFF
%endif

%if %{build_with_apache}
%define _build_with_apache -DPLUGIN_APACHE:BOOL=ON
%else
%define _build_with_apache -DPLUGIN_APACHE:BOOL=OFF
%endif

%if %{build_with_apcups}
%define _build_with_apcups -DPLUGIN_APCUPS:BOOL=ON
%else
%define _build_with_apcups -DPLUGIN_APCUPS:BOOL=OFF
%endif

%if %{build_with_avccache}
%define _build_with_avccache -DPLUGIN_AVCCACHE:BOOL=ON
%else
%define _build_with_avccache -DPLUGIN_AVCCACHE:BOOL=OFF
%endif

%if %{build_with_battery}
%define _build_with_battery -DPLUGIN_BATTERY:BOOL=ON
%else
%define _build_with_battery -DPLUGIN_BATTERY:BOOL=OFF
%endif

%if %{build_with_bcache}
%define _build_with_bcache -DPLUGIN_BCACHE:BOOL=ON
%else
%define _build_with_bcache -DPLUGIN_BCACHE:BOOL=OFF
%endif

%if %{build_with_beanstalkd}
%define _build_with_beanstalkd -DPLUGIN_BEANSTALKD:BOOL=ON
%else
%define _build_with_beanstalkd -DPLUGIN_BEANSTALKD:BOOL=OFF
%endif

%if %{build_with_bind}
%define _build_with_bind -DPLUGIN_BIND:BOOL=ON
%else
%define _build_with_bind -DPLUGIN_BIND:BOOL=OFF
%endif

%if %{build_with_bonding}
%define _build_with_bonding -DPLUGIN_BONDING:BOOL=ON
%else
%define _build_with_bonding -DPLUGIN_BONDING:BOOL=OFF
%endif

%if %{build_with_btrfs}
%define _build_with_btrfs -DPLUGIN_BTRFS:BOOL=ON
%else
%define _build_with_btrfs -DPLUGIN_BTRFS:BOOL=OFF
%endif

%if %{build_with_buddyinfo}
%define _build_with_buddyinfo -DPLUGIN_BUDDYINFO:BOOL=ON
%else
%define _build_with_buddyinfo -DPLUGIN_BUDDYINFO:BOOL=OFF
%endif

%if %{build_with_ceph}
%define _build_with_ceph -DPLUGIN_CEPH:BOOL=ON
%else
%define _build_with_ceph -DPLUGIN_CEPH:BOOL=OFF
%endif

%if %{build_with_cert}
%define _build_with_cert -DPLUGIN_CERT:BOOL=ON
%else
%define _build_with_cert -DPLUGIN_CERT:BOOL=OFF
%endif

%if %{build_with_cgroups}
%define _build_with_cgroups -DPLUGIN_CGROUPS:BOOL=ON
%else
%define _build_with_cgroups -DPLUGIN_CGROUPS:BOOL=OFF
%endif

%if %{build_with_chrony}
%define _build_with_chrony -DPLUGIN_CHRONY:BOOL=ON
%else
%define _build_with_chrony -DPLUGIN_CHRONY:BOOL=OFF
%endif

%if %{build_with_cifs}
%define _build_with_cifs -DPLUGIN_CIFS:BOOL=ON
%else
%define _build_with_cifs -DPLUGIN_CIFS:BOOL=OFF
%endif

%if %{build_with_conntrack}
%define _build_with_conntrack -DPLUGIN_CONNTRACK:BOOL=ON
%else
%define _build_with_conntrack -DPLUGIN_CONNTRACK:BOOL=OFF
%endif

%if %{build_with_contextswitch}
%define _build_with_contextswitch -DPLUGIN_CONTEXTSWITCH:BOOL=ON
%else
%define _build_with_contextswitch -DPLUGIN_CONTEXTSWITCH:BOOL=OFF
%endif

%if %{build_with_cpu}
%define _build_with_cpu -DPLUGIN_CPU:BOOL=ON
%else
%define _build_with_cpu -DPLUGIN_CPU:BOOL=OFF
%endif

%if %{build_with_cpufreq}
%define _build_with_cpufreq -DPLUGIN_CPUFREQ:BOOL=ON
%else
%define _build_with_cpufreq -DPLUGIN_CPUFREQ:BOOL=OFF
%endif

%if %{build_with_cpusleep}
%define _build_with_cpusleep -DPLUGIN_CPUSLEEP:BOOL=ON
%else
%define _build_with_cpusleep -DPLUGIN_CPUSLEEP:BOOL=OFF
%endif

%if %{build_with_cups}
%define _build_with_cups -DPLUGIN_CUPS:BOOL=ON
%else
%define _build_with_cups -DPLUGIN_CUPS:BOOL=OFF
%endif

%if %{build_with_db2}
%if %{?_db2_path:1}
%define _build_with_db2 -DPLUGIN_DB2:BOOL=ON -DLibDb2_ROOT:STRING=%{_db2_path}
%else
%define _build_with_db2 -DPLUGIN_DB2:BOOL=ON
%endif
%else
%define _build_with_db2 -DPLUGIN_DB2:BOOL=OFF
%endif

%if %{build_with_dcpmm}
%define _build_with_dcpmm -DPLUGIN_DCPMM:BOOL=ON
%else
%define _build_with_dcpmm -DPLUGIN_DCPMM:BOOL=OFF
%endif

%if %{build_with_df}
%define _build_with_df -DPLUGIN_DF:BOOL=ON
%else
%define _build_with_df -DPLUGIN_DF:BOOL=OFF
%endif

%if %{build_with_disk}
%define _build_with_disk -DPLUGIN_DISK:BOOL=ON
%else
%define _build_with_disk -DPLUGIN_DISK:BOOL=OFF
%endif

%if %{build_with_dns}
%define _build_with_dns -DPLUGIN_DNS:BOOL=ON
%else
%define _build_with_dns -DPLUGIN_DNS:BOOL=OFF
%endif

%if %{build_with_dnsmasq}
%define _build_with_dnsmasq -DPLUGIN_DNSMASQ:BOOL=ON
%else
%define _build_with_dnsmasq -DPLUGIN_DNSMASQ:BOOL=OFF
%endif

%if %{build_with_dmi}
%define _build_with_dmi -DPLUGIN_DMI:BOOL=ON
%else
%define _build_with_dmi -DPLUGIN_DMI:BOOL=OFF
%endif

%if %{build_with_docker}
%define _build_with_docker -DPLUGIN_DOCKER:BOOL=ON
%else
%define _build_with_docker -DPLUGIN_DOCKER:BOOL=OFF
%endif

%if %{build_with_ds389}
%define _build_with_ds389 -DPLUGIN_DS389:BOOL=ON
%else
%define _build_with_ds389 -DPLUGIN_DS389:BOOL=OFF
%endif

%if %{build_with_drbd}
%define _build_with_drbd -DPLUGIN_DRBD:BOOL=ON
%else
%define _build_with_drbd -DPLUGIN_DRBD:BOOL=OFF
%endif

%if %{build_with_edac}
%define _build_with_edac -DPLUGIN_EDAC:BOOL=ON
%else
%define _build_with_edac -DPLUGIN_EDAC:BOOL=OFF
%endif

%if %{build_with_entropy}
%define _build_with_entropy -DPLUGIN_ENTROPY:BOOL=ON
%else
%define _build_with_entropy -DPLUGIN_ENTROPY:BOOL=OFF
%endif

%if %{build_with_ethstat}
%define _build_with_ethstat -DPLUGIN_ETHSTAT:BOOL=ON
%else
%define _build_with_ethstat -DPLUGIN_ETHSTAT:BOOL=OFF
%endif

%if %{build_with_exec}
%define _build_with_exec -DPLUGIN_EXEC:BOOL=ON
%else
%define _build_with_exec -DPLUGIN_EXEC:BOOL=OFF
%endif

%if %{build_with_fcgi}
%define _build_with_fcgi -DPLUGIN_FCGI:BOOL=ON
%else
%define _build_with_fcgi -DPLUGIN_FCGI:BOOL=OFF
%endif

%if %{build_with_fchost}
%define _build_with_fchost -DPLUGIN_FCHOST:BOOL=ON
%else
%define _build_with_fchost -DPLUGIN_FCHOST:BOOL=OFF
%endif

%if %{build_with_fhcount}
%define _build_with_fhcount -DPLUGIN_FHCOUNT:BOOL=ON
%else
%define _build_with_fhcount -DPLUGIN_FHCOUNT:BOOL=OFF
%endif

%if %{build_with_filecount}
%define _build_with_filecount -DPLUGIN_FILECOUNT:BOOL=ON
%else
%define _build_with_filecount -DPLUGIN_FILECOUNT:BOOL=OFF
%endif

%if %{build_with_fscache}
%define _build_with_fscache -DPLUGIN_FSCACHE:BOOL=ON
%else
%define _build_with_fscache -DPLUGIN_FSCACHE:BOOL=OFF
%endif

%if %{build_with_freeradius}
%define _build_with_freeradius -DPLUGIN_FREERADIUS:BOOL=ON
%else
%define _build_with_freeradius -DPLUGIN_FREERADIUS:BOOL=OFF
%endif

%if %{build_with_gps}
%define _build_with_gps -DPLUGIN_GPS:BOOL=ON
%else
%define _build_with_gps -DPLUGIN_GPS:BOOL=OFF
%endif

%if %{build_with_gpu_intel}
%define _build_with_gpu_intel -DPLUGIN_GPU_INTEL:BOOL=ON
%else
%define _build_with_gpu_intel -DPLUGIN_GPU_INTEL:BOOL=OFF
%endif

%if %{build_with_gpu_nvidia}
%define _build_with_gpu_nvidia -DPLUGIN_GPU_NVIDIA:BOOL=ON
%else
%define _build_with_gpu_nvidia -DPLUGIN_GPU_NVIDIA:BOOL=OFF
%endif

%if %{build_with_haproxy}
%define _build_with_haproxy -DPLUGIN_HAPROXY:BOOL=ON
%else
%define _build_with_haproxy -DPLUGIN_HAPROXY:BOOL=OFF
%endif

%if %{build_with_http}
%define _build_with_http -DPLUGIN_HTTP:BOOL=ON
%else
%define _build_with_http -DPLUGIN_HTTP:BOOL=OFF
%endif

%if %{build_with_hugepages}
%define _build_with_hugepages -DPLUGIN_HUGEPAGES:BOOL=ON
%else
%define _build_with_hugepages -DPLUGIN_HUGEPAGES:BOOL=OFF
%endif

%if %{build_with_infiniband}
%define _build_with_infiniband -DPLUGIN_INFINIBAND:BOOL=ON
%else
%define _build_with_infiniband -DPLUGIN_INFINIBAND:BOOL=OFF
%endif

%if %{build_with_info}
%define _build_with_info -DPLUGIN_INFO:BOOL=ON
%else
%define _build_with_info -DPLUGIN_INFO:BOOL=OFF
%endif

%if %{build_with_intel_rdt}
%define _build_with_intel_rdt -DPLUGIN_INTEL_RDT:BOOL=ON
%else
%define _build_with_intel_rdt -DPLUGIN_INTEL_RDT:BOOL=OFF
%endif

%if %{build_with_interface}
%define _build_with_interface -DPLUGIN_INTERFACE:BOOL=ON
%else
%define _build_with_interface -DPLUGIN_INTERFACE:BOOL=OFF
%endif

%if %{build_with_ipc}
%define _build_with_ipc -DPLUGIN_IPC:BOOL=ON
%else
%define _build_with_ipc -DPLUGIN_IPC:BOOL=OFF
%endif

%if %{build_with_ipmi}
%define _build_with_ipmi -DPLUGIN_IPMI:BOOL=ON
%else
%define _build_with_ipmi -DPLUGIN_IPMI:BOOL=OFF
%endif

%if %{build_with_iptables}
%define _build_with_iptables -DPLUGIN_IPTABLES:BOOL=ON
%else
%define _build_with_iptables -DPLUGIN_IPTABLES:BOOL=OFF
%endif

%if %{build_with_ipstats}
%define _build_with_ipstats -DPLUGIN_IPSTATS:BOOL=ON
%else
%define _build_with_ipstats -DPLUGIN_IPSTATS:BOOL=OFF
%endif

%if %{build_with_ipvs}
%define _build_with_ipvs -DPLUGIN_IPVS:BOOL=ON
%else
%define _build_with_ipvs -DPLUGIN_IPVS:BOOL=OFF
%endif

%if %{build_with_irq}
%define _build_with_irq -DPLUGIN_IRQ:BOOL=ON
%else
%define _build_with_irq -DPLUGIN_IRQ:BOOL=OFF
%endif

%if %{build_with_java}
%define _build_with_java -DPLUGIN_JAVA:BOOL=ON
%else
%define _build_with_java -DPLUGIN_JAVA:BOOL=OFF
%endif

%if %{build_with_jolokia}
%define _build_with_jolokia -DPLUGIN_JOLOKIA:BOOL=ON
%else
%define _build_with_jolokia -DPLUGIN_JOLOKIA:BOOL=OFF
%endif

%if %{build_with_journal}
%define _build_with_journal -DPLUGIN_IRQ:BOOL=ON
%else
%define _build_with_journal -DPLUGIN_IRQ:BOOL=OFF
%endif

%if %{build_with_kafka}
%define _build_with_kafka -DPLUGIN_KAFKA:BOOL=ON
%else
%define _build_with_kafka -DPLUGIN_KAFKA:BOOL=OFF
%endif

%if %{build_with_kea}
%define _build_with_kea -DPLUGIN_KEA:BOOL=ON
%else
%define _build_with_kea -DPLUGIN_KEA:BOOL=OFF
%endif

%if %{build_with_keepalived}
%define _build_with_keepalived -DPLUGIN_KEEPALIVED:BOOL=ON
%else
%define _build_with_keepalived -DPLUGIN_KEEPALIVED:BOOL=OFF
%endif

%if %{build_with_ksm}
%define _build_with_ksm -DPLUGIN_KSM:BOOL=ON
%else
%define _build_with_ksm -DPLUGIN_KSM:BOOL=OFF
%endif

%if %{build_with_ldap}
%define _build_with_ldap -DPLUGIN_LDAP:BOOL=ON
%else
%define _build_with_ldap -DPLUGIN_LDAP:BOOL=OFF
%endif

%if %{build_with_load}
%define _build_with_load -DPLUGIN_LOAD:BOOL=ON
%else
%define _build_with_load -DPLUGIN_LOAD:BOOL=OFF
%endif

%if %{build_with_locks}
%define _build_with_locks -DPLUGIN_LOCKS:BOOL=ON
%else
%define _build_with_locks -DPLUGIN_LOCKS:BOOL=OFF
%endif

%if %{build_with_logind}
%define _build_with_logind -DPLUGIN_LOGIND:BOOL=ON
%else
%define _build_with_logind -DPLUGIN_LOGIND:BOOL=OFF
%endif

%if %{build_with_log_file}
%define _build_with_log_file -DPLUGIN_LOG_FILE:BOOL=ON
%else
%define _build_with_log_file -DPLUGIN_LOG_FILE:BOOL=OFF
%endif

%if %{build_with_log_gelf}
%define _build_with_log_gelf -DPLUGIN_LOG_GELF:BOOL=ON
%else
%define _build_with_log_gelf -DPLUGIN_LOG_GELF:BOOL=OFF
%endif

%if %{build_with_log_syslog}
%define _build_with_log_syslog -DPLUGIN_LOG_SYSLOG:BOOL=ON
%else
%define _build_with_log_syslog -DPLUGIN_LOG_SYSLOG:BOOL=OFF
%endif

%if %{build_with_log_systemd}
%define _build_with_log_systemd -DPLUGIN_LOG_SYSTEMD:BOOL=ON
%else
%define _build_with_log_systemd -DPLUGIN_LOG_SYSTEMD:BOOL=OFF
%endif

%if %{build_with_lpar}
%define _build_with_lpar -DPLUGIN_LPAR:BOOL=ON
%else
%define _build_with_lpar -DPLUGIN_LPAR:BOOL=OFF
%endif

%if %{build_with_lua}
%define _build_with_lua -DPLUGIN_LUA:BOOL=ON
%else
%define _build_with_lua -DPLUGIN_LUA:BOOL=OFF
%endif

%if %{build_with_lvm}
%define _build_with_lvm -DPLUGIN_LVM:BOOL=ON
%else
%define _build_with_lvm -DPLUGIN_LVM:BOOL=OFF
%endif

%if %{build_with_match_csv}
%define _build_with_match_csv -DPLUGIN_MATCH_CSV:BOOL=ON
%else
%define _build_with_match_csv -DPLUGIN_MATCH_CSV:BOOL=OFF

%endif
%if %{build_with_match_jsonpath}
%define _build_with_match_jsonpath -DPLUGIN_MATCH_JSONPATH:BOOL=ON
%else
%define _build_with_match_jsonpath -DPLUGIN_MATCH_JSONPATH:BOOL=OFF
%endif

%if %{build_with_match_regex}
%define _build_with_match_regex -DPLUGIN_MATCH_REGEX:BOOL=ON
%else
%define _build_with_match_regex -DPLUGIN_MATCH_REGEX:BOOL=OFF
%endif

%if %{build_with_match_table}
%define _build_with_match_table -DPLUGIN_MATCH_TABLE:BOOL=ON
%else
%define _build_with_match_table -DPLUGIN_MATCH_TABLE:BOOL=OFF
%endif

%if %{build_with_match_xpath}
%define _build_with_match_xpath -DPLUGIN_MATCH_XPATH:BOOL=ON
%else
%define _build_with_match_xpath -DPLUGIN_MATCH_XPATH:BOOL=OFF
%endif

%if %{build_with_md}
%define _build_with_md -DPLUGIN_MD:BOOL=ON
%else
%define _build_with_md -DPLUGIN_MD:BOOL=OFF
%endif

%if %{build_with_memcached}
%define _build_with_memcached -DPLUGIN_MEMCACHED:BOOL=ON
%else
%define _build_with_memcached -DPLUGIN_MEMCACHED:BOOL=OFF
%endif

%if %{build_with_meminfo}
%define _build_with_meminfo -DPLUGIN_MEMINFO:BOOL=ON
%else
%define _build_with_meminfo -DPLUGIN_MEMINFO:BOOL=OFF
%endif

%if %{build_with_memory}
%define _build_with_memory -DPLUGIN_MEMORY:BOOL=ON
%else
%define _build_with_memory -DPLUGIN_MEMORY:BOOL=OFF
%endif

%if %{build_with_mmc}
%define _build_with_mmc -DPLUGIN_MMC:BOOL=ON
%else
%define _build_with_mmc -DPLUGIN_MMC:BOOL=OFF
%endif

%if %{build_with_modbus}
%define _build_with_modbus -DPLUGIN_MODBUS:BOOL=ON
%else
%define _build_with_modbus -DPLUGIN_MODBUS:BOOL=OFF
%endif

%if %{build_with_mongodb}
%define _build_with_mongodb -DPLUGIN_MONGODB:BOOL=ON
%else
%define _build_with_mongodb -DPLUGIN_MONGODB:BOOL=OFF
%endif

%if %{build_with_mosquitto}
%define _build_with_mosquitto -DPLUGIN_MOSQUITTO:BOOL=ON
%else
%define _build_with_mosquitto -DPLUGIN_MOSQUITTO:BOOL=OFF
%endif

%if %{build_with_mq}
%if %{?_mq_path:1}
%define _build_with_mq -DPLUGIN_MQ:BOOL=ON -DLibMq_ROOT:STRING=%{_mq_path}
%else
%define _build_with_mq -DPLUGIN_MQ:BOOL=ON
%endif
%else
%define _build_with_mq -DPLUGIN_MQ:BOOL=OFF
%endif

%if %{build_with_mssql}
%define _build_with_mssql -DPLUGIN_MSSQL:BOOL=ON
%else
%define _build_with_mssql -DPLUGIN_MSSQL:BOOL=OFF
%endif

%if %{build_with_mysql}
%define _build_with_mysql -DPLUGIN_MYSQL:BOOL=ON
%else
%define _build_with_mysql -DPLUGIN_MYSQL:BOOL=OFF
%endif

%if %{build_with_netstat_udp}
%define _build_with_netstat_udp -DPLUGIN_NETSTAT_UDP:BOOL=ON
%else
%define _build_with_netstat_udp -DPLUGIN_NETSTAT_UDP:BOOL=OFF
%endif

%if %{build_with_nagios_check}
%define _build_with_nagios_check -DPLUGIN_NAGIOS_CHECK:BOOL=ON
%else
%define _build_with_nagios_check -DPLUGIN_NAGIOS_CHECK:BOOL=OFF
%endif

%if %{build_with_nfacct}
%define _build_with_nfacct -DPLUGIN_NFACCT:BOOL=ON
%else
%define _build_with_nfacct -DPLUGIN_NFACCT:BOOL=OFF
%endif

%if %{build_with_nfconntrack}
%define _build_with_nfconntrack -DPLUGIN_NFCONNTRACK:BOOL=ON
%else
%define _build_with_nfconntrack -DPLUGIN_NFCONNTRACK:BOOL=OFF
%endif

%if %{build_with_nfs}
%define _build_with_nfs -DPLUGIN_NFS:BOOL=ON
%else
%define _build_with_nfs -DPLUGIN_NFS:BOOL=OFF
%endif

%if %{build_with_nfsd}
%define _build_with_nfsd -DPLUGIN_NFSD:BOOL=ON
%else
%define _build_with_nfsd -DPLUGIN_NFSD:BOOL=OFF
%endif

%if %{build_with_nftables}
%define _build_with_nftables -DPLUGIN_NFTABLES:BOOL=ON
%else
%define _build_with_nftables -DPLUGIN_NFTABLES:BOOL=OFF
%endif

%if %{build_with_nginx}
%define _build_with_nginx -DPLUGIN_NGINX:BOOL=ON
%else
%define _build_with_nginx -DPLUGIN_NGINX:BOOL=OFF
%endif

%if %{build_with_nginx_vts}
%define _build_with_nginx_vts -DPLUGIN_NGINX_VTS:BOOL=ON
%else
%define _build_with_nginx_vts -DPLUGIN_NGINX_VTS:BOOL=OFF
%endif

%if %{build_with_notify_email}
%define _build_with_notify_email -DPLUGIN_NOTIFY_EMAIL:BOOL=ON
%else
%define _build_with_notify_email -DPLUGIN_NOTIFY_EMAIL:BOOL=OFF
%endif

%if %{build_with_notify_exec}
%define _build_with_notify_exec -DPLUGIN_NOTIFY_EXEC:BOOL=ON
%else
%define _build_with_notify_exec -DPLUGIN_NOTIFY_EXEC:BOOL=OFF
%endif

%if %{build_with_notify_nagios}
%define _build_with_notify_nagios -DPLUGIN_NOTIFY_NAGIOS:BOOL=ON
%else
%define _build_with_notify_nagios -DPLUGIN_NOTIFY_NAGIOS:BOOL=OFF
%endif

%if %{build_with_notify_snmp}
%define _build_with_notify_snmp -DPLUGIN_NOTIFY_SNMP:BOOL=ON
%else
%define _build_with_notify_snmp -DPLUGIN_NOTIFY_SNMP:BOOL=OFF
%endif

%if %{build_with_nsd}
%define _build_with_nsd -DPLUGIN_NSD:BOOL=ON
%else
%define _build_with_nsd -DPLUGIN_NSD:BOOL=OFF
%endif

%if %{build_with_ntpd}
%define _build_with_ntpd -DPLUGIN_NTPD:BOOL=ON
%else
%define _build_with_ntpd -DPLUGIN_NTPD:BOOL=OFF
%endif

%if %{build_with_numa}
%define _build_with_numa -DPLUGIN_NUMA:BOOL=ON
%else
%define _build_with_numa -DPLUGIN_NUMA:BOOL=OFF
%endif

%if %{build_with_nut}
%define _build_with_nut -DPLUGIN_NUT:BOOL=ON
%else
%define _build_with_nut -DPLUGIN_NUT:BOOL=OFF
%endif

%if %{build_with_odbc}
%define _build_with_odbc -DPLUGIN_ODBC:BOOL=ON
%else
%define _build_with_odbc -DPLUGIN_ODBC:BOOL=OFF
%endif

%if %{build_with_olsrd}
%define _build_with_olsrd -DPLUGIN_OLSRD:BOOL=ON
%else
%define _build_with_olsrd -DPLUGIN_OLSRD:BOOL=OFF
%endif

%if %{build_with_openldap}
%define _build_with_openldap -DPLUGIN_OPENLDAP:BOOL=ON
%else
%define _build_with_openldap -DPLUGIN_OPENLDAP:BOOL=OFF
%endif

%if %{build_with_openvpn}
%define _build_with_openvpn -DPLUGIN_OPENVPN:BOOL=ON
%else
%define _build_with_openvpn -DPLUGIN_OPENVPN:BOOL=OFF
%endif

%if %{build_with_oracle}
%define _build_with_oracle -DPLUGIN_ORACLE:BOOL=ON -DORACLE_HOME:STRING=%{_oracle_path}
%else
%define _build_with_oracle -DPLUGIN_ORACLE:BOOL=OFF
%endif

%if %{build_with_pcap}
%define _build_with_pcap -DPLUGIN_PCAP:BOOL=ON
%else
%define _build_with_pcap -DPLUGIN_PCAP:BOOL=OFF
%endif

%if %{build_with_perl}
%define _build_with_perl -DPLUGIN_PERL:BOOL=ON
%else
%define _build_with_perl -DPLUGIN_PERL:BOOL=OFF
%endif

%if %{build_with_pf}
%define _build_with_pf -DPLUGIN_PF:BOOL=ON
%else
%define _build_with_pf -DPLUGIN_PF:BOOL=OFF
%endif

%if %{build_with_pgbouncer}
%define _build_with_pgbouncer -DPLUGIN_PGBOUNCER:BOOL=ON
%else
%define _build_with_pgbouncer -DPLUGIN_PGBOUNCER:BOOL=OFF
%endif

%if %{build_with_ping}
%define _build_with_ping -DPLUGIN_PING:BOOL=ON
%else
%define _build_with_ping -DPLUGIN_PING:BOOL=OFF
%endif

%if %{build_with_podman}
%define _build_with_podman -DPLUGIN_PODMAN:BOOL=ON
%else
%define _build_with_podman -DPLUGIN_PODMAN:BOOL=OFF
%endif

%if %{build_with_postfix}
%define _build_with_postfix -DPLUGIN_POSTFIX:BOOL=ON
%else
%define _build_with_postfix -DPLUGIN_POSTFIX:BOOL=OFF
%endif

%if %{build_with_postgresql}
%define _build_with_postgresql -DPLUGIN_POSTGRESQL:BOOL=ON
%else
%define _build_with_postgresql -DPLUGIN_POSTGRESQL:BOOL=OFF
%endif

%if %{build_with_pdns}
%define _build_with_pdns -DPLUGIN_PDNS:BOOL=ON
%else
%define _build_with_pdns -DPLUGIN_PDNS:BOOL=OFF
%endif

%if %{build_with_pressure}
%define _build_with_pressure -DPLUGIN_PRESSURE:BOOL=ON
%else
%define _build_with_pressure -DPLUGIN_PRESSURE:BOOL=OFF
%endif

%if %{build_with_processes}
%define _build_with_processes -DPLUGIN_PROCESSES:BOOL=ON
%else
%define _build_with_processes -DPLUGIN_PROCESSES:BOOL=OFF
%endif

%if %{build_with_protocols}
%define _build_with_protocols -DPLUGIN_PROTOCOLS:BOOL=ON
%else
%define _build_with_protocols -DPLUGIN_PROTOCOLS:BOOL=OFF
%endif

%if %{build_with_proxysql}
%define _build_with_proxysql -DPLUGIN_PROXYSQL:BOOL=ON
%else
%define _build_with_proxysql -DPLUGIN_PROXYSQL:BOOL=OFF
%endif

%if %{build_with_python}
%define _build_with_python -DPLUGIN_PYTHON:BOOL=ON
%else
%define _build_with_python -DPLUGIN_PYTHON:BOOL=OFF
%endif

%if %{build_with_quota}
%define _build_with_quota -DPLUGIN_QUOTA:BOOL=ON
%else
%define _build_with_quota -DPLUGIN_QUOTA:BOOL=OFF
%endif

%if %{build_with_rapl}
%define _build_with_rapl -DPLUGIN_RAPL:BOOL=ON
%else
%define _build_with_rapl -DPLUGIN_RAPL:BOOL=OFF
%endif

%if %{build_with_recursor}
%define _build_with_recursor -DPLUGIN_RECURSOR:BOOL=ON
%else
%define _build_with_recursor -DPLUGIN_RECURSOR:BOOL=OFF
%endif

%if %{build_with_redis}
%define _build_with_redis -DPLUGIN_REDIS:BOOL=ON
%else
%define _build_with_redis -DPLUGIN_REDIS:BOOL=OFF
%endif

%if %{build_with_resctrl}
%define _build_with_resctrl -DPLUGIN_RESCTRL:BOOL=ON
%else
%define _build_with_resctrl -DPLUGIN_RESCTRL:BOOL=OFF
%endif

%if %{build_with_routeros}
%define _build_with_routeros -DPLUGIN_ROUTEROS:BOOL=ON
%else
%define _build_with_routeros -DPLUGIN_ROUTEROS:BOOL=OFF
%endif

%if %{build_with_rtcache}
%define _build_with_rtcache -DPLUGIN_RTCACHE:BOOL=ON
%else
%define _build_with_rtcache -DPLUGIN_RTCACHE:BOOL=OFF
%endif

%if %{build_with_schedstat}
%define _build_with_schedstat -DPLUGIN_SCHEDSTAT:BOOL=ON
%else
%define _build_with_schedstat -DPLUGIN_SCHEDSTAT:BOOL=OFF
%endif

%if %{build_with_scraper}
%define _build_with_scraper -DPLUGIN_SCRAPER:BOOL=ON
%else
%define _build_with_scraper -DPLUGIN_SCRAPER:BOOL=OFF
%endif

%if %{build_with_sendmail}
%define _build_with_sendmail -DPLUGIN_SENDMAIL:BOOL=ON
%else
%define _build_with_sendmail -DPLUGIN_SENDMAIL:BOOL=OFF
%endif

%if %{build_with_sensors}
%define _build_with_sensors -DPLUGIN_SENSORS:BOOL=ON
%else
%define _build_with_sensors -DPLUGIN_SENSORS:BOOL=OFF
%endif

%if %{build_with_serial}
%define _build_with_serial -DPLUGIN_SERIAL:BOOL=ON
%else
%define _build_with_serial -DPLUGIN_SERIAL:BOOL=OFF
%endif

%if %{build_with_sigrok}
%define _build_with_sigrok -DPLUGIN_SIGROK:BOOL=ON
%else
%define _build_with_sigrok -DPLUGIN_SIGROK:BOOL=OFF
%endif

%if %{build_with_slabinfo}
%define _build_with_slabinfo -DPLUGIN_SLABINFO:BOOL=ON
%else
%define _build_with_slabinfo -DPLUGIN_SLABINFO:BOOL=OFF
%endif

%if %{build_with_slurm}
%define _build_with_slurm -DPLUGIN_SLURM:BOOL=ON
%else
%define _build_with_slurm -DPLUGIN_SLURM:BOOL=OFF
%endif

%if %{build_with_smart}
%define _build_with_smart -DPLUGIN_SMART:BOOL=ON
%else
%define _build_with_smart -DPLUGIN_SMART:BOOL=OFF
%endif

%if %{build_with_snmp}
%define _build_with_snmp -DPLUGIN_SNMP:BOOL=ON
%else
%define _build_with_snmp -DPLUGIN_SNMP:BOOL=OFF
%endif

%if %{build_with_sockstat}
%define _build_with_sockstat -DPLUGIN_SOCKSTAT:BOOL=ON
%else
%define _build_with_sockstat -DPLUGIN_SOCKSTAT:BOOL=OFF
%endif

%if %{build_with_softirq}
%define _build_with_softirq -DPLUGIN_SOFTIRQ:BOOL=ON
%else
%define _build_with_softirq -DPLUGIN_SOFTIRQ:BOOL=OFF
%endif

%if %{build_with_softnet}
%define _build_with_softnet -DPLUGIN_SOFTNET:BOOL=ON
%else
%define _build_with_softnet -DPLUGIN_SOFTNET:BOOL=OFF
%endif

%if %{build_with_squid}
%define _build_with_squid -DPLUGIN_SQUID:BOOL=ON
%else
%define _build_with_squid -DPLUGIN_SQUID:BOOL=OFF
%endif

%if %{build_with_statsd}
%define _build_with_statsd -DPLUGIN_STATSD:BOOL=ON
%else
%define _build_with_statsd -DPLUGIN_STATSD:BOOL=OFF
%endif

%if %{build_with_swap}
%define _build_with_swap -DPLUGIN_SWAP:BOOL=ON
%else
%define _build_with_swap -DPLUGIN_SWAP:BOOL=OFF
%endif

%if %{build_with_synproxy}
%define _build_with_synproxy -DPLUGIN_SYNPROXY:BOOL=ON
%else
%define _build_with_synproxy -DPLUGIN_SYNPROXY:BOOL=OFF
%endif

%if %{build_with_systemd}
%define _build_with_systemd -DPLUGIN_SYSTEMD:BOOL=ON
%else
%define _build_with_systemd -DPLUGIN_SYSTEMD:BOOL=OFF
%endif

%if %{build_with_table}
%define _build_with_table -DPLUGIN_TABLE:BOOL=ON
%else
%define _build_with_table -DPLUGIN_TABLE:BOOL=OFF
%endif

%if %{build_with_tail}
%define _build_with_tail -DPLUGIN_TAIL:BOOL=ON
%else
%define _build_with_tail -DPLUGIN_TAIL:BOOL=OFF
%endif

%if %{build_with_tape}
%define _build_with_tape -DPLUGIN_TAPE:BOOL=ON
%else
%define _build_with_tape -DPLUGIN_TAPE:BOOL=OFF
%endif

%if %{build_with_tc}
%define _build_with_tc -DPLUGIN_TC:BOOL=ON
%else
%define _build_with_tc -DPLUGIN_TC:BOOL=OFF
%endif

%if %{build_with_tcpconns}
%define _build_with_tcpconns -DPLUGIN_TCPCONNS:BOOL=ON
%else
%define _build_with_tcpconns -DPLUGIN_TCPCONNS:BOOL=OFF
%endif

%if %{build_with_timex}
%define _build_with_timex -DPLUGIN_TIMEX:BOOL=ON
%else
%define _build_with_timex -DPLUGIN_TIMEX:BOOL=OFF
%endif

%if %{build_with_thermal}
%define _build_with_thermal -DPLUGIN_THERMAL:BOOL=ON
%else
%define _build_with_thermal -DPLUGIN_THERMAL:BOOL=OFF
%endif

%if %{build_with_turbostat}
%define _build_with_turbostat -DPLUGIN_TURBOSTAT:BOOL=ON
%else
%define _build_with_turbostat -DPLUGIN_TURBOSTAT:BOOL=OFF
%endif

%if %{build_with_ubi}
%define _build_with_ubi -DPLUGIN_UBI:BOOL=ON
%else
%define _build_with_ubi -DPLUGIN_UBI:BOOL=OFF
%endif

%if %{build_with_uname}
%define _build_with_uname -DPLUGIN_UNAME:BOOL=ON
%else
%define _build_with_uname -DPLUGIN_UNAME:BOOL=OFF
%endif

%if %{build_with_unbound}
%define _build_with_unbound -DPLUGIN_UNBOUND:BOOL=ON
%else
%define _build_with_unbound -DPLUGIN_UNBOUND:BOOL=OFF
%endif

%if %{build_with_uptime}
%define _build_with_uptime -DPLUGIN_UPTIME:BOOL=ON
%else
%define _build_with_uptime -DPLUGIN_UPTIME:BOOL=OFF
%endif

%if %{build_with_users}
%define _build_with_users -DPLUGIN_USERS:BOOL=ON
%else
%define _build_with_users -DPLUGIN_USERS:BOOL=OFF
%endif

%if %{build_with_uuid}
%define _build_with_uuid -DPLUGIN_UUID:BOOL=ON
%else
%define _build_with_uuid -DPLUGIN_UUID:BOOL=OFF
%endif

%if %{build_with_varnish}
%define _build_with_varnish -DPLUGIN_VARNISH:BOOL=ON
%else
%define _build_with_varnish -DPLUGIN_VARNISH:BOOL=OFF
%endif

%if %{build_with_virt}
%define _build_with_virt -DPLUGIN_VIRT:BOOL=ON
%else
%define _build_with_virt -DPLUGIN_VIRT:BOOL=OFF
%endif

%if %{build_with_vmem}
%define _build_with_vmem -DPLUGIN_VMEM:BOOL=ON
%else
%define _build_with_vmem -DPLUGIN_VMEM:BOOL=OFF
%endif

%if %{build_with_wireguard}
%define _build_with_wireguard -DPLUGIN_WIREGUARD:BOOL=ON
%else
%define _build_with_wireguard -DPLUGIN_WIREGUARD:BOOL=OFF
%endif

%if %{build_with_wireless}
%define _build_with_wireless -DPLUGIN_WIRELESS:BOOL=ON
%else
%define _build_with_wireless -DPLUGIN_WIRELESS:BOOL=OFF
%endif

%if %{build_with_write_amqp}
%define _build_with_write_amqp -DPLUGIN_WRITE_AMQP:BOOL=ON
%else
%define _build_with_write_amqp -DPLUGIN_WRITE_AMQP:BOOL=OFF
%endif

%if %{build_with_write_amqp1}
%define _build_with_write_amqp1 -DPLUGIN_WRITE_AMQP1:BOOL=ON
%else
%define _build_with_write_amqp1 -DPLUGIN_WRITE_AMQP1:BOOL=OFF
%endif

%if %{build_with_write_exporter}
%define _build_with_write_exporter -DPLUGIN_WRITE_EXPORTER:BOOL=ON
%else
%define _build_with_write_exporter -DPLUGIN_WRITE_EXPORTER:BOOL=OFF
%endif

%if %{build_with_write_file}
%define _build_with_write_file -DPLUGIN_WRITE_FILE:BOOL=ON
%else
%define _build_with_write_file -DPLUGIN_WRITE_FILE:BOOL=OFF
%endif

%if %{build_with_write_http}
%define _build_with_write_http -DPLUGIN_WRITE_HTTP:BOOL=ON
%else
%define _build_with_write_http -DPLUGIN_WRITE_HTTP:BOOL=OFF
%endif

%if %{build_with_write_kafka}
%define _build_with_write_kafka -DPLUGIN_WRITE_KAFKA:BOOL=ON
%else
%define _build_with_write_kafka -DPLUGIN_WRITE_KAFKA:BOOL=OFF
%endif

%if %{build_with_write_log}
%define _build_with_write_log -DPLUGIN_WRITE_LOG:BOOL=ON
%else
%define _build_with_write_log -DPLUGIN_WRITE_LOG:BOOL=OFF
%endif

%if %{build_with_write_mongodb}
%define _build_with_write_mongodb -DPLUGIN_WRITE_MONGODB:BOOL=ON
%else
%define _build_with_write_mongodb -DPLUGIN_WRITE_MONGODB:BOOL=OFF
%endif

%if %{build_with_write_mqtt}
%define _build_with_write_mqtt -DPLUGIN_WRITE_MQTT:BOOL=ON
%else
%define _build_with_write_mqtt -DPLUGIN_WRITE_MQTT:BOOL=OFF
%endif

%if %{build_with_write_postgresql}
%define _build_with_write_postgresql -DPLUGIN_WRITE_POSTGRESQL:BOOL=ON
%else
%define _build_with_write_postgresql -DPLUGIN_WRITE_POSTGRESQL:BOOL=OFF
%endif

%if %{build_with_write_redis}
%define _build_with_write_redis -DPLUGIN_WRITE_REDIS:BOOL=ON
%else
%define _build_with_write_redis -DPLUGIN_WRITE_REDIS:BOOL=OFF
%endif

%if %{build_with_write_tcp}
%define _build_with_write_tcp -DPLUGIN_WRITE_TCP:BOOL=ON
%else
%define _build_with_write_tcp -DPLUGIN_WRITE_TCP:BOOL=OFF
%endif

%if %{build_with_write_udp}
%define _build_with_write_udp -DPLUGIN_WRITE_UDP:BOOL=ON
%else
%define _build_with_write_udp -DPLUGIN_WRITE_UDP:BOOL=OFF
%endif

%if %{build_with_xencpu}
%define _build_with_xencpu -DPLUGIN_XENCPU:BOOL=ON
%else
%define _build_with_xencpu -DPLUGIN_XENCPU:BOOL=OFF
%endif

%if %{build_with_xfrm}
%define _build_with_xfrm -DPLUGIN_XFRM:BOOL=ON
%else
%define _build_with_xfrm -DPLUGIN_XFRM:BOOL=OFF
%endif

%if %{build_with_xfs}
%define _build_with_xfs -DPLUGIN_XFS:BOOL=ON
%else
%define _build_with_xfs -DPLUGIN_XFS:BOOL=OFF
%endif

%if %{build_with_zfs}
%define _build_with_zfs -DPLUGIN_ZFS:BOOL=ON
%else
%define _build_with_zfs -DPLUGIN_ZFS:BOOL=OFF
%endif

%if %{build_with_zone}
%define _build_with_zone -DPLUGIN_ZONE:BOOL=ON
%else
%define _build_with_zone -DPLUGIN_ZONE:BOOL=OFF
%endif

%if %{build_with_zoneinfo}
%define _build_with_zoneinfo -DPLUGIN_ZONEINFO:BOOL=ON
%else
%define _build_with_zoneinfo -DPLUGIN_ZONEINFO:BOOL=OFF
%endif

%if %{build_with_zookeeper}
%define _build_with_zookeeper -DPLUGIN_ZOOKEEPER:BOOL=ON
%else
%define _build_with_zookeeper -DPLUGIN_ZOOKEEPER:BOOL=OFF
%endif

%if %{build_with_zram}
%define _build_with_zram -DPLUGIN_ZRAM:BOOL=ON
%else
%define _build_with_zram -DPLUGIN_ZRAM:BOOL=OFF
%endif

%if %{build_with_zswap}
%define _build_with_zswap -DPLUGIN_ZSWAP:BOOL=ON
%else
%define _build_with_zswap -DPLUGIN_ZSWAP:BOOL=OFF
%endif

%cmake -DCMAKE_INSTALL_SYSCONFDIR:STRING=/etc \
    -DENABLE_ALL_PLUGINS:BOOL=ON \
    %{?_build_with_arp} \
    %{?_build_with_arpcache} \
    %{?_build_with_ats} \
    %{?_build_with_apache} \
    %{?_build_with_apcups} \
    %{?_build_with_avccache} \
    %{?_build_with_battery} \
    %{?_build_with_bcache} \
    %{?_build_with_bind} \
    %{?_build_with_beanstalkd} \
    %{?_build_with_bonding} \
    %{?_build_with_btrfs} \
    %{?_build_with_buddyinfo} \
    %{?_build_with_ceph} \
    %{?_build_with_cert} \
    %{?_build_with_cgroups} \
    %{?_build_with_chrony} \
    %{?_build_with_cifs} \
    %{?_build_with_conntrack} \
    %{?_build_with_contextswitch} \
    %{?_build_with_cpu} \
    %{?_build_with_cpufreq} \
    %{?_build_with_cpusleep} \
    %{?_build_with_cups} \
    %{?_build_with_db2} \
    %{?_build_with_dcpmm} \
    %{?_build_with_df} \
    %{?_build_with_disk} \
    %{?_build_with_dns} \
    %{?_build_with_dnsmasq} \
    %{?_build_with_dmi} \
    %{?_build_with_docker} \
    %{?_build_with_ds389} \
    %{?_build_with_drbd} \
    %{?_build_with_edac} \
    %{?_build_with_entropy} \
    %{?_build_with_ethstat} \
    %{?_build_with_exec} \
    %{?_build_with_fcgi} \
    %{?_build_with_fchost} \
    %{?_build_with_fhcount} \
    %{?_build_with_filecount} \
    %{?_build_with_freeradius} \
    %{?_build_with_fscache} \
    %{?_build_with_gps} \
    %{?_build_with_gpu_intel} \
    %{?_build_with_gpu_nvidia} \
    %{?_build_with_haproxy} \
    %{?_build_with_http} \
    %{?_build_with_hugepages} \
    %{?_build_with_infiniband} \
    %{?_build_with_info} \
    %{?_build_with_intel_rdt} \
    %{?_build_with_interface} \
    %{?_build_with_ipc} \
    %{?_build_with_ipmi} \
    %{?_build_with_iptables} \
    %{?_build_with_ipstats} \
    %{?_build_with_ipvs} \
    %{?_build_with_irq} \
    %{?_build_with_java} \
    %{?_build_with_jolokia} \
    %{?_build_with_journal} \
    %{?_build_with_kafka} \
    %{?_build_with_kea} \
    %{?_build_with_keepalived} \
    %{?_build_with_ksm} \
    %{?_build_with_ldap} \
    %{?_build_with_load} \
    %{?_build_with_locks} \
    %{?_build_with_logind} \
    %{?_build_with_log_file} \
    %{?_build_with_log_gelf} \
    %{?_build_with_log_syslog} \
    %{?_build_with_log_systemd} \
    %{?_build_with_lpar} \
    %{?_build_with_lua} \
    %{?_build_with_lvm} \
    %{?_build_with_match_csv} \
    %{?_build_with_match_jsonpath} \
    %{?_build_with_match_regex} \
    %{?_build_with_match_table} \
    %{?_build_with_match_xpath} \
    %{?_build_with_md} \
    %{?_build_with_memcached} \
    %{?_build_with_meminfo} \
    %{?_build_with_memory} \
    %{?_build_with_mmc} \
    %{?_build_with_modbus} \
    %{?_build_with_mongodb} \
    %{?_build_with_mosquitto} \
    %{?_build_with_mq} \
    %{?_build_with_mssql} \
    %{?_build_with_mysql} \
    %{?_build_with_netstat_udp} \
    %{?_build_with_nagios_check} \
    %{?_build_with_nfacct} \
    %{?_build_with_nfconntrack} \
    %{?_build_with_nfs} \
    %{?_build_with_nfsd} \
    %{?_build_with_nftables} \
    %{?_build_with_nginx} \
    %{?_build_with_nginx_vts} \
    %{?_build_with_notify_email} \
    %{?_build_with_notify_exec} \
    %{?_build_with_notify_nagios} \
    %{?_build_with_notify_snmp} \
    %{?_build_with_nsd} \
    %{?_build_with_ntpd} \
    %{?_build_with_numa} \
    %{?_build_with_nut} \
    %{?_build_with_odbc} \
    %{?_build_with_olsrd} \
    %{?_build_with_openldap} \
    %{?_build_with_openvpn} \
    %{?_build_with_oracle} \
    %{?_build_with_pcap} \
    %{?_build_with_perl} \
    %{?_build_with_pf} \
    %{?_build_with_pgbouncer} \
    %{?_build_with_ping} \
    %{?_build_with_podman} \
    %{?_build_with_postfix} \
    %{?_build_with_postgresql} \
    %{?_build_with_pdns} \
    %{?_build_with_pressure} \
    %{?_build_with_processes} \
    %{?_build_with_protocols} \
    %{?_build_with_proxysql} \
    %{?_build_with_python} \
    %{?_build_with_quota} \
    %{?_build_with_rapl} \
    %{?_build_with_recursor} \
    %{?_build_with_redis} \
    %{?_build_with_resctrl} \
    %{?_build_with_routeros} \
    %{?_build_with_rtcache} \
    %{?_build_with_schedstat} \
    %{?_build_with_scraper} \
    %{?_build_with_sensors} \
    %{?_build_with_sendmail} \
    %{?_build_with_serial} \
    %{?_build_with_sigrok} \
    %{?_build_with_slabinfo} \
    %{?_build_with_slurm} \
    %{?_build_with_smart} \
    %{?_build_with_snmp} \
    %{?_build_with_sockstat} \
    %{?_build_with_softirq} \
    %{?_build_with_softnet} \
    %{?_build_with_squid} \
    %{?_build_with_statsd} \
    %{?_build_with_swap} \
    %{?_build_with_synproxy} \
    %{?_build_with_systemd} \
    %{?_build_with_table} \
    %{?_build_with_tail} \
    %{?_build_with_tape} \
    %{?_build_with_tc} \
    %{?_build_with_tcpconns} \
    %{?_build_with_timex} \
    %{?_build_with_thermal} \
    %{?_build_with_turbostat} \
    %{?_build_with_ubi} \
    %{?_build_with_uname} \
    %{?_build_with_unbound} \
    %{?_build_with_uptime} \
    %{?_build_with_users} \
    %{?_build_with_uuid} \
    %{?_build_with_varnish} \
    %{?_build_with_virt} \
    %{?_build_with_vmem} \
    %{?_build_with_wireguard} \
    %{?_build_with_wireless} \
    %{?_build_with_write_amqp} \
    %{?_build_with_write_amqp1} \
    %{?_build_with_write_file} \
    %{?_build_with_write_http} \
    %{?_build_with_write_kafka} \
    %{?_build_with_write_log} \
    %{?_build_with_write_mongodb} \
    %{?_build_with_write_mqtt} \
    %{?_build_with_write_postgresql} \
    %{?_build_with_write_exporter} \
    %{?_build_with_write_redis} \
    %{?_build_with_write_tcp} \
    %{?_build_with_write_udp} \
    %{?_build_with_xencpu} \
    %{?_build_with_xfrm} \
    %{?_build_with_xfs} \
    %{?_build_with_zfs} \
    %{?_build_with_zone} \
    %{?_build_with_zoneinfo} \
    %{?_build_with_zookeeper} \
    %{?_build_with_zram} \
    %{?_build_with_zswap} \
    .

%cmake_build

%install
rm -rf %{buildroot}
%cmake_install

%{__install} -Dp -m0644 %{SOURCE1} %{buildroot}%{_unitdir}/ncollectd.service
#%{__install} -Dp -m0644 src/ncollectd.conf %{buildroot}%{_sysconfdir}/ncollectd.conf
%{__install} -d %{buildroot}%{_sharedstatedir}/ncollectd/
%{__install} -d %{buildroot}%{_sysconfdir}/ncollectd.d/

# *.la files shouldn't be distributed.
rm -f %{buildroot}/%{_libdir}/{ncollectd/,}*.la

# Remove Perl hidden .packlist files.
find %{buildroot} -type f -name .packlist -delete
# Remove Perl temporary file perllocal.pod
find %{buildroot} -type f -name perllocal.pod -delete

%clean
rm -rf %{buildroot}

%post
%systemd_post ncollectd.service

%preun
%systemd_preun ncollectd.service

%postun
%systemd_postun_with_restart ncollectd.service


%files
%config(noreplace) %{_sysconfdir}/ncollectd.conf
%{_unitdir}/ncollectd.service
%{_sbindir}/ncollectd
%{_sbindir}/ncollectdmon
%{_bindir}/ncollectdctl
%{_sharedstatedir}/ncollectd
%{_mandir}/man1/ncollectd.1*
%{_mandir}/man1/ncollectdmon.1*
%{_mandir}/man1/ncollectdctl.1*
%{_mandir}/man5/ncollectd.conf.5*
%if %{build_with_apcups}
%{_libdir}/%{name}/apcups.so
%{_mandir}/man5/ncollectd-apcups.5*
%endif
%if %{build_with_arp}
%{_libdir}/%{name}/arp.so
%{_mandir}/man5/ncollectd-arp.5*
%endif
%if %{build_with_arpcache}
%{_libdir}/%{name}/arpcache.so
%{_mandir}/man5/ncollectd-arpcache.5*
%endif
%if %{build_with_avccache}
%{_libdir}/%{name}/avccache.so
%{_mandir}/man5/ncollectd-avccache.5*
%endif
%if %{build_with_battery}
%{_libdir}/%{name}/battery.so
%{_mandir}/man5/ncollectd-battery.5*
%endif
%if %{build_with_bcache}
%{_libdir}/%{name}/bcache.so
%{_mandir}/man5/ncollectd-bcache.5*
%endif
%if %{build_with_beanstalkd}
%{_libdir}/%{name}/beanstalkd.so
%{_mandir}/man5/ncollectd-beanstalkd.5*
%endif
%if %{build_with_bonding}
%{_libdir}/%{name}/bonding.so
%{_mandir}/man5/ncollectd-bonding.5*
%endif
%if %{build_with_btrfs}
%{_libdir}/%{name}/btrfs.so
%{_mandir}/man5/ncollectd-btrfs.5*
%endif
%if %{build_with_buddyinfo}
%{_libdir}/%{name}/buddyinfo.so
%{_mandir}/man5/ncollectd-buddyinfo.5*
%endif
%if %{build_with_ceph}
%{_libdir}/%{name}/ceph.so
%{_mandir}/man5/ncollectd-ceph.5*
%endif
%if %{build_with_cgroups}
%{_libdir}/%{name}/cgroups.so
%{_mandir}/man5/ncollectd-cgroups.5*
%endif
%if %{build_with_chrony}
%{_libdir}/%{name}/chrony.so
%{_mandir}/man5/ncollectd-chrony.5*
%endif
%if %{build_with_cifs}
%{_libdir}/%{name}/cifs.so
%{_mandir}/man5/ncollectd-cifs.5*
%endif
%if %{build_with_conntrack}
%{_libdir}/%{name}/conntrack.so
%{_mandir}/man5/ncollectd-conntrack.5*
%endif
%if %{build_with_contextswitch}
%{_libdir}/%{name}/contextswitch.so
%{_mandir}/man5/ncollectd-contextswitch.5*
%endif
%if %{build_with_cpu}
%{_libdir}/%{name}/cpu.so
%{_mandir}/man5/ncollectd-cpu.5*
%endif
%if %{build_with_cpufreq}
%{_libdir}/%{name}/cpufreq.so
%{_mandir}/man5/ncollectd-cpufreq.5*
%endif
%if %{build_with_cpusleep}
%{_libdir}/%{name}/cpusleep.so
%{_mandir}/man5/ncollectd-cpusleep.5*
%endif
%if %{build_with_df}
%{_libdir}/%{name}/df.so
%{_mandir}/man5/ncollectd-df.5*
%endif
%if %{build_with_disk}
%{_libdir}/%{name}/disk.so
%{_mandir}/man5/ncollectd-disk.5*
%endif
%if %{build_with_dnsmasq}
%{_libdir}/%{name}/dnsmasq.so
%{_mandir}/man5/ncollectd-dnsmasq.5*
%endif
%if %{build_with_dmi}
%{_libdir}/%{name}/dmi.so
%{_mandir}/man5/ncollectd-dmi.5*
%endif
%if %{build_with_drbd}
%{_libdir}/%{name}/drbd.so
%{_mandir}/man5/ncollectd-drbd.5*
%endif
%if %{build_with_edac}
%{_libdir}/%{name}/edac.so
%{_mandir}/man5/ncollectd-edac.5*
%endif
%if %{build_with_entropy}
%{_libdir}/%{name}/entropy.so
%{_mandir}/man5/ncollectd-entropy.5*
%endif
%if %{build_with_ethstat}
%{_libdir}/%{name}/ethstat.so
%{_mandir}/man5/ncollectd-ethstat.5*
%endif
%if %{build_with_exec}
%{_libdir}/%{name}/exec.so
%{_mandir}/man5/ncollectd-exec.5*
%endif
%if %{build_with_fcgi}
%{_libdir}/%{name}/fcgi.so
%{_mandir}/man5/ncollectd-fcgi.5*
%endif
%if %{build_with_fchost}
%{_libdir}/%{name}/fchost.so
%{_mandir}/man5/ncollectd-fchost.5*
%endif
%if %{build_with_fhcount}
%{_libdir}/%{name}/fhcount.so
%{_mandir}/man5/ncollectd-fhcount.5*
%endif
%if %{build_with_filecount}
%{_libdir}/%{name}/filecount.so
%{_mandir}/man5/ncollectd-filecount.5*
%endif
%if %{build_with_freeradius}
%{_libdir}/%{name}/freeradius.so
%{_mandir}/man5/ncollectd-freeradius.5*
%endif
%if %{build_with_fscache}
%{_libdir}/%{name}/fscache.so
%{_mandir}/man5/ncollectd-fscache.5*
%endif
%if %{build_with_hugepages}
%{_libdir}/%{name}/hugepages.so
%{_mandir}/man5/ncollectd-hugepages.5*
%endif
%if %{build_with_infiniband}
%{_libdir}/%{name}/infiniband.so
%{_mandir}/man5/ncollectd-infiniband.5*
%endif
%if %{build_with_info}
%{_libdir}/%{name}/info.so
%{_mandir}/man5/ncollectd-info.5*
%endif
%if %{build_with_interface}
%{_libdir}/%{name}/interface.so
%{_mandir}/man5/ncollectd-interface.5*
%endif
%if %{build_with_ipc}
%{_libdir}/%{name}/ipc.so
%{_mandir}/man5/ncollectd-ipc.5*
%endif
%if %{build_with_ipstats}
%{_libdir}/%{name}/ipstats.so
%{_mandir}/man5/ncollectd-ipstats.5*
%endif
%if %{build_with_ipvs}
%{_libdir}/%{name}/ipvs.so
%{_mandir}/man5/ncollectd-ipvs.5*
%endif
%if %{build_with_irq}
%{_libdir}/%{name}/irq.so
%{_mandir}/man5/ncollectd-irq.5*
%endif
%if %{build_with_journal}
%{_libdir}/%{name}/journal.so
%{_mandir}/man5/ncollectd-journal.5*
%endif
%if %{build_with_kea}
%{_libdir}/%{name}/kea.so
%{_mandir}/man5/ncollectd-kea.5*
%endif
%if %{build_with_keepalived}
%{_libdir}/%{name}/keepalived.so
%{_mandir}/man5/ncollectd-keepalived.5*
%endif
%if %{build_with_ksm}
%{_libdir}/%{name}/ksm.so
%{_mandir}/man5/ncollectd-ksm.5*
%endif
%if %{build_with_load}
%{_libdir}/%{name}/load.so
%{_mandir}/man5/ncollectd-load.5*
%endif
%if %{build_with_locks}
%{_libdir}/%{name}/locks.so
%{_mandir}/man5/ncollectd-locks.5*
%endif
%if %{build_with_logind}
%{_libdir}/%{name}/logind.so
%{_mandir}/man5/ncollectd-logind.5*
%endif
%if %{build_with_log_file}
%{_libdir}/%{name}/log_file.so
%{_mandir}/man5/ncollectd-log_file.5*
%endif
%if %{build_with_log_gelf}
%{_libdir}/%{name}/log_gelf.so
%{_mandir}/man5/ncollectd-log_gelf.5*
%endif
%if %{build_with_log_syslog}
%{_libdir}/%{name}/log_syslog.so
%{_mandir}/man5/ncollectd-log_syslog.5*
%endif
%if %{build_with_log_systemd}
%{_libdir}/%{name}/log_systemd.so
%{_mandir}/man5/ncollectd-log_systemd.5*
%endif
%if %{build_with_lpar}
%{_libdir}/%{name}/lpar.so
%{_mandir}/man5/ncollectd-lpar.5*
%endif
%if %{build_with_lvm}
%{_libdir}/%{name}/lvm.so
%{_mandir}/man5/ncollectd-lvm.5*
%endif
%if %{build_with_match_csv}
%{_libdir}/%{name}/match_csv.so
%{_mandir}/man5/ncollectd-match_csv.5*
%endif
%if %{build_with_match_jsonpath}
%{_libdir}/%{name}/match_jsonpath.so
%{_mandir}/man5/ncollectd-match_jsonpath.5*
%endif
%if %{build_with_match_regex}
%{_libdir}/%{name}/match_regex.so
%{_mandir}/man5/ncollectd-match_regex.5*
%endif
%if %{build_with_match_table}
%{_libdir}/%{name}/match_table.so
%{_mandir}/man5/ncollectd-match_table.5*
%endif
%if %{build_with_md}
%{_libdir}/%{name}/md.so
%{_mandir}/man5/ncollectd-md.5*
%endif
%if %{build_with_memcached}
%{_libdir}/%{name}/memcached.so
%{_mandir}/man5/ncollectd-memcached.5*
%endif
%if %{build_with_meminfo}
%{_libdir}/%{name}/meminfo.so
%{_mandir}/man5/ncollectd-meminfo.5*
%endif
%if %{build_with_memory}
%{_libdir}/%{name}/memory.so
%{_mandir}/man5/ncollectd-memory.5*
%endif
%if %{build_with_mmc}
%{_libdir}/%{name}/mmc.so
%{_mandir}/man5/ncollectd-mmc.5*
%endif
%if %{build_with_netstat_udp}
%{_libdir}/%{name}/netstat_udp.so
%{_mandir}/man5/ncollectd-netstat_udp.5*
%endif
%if %{build_with_nagios_check}
%{_libdir}/%{name}/nagios_check.so
%{_mandir}/man5/ncollectd-nagios_check.5*
%endif
%if %{build_with_nfacct}
%{_libdir}/%{name}/nfacct.so
%{_mandir}/man5/ncollectd-nfacct.5*
%endif
%if %{build_with_nfconntrack}
%{_libdir}/%{name}/nfconntrack.so
%{_mandir}/man5/ncollectd-nfconntrack.5*
%endif
%if %{build_with_nfs}
%{_libdir}/%{name}/nfs.so
%{_mandir}/man5/ncollectd-nfs.5*
%endif
%if %{build_with_nfsd}
%{_libdir}/%{name}/nfsd.so
%{_mandir}/man5/ncollectd-nfsd.5*
%endif
%if %{build_with_notify_exec}
%{_libdir}/%{name}/notify_exec.so
%{_mandir}/man5/ncollectd-notify_exec.5*
%endif
%if %{build_with_notify_nagios}
%{_libdir}/%{name}/notify_nagios.so
%{_mandir}/man5/ncollectd-notify_nagios.5*
%endif
%if %{build_with_ntpd}
%{_libdir}/%{name}/ntpd.so
%{_mandir}/man5/ncollectd-ntpd.5*
%endif
%if %{build_with_numa}
%{_libdir}/%{name}/numa.so
%{_mandir}/man5/ncollectd-numa.5*
%endif
%if %{build_with_olsrd}
%{_libdir}/%{name}/olsrd.so
%{_mandir}/man5/ncollectd-olsrd.5*
%endif
%if %{build_with_openvpn}
%{_libdir}/%{name}/openvpn.so
%{_mandir}/man5/ncollectd-openvpn.5*
%endif
%if %{build_with_pf}
%{_libdir}/%{name}/pf.so
%{_mandir}/man5/ncollectd-pf.5*
%endif
%if %{build_with_ping}
%{_libdir}/%{name}/ping.so
%{_mandir}/man5/ncollectd-ping.5*
%endif
%if %{build_with_pdns}
%{_libdir}/%{name}/pdns.so
%{_mandir}/man5/ncollectd-pdns.5*
%endif
%if %{build_with_postfix}
%{_libdir}/%{name}/postfix.so
%{_mandir}/man5/ncollectd-postfix.5*
%endif
%if %{build_with_pressure}
%{_libdir}/%{name}/pressure.so
%{_mandir}/man5/ncollectd-pressure.5*
%endif
%if %{build_with_processes}
%{_libdir}/%{name}/processes.so
%{_mandir}/man5/ncollectd-processes.5*
%endif
%if %{build_with_protocols}
%{_libdir}/%{name}/protocols.so
%{_mandir}/man5/ncollectd-protocols.5*
%endif
%if %{build_with_quota}
%{_libdir}/%{name}/quota.so
%{_mandir}/man5/ncollectd-quota.5*
%endif
%if %{build_with_rapl}
%{_libdir}/%{name}/rapl.so
%{_mandir}/man5/ncollectd-rapl.5*
%endif
%if %{build_with_recursor}
%{_libdir}/%{name}/recursor.so
%{_mandir}/man5/ncollectd-recursor.5*
%endif
%if %{build_with_resctrl}
%{_libdir}/%{name}/resctrl.so
%{_mandir}/man5/ncollectd-resctrl.5*
%endif
%if %{build_with_routeros}
%{_libdir}/%{name}/routeros.so
%{_mandir}/man5/ncollectd-routeros.5*
%endif
%if %{build_with_rtcache}
%{_libdir}/%{name}/rtcache.so
%{_mandir}/man5/ncollectd-rtcache.5*
%endif
%if %{build_with_schedstat}
%{_libdir}/%{name}/schedstat.so
%{_mandir}/man5/ncollectd-schedstat.5*
%endif
%if %{build_with_sendmail}
%{_libdir}/%{name}/sendmail.so
%{_mandir}/man5/ncollectd-sendmail.5*
%endif
%if %{build_with_serial}
%{_libdir}/%{name}/serial.so
%{_mandir}/man5/ncollectd-serial.5*
%endif
%if %{build_with_slabinfo}
%{_libdir}/%{name}/slabinfo.so
%{_mandir}/man5/ncollectd-slabinfo.5*
%endif
%if %{build_with_sockstat}
%{_libdir}/%{name}/sockstat.so
%{_mandir}/man5/ncollectd-sockstat.5*
%endif
%if %{build_with_softirq}
%{_libdir}/%{name}/softirq.so
%{_mandir}/man5/ncollectd-softirq.5*
%endif
%if %{build_with_softnet}
%{_libdir}/%{name}/softnet.so
%{_mandir}/man5/ncollectd-softnet.5*
%endif
%if %{build_with_statsd}
%{_libdir}/%{name}/statsd.so
%{_mandir}/man5/ncollectd-statsd.5*
%endif
%if %{build_with_swap}
%{_libdir}/%{name}/swap.so
%{_mandir}/man5/ncollectd-swap.5*
%endif
%if %{build_with_synproxy}
%{_libdir}/%{name}/synproxy.so
%{_mandir}/man5/ncollectd-synproxy.5*
%endif
%if %{build_with_systemd}
%{_libdir}/%{name}/systemd.so
%{_mandir}/man5/ncollectd-systemd.5*
%endif
%if %{build_with_table}
%{_libdir}/%{name}/table.so
%{_mandir}/man5/ncollectd-table.5*
%endif
%if %{build_with_tail}
%{_libdir}/%{name}/tail.so
%{_mandir}/man5/ncollectd-tail.5*
%endif
%if %{build_with_tape}
%{_libdir}/%{name}/tape.so
%{_mandir}/man5/ncollectd-tape.5*
%endif
%if %{build_with_tc}
%{_libdir}/%{name}/tc.so
%{_mandir}/man5/ncollectd-tc.5*
%endif
%if %{build_with_tcpconns}
%{_libdir}/%{name}/tcpconns.so
%{_mandir}/man5/ncollectd-tcpconns.5*
%endif
%if %{build_with_timex}
%{_libdir}/%{name}/timex.so
%{_mandir}/man5/ncollectd-timex.5*
%endif
%if %{build_with_thermal}
%{_libdir}/%{name}/thermal.so
%{_mandir}/man5/ncollectd-thermal.5*
%endif
%if %{build_with_turbostat}
%{_libdir}/%{name}/turbostat.so
%{_mandir}/man5/ncollectd-turbostat.5*
%endif
%if %{build_with_ubi}
%{_libdir}/%{name}/ubi.so
%{_mandir}/man5/ncollectd-ubi.5*
%endif
%if %{build_with_uname}
%{_libdir}/%{name}/uname.so
%{_mandir}/man5/ncollectd-uname.5*
%endif
%if %{build_with_uptime}
%{_libdir}/%{name}/uptime.so
%{_mandir}/man5/ncollectd-uptime.5*
%endif
%if %{build_with_users}
%{_libdir}/%{name}/users.so
%{_mandir}/man5/ncollectd-users.5*
%endif
%if %{build_with_uuid}
%{_libdir}/%{name}/uuid.so
%{_mandir}/man5/ncollectd-uuid.5*
%endif
%if %{build_with_vmem}
%{_libdir}/%{name}/vmem.so
%{_mandir}/man5/ncollectd-vmem.5*
%endif
%if %{build_with_wireguard}
%{_libdir}/%{name}/wireguard.so
%{_mandir}/man5/ncollectd-wireguard.5*
%endif
%if %{build_with_wireless}
%{_libdir}/%{name}/wireless.so
%{_mandir}/man5/ncollectd-wireless.5*
%endif
%if %{build_with_write_file}
%{_libdir}/%{name}/write_file.so
%{_mandir}/man5/ncollectd-write_file.5*
%endif
%if %{build_with_write_log}
%{_libdir}/%{name}/write_log.so
%{_mandir}/man5/ncollectd-write_log.5*
%endif
%if %{build_with_write_tcp}
%{_libdir}/%{name}/write_tcp.so
%{_mandir}/man5/ncollectd-write_tcp.5*
%endif
%if %{build_with_write_udp}
%{_libdir}/%{name}/write_udp.so
%{_mandir}/man5/ncollectd-write_udp.5*
%endif
%if %{build_with_xfrm}
%{_libdir}/%{name}/xfrm.so
%{_mandir}/man5/ncollectd-xfrm.5*
%endif
%if %{build_with_xfs}
%{_libdir}/%{name}/xfs.so
%{_mandir}/man5/ncollectd-xfs.5*
%endif
%if %{build_with_zfs}
%{_libdir}/%{name}/zfs.so
%{_mandir}/man5/ncollectd-zfs.5*
%endif
%if %{build_with_zone}
%{_libdir}/%{name}/zone.so
%{_mandir}/man5/ncollectd-zone.5*
%endif
%if %{build_with_zoneinfo}
%{_libdir}/%{name}/zoneinfo.so
%{_mandir}/man5/ncollectd-zoneinfo.5*
%endif
%if %{build_with_zookeeper}
%{_libdir}/%{name}/zookeeper.so
%{_mandir}/man5/ncollectd-zookeeper.5*
%endif
%if %{build_with_zram}
%{_libdir}/%{name}/zram.so
%{_mandir}/man5/ncollectd-zram.5*
%endif
%if %{build_with_zswap}
%{_libdir}/%{name}/zswap.so
%{_mandir}/man5/ncollectd-zswap.5*
%endif


%if %{build_with_apache}
%files apache
%{_libdir}/%{name}/apache.so
%{_mandir}/man5/ncollectd-apache.5*
%endif

%if %{build_with_ats}
%files ats
%{_libdir}/%{name}/ats.so
%{_mandir}/man5/ncollectd-ats.5*
%endif

%if %{build_with_bind}
%files bind
%{_libdir}/%{name}/bind.so
%{_mandir}/man5/ncollectd-bind.5*
%endif

%if %{build_with_cert}
%files cert
%{_libdir}/%{name}/cert.so
%{_mandir}/man5/ncollectd-cert.5*
%endif

%if %{build_with_cups}
%files cups
%{_libdir}/%{name}/cups.so
%{_mandir}/man5/ncollectd-cups.5*
%endif

%if %{build_with_db2}
%files db2
%{_libdir}/%{name}/db2.so
%{_mandir}/man5/ncollectd-db2.5*
%endif

%if %{build_with_dcpmm}
%files dcpmm
%{_libdir}/%{name}/dcpmm.so
%{_mandir}/man5/ncollectd-dcpmm.5*
%endif

%if %{build_with_dns}
%files dns
%{_libdir}/%{name}/dns.so
%{_mandir}/man5/ncollectd-dns.5*
%endif

%if %{build_with_docker}
%files docker
%{_libdir}/%{name}/docker.so
%{_mandir}/man5/ncollectd-docker.5*
%endif

%if %{build_with_ds389}
%files ds389
%{_libdir}/%{name}/ds389.so
%{_mandir}/man5/ncollectd-ds389.5*
%endif

%if %{build_with_gps}
%files gps
%{_libdir}/%{name}/gps.so
%{_mandir}/man5/ncollectd-gps.5*
%endif

%if %{build_with_gpu_intel}
%files gpu_intel
%{_libdir}/%{name}/gpu_intel.so
%{_mandir}/man5/ncollectd-gpu_intel.5*
%endif

%if %{build_with_gpu_nvidia}
%files gpu_nvidia
%{_libdir}/%{name}/gpu_nvidia.so
%{_mandir}/man5/ncollectd-gpu_nvidia.5*
%endif

%if %{build_with_haproxy}
%files haproxy
%{_libdir}/%{name}/haproxy.so
%{_mandir}/man5/ncollectd-haproxy.5*
%endif

%if %{build_with_http}
%files http
%{_libdir}/%{name}/http.so
%{_mandir}/man5/ncollectd-http.5*
%endif

%if %{build_with_intel_rdt}
%files intel_rdt
%{_libdir}/%{name}/intel_rdt.so
%{_mandir}/man5/ncollectd-intel_rdt.5*
%endif

%if %{build_with_ipmi}
%files ipmi
%{_libdir}/%{name}/ipmi.so
%{_mandir}/man5/ncollectd-ipmi.5*
%endif

%if %{build_with_iptables}
%files iptables
%{_libdir}/%{name}/iptables.so
%{_mandir}/man5/ncollectd-iptables.5*
%endif

%if %{build_with_java}
%files java
%{_libdir}/%{name}/java.so
%{_datadir}/ncollectd/ncollectd-api.jar
%{_datadir}/ncollectd/generic-jmx.jar
%{_mandir}/man5/ncollectd-java.5*
%endif

%if %{build_with_jolokia}
%files jolokia
%{_libdir}/%{name}/jolokia.so
%{_mandir}/man5/ncollectd-jolokia.5*
%endif

%if %{build_with_kafka}
%files kafka
%{_libdir}/%{name}/kafka.so
%{_mandir}/man5/ncollectd-kafka.5*
%endif

%if %{build_with_ldap}
%files ldap
%{_libdir}/%{name}/ldap.so
%{_mandir}/man5/ncollectd-ldap.5*
%endif

%if %{build_with_lua}
%files lua
%{_libdir}/%{name}/lua.so
%{_mandir}/man5/ncollectd-lua.5*
%endif

%if %{build_with_match_xpath}
%files match_xpath
%{_libdir}/%{name}/match_xpath.so
%{_mandir}/man5/ncollectd-match_xpath.5*
%endif

%if %{build_with_modbus}
%files modbus
%{_libdir}/%{name}/modbus.so
%{_mandir}/man5/ncollectd-modbus.5*
%endif

%if %{build_with_mongodb}
%files mongodb
%{_libdir}/%{name}/mongodb.so
%{_mandir}/man5/ncollectd-mongodb.5*
%endif

%if %{build_with_mosquitto}
%files mosquitto
%{_libdir}/%{name}/mosquitto.so
%{_mandir}/man5/ncollectd-mosquitto.5*
%endif

%if %{build_with_mq}
%files mq
%{_libdir}/%{name}/mq.so
%{_mandir}/man5/ncollectd-mq.5*
%endif

%if %{build_with_mssql}
%files mssql
%{_libdir}/%{name}/mssql.so
%{_mandir}/man5/ncollectd-mssql.5*
%endif

%if %{build_with_mysql}
%files mysql
%{_libdir}/%{name}/mysql.so
%{_mandir}/man5/ncollectd-mysql.5*
%endif

%if %{build_with_nftables}
%files nftables
%{_libdir}/%{name}/nftables.so
%{_mandir}/man5/ncollectd-nftables.5*
%endif

%if %{build_with_nginx}
%files nginx
%{_libdir}/%{name}/nginx.so
%{_mandir}/man5/ncollectd-nginx.5*
%endif

%if %{build_with_nginx_vts}
%files nginx_vts
%{_libdir}/%{name}/nginx_vts.so
%{_mandir}/man5/ncollectd-nginx_vts.5*
%endif

%if %{build_with_notify_email}
%files notify_email
%{_libdir}/%{name}/notify_email.so
%{_mandir}/man5/ncollectd-notify_email.5*
%endif

%if %{build_with_notify_snmp}
%files notify_snmp
%{_libdir}/%{name}/notify_snmp.so
%{_mandir}/man5/ncollectd-notify_snmp.5*
%endif

%if %{build_with_nsd}
%files nsd
%{_libdir}/%{name}/nsd.so
%{_mandir}/man5/ncollectd-nsd.5*
%endif

%if %{build_with_nut}
%files nut
%{_libdir}/%{name}/nut.so
%{_mandir}/man5/ncollectd-nut.5*
%endif

%if %{build_with_odbc}
%files odbc
%{_libdir}/%{name}/odbc.so
%{_mandir}/man5/ncollectd-odbc.5*
%endif

%if %{build_with_openldap}
%files openldap
%{_libdir}/%{name}/openldap.so
%{_mandir}/man5/ncollectd-openldap.5*
%endif

%if %{build_with_oracle}
%files oracle
%{_libdir}/%{name}/oracle.so
%{_mandir}/man5/ncollectd-oracle.5*
%endif

%if %{build_with_pcap}
%files pcap
%{_libdir}/%{name}/pcap.so
%{_mandir}/man5/ncollectd-pcap.5*
%endif

%if %{build_with_perl}
%files perl
%{_libdir}/%{name}/perl.so
%{_mandir}/man5/ncollectd-perl.5*
%endif

%if %{build_with_pgbouncer}
%files pgbouncer
%{_libdir}/%{name}/pgbouncer.so
%{_mandir}/man5/ncollectd-pgbouncer.5*
%endif

%if %{build_with_podman}
%files podman
%{_libdir}/%{name}/podman.so
%{_mandir}/man5/ncollectd-podman.5*
%endif

%if %{build_with_postgresql}
%files postgresql
%{_libdir}/%{name}/postgresql.so
%{_mandir}/man5/ncollectd-postgresql.5*
%endif

%if %{build_with_proxysql}
%files proxysql
%{_libdir}/%{name}/proxysql.so
%{_mandir}/man5/ncollectd-proxysql.5*
%endif

%if %{build_with_python}
%files python
%{_libdir}/%{name}/python.so
%{_mandir}/man5/ncollectd-python.5*
%endif

%if %{build_with_redis}
%files redis
%{_libdir}/%{name}/redis.so
%{_mandir}/man5/ncollectd-redis.5*
%endif

%if %{build_with_scraper}
%files scraper
%{_libdir}/%{name}/scraper.so
%{_mandir}/man5/ncollectd-scraper.5*
%endif

%if %{build_with_sensors}
%files sensors
%{_libdir}/%{name}/sensors.so
%{_mandir}/man5/ncollectd-sensors.5*
%endif

%if %{build_with_sigrok}
%files sigrok
%{_libdir}/%{name}/sigrok.so
%{_mandir}/man5/ncollectd-sigrok.5*
%endif

%if %{build_with_slurm}
%files slurm
%{_libdir}/%{name}/slurm.so
%{_mandir}/man5/ncollectd-slurm.5*
%endif

%if %{build_with_smart}
%files smart
%{_libdir}/%{name}/smart.so
%{_mandir}/man5/ncollectd-smart.5*
%endif

%if %{build_with_snmp}
%files snmp
%{_libdir}/%{name}/snmp.so
%{_mandir}/man5/ncollectd-snmp.5*
%endif

%if %{build_with_squid}
%files squid
%{_libdir}/%{name}/squid.so
%{_mandir}/man5/ncollectd-squid.5*
%endif

%if %{build_with_unbound}
%files unbound
%{_libdir}/%{name}/unbound.so
%{_mandir}/man5/ncollectd-unbound.5*
%endif

%if %{build_with_varnish}
%files varnish
%{_libdir}/%{name}/varnish.so
%{_mandir}/man5/ncollectd-varnish.5*
%endif

%if %{build_with_virt}
%files virt
%{_libdir}/%{name}/virt.so
%{_mandir}/man5/ncollectd-virt.5*
%endif

%if %{build_with_write_amqp}
%files write_amqp
%{_libdir}/%{name}/write_amqp.so
%{_mandir}/man5/ncollectd-write_amqp.5*
%endif

%if %{build_with_write_amqp1}
%files write_amqp1
%{_libdir}/%{name}/write_amqp1.so
%{_mandir}/man5/ncollectd-write_amqp1.5*
%endif

%if %{build_with_write_exporter}
%files write_exporter
%{_libdir}/%{name}/write_exporter.so
%{_mandir}/man5/ncollectd-write_exporter.5*
%endif

%if %{build_with_write_http}
%files write_http
%{_libdir}/%{name}/write_http.so
%{_mandir}/man5/ncollectd-write_http.5*
%endif

%if %{build_with_write_kafka}
%files write_kafka
%{_libdir}/%{name}/write_kafka.so
%{_mandir}/man5/ncollectd-write_kafka.5*
%endif

%if %{build_with_write_mongodb}
%files write_mongodb
%{_libdir}/%{name}/write_mongodb.so
%{_mandir}/man5/ncollectd-write_mongodb.5*
%endif

%if %{build_with_write_mqtt}
%files write_mqtt
%{_libdir}/%{name}/write_mqtt.so
%{_mandir}/man5/ncollectd-write_mqtt.5*
%endif

%if %{build_with_write_postgresql}
%files write_postgresql
%{_libdir}/%{name}/write_postgresql.so
%{_mandir}/man5/ncollectd-write_postgresql.5*
%endif

%if %{build_with_write_redis}
%files write_redis
%{_libdir}/%{name}/write_redis.so
%{_mandir}/man5/ncollectd-write_redis.5*
%endif

%if %{build_with_xencpu}
%files xencpu
%{_libdir}/%{name}/xencpu.so
%{_mandir}/man5/ncollectd-xencpu.5*
%endif

%changelog
* Mon May 01 2023 ncollectd <info@ncollectd.org> - 0.0.1-1
- Initial release 0.0.1
