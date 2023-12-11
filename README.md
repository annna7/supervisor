# Supervisor

## Structure

* cli - command line interface
* daemon - daemon process
* parrot_service - service that writes to a file every 2 secs

The cli and daemon communicate via a unix socket, with the cli sending the commands received to the daemon.

### Options

* 'instance' - the index of the supervisor instance to initialize (0-99)

### Usage
* `./cli init --instance 2`
* `./cli list-supervisors`
* `./cli close --instance 2`
* `./cli create-service /home/anna/Desktop/so/supervisor/build/bin/parrot parrot 2 wow -i 10`
* `./cli suspend-service <pid> -i 10` (pid known from create-service output)
* etc


### Logs
`journalctl -u supervisor -n 10` - show last 10 commands

### Run application
* `cd build`
* `cmake ..`
* `make`
* executables and parrot log.txt file are in `build/bin`
* start daemon with `./daemon`
* run `cli` commands (from usage section)

## TODO
* create simple program with segmentation fault (e.g. acceseaza null pointer dupa 1000s)
* restart logic (e scris ceva dar nu e testat + tb. scoase fct. async unsafe din signal handling precum syslog)
* testat create-stopped
* testat status-uri si stabilit exact tranzitiile 
* polling mechanism pt open (nu va merge SIGCHILD) - ce feature-uri vrem pt ele mai exact?
* opened/created services discriminator
* dupa ce se termina un service (sau e omorat), mai apare in lista de servicii?
* concurrency - tb thread uri la ceva?
* ordonat printre loguri!
* ordonat printre erori
* ce se vrea mai exact cu cancel si pending? schedule jobs??
* library?
* terminat documentatia
* release github