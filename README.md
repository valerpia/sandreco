# Requirements
- [ROOT](https://root.cern/)
- [edep-sim](https://github.com/ClarkMcGrew/edep-sim)

# Installation

### Get the code

```console
$ git clone https://baltig.infn.it/dune/sand-reco.git
```

### Build the binaries

```console
$ cd sand-reco
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=./.. ./..
$ make
$ make install
```

In the `bin` folder, there will be five executables:
- **Digitize** will perform digitization, 
- **Reconstruct** will reconstruct tracks in STT and clusters in ECAL
- **Analyze** will identify particles and assign them a momentum
- **FastCheck** will produce a lot of plots to check everything is ok
- **Display** displays events

In the `lib` folder, there will be two libraries:
- **libUtils.so** for utilities
- **libStruct.so** for i/o

### Setup

```console
$ source setup.sh
```

# Run

### Digitize
- Create digits of STT and cells of calorimeter

```console
$ Digitize <MC file> <digit file>
```

### Reconstruct
- Track find and fit of STT track
- Clustering of calorimeter cells

```console
$ Reconstruct <MC file> <digiti file> <reco file>
```

### Analyze
- Evaluate parameters of particles
- Evaluate neutrino energy

```console
$ Analyze <MC file> <reco file>
```

### FastCheck
- Produce several plots to check everything is ok

```console
$ FastCheck <root file> <pdf file>
```

### Display
- Display an event

```console
$ Display <event number> <MC file> <input file> [show trajectories] [show fits] [show digits]
```

# Data format

The description of the data format can be found [here](../../wiki/Data-Model)


# Contribute

- The code format can be find [here](../../wiki/Code-Formatting)
- The developing scheme is described [HowToDevelop.pdf](https://baltig.infn.it/dune/sand-reco/-/wikis/uploads/8b897fb0ea753ef767b96312bdf9ccac/HowToDevelop.pdf)

# Support

For any communication, please refer to [DUNE-ND-SAND-SOFTWARE@fnal.gov](mailto:DUNE-ND-SAND-SOFTWARE@fnal.gov)
