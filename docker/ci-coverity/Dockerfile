FROM fedora:42
RUN --mount=type=secret,id=coverity.data,dst=/tmp/coverity.data \
    dnf install -y gcc cpp clang-analyzer clang make cmake flex bison gperf pkgconf-pkg-config \
                   kernel-headers c-ares-devel curl-devel gpsd-devel \
                   hiredis-devel iptables-devel java-devel jpackage-utils libatasmart-devel \
                   libesmtp-devel libipmctl-devel libmicrohttpd-devel libmodbus-devel \
                   libnftnl-devel libpcap-devel librabbitmq-devel librdkafka-devel libvirt-devel \
                   libxml2-devel lm_sensors-devel lua-devel mariadb-connector-c-devel \
                   mongo-c-driver-devel mosquitto-devel net-snmp-devel nut-devel OpenIPMI-devel \
                   openldap-devel openssl-devel postgresql-devel qpid-proton-c-devel slurm-devel \
                   unixODBC-devel varnish-libs-devel xen-devel xfsprogs-devel yajl-devel \
                   cups-devel systemd-devel libsigrok-devel btrfs-progs-devel \
                   libmemcached-awesome-devel oneapi-level-zero-devel perl-ExtUtils-Embed \
                   python3-devel freetds-devel && \
    dnf clean all && \
    rm -rf /var/cache/yum && \
    curl -s -d @/tmp/coverity.data -o /opt/coverity_tool.tgz https://scan.coverity.com/download/linux64 && \
    cd /opt && \
    tar xzf /opt/coverity_tool.tgz && \
    rm -f /opt/coverity_tool.tgz && \
    mv cov-analysis-* cov-analysis

ENV PATH="$PATH:/opt/cov-analysis/bin"
