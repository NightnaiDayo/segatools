FROM fedora:latest

LABEL description="Build environment for segatools"

RUN dnf -y install meson ninja-build make zip clang mingw64-gcc.x86_64 mingw32-gcc.x86_64 git

RUN mkdir /segatools
WORKDIR /segatools

VOLUME [ "/segatools" ]

ENTRYPOINT [ "make", "dist" ]
