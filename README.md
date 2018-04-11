# cscan

cscan - internet-wide IPv4 TCP scanner for linux that doesn't require root privileges/capabilities, but is much slower than scanners that do require them.

If you can use masscan or ZMap, there's no need to use `cscan`, since those are much faster, easier to use and more portable.
`cscan` can currently run only on Linux, because it uses the `epoll` interface.

It does this:
* Tries to connect (TCP) to random specified hosts (IPv4) on random specified ports (one port per host) using... `connect`. This is of course done in non-blocking mode, in a loop, and a little more complicated.
* Spawns "module" threads (currently one), which pass scan results to all enabled "modules". They can do whatever (the default "print" module just prints the results, but it's a module since other modules may want to filter them).
* Provides an interface for modules to write their results into a ... CBA file.

This repo also contains a python "class" to read that "CBA file".

Note that the scan is synchronous. This means that for each packet we send, we expect a reply from that exact host to arrive within a specified time range. A side effect of that is that we (and the kernel) also have to remember those pending connections, which takes up memory...
It's possible to bind to a specified port and send all packets from that port (**it's done by default on port 32005**). This ensures linux won't run out of available ports and also on some weird NAT devices it prevents the NAT table from being full if only `(source IP, source port)` pairs are remembered.


## What limits cscan's speed

At least this:
* `RLIMIT_NOFILES` - determines max open (pending) sockets, `cscan` tries to increase it using rlimit, but this will usually be `1024` or `4096`.
* `nf_conntrack_max`. This practically limits `cscan` to 0.5K packets per second (65.5K per 120 seconds). If you have root privileges, it's best to disable `nf_conntrack`.
* `fs.epoll.max_user_watches` determines max number of watched sockets via epoll, it's generally bigger than 100K
* Network upload speed (one packet is usually 60 bytes) and NIC-related limits.
* If you're behind NAT, it probably has its own equivalent of `nf_conntrack` which is very bad.
* Memory - for storing pending sockets, in cscan and in the kernel internally
* CPU - i.e for generating random addresses from allowed ranges (currently that's not parallelized)

So if masscan can send 10 million packets per second, and cscan can send only 500, then this makes cscan roughly 20000 times slower :)  
There's a giant "however" here. If these limits apply to you, you can't use other scanners at all. But if you can use them because you have  privileges, then you can get rid of some local limits anyway. `cscan` won't win in that case either, but the difference won't be as big as "advertised" ... here.


## Usage

### Building

To build it you need (at least):
* Linux kernel with headers
* `gcc` (probably, uses obvious GCC extensions)
* binutils
* GNU make and it's friends

```
$ git clone --recursive https://github.com/zb3/cscan
$ cd cscan
```

Go:
```
$ make
```

There are some additional options you can pass to make:
* `METRICS=1` - build with metrics support
* `MODULES='print somemodule othermodule'` - specify modules to compile
* `ENABLED_MODULES='print othermodule'` - enable these modules by default (other compiled ones need to be enabled manually via the `-mod` parameter)
* `MUSL=1` build static executable using Musl as the C library. `musl-gcc` must be installed. 
* `GPROF=1` - actually `perf` is better...
* `DEBUG=1`
* `WITH_TLS=1` - enable Mbed TLS for use in modules, see the "TLS support" section.

```
$ make MUSL=1 WITH_TLS=1 MODULES='print rfb'
```

### Specifying targets

`cscan` understands only binary CSW files representing target ranges and binary CSP files representing target port distribution.

There are some presets:
* `presets/public-all.csw` - all public IPv4 addresses which are not reserved
* `presets/public.csw` - like the above, but excludes some /8 blocks not being used / being used by one entity.
* `presets/private.csw`

They can be used like this:
```
$ ./cscan -f presets/public.csw
```

In order to specify custom targets you need to compile them into a CSW file using the `compile-targets.py` tool. It reads input ranges from `stdin` assuming they represent a whitelist, or a blacklist if the `-b` option is specified. 

Target port distribution is specified using so called "portspec" strings containing comma separated `port:weight` pairs in the `-p` argument.

So, to scan `10.0.0.0/8` on ports 80 and 8080, sending packets to port 80 twice as often as to port 8080, you'd do this:
```
echo '10.0.0.0/8' | python compile-targets.py -p 80:2,8080:1 testtargets
```

This generates `testtargets.csw` and `testtargets.csp` files that are suitable for use with `cscan`:
```
$ ./cscan -f testtargets.csw -fp testtarget.csp
```

Actually if you pass the `-c` option to `compile-targets.py`, it will generate the `hardcoded.c` file so that after compiling,  cscan will use these by default.

You can also specify target port distribution directly as an argument to cscan:
```
$ ./cscan -f ips.csw -p 80:4,8080:1
```

**Note** that by default `cscan` uses the list generated by [this](https://github.com/zb3/scan-ignore-utils) script as the default scan targets list (`hardcoded.c` file) which excludes huge amount of addresses. **It probably excludes targets you want to scan too.**



### Setting up scan parameters

The goal here is to adjust the `-r` parameter which sets packets sent per second and the `-t` parameter which sets connect timeout in milliseconds.

First of all you should remove any obstacles like `nf_conntrack`, rlimits, qdisc limits etc if you can.

Choosing optimal parameters often requires some measurements, since too high rate may cause packets to be dropped (those received too) which will degrade scan performance. It's best to test the scan speed using port 80 or 443 and then compare how many results were there **per** X sent packets. This should also reveal other rate limits like NAT table limit if you're behind NAT...

That's why you can compile `cscan` with `METRICS` option enabled. Running `cscan` will then produce the `metrics.dat` file. You can then use the `metrics.py` tool to plot some ~~random~~ graphs if you have matplotlib installed:
```
$ python metrics.py metrics.dat
```

To test for packets being dropped, you can send so called "canary packet" to a specified host after `-ca` normal packets sent. If connection to that address fails, `cscan` will let you know.
```
$ ./cscan -c 1.1.1.1 -cp 443 -ca 5000
```

Currently cscan can only do this once... otherwise we may run out of ports (also note that sending canary packet more than once could cause false positives since NAT would remember the destination host).

### More precisely...

```
$ ./cscan -h
-f ranges_file
-pf ports_file
-p portspec

-x = don't scan, read results from stdin
-l = don't scan, just generate random targets
-d = daemonize
-o output_cba_file
-of output_files_dir = where to save files if .cba output not specified
-mod enabled_modules=print
-mcfg modules_config=

-r packets_per_second=520
-t connect_timeout=800
-m max_pending_sockets, overrides packets_per_second
-pr max_pending_results=50
-b bind_port=32005
-bi bind_ip
-bt bind_retries=5
-a auto_interval=90%, make sending packets take at least this% of timeout
-ct connect_interval = interval between sending packets in microseconds, overrides auto_interval
-e socket_list_excess=1000 = this + max_pending sockets = biggest supported fd
-c canary_ip
-cp canary_port=80
-ca canary_after=5600 packets = try to connect to canary_ip and report on failure
-st stats_every=1000 or 10000 when using cba output
```

### Scan output

By default `cscan` just prints the results to `stdout` (via the `print` module), but it can also write it's output into so a called "CBA file" explained below.
```
$ ./cscan -o file.cba
```



## More info

### CBA files

While `stdout` works well when you're only interested in open ports, it gets messy when modules which are running in separate threads also want to produce some output, especially when they want to return files. 

Since we didn't want any additional dependencies here like database servers and so on, the "CBA" file format was born.
It's a simple binary format which contains entries, each having it's module name and a series of events. Each event has it's type, code and value (codes are integers).

But the file actually consists of (thread id, event data) pairs which allows entries' events to be interleaved, and then deinterleaved by the reader library.

It's possible to store flags, numbers, strings and files there.

`cba_reader.py` contains a class to read entries (and also extract files).
```
CBA(fileobj, extract_dir='cba_files', extract_for=[], overwrite=False)
CBA.entries(module=None) # None means all.. (wat?)
```

`cba_reader_cscan.py` can read scan results and `cscan`'s statistic data and other events. For other modules, you'd need to write your own reader based on the `CBA` class that recognizes module's event codes.


### TLS support for modules

Modules can support SSL/TLS client connections using the `tsocket` interface, but only when compiled with TLS support which is disabled by default. `tsocket` supports the Mbed TLS library. In order to compile `cscan` with TLS support, you first need to build Mbed TLS from source, then use the `WITH_TLS=1` make option.


```
$ cd modules
$ git clone https://github.com/ARMmbed/mbedtls/
```

If you want [censored] to work you need to patch Mbed TLS so that it supports Anonymous Diffie-Hellman key exchange scheme. This patch is experimental though. 
```
$ patch -p0 < mbedtls_anon_dh.patch
```

Then you need to enable/disable certain configuration options. `mbedtls-configure.sh` does this, but note that it disables many other things to reduce the resulting binary size.
```
$ ./mbedtls-configure.sh
```

Then you can build Mbed TLS, using `CC='musl-gcc -static'` if you're using musl:
```
$ cd mbedtls
$ make CC='musl-gcc -static'
```

That should be enough to make the `WITH_TLS=1` option work. Static libraries belonging to Mbed TLS will be merged into `modules.a`, because `cscan` only cares about that file.


### Code

* `queue.c` - yet another blocking queue in C... 
* `socket_list.c` contains a data structure which is a hash table and a doubly linked list at the same time
* `target.c` contains methods used to generate a random target address. Uses binary search, but it's not so fast. Note that I wanted it to generate targets with uniform distribution


## FAQ

### Yes?
No.

### No?
Yes.

### Does this FAQ exist?
No.
