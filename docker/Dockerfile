FROM ubuntu:22.04

RUN apt-get update && apt-get upgrade
RUN apt-get install -y vim patch
RUN apt-get install -y build-essential bison flex libncurses5-dev gzip nkf
RUN apt-get install -y language-pack-ja-base language-pack-ja locales locales-all
RUN update-locale LANG=ja_JP.UTF-8 LANGUAGE=ja_JP:ja

ENV LANG C.UTF-8
ENV LANGUAGE en_US:

RUN mkdir -p /nethack/
COPY docker/nethack-367-src.tgz /
COPY docker/jnethack-3.6.7-0.1.diff.gz /
COPY docker/post367.diff /

RUN cd / && \
    tar xvf nethack-367-src.tgz && \
    rm nethack-367-src.tgz && \
    gzip -d jnethack-3.6.7-0.1.diff.gz

RUN cd /NetHack-3.6.7 && \
    cat ../post367.diff | patch -p1 && \
    cat ../jnethack-3.6.7-0.1.diff | patch -p1 && \
    rm ../post367.diff && \
    rm ../jnethack-3.6.7-0.1.diff

# RUN cd /NetHack-3.6.7 && \
#     sh sys/unix/setup.sh sys/unix/hints/linux && \
#     make all && \
#     make install

# Patch
COPY docker/NetHack-3.6.7-custom/src/allmain.c /NetHack-3.6.7/src/
COPY docker/NetHack-3.6.7-custom/src/eat.c /NetHack-3.6.7/src/
COPY docker/NetHack-3.6.7-custom/src/hack.c /NetHack-3.6.7/src/
COPY docker/NetHack-3.6.7-custom/src/save.c /NetHack-3.6.7/src/
COPY docker/NetHack-3.6.7-custom/src/spell.c /NetHack-3.6.7/src/
COPY docker/NetHack-3.6.7-custom/src/read.c /NetHack-3.6.7/src/
COPY docker/NetHack-3.6.7-custom/src/pickup.c /NetHack-3.6.7/src/
COPY docker/NetHack-3.6.7-custom/src/restore.c /NetHack-3.6.7/src/
COPY docker/NetHack-3.6.7-custom/src/zap.c /NetHack-3.6.7/src/
COPY docker/NetHack-3.6.7-custom/src/pray.c /NetHack-3.6.7/src/
# COPY docker/NetHack-3.6.7-custom/src/potion.c /NetHack-3.6.7/src/
COPY docker/NetHack-3.6.7-custom/sys/unix/unixmain.c /NetHack-3.6.7/sys/unix/
#COPY docker/NetHack-3.6.7-custom/sys/share/pcmain.c /NetHack-3.6.7/sys/share/

RUN cd /NetHack-3.6.7/sys/unix && \
    ./setup.sh && \
    cd /NetHack-3.6.7 && \
    sh configure && \
    make install

COPY docker/nethackrc /root/.nethackrc
