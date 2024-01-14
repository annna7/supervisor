# Supervisor

## Structure

* cli - command line interface
* daemon - daemon process
* parrot_service - service that writes to a file every 2 secs

The cli and daemon communicate via a unix socket, with the cli sending the commands received to the daemon.

### Options

* 'instance' - the index of the supervisor instance to initialize (0-99)
* 'create-stopped' - no arguments
* 'restart-times' - number of times to restart the process

### Usage
* `./cli init --instance 2`
* `./cli list-supervisors`
* `./cli close --instance 2`
* `./cli create-service /home/anna/Desktop/so/supervisor/build/bin/parrot parrot 2 wow -i 10`
* `./cli suspend-service <pid> -i 10` (pid known from create-service output)
* `./cli open-service <pid> -i 10`
* `./cli close-service <pid> -i 10`
* `./cli suspend-service <pid> -i 10`
* `./cli service-status <pid> -i 10`
* `./cli resume-service <pid> -i 10`
* `./cli list-supervisor -i 10`
* `./cli supervisor-freelist <pid> <pid> ... -i 10`


### Logs
`journalctl -t supervisor -n 10` - show last 10 commands

### Run application
* `cd build`
* `cmake ..`
* `make`
* executables and parrot log.txt file are in `build/bin, so `cd bin`
* start daemon with `./daemon`
* run `cli` commands (from usage section)

## TODO
* Add mutex throughout the code, whenever `status` is queried/updated (1)
* Create a simple toy program with predictable segmentation fault - access null pointer after 1000s (1)
* Restart logic only for created by user programs - handle async unsafe stuff (2)
* Add discriminator for opened/created by user services and update throughout the code (1)
* Polling mechanism for open - for loop in another thread that sends signal zero to opened services and updates (with mutex) their status -> everything except the flags should work (3)
* Create custom errors (2)
* Print result of each command for user to see - use global result string, probably remove existing char *response from function signatures (3)
* Tidy up logs! (2)
* Documentation (2)
* Release (1)
* Fix date bug in existing process handler (1)
* Fix why sending signals externally (kill -SIGCONT) doesn't successfully update status
* Refactor code in daemon.c for command handling - create new file `command-parser.c` and each cli call should be a separate function (so you can call them), like for status (2)
* scheduling (cli option to wait x seconds, nice to have - for testing cancel) (2)
