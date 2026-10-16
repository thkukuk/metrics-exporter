# metrics-exporter

`metrics-exporter` is a [systemd](https://www.freedesktop.org/wiki/Software/systemd/)
socket-activated service which collects additional system metrics and
provides them via a `varlink` interface to
[systemd-report](https://www.freedesktop.org/software/systemd/man/latest/systemd-report.html).

Currently, `metrics-exporter` provides metrics about the SELinux status of
the system, such as whether SELinux is enabled, the running and configured
mode, the loaded policy and its version, and the state of the SELinux
policy booleans.

`metrics-exporter@.service` is started on demand by `metrics-exporter.socket`
whenever a client connects to the Varlink socket. It is not meant to be
started or enabled manually.

## Usage

```
systemctl enable --now metrics-exporter.socket
```

Alternative: this socket will be auto-activated if
[sd-report-collector-node.timer](https://github.com/thkukuk/sd-report-collector) get's enabled.

### Options

| Option | Description |
| --- | --- |
| `-d`, `--debug` | Enable debug mode. This implies `--verbose`. |
| `-v`, `--verbose` | Enable verbose logging. |
| `-?`, `--help` | Give the help list. |
| `--version` | Print program version. |

## Varlink Interface

`metrics-exporter` implements the `io.systemd.Metrics` Varlink interface,
the interface `systemd-report` uses to query the different services for 
their metrics. It is reachable through the socket
`/run/systemd/report/org.openSUSE.Metrics`, which is created by
`metrics-exporter.socket`.

| Method | Description |
| --- | --- |
| `List` | Returns the current value of every metric provided by `metrics-exporter`. |
| `Describe` | Returns the name, description and type (`counter`, `gauge`, `string` or `object`) of every metric family provided by `metrics-exporter`. |

All metric names use the `org.openSUSE.Metrics` prefix.

## Metrics

The following metric families are currently provided:

| Metric | Description |
| --- | --- |
| `org.openSUSE.Metrics.Version` | Version of the running `metrics-exporter`. |
| `org.openSUSE.Metrics.SELinux.Status` | Whether SELinux is `enabled` or `disabled`. |
| `org.openSUSE.Metrics.SELinux.Mode` | The currently running SELinux mode, `permissive` or `enforcing`. |
| `org.openSUSE.Metrics.SELinux.ConfigfileMode` | The SELinux mode configured in `/etc/selinux/config`. |
| `org.openSUSE.Metrics.SELinux.RootDir` | The SELinux root directory. |
| `org.openSUSE.Metrics.SELinux.Policy` | The name of the loaded SELinux policy. |
| `org.openSUSE.Metrics.SELinux.PolicyMLS` | Whether the loaded policy has MLS support `enabled` or `disabled`. |
| `org.openSUSE.Metrics.SELinux.PolicyDenyUnknown` | Whether unknown permissions are `allowed` or `denied` by the loaded policy. |
| `org.openSUSE.Metrics.SELinux.CheckReqProt` | The memory protection checking mode enforced by the kernel. |
| `org.openSUSE.Metrics.SELinux.MaxKernelPolicyVersion` | The maximum SELinux policy version supported by the running kernel. |
| `org.openSUSE.Metrics.SELinux.PolicyRPMVersion` | Name, version, release and architecture of the installed `selinux-policy` package. |
| `org.openSUSE.Metrics.SELinux.PolicyBooleans` | The current state (`on` or `off`) of every SELinux policy boolean, one metric object per boolean. If the pending value of a boolean differs from its active value, an additional `pending` field is reported. |

The SELinux metrics are only reported if SELinux support was compiled into
`metrics-exporter`. It can be disabled or restricted via
`metrics-exporter.conf` (see [Configuration](#configuration) below).

## Configuration

`metrics-exporter` reads its configuration from `metrics-exporter.conf`.

### Locations and precedence

The configuration is read following the [UAPI Configuration Files
Specification](https://uapi-group.org/specifications/specs/configuration_files_specification/).
The main configuration file `metrics-exporter.conf` is looked up in the
`metrics-exporter` namespace below the following directories, listed here
from lowest to highest precedence:

| Directory | Purpose |
| --- | --- |
| `/usr/share/metrics-exporter/` | Vendor defaults shipped by the distribution package. Files here should not be edited by the administrator, as they may be overwritten on package updates. |
| `/etc/metrics-exporter/` | Local administrator configuration. Settings here take precedence over the vendor defaults and are the recommended place for local changes. |

In addition to the main configuration file, drop-in files with a `.conf`
suffix are read from a `metrics-exporter.conf.d/` directory located next to
the main file in each of the directories above (for example
`/etc/metrics-exporter/metrics-exporter.conf.d/` and
`/usr/share/metrics-exporter/metrics-exporter.conf.d/`).

All files are merged into a single configuration.

### File format

The file uses a simple INI-style syntax. It is organized into sections
(groups) and `key=value` assignments. Everything following a `#` character
is treated as a comment and ignored. Leading and trailing whitespace around
values are stripped.

### Sections

| Section | Purpose |
| --- | --- |
| `[Global]` | General options controlling the behaviour of `metrics-exporter` itself. |
| `[Metrics]` | Options controlling which groups of metrics are exported. |
| `[SELinux]` | Options specific to the SELinux metrics. |

#### `[Global]`

| Option | Description |
| --- | --- |
| `debug` | A boolean value which enables debug mode. Enabling debug mode also enables verbose logging. The built-in default is `false`. This is equivalent to the `--debug` command line option. |
| `verbose` | A boolean value which enables verbose logging. The built-in default is `true`. This is equivalent to the `--verbose` command line option. |

#### `[Metrics]`

| Option | Description |
| --- | --- |
| `selinux` | A boolean value which controls whether the SELinux metrics are exported at all. The built-in default is `true`. |

#### `[SELinux]`

| Option | Description |
| --- | --- |
| `booleans` | A boolean value which controls whether the state of the SELinux policy booleans is included in the `org.openSUSE.Metrics.SELinux.PolicyBooleans` metric. The built-in default is `true`. This option has no effect if `selinux` is disabled in the `[Metrics]` section. |

### Example

`/etc/metrics-exporter/metrics-exporter.conf`:

```ini
[Global]
#debug=false
verbose=true

[Metrics]
# SELinux metrics are enabled per default
selinux=true

[SELinux]
# Do not report the individual SELinux policy booleans
booleans=false
```
