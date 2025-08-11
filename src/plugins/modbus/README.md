NCOLLECTD-MODBUS(5) - File Formats Manual

# NAME

**ncollectd-modbus** - Documentation of ncollectd's modbus plugin

# SYNOPSIS

	load-plugin modbus
	plugin modbus {
	    data data-name {
	        register-base base
	        register-type int16|int32|int64|uint16|uint32|uint64|float|int32le|int32le|floatle
	        register-cmd read-holding|read-input
	        metric metric
	        help help
	        label key value
	        scale scale
	        shift shift
	    }
	    host host {
	        address address
	        port port
	        device devicenode
	        baudrate baudrate
	        uart-type RS485|RS422|RS232
	        interval interval
	        metric-prefix prefix
	        label key value
	        slave slave {
	            metric-prefix prefix
	            label key value
	            collect data-name
	        }
	    }
	}

# DESCRIPTION

The **modbus** connects to a Modbus "slave" via Modbus/TCP or Modbus/RTU and
reads register values.
It supports reading single registers (unsigned 16 bit values), large integer
values (unsigned 32 bit and 64 bit values) and floating point values
(two registers interpreted as IEEE floats in big endian notation).

**data** *name*

> Data blocks define a mapping between register numbers and the metrics used by
> *ncollectd*.

> Within **data** blocks, the following options are allowed:

> **register-base** *number*

> > Configures the base register to read from the device.
> > If the option **register-type** has been set to **uint32** or **float**,
> > this and the next register will be read
> > (the register number is increased by one).

> **register-type** *int16|int32|int64|uint16|uint32|uint64|float|int32le|int32le|floatle*

> > Specifies what kind of data is returned by the device.
> > This defaults to **uint16**.
> > If the type is **int32**, **int32le**, **uint32**, **int32le**,
> > **float** or **floatle**, two 16 bit registers
> > at **register-base** and **register-rase+1** will be read and the data is
> > combined into one 32 value.
> > For **int32**, **uint32** and **float** the
> > most significant 16 bits are in the register at **register-base** and
> > the least significant 16 bits are in the register at **register-base+1**.
> > For **int32le**, **uint32le**, or **float32le**, the high and low order
> > registers are swapped with the most significant 16 bits in
> > the **register-base+1** and the least significant 16 bits in
> > **register-base**.
> > If the type is **int64** or **uint64**, four 16 bit
> > registers at **register-base**, **register-base+1**, **register-base+2**
> > and **register-base+3** will be read and the data combined into one
> > 64 value.

> **register-cmd** *read-holding|read-input*

> > Specifies register type to be collected from device.
> > Works only with libmodbus 2.9.2 or higher.
> > Defaults to **read-holding**.

> **metric** *metric*

> > Set the metric name.

> **help** *help*

> > Set the **help** text for the metric.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple times in the **data** block.

> **type** *gauge|counter|unknow*

> > The **type** for the metric.
> > Must be *gauge*, *counter* or *unknow*.
> > If not set is *unknow*.
> > There must be exactly one **type** option inside each **data** block.

> **scale** *value*

> > The values taken from device are multiplied by *value*.
> > The field is optional and the default is **1.0**.

> **shift** *value*

> > *Value* is added to values from device after they have been multiplied by
> > **scale** value.
> > The field is optional and the default value is **0.0**.

**host** *name*&zwnj;**&zwnj;**

> Host blocks are used to specify to which hosts to connect and what data to read
> from their "slaves".
> The string argument *name* is used as hostname when dispatching the values
> to *ncollectd*.

> Within **host** blocks, the following options are allowed:

> **address** *hostname*

> > For Modbus/TCP, specifies the node name (the actual network address) used to
> > connect to the host.
> > This may be an IP address or a hostname.
> > Please note that the used
> > *libmodbus* library only supports IPv4 at the moment.

> **port** *service*

> > For Modbus/TCP, specifies the port used to connect to the host.
> > The port can either be given as a number or as a service name.
> > Please note that the *service* argument must be a string, even if ports
> > are given in their numerical form.
> > Defaults to "502".

> **device** *devicenode*

> > For Modbus/RTU, specifies the path to the serial device being used.

> **baudrate** *baudrate*

> > For Modbus/RTU, specifies the baud rate of the serial device.
> > Note, connections currently support only 8/N/1.

> **uart-type** *RS485|RS422|RS232*

> > For Modbus/RTU, specifies the type of the serial device.
> > RS232, RS422 and RS485 are supported.
> > Defaults to RS232.
> > Available only on Linux systems with libmodbus&gt;=2.9.4.

> **interval** *interval*

> > Sets the interval (in seconds) in which the values will be collected from this
> > host.
> > By default the global **interval** setting will be used.

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metric name in the **data**.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple times in the **host** block.

> **slave** *salve*

> > Over each connection, multiple Modbus devices may be reached.
> > The slave ID is used to specify which device should be addressed.
> > For each device you want to query, one **slave** block must be given.

> > Within **slave** blocks, the following options are allowed:

> > **metric-prefix** *prefix*

> > > Prepends *prefix* to the metric name in the **data**.

> > **label** *key* *value*

> > > Append the label *key*=*value* to the submitting metrics.
> > > Can appear multiple times in the **slave** block.

> > **collect** *data-name*

> > > Specifies which data to retrieve from the device.
> > > *data-name* must be the same string as the *name* argument passed
> > > to a **data** block.
> > > You can specify this option multiple times to collect more than one value
> > > from a slave.
> > > At least one **collect** option is mandatory.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
