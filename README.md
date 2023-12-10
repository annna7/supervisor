# Supervisor

## Structure

* cli - command line interface
* daemon - daemon process
* parrot_service - service that writes to a file every 2 secs

The two communicate via a unix socket, with the cli sending the commands received to the daemon.

## Commands

* 'init' - initialize a new supervisor instance

### Options

* 'instance' - the index of the supervisor instance to initialize (0-99)

### Usage
* `./cli init --instance 2`
* `./cli list-supervisors`
* `./cli close --instance 2`

### Logs
`journalctl -u supervisor -n 10` - show last 10 commands

### Run application
* `cd build`
* `cmake ..`
* `make`
* executables and parrot log.txt file are in `build/bin`

## TODO
* polling mechanism pt open
* opened/created services discriminator
* concurrency - tb thread uri la ceva?
* ordonat printre loguri!
* ordonat printre erori (gen service.service_name in loc de service.pid)
* library?

## testat
* supervisor free list
* flags
* service open
* status
* resume
* restarts