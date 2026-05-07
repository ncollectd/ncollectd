function reader() {
    fam = new ncollectd.MetricFamily("metric_unknow", ncollectd.METRIC_UNKNOWN)
    fam.append(new ncollectd.MetricUnknown(42, null, 1592748157.125))
    fam.dispatch()

    fam = new ncollectd.MetricFamily("metric_gauge", ncollectd.METRIC_GAUGE)
    fam.append(new ncollectd.MetricGauge(42, null, 1592748157.125))
    fam.dispatch()

    fam = new ncollectd.MetricFamily("metric_counter_with_label", ncollectd.METRIC_COUNTER)
    labels = {
        alpha: "first",
        beta: "second"
    }
    fam.append(new ncollectd.MetricCounter(BigInt(0), labels, 1592748157.125))
    fam.dispatch()

    fam = new ncollectd.MetricFamily("escaped_label_value", ncollectd.METRIC_COUNTER)
    labels = {
        alpha: "first \"value\"",
        beta: "second value"
    }
    fam.append(new ncollectd.MetricCounter(BigInt("18446744073709551615"), labels, 1592748157.125))
    fam.dispatch()

    fam = new ncollectd.MetricFamily("system_uname", ncollectd.METRIC_INFO)
    value = {
        machine: "riscv128",
        nodename: "arrakis.canopus",
        release: "998",
        sysname: "Linux",
        version: "#1 SMP PREEMPT_DYNAMIC 10191"
    }
    labels = {
        hostname: "arrakis.canopus"
    }
    fam.append(new ncollectd.MetricInfo(value, labels, 1592748157.125))
    fam.dispatch()

    fam = new ncollectd.MetricFamily("stateset",  ncollectd.METRIC_STATE_SET)
    state = {
        a: false,
        bb: true,
        ccc: false
    }
    labels = {
        hostname: "arrakis.canopus"
    }
    fam.append(new ncollectd.MetricStateSet(state, labels, 1592748157.125))
    fam.dispatch()

    fam = new ncollectd.MetricFamily("summary", ncollectd.METRIC_SUMMARY)
    quantiles = [[0.5,  0.232227334],
                 [0.90, 0.821139321],
                 [0.95, 1.528948804],
                 [0.99, 2.829188272],
                 [1,    34.283829292]]
    labels = {
        hostname: "arrakis.canopus"
    }
    fam.append(new ncollectd.MetricSummary(8953.332, 27892, quantiles, labels, 1592748157.125))
    fam.dispatch()

    fam = new ncollectd.MetricFamily("histogram", ncollectd.METRIC_HISTOGRAM)
    buckets = [[27892, Infinity],
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
               [0, 0.01]]
    labels = {
        hostname: "arrakis.canopus"
    }
    fam.append(new ncollectd.MetricHistogram(8953.332, buckets, labels, 1592748157.125))
    fam.dispatch()

    fam = new ncollectd.MetricFamily("gauge_histogram", ncollectd.METRIC_GAUGE_HISTOGRAM)
    buckets = [[120, Infinity],
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
               [4, 1024]]
    labels = {
        hostname: "arrakis.canopus"
    }
    fam.append(new ncollectd.MetricGaugeHistogram(120, buckets, labels, 1592748157.125))
    fam.dispatch()

}


ncollectd.register_read(reader)
