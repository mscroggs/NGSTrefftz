FROM ubuntu:latest 

WORKDIR /home/app

RUN apt-get update && DEBIAN_FRONTEND="noninteractive" apt-get -y install vim python3 python3-pip python3-distutils python3-tk libpython3-dev libxmu-dev tk-dev tcl-dev cmake git g++ libglu1-mesa-dev libblas-dev liblapack-dev

#RUN pip install ngsolve ngstrefftz

RUN apt-get update
RUN apt-get update
RUN apt-get install -y software-properties-common
RUN add-apt-repository universe
RUN add-apt-repository ppa:ngsolve/nightly -y
RUN apt-get install ngsolve -y

ENV PYTHONPATH=/usr/lib/python3/dist-packages/
ENV NETGENDIR=/usr/bin/

RUN git clone --recursive https://github.com/PaulSt/NGSTrefftz /home/app/ngstrefftz
RUN cmake -B/home/app/ngstrefftz/make -S/home/app/ngstrefftz/src
RUN make -C/home/app/ngstrefftz/make
RUN make -C/home/app/ngstrefftz/make install

RUN apt-get install npm nodejs -y
RUN pip3 install jupyter_contrib_nbextensions
RUN pip3 install jupyter_nbextensions_configurator
RUN pip3 install RISE
RUN pip3 install ipywidgets
RUN pip3 install numpy scipy

RUN jupyter contrib nbextension install 
RUN jupyter nbextensions_configurator enable 
RUN jupyter nbextension enable codefolding/main
RUN jupyter nbextension enable scratchpad/main
RUN jupyter nbextension enable toc2/main
RUN jupyter nbextension enable hide_header/main
RUN jupyter nbextension enable --py widgetsnbextension
RUN pip3 install webgui_jupyter_widgets
RUN jupyter nbextension enable --py webgui_jupyter_widgets

RUN cd /home/app/ngstrefftz/external_dependencies/ngstents/tentswebgui && ./build.sh
RUN cd /home/app/ngstrefftz/external_dependencies/ngstents/tentswebgui && pip3 install .
RUN jupyter nbextension install --py tentswebgui
RUN jupyter nbextension enable --py tentswebgui
