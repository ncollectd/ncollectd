%global _hardened_build 1
%global __requires_exclude ^.*(libdb2|libmqic_r|libmqm_r|libclntsh)[.](so|a).*$

%if 0%{?bundle_version:1}
%define _bundle_version %{bundle_version}
%else
%define _bundle_version 0.0.0+git
%endif

%define _pkg_version %(echo %{_bundle_version} | sed -E 's/-rc/~rc/;s/\\+([0-9]+)\\.g([a-f0-9]+)$/^\\1.git\\2/')

%define build_with_apache 0%{!?_without_apache:1}
%define build_with_apcups 0%{!?_without_apcups:1}
%define build_with_ats 0%{!?_without_ats:1}
%define build_with_beanstalkd 0%{!?_without_beanstalkd:1}
%define build_with_bind 0%{!?_without_bind:1}
%define build_with_contextswitch 0%{!?_without_contextswitch:1}
%define build_with_cpu 0%{!?_without_cpu:1}
%define build_with_df 0%{!?_without_df:1}
%define build_with_disk 0%{!?_without_disk:1}
%define build_with_dnsmasq 0%{!?_without_dnsmasq:1}
%define build_with_exec 0%{!?_without_exec:1}
%define build_with_fcgi 0%{!?_without_fcgi:1}
%define build_with_freeradius 0%{!?_without_freeradius:1}
%define build_with_filecount 0%{!?_without_filecount:1}
%define build_with_haproxy 0%{!?_without_haproxy:1}
%define build_with_hba 0%{!?_without_hba:1}
%define build_with_http 0%{!?_without_http:1}
%define build_with_info 0%{!?_without_info:1}
%define build_with_interface 0%{!?_without_interface:1}
%define build_with_ipc 0%{!?_without_ipc:1}
%define build_with_jolokia 0%{!?_without_jolokia:1}
%define build_with_kea 0%{!?_without_kea:1}
%define build_with_keepalived 0%{!?_without_keepalived:1}
%define build_with_load 0%{!?_without_load:1}
%define build_with_log_file 0%{!?_without_log_file:1}
%define build_with_log_gelf 0%{!?_without_log_gelf:1}
%define build_with_log_syslog 0%{!?_without_log_syslog:1}
%define build_with_lpar 0%{!?_without_lpar:1}
%define build_with_lua 0%{!?_without_lua:1}
%define build_with_match_csv 0%{!?_without_match_csv:1}
%define build_with_match_jsonpath 0%{!?_without_match_jsonpath:1}
%define build_with_match_regex 0%{!?_without_match_regex:1}
%define build_with_match_table 0%{!?_without_match_table:1}
%define build_with_match_xpath 0%{!?_without_match_xpath:1}
%define build_with_memcached 0%{!?_without_memcached:1}
%define build_with_memory 0%{!?_without_memory:1}
%define build_with_mongodb 0%{!?_without_mongodb:1}
%define build_with_mysql 0%{!?_without_mysql:1}
%define build_with_nagios_check 0%{!?_without_nagios_check:1}
%define build_with_nginx 0%{!?_without_nginx:1}
%define build_with_nginx_vts 0%{!?_without_nginx_vts:1}
%define build_with_notify_exec 0%{!?_without_notify_exec:1}
%define build_with_notify_nagios 0%{!?_without_notify_nagios:1}
%define build_with_ntpd 0%{!?_without_ntpd:1}
%define build_with_odbc 0%{!?_without_odbc:1}
%define build_with_olsrd 0%{!?_without_olsrd:1}
%define build_with_openvpn 0%{!?_without_openvpn:1}
%define build_with_pdns 0%{!?_without_pdns:1}
%define build_with_ping 0%{!?_without_ping:1}
%define build_with_pgbouncer 0%{!?_without_pgbouncer:1}
%define build_with_pgpool 0%{!?_without_pgpool:1}
%define build_with_postgresql 0%{!?_without_postgresql:1}
%define build_with_postfix 0%{!?_without_postfix:1}
%define build_with_processes 0%{!?_without_processes:1}
%define build_with_proxysql 0%{!?_without_proxysql:1}
%define build_with_python 0%{!?_without_python:1}
%define build_with_recursor 0%{!?_without_recursor:1}
%define build_with_routeros 0%{!?_without_routeros:1}
%define build_with_scraper 0%{!?_without_scraper:1}
%define build_with_sendmail 0%{!?_without_sendmail:1}
%define build_with_squid 0%{!?_without_squid:1}
%define build_with_statsd 0%{!?_without_statsd:1}
%define build_with_swap 0%{!?_without_swap:1}
%define build_with_table 0%{!?_without_table:1}
%define build_with_tail 0%{!?_without_tail:1}
%define build_with_tape 0%{!?_without_tape:1}
%define build_with_tcpconns 0%{!?_without_tcpconns:1}
%define build_with_uname 0%{!?_without_uname:1}
%define build_with_uptime 0%{!?_without_uptime:1}
%define build_with_users 0%{!?_without_users:1}
%define build_with_uuid 0%{!?_without_uuid:1}
%define build_with_wlm 0%{!?_without_wlm:1}
%define build_with_wpar 0%{!?_without_wpar:1}
%define build_with_write_file 0%{!?_without_write_file:1}
%define build_with_write_http 0%{!?_without_write_http:1}
%define build_with_write_log 0%{!?_without_write_log:1}
%define build_with_write_mongodb 0%{!?_without_write_mongodb:1}
%define build_with_write_postgresql 0%{!?_without_write_postgresql:1}
%define build_with_write_tcp 0%{!?_without_write_tcp:1}
%define build_with_write_udp 0%{!?_without_write_udp:1}
%define build_with_zookeeper 0%{!?_without_zookeeper:1}
%define build_with_notify_snmp 0%{!?_without_notify_snmp:1}
%define build_with_snmp 0%{!?_without_snmp:1}

%define build_with_db2 0%{?_with_db2:1}
%define build_with_mq 0%{?_with_mq:1}

Summary: Statistics collection and monitoring daemon
Name: ncollectd
Version: %{_pkg_version}
Release: 1%{?dist}
URL: https://ncollectd.org
Source: https://ncollectd.org/files/%{name}-%{_bundle_version}.tar.xz
License: GPLv2
Group: System Environment/Daemons
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildRequires: tar, xz
BuildRequires: gcc, gcc-cpp, cmake, bison, flex, gperf
Vendor: ncollectd development team

Source1: init.d-ncollectd

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

%if %{build_with_db2}
%package db2
Summary: DB2 plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
%description db2
The db2 plugin retrieves information from a DB2 database,
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

%if %{build_with_mq}
%package mq
Summary: MQ plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
%description mq
The mq plugin get metrics from a MQ broker.
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

%if %{build_with_mysql}
%package mysql
Summary: MySQL plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: mariadb-connector-c-devel
%description mysql
MySQL querying plugin. This plugin provides data of issued commands, called
handlers and database traffic.
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

%if %{build_with_notify_snmp}
%package notify_snmp
Summary: Notify snmp plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: net-snmp-devel
%description notify_snmp
The notify smp plugin send notification as SNMP traps.
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

%if %{build_with_pgbouncer}
%package pgbouncer
Summary: PgBouncer plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: postgresql-devel
%description pgbouncer
The pgbouncer plugin collects metrics from a PgBouncer instance.
%endif

%if %{build_with_pgpool}
%package pgpool
Summary: Pgpool-II plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: postgresql-devel
%description pgpool
The pgpool plugin collects metrics from a Pgpool-II instance.
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
BuildRequires: mariadb-connector-c-devel
%description proxysql
The ProxySQL plugin collect metrics from the MySQL proxy: ProxySQL.
%endif

%if %{build_with_python}
%package python
Summary: Python plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: python-devel
%description python
The Python plugin embeds a Python interpreter into collectd and exposes the
application programming interface (API) to Python-scripts.
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

%if %{build_with_write_mongodb}
%package write_mongodb
Summary: Write mongodb plugin for ncollectd
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}
BuildRequires: mongo-c-driver-devel
%description write_mongodb
The write_mongodb plugin write values to a mongodb database.
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

%prep
%setup -q -n %{name}-%{_bundle_version}

%build

%if %{build_with_apache}
%define _build_with_apache -DPLUGIN_APACHE:BOOL=ON
%else
%define _build_with_apache -DPLUGIN_APACHE:BOOL==OFF
%endif

%if %{build_with_apcups}
%define _build_with_apcups -DPLUGIN_APCUPS:BOOL=ON
%else
%define _build_with_apcups -DPLUGIN_APCUPS:BOOL==OFF
%endif

%if %{build_with_ats}
%define _build_with_ats -DPLUGIN_ATS:BOOL=ON
%else
%define _build_with_ats -DPLUGIN_ATS:BOOL==OFF
%endif

%if %{build_with_beanstalkd}
%define _build_with_beanstalkd -DPLUGIN_BEANSTALKD:BOOL=ON
%else
%define _build_with_beanstalkd -DPLUGIN_BEANSTALKD:BOOL=OFF
%endif

%if %{build_with_bind}
%define _build_with_bind -DPLUGIN_BIND:BOOL=ON
%else
%define _build_with_bind -DPLUGIN_BIND:BOOL==OFF
%endif

%if %{build_with_contextswitch}
%define _build_with_contextswitch -DPLUGIN_CONTEXTSWITCH:BOOL=ON
%else
%define _build_with_contextswitch -DPLUGIN_CONTEXTSWITCH:BOOL==OFF
%endif

%if %{build_with_cpu}
%define _build_with_cpu -DPLUGIN_CPU:BOOL=ON
%else
%define _build_with_cpu -DPLUGIN_CPU:BOOL==OFF
%endif

%if %{build_with_df}
%define _build_with_df -DPLUGIN_DF:BOOL=ON
%else
%define _build_with_df -DPLUGIN_DF:BOOL==OFF
%endif

%if %{build_with_disk}
%define _build_with_disk -DPLUGIN_DISK:BOOL=ON
%else
%define _build_with_disk -DPLUGIN_DISK:BOOL==OFF
%endif

%if %{build_with_dnsmasq}
%define _build_with_dnsmasq -DPLUGIN_DNSMASQ:BOOL=ON
%else
%define _build_with_dnsmasq -DPLUGIN_DNSMASQ:BOOL==OFF
%endif

%if %{build_with_exec}
%define _build_with_exec -DPLUGIN_EXEC:BOOL=ON
%else
%define _build_with_exec -DPLUGIN_EXEC:BOOL==OFF
%endif

%if %{build_with_freeradius}
%define _build_with_freeradius -DPLUGIN_FREERADIUS:BOOL=ON
%else
%define _build_with_freeradius -DPLUGIN_FREERADIUS:BOOL=OFF
%endif

%if %{build_with_fcgi}
%define _build_with_fcgi -DPLUGIN_FCGI:BOOL=ON
%else
%define _build_with_fcgi -DPLUGIN_FCGI:BOOL==OFF
%endif

%if %{build_with_filecount}
%define _build_with_filecount -DPLUGIN_FILECOUNT:BOOL=ON
%else
%define _build_with_filecount -DPLUGIN_FILECOUNT:BOOL==OFF
%endif

%if %{build_with_haproxy}
%define _build_with_haproxy -DPLUGIN_HAPROXY:BOOL=ON
%else
%define _build_with_haproxy -DPLUGIN_HAPROXY:BOOL==OFF
%endif

%if %{build_with_hba}
%define _build_with_hba -DPLUGIN_HBA:BOOL=ON
%else
%define _build_with_hba -DPLUGIN_HBA:BOOL==OFF
%endif

%if %{build_with_http}
%define _build_with_http -DPLUGIN_HTTP:BOOL=ON
%else
%define _build_with_http -DPLUGIN_HTTP:BOOL==OFF
%endif

%if %{build_with_info}
%define _build_with_info -DPLUGIN_INFO:BOOL=ON
%else
%define _build_with_info -DPLUGIN_INFO:BOOL==OFF
%endif

%if %{build_with_interface}
%define _build_with_interface -DPLUGIN_INTERFACE:BOOL=ON
%else
%define _build_with_interface -DPLUGIN_INTERFACE:BOOL==OFF
%endif

%if %{build_with_ipc}
%define _build_with_ipc -DPLUGIN_IPC:BOOL=ON
%else
%define _build_with_ipc -DPLUGIN_IPC:BOOL==OFF
%endif

%if %{build_with_jolokia}
%define _build_with_jolokia -DPLUGIN_JOLOKIA:BOOL=ON
%else
%define _build_with_jolokia -DPLUGIN_JOLOKIA:BOOL==OFF
%endif

%if %{build_with_kea}
%define _build_with_kea -DPLUGIN_KEA:BOOL=ON
%else
%define _build_with_kea -DPLUGIN_KEA:BOOL==OFF
%endif

%if %{build_with_keepalived}
%define _build_with_keepalived -DPLUGIN_KEEPALIVED:BOOL=ON
%else
%define _build_with_keepalived -DPLUGIN_KEEPALIVED:BOOL==OFF
%endif

%if %{build_with_load}
%define _build_with_load -DPLUGIN_LOAD:BOOL=ON
%else
%define _build_with_load -DPLUGIN_LOAD:BOOL==OFF
%endif

%if %{build_with_log_file}
%define _build_with_log_file -DPLUGIN_LOG_FILE:BOOL=ON
%else
%define _build_with_log_file -DPLUGIN_LOG_FILE:BOOL==OFF
%endif

%if %{build_with_log_gelf}
%define _build_with_log_gelf -DPLUGIN_LOG_GELF:BOOL=ON
%else
%define _build_with_log_gelf -DPLUGIN_LOG_GELF:BOOL==OFF
%endif

%if %{build_with_log_syslog}
%define _build_with_log_syslog -DPLUGIN_LOG_SYSLOG:BOOL=ON
%else
%define _build_with_log_syslog -DPLUGIN_LOG_SYSLOG:BOOL==OFF
%endif

%if %{build_with_lpar}
%define _build_with_lpar -DPLUGIN_LPAR:BOOL=ON
%else
%define _build_with_lpar -DPLUGIN_LPAR:BOOL==OFF
%endif

%if %{build_with_lua}
%define _build_with_lua -DPLUGIN_LUA:BOOL=ON
%else
%define _build_with_lua -DPLUGIN_LUA:BOOL==OFF
%endif

%if %{build_with_match_csv}
%define _build_with_match_csv -DPLUGIN_MATCH_CSV:BOOL=ON
%else
%define _build_with_match_csv -DPLUGIN_MATCH_CSV:BOOL==OFF
%endif

%if %{build_with_match_jsonpath}
%define _build_with_match_jsonpath -DPLUGIN_MATCH_JSONPATH:BOOL=ON
%else
%define _build_with_match_jsonpath -DPLUGIN_MATCH_JSONPATH:BOOL==OFF
%endif

%if %{build_with_match_regex}
%define _build_with_match_regex -DPLUGIN_MATCH_REGEX:BOOL=ON
%else
%define _build_with_match_regex -DPLUGIN_MATCH_REGEX:BOOL==OFF
%endif

%if %{build_with_match_table}
%define _build_with_match_table -DPLUGIN_MATCH_TABLE:BOOL=ON
%else
%define _build_with_match_table -DPLUGIN_MATCH_TABLE:BOOL==OFF
%endif

%if %{build_with_match_xpath}
%define _build_with_match_xpath -DPLUGIN_MATCH_XPATH:BOOL=ON
%else
%define _build_with_match_xpath -DPLUGIN_MATCH_XPATH:BOOL==OFF
%endif

%if %{build_with_memcached}
%define _build_with_memcached -DPLUGIN_MEMCACHED:BOOL=ON
%else
%define _build_with_memcached -DPLUGIN_MEMCACHED:BOOL==OFF
%endif

%if %{build_with_memory}
%define _build_with_memory -DPLUGIN_MEMORY:BOOL=ON
%else
%define _build_with_memory -DPLUGIN_MEMORY:BOOL==OFF
%endif

%if %{build_with_mongodb}
%define _build_with_mongodb -DPLUGIN_MONGODB:BOOL=ON
%else
%define _build_with_mongodb -DPLUGIN_MONGODB:BOOL=OFF
%endif

%if %{build_with_mysql}
%define _build_with_mysql -DPLUGIN_MYSQL:BOOL=ON
%else
%define _build_with_mysql -DPLUGIN_MYSQL:BOOL=OFF
%endif

%if %{build_with_nagios_check}
%define _build_with_nagios_check -DPLUGIN_NAGIOS_CHECK:BOOL=ON
%else
%define _build_with_nagios_check -DPLUGIN_NAGIOS_CHECK:BOOL==OFF
%endif

%if %{build_with_nginx}
%define _build_with_nginx -DPLUGIN_NGINX:BOOL=ON
%else
%define _build_with_nginx -DPLUGIN_NGINX:BOOL==OFF
%endif

%if %{build_with_nginx_vts}
%define _build_with_nginx_vts -DPLUGIN_NGINX_VTS:BOOL=ON
%else
%define _build_with_nginx_vts -DPLUGIN_NGINX_VTS:BOOL==OFF
%endif

%if %{build_with_notify_exec}
%define _build_with_notify_exec -DPLUGIN_NOTIFY_EXEC:BOOL=ON
%else
%define _build_with_notify_exec -DPLUGIN_NOTIFY_EXEC:BOOL==OFF
%endif

%if %{build_with_notify_nagios}
%define _build_with_arp -DPLUGIN_NOTIFY_NAGIOS:BOOL=ON
%else
%define _build_with_arp -DPLUGIN_NOTIFY_NAGIOS:BOOL==OFF
%endif

%if %{build_with_ntpd}
%define _build_with_ntpd -DPLUGIN_NTPD:BOOL=ON
%else
%define _build_with_ntpd -DPLUGIN_NTPD:BOOL==OFF
%endif

%if %{build_with_odbc}
%define _build_with_odbc -DPLUGIN_ODBC:BOOL=ON
%else
%define _build_with_odbc -DPLUGIN_ODBC:BOOL==OFF
%endif

%if %{build_with_olsrd}
%define _build_with_olsrd -DPLUGIN_OLSRD:BOOL=ON
%else
%define _build_with_olsrd -DPLUGIN_OLSRD:BOOL==OFF
%endif

%if %{build_with_openvpn}
%define _build_with_openvpn -DPLUGIN_OPENVPN:BOOL=ON
%else
%define _build_with_openvpn -DPLUGIN_OPENVPN:BOOL==OFF
%endif

%if %{build_with_pdns}
%define _build_with_pdns -DPLUGIN_PDNS:BOOL=ON
%else
%define _build_with_pdns -DPLUGIN_PDNS:BOOL==OFF
%endif

%if %{build_with_ping}
%define _build_with_ping -DPLUGIN_PING:BOOL=ON
%else
%define _build_with_ping -DPLUGIN_PING:BOOL==OFF
%endif

%if %{build_with_pgbouncer}
%define _build_with_pgbouncer -DPLUGIN_PGBOUNCER:BOOL=ON
%else
%define _build_with_pgbouncer -DPLUGIN_PGBOUNCER:BOOL=OFF
%endif

%if %{build_with_pgpool}
%define _build_with_pgpool -DPLUGIN_PGPOOL:BOOL=ON
%else
%define _build_with_pgpool -DPLUGIN_PGPOOL:BOOL=OFF
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

%if %{build_with_processes}
%define _build_with_processes -DPLUGIN_PROCESSES:BOOL=ON
%else
%define _build_with_processes -DPLUGIN_PROCESSES:BOOL==OFF
%endif

%if %{build_with_proxysql}
%define _build_with_proxysql -DPLUGIN_PROXYSQL:BOOL=ON
%else
%define _build_with_proxysql -DPLUGIN_PROXYSQL:BOOL==OFF
%endif

%if %{build_with_python}
%define _build_with_python -DPLUGIN_PYTHON:BOOL=ON
%else
%define _build_with_python -DPLUGIN_PYTHON:BOOL==OFF
%endif

%if %{build_with_recursor}
%define _build_with_recursor -DPLUGIN_RECURSOR:BOOL=ON
%else
%define _build_with_recursor -DPLUGIN_RECURSOR:BOOL==OFF
%endif

%if %{build_with_routeros}
%define _build_with_routeros -DPLUGIN_ROUTEROS:BOOL=ON
%else
%define _build_with_routeros -DPLUGIN_ROUTEROS:BOOL==OFF
%endif

%if %{build_with_scraper}
%define _build_with_scraper -DPLUGIN_SCRAPER:BOOL=ON
%else
%define _build_with_scraper -DPLUGIN_SCRAPER:BOOL==OFF
%endif

%if %{build_with_sendmail}
%define _build_with_sendmail -DPLUGIN_SENDMAIL:BOOL=ON
%else
%define _build_with_sendmail -DPLUGIN_SENDMAIL:BOOL=OFF
%endif

%if %{build_with_squid}
%define _build_with_squid -DPLUGIN_SQUID:BOOL=ON
%else
%define _build_with_squid -DPLUGIN_SQUID:BOOL==OFF
%endif

%if %{build_with_statsd}
%define _build_with_statsd -DPLUGIN_STATSD:BOOL=ON
%else
%define _build_with_statsd -DPLUGIN_STATSD:BOOL==OFF
%endif

%if %{build_with_swap}
%define _build_with_swap -DPLUGIN_SWAP:BOOL=ON
%else
%define _build_with_swap -DPLUGIN_SWAP:BOOL==OFF
%endif

%if %{build_with_table}
%define _build_with_table -DPLUGIN_TABLE:BOOL=ON
%else
%define _build_with_table -DPLUGIN_TABLE:BOOL==OFF
%endif

%if %{build_with_tail}
%define _build_with_tail -DPLUGIN_TAIL:BOOL=ON
%else
%define _build_with_tail -DPLUGIN_TAIL:BOOL==OFF
%endif

%if %{build_with_tape}
%define _build_with_tape -DPLUGIN_TAPE:BOOL=ON
%else
%define _build_with_tape -DPLUGIN_TAPE:BOOL==OFF
%endif

%if %{build_with_tcpconns}
%define _build_with_tcpconns -DPLUGIN_TCPCONNS:BOOL=ON
%else
%define _build_with_tcpconns -DPLUGIN_TCPCONNS:BOOL==OFF
%endif

%if %{build_with_uname}
%define _build_with_uname -DPLUGIN_UNAME:BOOL=ON
%else
%define _build_with_uname -DPLUGIN_UNAME:BOOL==OFF
%endif

%if %{build_with_uptime}
%define _build_with_uptime -DPLUGIN_UPTIME:BOOL=ON
%else
%define _build_with_uptime -DPLUGIN_UPTIME:BOOL==OFF
%endif

%if %{build_with_users}
%define _build_with_users -DPLUGIN_USERS:BOOL=ON
%else
%define _build_with_users -DPLUGIN_USERS:BOOL==OFF
%endif

%if %{build_with_uuid}
%define _build_with_uuid -DPLUGIN_UUID:BOOL=ON
%else
%define _build_with_uuid -DPLUGIN_UUID:BOOL==OFF
%endif

%if %{build_with_wlm}
%define _build_with_wlm -DPLUGIN_WLM:BOOL=ON
%else
%define _build_with_wlm -DPLUGIN_WLM:BOOL==OFF
%endif

%if %{build_with_wpar}
%define _build_with_wpar -DPLUGIN_WPAR:BOOL=ON
%else
%define _build_with_wpar -DPLUGIN_WPAR:BOOL==OFF
%endif

%if %{build_with_write_file}
%define _build_with_write_file -DPLUGIN_WRITE_FILE:BOOL=ON
%else
%define _build_with_write_file -DPLUGIN_WRITE_FILE:BOOL==OFF
%endif

%if %{build_with_write_http}
%define _build_with_write_http -DPLUGIN_WRITE_HTTP:BOOL=ON
%else
%define _build_with_write_http -DPLUGIN_WRITE_HTTP:BOOL==OFF
%endif

%if %{build_with_write_log}
%define _build_with_write_log -DPLUGIN_WRITE_LOG:BOOL=ON
%else
%define _build_with_write_log -DPLUGIN_WRITE_LOG:BOOL==OFF
%endif

%if %{build_with_write_mongodb}
%define _build_with_write_mongodb -DPLUGIN_WRITE_MONGODB:BOOL=ON
%else
%define _build_with_write_mongodb -DPLUGIN_WRITE_MONGODB:BOOL=OFF
%endif

%if %{build_with_write_postgresql}
%define _build_with_write_postgresql -DPLUGIN_WRITE_POSTGRESQL:BOOL=ON
%else
%define _build_with_write_postgresql -DPLUGIN_WRITE_POSTGRESQL:BOOL=OFF
%endif

%if %{build_with_write_tcp}
%define _build_with_write_tcp -DPLUGIN_WRITE_TCP:BOOL=ON
%else
%define _build_with_write_tcp -DPLUGIN_WRITE_TCP:BOOL==OFF
%endif

%if %{build_with_write_udp}
%define _build_with_write_udp -DPLUGIN_WRITE_UDP:BOOL=ON
%else
%define _build_with_write_udp -DPLUGIN_WRITE_UDP:BOOL==OFF
%endif

%if %{build_with_zookeeper}
%define _build_with_zookeeper -DPLUGIN_ZOOKEEPER:BOOL=ON
%else
%define _build_with_zookeeper -DPLUGIN_ZOOKEEPER:BOOL==OFF
%endif

%if %{build_with_notify_snmp}
%define _build_with_notify_snmp -DPLUGIN_NOTIFY_SNMP:BOOL=ON
%else
%define _build_with_notify_snmp -DPLUGIN_NOTIFY_SNMP:BOOL==OFF
%endif

%if %{build_with_snmp}
%define _build_with_snmp -DPLUGIN_SNMP:BOOL=ON
%else
%define _build_with_snmp -DPLUGIN_SNMP:BOOL==OFF
%endif

%if %{build_with_db2}
%if 0%{?_db2_path:1}
%define _build_with_db2 -DPLUGIN_DB2:BOOL=ON -DLibDb2_ROOT:STRING=%{_db2_path}
%else
%define _build_with_db2 -DPLUGIN_DB2:BOOL=ON
%endif
%else
%define _build_with_db2 -DPLUGIN_DB2:BOOL=OFF
%endif

%if %{build_with_mq}
%if 0%{?_mq_path:1}
%define _build_with_mq -DPLUGIN_MQ:BOOL=ON -DLibMq_ROOT:STRING=%{_mq_path}
%else
%define _build_with_mq -DPLUGIN_MQ:BOOL=ON
%endif
%else
%define _build_with_mq -DPLUGIN_MQ:BOOL=OFF
%endif

%{set_build_flags}
export OBJECT_MODE=64
/opt/freeware/bin/cmake -S . -B aix-build \
        -DCMAKE_C_FLAGS_RELEASE:STRING="-DNDEBUG" \
        -DCMAKE_CXX_FLAGS_RELEASE:STRING="-DNDEBUG" \
        -DCMAKE_Fortran_FLAGS_RELEASE:STRING="-DNDEBUG" \
        -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
        -DCMAKE_INSTALL_DO_STRIP:BOOL=OFF \
        -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \
        -DSYSCONF_INSTALL_DIR:PATH=%{_sysconfdir} \
        -DSHARE_INSTALL_PREFIX:PATH=%{_libdir}/ncollectd \
        -DENABLE_ALL_PLUGINS:BOOL=ON \
        %{?_build_with_apache} \
        %{?_build_with_apcups} \
        %{?_build_with_ats} \
        %{?_build_with_beanstalkd} \
        %{?_build_with_bind} \
        %{?_build_with_contextswitch} \
        %{?_build_with_cpu} \
        %{?_build_with_df} \
        %{?_build_with_disk} \
        %{?_build_with_dnsmasq} \
        %{?_build_with_exec} \
        %{?_build_with_freeradius} \
        %{?_build_with_fcgi} \
        %{?_build_with_filecount} \
        %{?_build_with_haproxy} \
        %{?_build_with_hba} \
        %{?_build_with_http} \
        %{?_build_with_info} \
        %{?_build_with_interface} \
        %{?_build_with_ipc} \
        %{?_build_with_jolokia} \
        %{?_build_with_kea} \
        %{?_build_with_keepalived} \
        %{?_build_with_load} \
        %{?_build_with_log_file} \
        %{?_build_with_log_gelf} \
        %{?_build_with_log_syslog} \
        %{?_build_with_lpar} \
        %{?_build_with_lua} \
        %{?_build_with_match_csv} \
        %{?_build_with_match_jsonpath} \
        %{?_build_with_match_regex} \
        %{?_build_with_match_table} \
        %{?_build_with_match_xpath} \
        %{?_build_with_memcached} \
        %{?_build_with_memory} \
        %{?_build_with_mongodb} \
        %{?_build_with_mysql} \
        %{?_build_with_nagios_check} \
        %{?_build_with_nginx} \
        %{?_build_with_nginx_vts} \
        %{?_build_with_notify_exec} \
        %{?_build_with_notify_nagios} \
        %{?_build_with_ntpd} \
        %{?_build_with_odbc} \
        %{?_build_with_olsrd} \
        %{?_build_with_openvpn} \
        %{?_build_with_pdns} \
        %{?_build_with_ping} \
        %{?_build_with_pgbouncer} \
        %{?_build_with_pgpool} \
        %{?_build_with_postfix} \
        %{?_build_with_postgresql} \
        %{?_build_with_processes} \
        %{?_build_with_proxysql} \
        %{?_build_with_python} \
        %{?_build_with_recursor} \
        %{?_build_with_routeros} \
        %{?_build_with_scraper} \
        %{?_build_with_sendmail} \
        %{?_build_with_squid} \
        %{?_build_with_statsd} \
        %{?_build_with_swap} \
        %{?_build_with_table} \
        %{?_build_with_tail} \
        %{?_build_with_tape} \
        %{?_build_with_tcpconns} \
        %{?_build_with_uname} \
        %{?_build_with_uptime} \
        %{?_build_with_users} \
        %{?_build_with_uuid} \
        %{?_build_with_wlm} \
        %{?_build_with_wpar} \
        %{?_build_with_write_file} \
        %{?_build_with_write_http} \
        %{?_build_with_write_log} \
        %{?_build_with_write_mongodb} \
        %{?_build_with_write_postgresql} \
        %{?_build_with_write_tcp} \
        %{?_build_with_write_udp} \
        %{?_build_with_zookeeper} \
        %{?_build_with_notify_snmp} \
        %{?_build_with_snmp} \
        %{?_build_with_db2} \
        %{?_build_with_mq} \
        -DPLUGIN_DS389:BOOL=OFF \
        -DPLUGIN_OPENLDAP:BOOL=OFF \
        -DPLUGIN_LDAP:BOOL=OFF \
        -DPLUGIN_CHRONY:BOOL=OFF \
        -DPLUGIN_DOCKER:BOOL=OFF \
        -DPLUGIN_PODMAN:BOOL=OFF \
        -DPLUGIN_ARP:BOOL=OFF \
        -DPLUGIN_ARPCACHE:BOOL=OFF \
        -DPLUGIN_AVCCACHE:BOOL=OFF \
        -DPLUGIN_BATTERY:BOOL=OFF \
        -DPLUGIN_BCACHE:BOOL=OFF \
        -DPLUGIN_BONDING:BOOL=OFF \
        -DPLUGIN_BTRFS:BOOL=OFF \
        -DPLUGIN_BUDDYINFO:BOOL=OFF \
        -DPLUGIN_CERT:BOOL=OFF \
        -DPLUGIN_CGROUPS:BOOL=OFF \
        -DPLUGIN_CIFS:BOOL=OFF \
        -DPLUGIN_CONNTRACK:BOOL=OFF \
        -DPLUGIN_CPUFREQ:BOOL=OFF \
        -DPLUGIN_CPUSLEEP:BOOL=OFF \
        -DPLUGIN_CUPS:BOOL=OFF \
        -DPLUGIN_NSD:BOOL=OFF \
        -DPLUGIN_UNBOUND:BOOL=OFF \
        -DPLUGIN_DCPMM:BOOL=OFF \
        -DPLUGIN_DMI:BOOL=OFF \
        -DPLUGIN_DNS:BOOL=OFF \
        -DPLUGIN_DRBD:BOOL=OFF \
        -DPLUGIN_EDAC:BOOL=OFF \
        -DPLUGIN_ENTROPY:BOOL=OFF \
        -DPLUGIN_ETHSTAT:BOOL=OFF \
        -DPLUGIN_FCHOST:BOOL=OFF \
        -DPLUGIN_FHCOUNT:BOOL=OFF \
        -DPLUGIN_FSCACHE:BOOL=OFF \
        -DPLUGIN_GPS:BOOL=OFF \
        -DPLUGIN_GPU_INTEL:BOOL=OFF \
        -DPLUGIN_GPU_NVIDIA:BOOL=OFF \
        -DPLUGIN_HUGEPAGES:BOOL=OFF \
        -DPLUGIN_INFINIBAND:BOOL=OFF \
        -DPLUGIN_INTEL_RDT:BOOL=OFF \
        -DPLUGIN_IPMI:BOOL=OFF \
        -DPLUGIN_IPSTATS:BOOL=OFF \
        -DPLUGIN_IPTABLES:BOOL=OFF \
        -DPLUGIN_IPVS:BOOL=OFF \
        -DPLUGIN_IRQ:BOOL=OFF \
        -DPLUGIN_ISCSI:BOOL=OFF \
        -DPLUGIN_JAVA:BOOL=OFF \
        -DPLUGIN_JOURNAL:BOOL=OFF \
        -DPLUGIN_KAFKA:BOOL=OFF \
        -DPLUGIN_KSM:BOOL=OFF \
        -DPLUGIN_LOCKS:BOOL=OFF \
        -DPLUGIN_LOGIND:BOOL=OFF \
        -DPLUGIN_LOG_SYSTEMD:BOOL=OFF \
        -DPLUGIN_LVM:BOOL=OFF \
        -DPLUGIN_MD:BOOL=OFF \
        -DPLUGIN_MEMINFO:BOOL=OFF \
        -DPLUGIN_MMC:BOOL=OFF \
        -DPLUGIN_MODBUS:BOOL=OFF \
        -DPLUGIN_MOSQUITTO:BOOL=OFF \
        -DPLUGIN_NETSTAT_UDP:BOOL=OFF \
        -DPLUGIN_NFACCT:BOOL=OFF \
        -DPLUGIN_NFCONNTRACK:BOOL=OFF \
        -DPLUGIN_NFS:BOOL=OFF \
        -DPLUGIN_NFSD:BOOL=OFF \
        -DPLUGIN_NFTABLES:BOOL=OFF \
        -DPLUGIN_NOTIFY_EMAIL:BOOL=OFF \
        -DPLUGIN_NUMA:BOOL=OFF \
        -DPLUGIN_NUT:BOOL=OFF \
        -DPLUGIN_ORACLE:BOOL=OFF \
        -DPLUGIN_PCAP:BOOL=OFF \
        -DPLUGIN_PERL:BOOL=OFF \
        -DPLUGIN_PF:BOOL=OFF \
        -DPLUGIN_PRESSURE:BOOL=OFF \
        -DPLUGIN_PROTOCOLS:BOOL=OFF \
        -DPLUGIN_QUOTA:BOOL=OFF \
        -DPLUGIN_RAPL:BOOL=OFF \
        -DPLUGIN_REDIS:BOOL=OFF \
        -DPLUGIN_RESCTRL:BOOL=OFF \
        -DPLUGIN_RTCACHE:BOOL=OFF \
        -DPLUGIN_SCHEDSTAT:BOOL=OFF \
        -DPLUGIN_SENSORS:BOOL=OFF \
        -DPLUGIN_SERIAL:BOOL=OFF \
        -DPLUGIN_SIGROK:BOOL=OFF \
        -DPLUGIN_SLABINFO:BOOL=OFF \
        -DPLUGIN_SLURM:BOOL=OFF \
        -DPLUGIN_SMART:BOOL=OFF \
        -DPLUGIN_SOCKSTAT:BOOL=OFF \
        -DPLUGIN_SOFTIRQ:BOOL=OFF \
        -DPLUGIN_SOFTNET:BOOL=OFF \
        -DPLUGIN_SYNPROXY:BOOL=OFF \
        -DPLUGIN_TC:BOOL=OFF \
        -DPLUGIN_THERMAL:BOOL=OFF \
        -DPLUGIN_TIMEX:BOOL=OFF \
        -DPLUGIN_TURBOSTAT:BOOL=OFF \
        -DPLUGIN_UBI:BOOL=OFF \
        -DPLUGIN_VARNISH:BOOL=OFF \
        -DPLUGIN_VIRT:BOOL=OFF \
        -DPLUGIN_VMEM:BOOL=OFF \
        -DPLUGIN_WIREGUARD:BOOL=OFF \
        -DPLUGIN_WIRELESS:BOOL=OFF \
        -DPLUGIN_WRITE_AMQP:BOOL=OFF \
        -DPLUGIN_WRITE_AMQP1:BOOL=OFF \
        -DPLUGIN_WRITE_EXPORTER:BOOL=OFF \
        -DPLUGIN_WRITE_KAFKA:BOOL=OFF \
        -DPLUGIN_WRITE_MQTT:BOOL=OFF \
        -DPLUGIN_WRITE_REDIS:BOOL=OFF \
        -DPLUGIN_XENCPU:BOOL=OFF \
        -DPLUGIN_XFRM:BOOL=OFF \
        -DPLUGIN_XFS:BOOL=OFF \
        -DPLUGIN_ZFS:BOOL=OFF \
        -DPLUGIN_ZONE:BOOL=OFF \
        -DPLUGIN_ZONEINFO:BOOL=OFF \
        -DPLUGIN_ZRAM:BOOL=OFF \
        -DPLUGIN_ZSWAP:BOOL=OFF

/opt/freeware/bin/cmake --build aix-build %{?_smp_mflags} --verbose

%install
rm -rf %{buildroot}
DESTDIR="%{buildroot}" /opt/freeware/bin/cmake --install aix-build

mkdir -p %{buildroot}/%{_localstatedir}/lib/ncollectd
mkdir -p %{buildroot}/%{_localstatedir}/run

# mv %{buildroot}/%{_sysconfdir}/ncollectd.conf %{buildroot}/etc/ncollectd.conf
mv %{buildroot}/etc/opt/freeware/ncollectd.conf %{buildroot}/etc/ncollectd.conf

mkdir -p %{buildroot}/etc/rc.d/init.d
cp %{SOURCE1} %{buildroot}/etc/rc.d/init.d/ncollectd

mkdir -p %{buildroot}/etc/rc.d/rc2.d/
ln -sf ../init.d/ncollectd %{buildroot}/etc/rc.d/rc2.d/Sncollectd
ln -sf ../init.d/ncollectd %{buildroot}/etc/rc.d/rc2.d/Kncollectd

mkdir -p %{buildroot}/etc/rc.d/rc3.d/
ln -sf ../init.d/ncollectd %{buildroot}/etc/rc.d/rc3.d/Sncollectd
ln -sf ../init.d/ncollectd %{buildroot}/etc/rc.d/rc3.d/Kncollectd

%clean
rm -rf %{buildroot}

%preun
if [ $1 -eq 0 ]; then
    /etc/rc.d/init.d/ncollectd stop > /dev/null 2>&1 || :
fi

%postun
if [ $1 -eq 1 ]; then
    /etc/rc.d/init.d/ncollectd restart >/dev/null 2>&1 || :
fi

%files
%defattr(-,root,system)
%config(noreplace) %attr(0644,root,system) /etc/ncollectd.conf
%attr(0755,root,system) %{_sbindir}/ncollectd
%attr(0755,root,system) %{_sbindir}/ncollectdmon
%attr(0755,root,system) %{_bindir}/ncollectdctl
%{_datadir}/man/man1/ncollectd.1*
%{_datadir}/man/man1/ncollectdmon.1*
%{_datadir}/man/man1/ncollectdctl.1*
%{_datadir}/man/man5/ncollectd.conf.5*
%dir %{_localstatedir}/lib/ncollectd
%dir %{_localstatedir}/run
%attr(0755,root,system) /etc/rc.d/init.d/ncollectd
/etc/rc.d/rc2.d/Sncollectd
/etc/rc.d/rc2.d/Kncollectd
/etc/rc.d/rc3.d/Sncollectd
/etc/rc.d/rc3.d/Kncollectd

%if %{build_with_apcups}
%{_libdir}/%{name}/apcups.so
%{_datadir}/man/man5/ncollectd-apcups.5*
%endif
%if %{build_with_beanstalkd}
%{_libdir}/%{name}/beanstalkd.so
%{_datadir}/man/man5/ncollectd-beanstalkd.5*
%endif
%if %{build_with_contextswitch}
%{_libdir}/%{name}/contextswitch.so
%{_datadir}/man/man5/ncollectd-contextswitch.5*
%endif
%if %{build_with_cpu}
%{_libdir}/%{name}/cpu.so
%{_datadir}/man/man5/ncollectd-cpu.5*
%endif
%if %{build_with_df}
%{_libdir}/%{name}/df.so
%{_datadir}/man/man5/ncollectd-df.5*
%endif
%if %{build_with_disk}
%{_libdir}/%{name}/disk.so
%{_datadir}/man/man5/ncollectd-disk.5*
%endif
%if %{build_with_dnsmasq}
%{_libdir}/%{name}/dnsmasq.so
%{_datadir}/man/man5/ncollectd-dnsmasq.5*
%endif
%if %{build_with_exec}
%{_libdir}/%{name}/exec.so
%{_datadir}/man/man5/ncollectd-exec.5*
%endif
%if %{build_with_fcgi}
%{_libdir}/%{name}/fcgi.so
%{_datadir}/man/man5/ncollectd-fcgi.5*
%endif
%if %{build_with_freeradius}
%{_libdir}/%{name}/freeradius.so
%{_datadir}/man/man5/ncollectd-freeradius.5*
%endif
%if %{build_with_filecount}
%{_libdir}/%{name}/filecount.so
%{_datadir}/man/man5/ncollectd-filecount.5*
%endif
%if %{build_with_hba}
%{_libdir}/%{name}/hba.so
%{_datadir}/man/man5/ncollectd-hba.5*
%endif
%if %{build_with_info}
%{_libdir}/%{name}/info.so
%{_datadir}/man/man5/ncollectd-info.5*
%endif
%if %{build_with_interface}
%{_libdir}/%{name}/interface.so
%{_datadir}/man/man5/ncollectd-interface.5*
%endif
%if %{build_with_ipc}
%{_libdir}/%{name}/ipc.so
%{_datadir}/man/man5/ncollectd-ipc.5*
%endif
%if %{build_with_kea}
%{_libdir}/%{name}/kea.so
%{_datadir}/man/man5/ncollectd-kea.5*
%endif
%if %{build_with_keepalived}
%{_libdir}/%{name}/keepalived.so
%{_datadir}/man/man5/ncollectd-keepalived.5*
%endif
%if %{build_with_load}
%{_libdir}/%{name}/load.so
%{_datadir}/man/man5/ncollectd-load.5*
%endif
%if %{build_with_log_file}
%{_libdir}/%{name}/log_file.so
%{_datadir}/man/man5/ncollectd-log_file.5*
%endif
%if %{build_with_log_gelf}
%{_libdir}/%{name}/log_gelf.so
%{_datadir}/man/man5/ncollectd-log_gelf.5*
%endif
%if %{build_with_log_syslog}
%{_libdir}/%{name}/log_syslog.so
%{_datadir}/man/man5/ncollectd-log_syslog.5*
%endif
%if %{build_with_lpar}
%{_libdir}/%{name}/lpar.so
%{_datadir}/man/man5/ncollectd-lpar.5*
%endif
%if %{build_with_match_csv}
%{_libdir}/%{name}/match_csv.so
%{_datadir}/man/man5/ncollectd-match_csv.5*
%endif
%if %{build_with_match_jsonpath}
%{_libdir}/%{name}/match_jsonpath.so
%{_datadir}/man/man5/ncollectd-match_jsonpath.5*
%endif
%if %{build_with_match_regex}
%{_libdir}/%{name}/match_regex.so
%{_datadir}/man/man5/ncollectd-match_regex.5*
%endif
%if %{build_with_match_table}
%{_libdir}/%{name}/match_table.so
%{_datadir}/man/man5/ncollectd-match_table.5*
%endif
%if %{build_with_memcached}
%{_libdir}/%{name}/memcached.so
%{_datadir}/man/man5/ncollectd-memcached.5*
%endif
%if %{build_with_memory}
%{_libdir}/%{name}/memory.so
%{_datadir}/man/man5/ncollectd-memory.5*
%endif
%if %{build_with_nagios_check}
%{_libdir}/%{name}/nagios_check.so
%{_datadir}/man/man5/ncollectd-nagios_check.5*
%endif
%if %{build_with_notify_exec}
%{_libdir}/%{name}/notify_exec.so
%{_datadir}/man/man5/ncollectd-notify_exec.5*
%endif
%if %{build_with_notify_nagios}
%{_libdir}/%{name}/notify_nagios.so
%{_datadir}/man/man5/ncollectd-notify_nagios.5*
%endif
%if %{build_with_ntpd}
%{_libdir}/%{name}/ntpd.so
%{_datadir}/man/man5/ncollectd-ntpd.5*
%endif
%if %{build_with_olsrd}
%{_libdir}/%{name}/olsrd.so
%{_datadir}/man/man5/ncollectd-olsrd.5*
%endif
%if %{build_with_openvpn}
%{_libdir}/%{name}/openvpn.so
%{_datadir}/man/man5/ncollectd-openvpn.5*
%endif
%if %{build_with_pdns}
%{_libdir}/%{name}/pdns.so
%{_datadir}/man/man5/ncollectd-pdns.5*
%endif
%if %{build_with_ping}
%{_libdir}/%{name}/ping.so
%{_datadir}/man/man5/ncollectd-ping.5*
%endif
%if %{build_with_postfix}
%{_libdir}/%{name}/postfix.so
%{_datadir}/man/man5/ncollectd-postfix.5*
%endif
%if %{build_with_processes}
%{_libdir}/%{name}/processes.so
%{_datadir}/man/man5/ncollectd-processes.5*
%endif
%if %{build_with_recursor}
%{_libdir}/%{name}/recursor.so
%{_datadir}/man/man5/ncollectd-recursor.5*
%endif
%if %{build_with_routeros}
%{_libdir}/%{name}/routeros.so
%{_datadir}/man/man5/ncollectd-routeros.5*
%endif
%if %{build_with_sendmail}
%{_libdir}/%{name}/sendmail.so
%{_datadir}/man/man5/ncollectd-sendmail.5*
%endif
%if %{build_with_statsd}
%{_libdir}/%{name}/statsd.so
%{_datadir}/man/man5/ncollectd-statsd.5*
%endif
%if %{build_with_swap}
%{_libdir}/%{name}/swap.so
%{_datadir}/man/man5/ncollectd-swap.5*
%endif
%if %{build_with_table}
%{_libdir}/%{name}/table.so
%{_datadir}/man/man5/ncollectd-table.5*
%endif
%if %{build_with_tail}
%{_libdir}/%{name}/tail.so
%{_datadir}/man/man5/ncollectd-tail.5*
%endif
%if %{build_with_tape}
%{_libdir}/%{name}/tape.so
%{_datadir}/man/man5/ncollectd-tape.5*
%endif
%if %{build_with_tcpconns}
%{_libdir}/%{name}/tcpconns.so
%{_datadir}/man/man5/ncollectd-tcpconns.5*
%endif
%if %{build_with_uname}
%{_libdir}/%{name}/uname.so
%{_datadir}/man/man5/ncollectd-uname.5*
%endif
%if %{build_with_uptime}
%{_libdir}/%{name}/uptime.so
%{_datadir}/man/man5/ncollectd-uptime.5*
%endif
%if %{build_with_users}
%{_libdir}/%{name}/users.so
%{_datadir}/man/man5/ncollectd-users.5*
%endif
%if %{build_with_uuid}
%{_libdir}/%{name}/uuid.so
%{_datadir}/man/man5/ncollectd-uuid.5*
%endif
%if %{build_with_wlm}
%{_libdir}/%{name}/wlm.so
%{_datadir}/man/man5/ncollectd-wlm.5*
%endif
%if %{build_with_wpar}
%{_libdir}/%{name}/wpar.so
%{_datadir}/man/man5/ncollectd-wpar.5*
%endif
%if %{build_with_write_file}
%{_libdir}/%{name}/write_file.so
%{_datadir}/man/man5/ncollectd-write_file.5*
%endif
%if %{build_with_write_log}
%{_libdir}/%{name}/write_log.so
%{_datadir}/man/man5/ncollectd-write_log.5*
%endif
%if %{build_with_write_tcp}
%{_libdir}/%{name}/write_tcp.so
%{_datadir}/man/man5/ncollectd-write_tcp.5*
%endif
%if %{build_with_write_udp}
%{_libdir}/%{name}/write_udp.so
%{_datadir}/man/man5/ncollectd-write_udp.5*
%endif
%if %{build_with_zookeeper}
%{_libdir}/%{name}/zookeeper.so
%{_datadir}/man/man5/ncollectd-zookeeper.5*
%endif

%if %{build_with_apache}
%files apache
%{_libdir}/%{name}/apache.so
%{_datadir}/man/man5/ncollectd-apache.5*
%endif

%if %{build_with_ats}
%files ats
%{_libdir}/%{name}/ats.so
%{_datadir}/man/man5/ncollectd-ats.5*
%endif

%if %{build_with_bind}
%files bind
%{_libdir}/%{name}/bind.so
%{_datadir}/man/man5/ncollectd-bind.5*
%endif

%if %{build_with_db2}
%files db2
%{_libdir}/%{name}/db2.so
%{_datadir}/man/man5/ncollectd-db2.5*
%endif

%if %{build_with_haproxy}
%files haproxy
%{_libdir}/%{name}/haproxy.so
%{_datadir}/man/man5/ncollectd-haproxy.5*
%endif

%if %{build_with_http}
%files http
%{_libdir}/%{name}/http.so
%{_datadir}/man/man5/ncollectd-http.5*
%endif

%if %{build_with_jolokia}
%files jolokia
%{_libdir}/%{name}/jolokia.so
%{_datadir}/man/man5/ncollectd-jolokia.5*
%endif

%if %{build_with_match_xpath}
%files match_xpath
%{_libdir}/%{name}/match_xpath.so
%{_datadir}/man/man5/ncollectd-match_xpath.5*
%endif

%if %{build_with_mq}
%files mq
%{_libdir}/%{name}/mq.so
%{_datadir}/man/man5/ncollectd-mq.5*
%endif

%if %{build_with_mongodb}
%files mongodb
%{_libdir}/%{name}/mongodb.so
%{_datadir}/man/man5/ncollectd-mongodb.5*
%endif

%if %{build_with_mysql}
%files mysql
%{_libdir}/%{name}/mysql.so
%{_datadir}/man/man5/ncollectd-mysql.5*
%endif

%if %{build_with_nginx}
%files nginx
%{_libdir}/%{name}/nginx.so
%{_datadir}/man/man5/ncollectd-nginx.5*
%endif

%if %{build_with_nginx_vts}
%files nginx_vts
%{_libdir}/%{name}/nginx_vts.so
%{_datadir}/man/man5/ncollectd-nginx_vts.5*
%endif

%if %{build_with_odbc}
%files odbc
%{_libdir}/%{name}/odbc.so
%{_datadir}/man/man5/ncollectd-odbc.5*
%endif

%if %{build_with_pgbouncer}
%files pgbouncer
%{_libdir}/%{name}/pgbouncer.so
%{_datadir}/man/man5/ncollectd-pgbouncer.5*
%endif

%if %{build_with_pgpool}
%files pgpool
%{_libdir}/%{name}/pgpool.so
%{_datadir}/man/man5/ncollectd-pgpool.5*
%endif

%if %{build_with_postgresql}
%files postgresql
%{_libdir}/%{name}/postgresql.so
%{_datadir}/man/man5/ncollectd-postgresql.5*
%endif

%if %{build_with_proxysql}
%files proxysql
%{_libdir}/%{name}/proxysql.so
%{_datadir}/man/man5/ncollectd-proxysql.5*
%endif

%if %{build_with_python}
%files python
%{_libdir}/%{name}/python.so
%{_datadir}/man/man5/ncollectd-python.5*
%endif

%if %{build_with_scraper}
%files scraper
%{_libdir}/%{name}/scraper.so
%{_datadir}/man/man5/ncollectd-scraper.5*
%endif

%if %{build_with_squid}
%files squid
%{_libdir}/%{name}/squid.so
%{_datadir}/man/man5/ncollectd-squid.5*
%endif

%if %{build_with_snmp}
%files snmp
%{_libdir}/%{name}/snmp.so
%{_datadir}/man/man5/ncollectd-snmp.5*
%endif

%if %{build_with_write_http}
%files write_http
%{_libdir}/%{name}/write_http.so
%{_datadir}/man/man5/ncollectd-write_http.5*
%endif

%if %{build_with_write_mongodb}
%files write_mongodb
%{_libdir}/%{name}/write_mongodb.so
%{_datadir}/man/man5/ncollectd-write_mongodb.5*
%endif

%if %{build_with_write_postgresql}
%files write_postgresql
%{_libdir}/%{name}/write_postgresql.so
%{_datadir}/man/man5/ncollectd-write_postgresql.5*
%endif

%changelog
* Mon Jan 01 2024 ncollectd <info@ncollectd.org> - 0.0.0-1
- Initial release 0.0.0
