function test_read()
    ncollectd.log_info("read function called")

    fam = {
        name = 'metric_counter',
        help = 'help for counter test',
        unit = 'some counter',
        type = ncollectd.METRIC_TYPE_COUNTER,
        metrics = {
            {
                time = 0,
                interval = 10,
                labels = { labela = 'keya', labelb = 'keyb' },
                value = 99
            },
            {
                time = 0,
                interval = 10,
                labels = { labelc = 'keyc', labeld = 'keyd' },
                value = 999
            },
        }
    }

    ncollectd.dispatch_metric_family(fam)

    fam = {
        name = 'metric_gauge',
        help = 'help for gauge test',
        unit = 'some gauge',
        type = ncollectd.METRIC_TYPE_GAUGE,
        metrics = {
            {
                time = 0,
                interval = 10,
                labels = { labela = 'keya', labelb = 'keyb' },
                value = 3.1415
            },
            {
                time = 0,
                interval = 10,
                labels = { labelc = 'keyc', labeld = 'keyd' },
                value = 2.7182
            },
        }
    }

    ncollectd.dispatch_metric_family(fam)

    return 0
end

ncollectd.register_read(test_read)
