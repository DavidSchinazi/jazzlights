FROM ubuntu:14.04

RUN apt-get update
RUN apt-get install -y build-essential
RUN apt-get install -y cmake
RUN apt-get install -y git
RUN apt-get install -y zip


ADD . /dfsparks
WORKDIR /dfsparks

# Building PC library 
RUN rm -rf build
RUN mkdir build
RUN cd build && cmake .. && make && sudo make install

# Building demo app (this will need X11)
#RUN cd /tmp && git clone https://github.com/glfw/glfw --depth 1 &&\
#        cd glfw && cmake . && make && sudo make install
#RUN cd build && make dfsparks_demo

#ENTRYPOINT ["/bin/bash"]
#CMD ["/bin/bash"]