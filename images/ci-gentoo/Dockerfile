FROM gentoo/stage3:systemd
RUN emerge-webrsync && \
    getuto && \
    echo "en_US.UTF-8 UTF-8" > /etc/locale.gen && \
    emerge --autounmask=n --verbose --getbinpkg  sys-devel/gcc dev-vcs/git app-arch/xz-utils \
           dev-build/make dev-build/cmake sys-devel/bison sys-devel/flex dev-util/gperf \
           dev-util/pkgconf net-dns/c-ares sci-libs/libsigrok  sys-fs/xfsprogs \
           sys-fs/btrfs-progs net-libs/libnftnl net-libs/libesmtp net-libs/libpcap \
           net-libs/libmicrohttpd dev-libs/hiredis dev-libs/libmodbus dev-libs/openssl \
           dev-libs/librdkafka dev-libs/mongo-c-driver dev-libs/libxml2 \
           dev-libs/libmemcached-awesome dev-libs/libatasmart dev-libs/yajl net-misc/curl \
           net-analyzer/net-snmp app-misc/mosquitto dev-lang/lua sys-power/nut \
           net-firewall/iptables net-nds/openldap sci-geosciences/gpsd net-print/cups \
           dev-db/unixODBC sys-apps/systemd sys-libs/openipmi dev-db/postgresql \
           www-servers/varnish net-libs/rabbitmq-c dev-db/freetds sys-apps/lm-sensors \
           dev-db/mariadb-connector-c dev-lang/perl dev-lang/python virtual/jdk && \
    rm -rf /var/db/repos/gentoo /var/cache/distfiles /var/cache/binpkgs
