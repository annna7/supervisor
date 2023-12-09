# Supervisor

## Structure

* cli - command line interface
* daemon - daemon process

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

## TODO
* supervisor-list
* handle pid/service_name 
* service_open
* handle status for services
* polling mechanism
* handle service restarts
* implement a clean interface for sending similar signals
* concurrency?
* ordonat printre loguri!