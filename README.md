# Supervisor

## Introduction

**Supervisor** is a daemon that is designed to **execute** and **monitor** multiple services, running concurrently. 
It can be used to **create** new services, **open** existing ones, **kill** them, as well as **halt** and **resume** their execution.
The daemon is able to **restart crashed services**, as well as **schedule** services to be started at a later time, akin to a cron job.
Supervisor also monitors the **status** of the services over time, tracking both **internal** and **external** changes to its services.
The daemon is able to handle multiple instances of itself, each with its own set of services, allowing for a clean separation of concerns.
The commands received by the daemon and its responses are logged via `syslog` and can be viewed using `journalctl -t supervisor`.

## Structure
### Core
* **cli** - command line interface, used to communicate with the daemon
* **daemon** - daemon process, responsible for the core functionality
### Toy Services for Testing
* **parrot_service** - service that writes a to a file every 2 secs
* **test_restart** - service that segfaults after a user-defined time

## Description

The cli and daemon communicate via a **unix socket**, with the cli sending the commands received to the daemon.
There are 3 main threads in the daemon:
* **main thread** 
  - listens for new commands from the cli received via the socket, parses them and sends them to the appropriate command handler
* **signal handler thread** 
  - handles the children services (i.e. have directly been created by the user via the supervisor daemon)
  - communicates with the SIGCHILD handler via a pipe and updates the status of the process that sent the signal (and restarts it if necessary)
* **polling thread** 
  - handles the opened services (i.e. have not been created by the supervisor daemon itself, but have been opened subsequently by the user)
  - periodically sends signal 0 to all opened services and updates their status accordingly

## Service Statuses

* **RUNNING** - service is running
* **STOPPED** - service execution is suspended, but can be resumed (e.g. **_SIGSTOP_**)
* **KILLED** - service execution is suspended and cannot be resumed (e.g. **_SIGKILL_**)
* **CRASHED** - service execution has crashed and cannot be resumed, since there are no restart times left (e.g. **_SIGSEGV_**)
* **PENDING** - service is scheduled by the user to be started at a later time

### Options

* `instance` - the index of the supervisor instance to initialize (0-99)
* `create-stopped`- no arguments
* `restart-times` - number of times to restart the process

### Usage
* `./cli init --instance <instance>`
* `./cli list-supervisors`
* `./cli close --instance <instance>`
* `./cli create-service <path> <args> {-c {delay_until_start}} {-r <restart_times>} -i <instance>`
* `./cli open-service <pid> -i <instance>`
* `./cli suspend-service <pid> -i <instance>` - pause service execution (**RUNNING** -> **STOPPED**)
* `./cli resume-service <pid> -i <instance>` - resume execution of a suspended service (**STOPPED** -> **RUNNING**)
* `./cli close-service <pid> -i <instance>` - kill service execution (**RUNNING** -> **KILLED**)
* `./cli remove-service <pid> -i <instance>` - remove service from supervisor, regardless of status
* `./cli service-status <pid> -i <instance>` - get status of service
* `./cli list-supervisor -i <instance>` - list all services of supervisor
* `./cli supervisor-freelist <pid> <pid> ... -i 10` - remove multiple services from supervisor
* `./cli supervisor-cancel <pid> -i <instance>` - cancel start of scheduled service and remove it from supervisor (**PENDING** -> none)

Note: The `pids` are known from the output of `create-service` and `list-supervisor` commands.

### Run application
* `cd build`
* `cmake ..`
* `make`
* executables and parrot log.txt file are in `build/bin, so `cd bin`
* start daemon with `./daemon`
* run `cli` commands (from usage section)

### Team
* Pecheanu Anna
* Mihalcea Alexandru
* Popa Bianca