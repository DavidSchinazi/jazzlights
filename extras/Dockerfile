FROM ubuntu:14.04

# Install dependencies
ADD ./install-dependencies ./setup
RUN ./setup

# Set up mount point and default commmand
VOLUME /dfsparks
CMD cd /dfsparks && mkdir -p build/ubuntu && cd build/ubuntu && cmake ../.. && make test  
